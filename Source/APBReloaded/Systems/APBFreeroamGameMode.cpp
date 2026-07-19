#include "APBFreeroamGameMode.h"
#include "APBGameInstanceSubsystem.h"
#include "APBPlayerState.h"
#include "APBFreeroamCharacter.h"
#include "APBDriveableVehicle.h"
#include "APBInteractable.h"
#include "APBBotNPC.h"
#include "APBFreeroamHUD.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"

// ASkyAtmosphere is declared in Components/SkyAtmosphereComponent.h in UE 5.8

AAPBFreeroamGameMode::AAPBFreeroamGameMode()
{
	DistrictId = TEXT("Financial");
	DefaultPawnClass = AAPBFreeroamCharacter::StaticClass();
	PlayerStateClass = AAPBPlayerState::StaticClass();
	HUDClass = AAPBFreeroamHUD::StaticClass();
	bLoadPlacementManifest = true;
	PrimaryActorTick.bCanEverTick = true;
	StreamRadiusCm = 60000.f; // multi-block San Paro actor spans need larger initial chunk
}

void AAPBFreeroamGameMode::BeginPlay()
{
	// Resolve district before Super so DistrictGameMode session id uses correct district
	ResolveDistrictFromMap();
	Super::BeginPlay();
	if (bLoadPlacementManifest && GetWorld() && GetWorld()->GetNetMode() != NM_Client)
	{
		LoadDistrictContent();
	}
}

static FString APB_ResolveFreeroamScratchDir()
{
	FString Scratch = FPlatformMisc::GetEnvironmentVariable(TEXT("APB_SCRATCH"));
	if (Scratch.IsEmpty())
	{
		// Current goal implementer scratch
		Scratch = TEXT("C:/Users/Support/AppData/Local/Temp/grok-goal-ceb9fe051078/implementer");
	}
	Scratch.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (!Scratch.EndsWith(TEXT("/"))) Scratch += TEXT("/");
	IFileManager::Get().MakeDirectory(*Scratch, true);
	return Scratch;
}

void AAPBFreeroamGameMode::AppendFreeroamLog(const FString& Line)
{
	const FString FreeroamLogFile = APB_ResolveFreeroamScratchDir() + TEXT("freeroam_district.log");
	FFileHelper::SaveStringToFile(Line, *FreeroamLogFile,
		FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
}

void AAPBFreeroamGameMode::ResolveDistrictFromMap()
{
	if (!GetWorld()) return;
	const FString MapName = GetWorld()->GetMapName();
	DistrictId = UAPBDistrictPlacementLoader::ResolveDistrictIdFromMapName(MapName);
	UE_LOG(LogTemp, Warning, TEXT("APB freeroam map=%s district_id=%s"), *MapName, *DistrictId);
	AppendFreeroamLog(FString::Printf(TEXT("MAP=%s DISTRICT=%s\n"), *MapName, *DistrictId));
}

void AAPBFreeroamGameMode::EnsureDistrictLighting(const FVector& At)
{
	UWorld* World = GetWorld();
	if (!World || bLightingReady) return;

	// Prefer simple lit path: Lumen + empty black sky makes untextured city look like a void
	if (IConsoleVariable* GI = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod")))
	{
		GI->Set(0, ECVF_SetByCode);
	}
	if (IConsoleVariable* Refl = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ReflectionMethod")))
	{
		Refl->Set(0, ECVF_SetByCode);
	}
	if (IConsoleVariable* Exp = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptationQuality")))
	{
		Exp->Set(0, ECVF_SetByCode); // kill auto-exposure crushing scene to black
	}

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Sun — freeroam umaps only had a PointLight shell; without this WorldGrid meshes are black
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(At + FVector(0, 0, 5000.f), FRotator(-50.f, 40.f, 0.f), Sp);
	if (Sun)
	{
		if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			C->SetIntensity(20.f);
			C->SetLightColor(FLinearColor(1.f, 0.98f, 0.92f));
			C->SetMobility(EComponentMobility::Movable);
			C->SetAtmosphereSunLight(true);
			C->SetCastShadows(true);
			C->SetSpecularScale(0.3f);
		}
		SpawnedActors.Add(Sun);
	}

	// Sky atmosphere → non-black clear color / ambient upper hemisphere
	ASkyAtmosphere* Atmos = World->SpawnActor<ASkyAtmosphere>(At, FRotator::ZeroRotator, Sp);
	if (Atmos)
	{
		SpawnedActors.Add(Atmos);
	}

	ASkyLight* Sky = World->SpawnActor<ASkyLight>(At + FVector(0, 0, 2000.f), FRotator::ZeroRotator, Sp);
	if (Sky)
	{
		if (USkyLightComponent* C = Cast<USkyLightComponent>(Sky->GetLightComponent()))
		{
			C->SetMobility(EComponentMobility::Movable);
			C->SetIntensity(3.5f);
			C->bRealTimeCapture = true;
			C->SetLowerHemisphereColor(FLinearColor(0.15f, 0.16f, 0.18f));
			C->bLowerHemisphereIsBlack = false;
			C->RecaptureSky();
		}
		SpawnedActors.Add(Sky);
	}

	// Height fog tints empty distance so the far field is not pure black
	AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(At, FRotator::ZeroRotator, Sp);
	if (Fog)
	{
		if (UExponentialHeightFogComponent* FC = Fog->GetComponent())
		{
			FC->SetFogDensity(0.0008f);
			FC->SetFogHeightFalloff(0.12f);
			FC->SetFogInscatteringColor(FLinearColor(0.45f, 0.55f, 0.75f));
			FC->SetDirectionalInscatteringColor(FLinearColor(1.f, 0.9f, 0.7f));
			FC->SetDirectionalInscatteringExponent(4.f);
			FC->SetDirectionalInscatteringStartDistance(5000.f);
			FC->SetVolumetricFog(false);
		}
		SpawnedActors.Add(Fog);
	}

	// Fixed exposure so Lumen/empty HDR does not crush the image to black
	APostProcessVolume* PPV = World->SpawnActor<APostProcessVolume>(At, FRotator::ZeroRotator, Sp);
	if (PPV)
	{
		PPV->bUnbound = true;
		PPV->Priority = 100.f;
		PPV->BlendWeight = 1.f;
		FPostProcessSettings& S = PPV->Settings;
		S.bOverride_AutoExposureMethod = true;
		S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = 1.0f;
		S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		S.AutoExposureApplyPhysicalCameraExposure = false;
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(1.05f, 1.05f, 1.05f, 1.f);
		SpawnedActors.Add(PPV);
	}

	// Large visible ground plane at street level (roads sit ~Z=0; player_start Z~600)
	{
		UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (Plane)
		{
			const FVector GroundAt(At.X, At.Y, 5.f);
			AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(GroundAt, FRotator::ZeroRotator, Sp);
			if (Floor)
			{
				Floor->SetMobility(EComponentMobility::Movable);
				UStaticMeshComponent* SMC = Floor->GetStaticMeshComponent();
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetStaticMesh(Plane);
				// Plane is 100x100 cm; scale to ~4km square
				Floor->SetActorScale3D(FVector(4000.f, 4000.f, 1.f));
				SMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				SMC->SetCollisionResponseToAllChannels(ECR_Block);
				UAPBDistrictPlacementLoader::EnsureVisibleMeshMaterials(SMC);
				SpawnedActors.Add(Floor);
			}
		}
	}

	bLightingReady = true;
	const int32 HasAtmos = Atmos ? 1 : 0;
	const int32 HasFog = Fog ? 1 : 0;
	const int32 HasPPV = PPV ? 1 : 0;
	UE_LOG(LogTemp, Warning, TEXT("APB freeroam DISTRICT_LIGHT sun=%d sky=%d atmos=%d fog=%d ppv=%d at=(%.0f,%.0f,%.0f)"),
		Sun ? 1 : 0, Sky ? 1 : 0, HasAtmos, HasFog, HasPPV, At.X, At.Y, At.Z);
	AppendFreeroamLog(FString::Printf(TEXT("DISTRICT_LIGHT sun=%d sky=%d atmos=%d fog=%d ppv=%d at=(%.0f,%.0f,%.0f)\n"),
		Sun ? 1 : 0, Sky ? 1 : 0, HasAtmos, HasFog, HasPPV, At.X, At.Y, At.Z));
}

void AAPBFreeroamGameMode::AlignPlayerStartsAndTeleport(const FVector& At)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Move every PlayerStart in the map to district city coords (shell maps often leave PS at origin)
	TArray<AActor*> Starts;
	UGameplayStatics::GetAllActorsOfClass(World, APlayerStart::StaticClass(), Starts);
	for (AActor* A : Starts)
	{
		if (A) A->SetActorLocation(At);
	}
	if (Starts.Num() == 0)
	{
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APlayerStart* PS = World->SpawnActor<APlayerStart>(At, FRotator::ZeroRotator, Sp);
		if (PS) SpawnedActors.Add(PS);
	}

	// Street-level stand: roads ~Z=0; raw player_start Z often mid-building (~600)
	const FVector Stand(At.X, At.Y, FMath::Max(At.Z, 120.f));
	// Teleport already-possessed pawns (PIE can spawn before LoadDistrictContent finishes props)
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		if (APawn* P = PC->GetPawn())
		{
			P->SetActorLocation(Stand + FVector(0, 0, 120.f), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(FRotator(-20.f, 45.f, 0.f));
		}
	}
	AppendFreeroamLog(FString::Printf(TEXT("PLAYER_ALIGN at=(%.0f,%.0f,%.0f) player_starts=%d\n"),
		At.X, At.Y, At.Z, Starts.Num()));
}

AActor* AAPBFreeroamGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (bManifestLoaded && GetWorld())
	{
		TArray<AActor*> Starts;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Starts);
		// Prefer a start near manifest PlayerStart
		AActor* Best = nullptr;
		float BestD = TNumericLimits<float>::Max();
		for (AActor* A : Starts)
		{
			if (!A) continue;
			const float D = FVector::DistSquared(A->GetActorLocation(), CachedManifest.PlayerStart);
			if (D < BestD) { BestD = D; Best = A; }
		}
		if (Best) return Best;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AAPBFreeroamGameMode::LoadDistrictContent()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FString ContentDir = FPaths::ProjectContentDir();
	if (!UAPBDistrictPlacementLoader::LoadManifestForDistrict(ContentDir, DistrictId, CachedManifest))
	{
		UE_LOG(LogTemp, Warning, TEXT("APB: no placement manifest for %s — still spawning playable props"), *DistrictId);
		AppendFreeroamLog(FString::Printf(TEXT("STREAM_SPAWN district=%s spawned=0 total=0 error=no_manifest\n"), *DistrictId));
		bManifestLoaded = false;
		EnsureDistrictLighting(FVector(0, 0, 200));
		SpawnPlayableWorldProps();
		return;
	}
	if (UAPBDistrictPlacementLoader::ManifestUsesEngineCubes(CachedManifest))
	{
		UE_LOG(LogTemp, Error, TEXT("APB: manifest references Engine cubes — refusing mesh spawn, still props"));
		AppendFreeroamLog(FString::Printf(TEXT("STREAM_SPAWN district=%s spawned=0 error=engine_cubes\n"), *DistrictId));
		bManifestLoaded = false;
		EnsureDistrictLighting(CachedManifest.PlayerStart);
		SpawnPlayableWorldProps();
		return;
	}
	bManifestLoaded = true;

	const FVector Center = CachedManifest.PlayerStart;
	EnsureDistrictLighting(Center);
	AlignPlayerStartsAndTeleport(Center);

	// Invisible ground for walk/drive (box collision, no BasicShapes mesh)
	{
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector GLoc = Center + FVector(0, 0, -100.f);
		AActor* Ground = World->SpawnActor<AActor>(AActor::StaticClass(), GLoc, FRotator::ZeroRotator, Sp);
		if (Ground)
		{
			UBoxComponent* Box = NewObject<UBoxComponent>(Ground, TEXT("DistrictGround"));
			Box->SetBoxExtent(FVector(200000.f, 200000.f, 80.f));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->SetMobility(EComponentMobility::Static);
			Ground->SetRootComponent(Box);
			Box->RegisterComponent();
			Ground->AddInstanceComponent(Box);
			Ground->SetActorEnableCollision(true);
			SpawnedActors.Add(Ground);
		}
	}

	// Initial stream around player start (not monolithic always-resident full set if large)
	int32 LoadFailed = 0, InRadius = 0, Skipped = 0;
	const int32 N = UAPBDistrictPlacementLoader::SpawnFromManifestNearEx(
		World, CachedManifest, Center, StreamRadiusCm, SpawnedActors, SpawnedPlacementKeys,
		&LoadFailed, &InRadius, &Skipped);
	LastMeshLoadFailed = LoadFailed;
	LastInRadius = InRadius;

	const int32 NearMetric = UAPBDistrictPlacementLoader::CountPlacementsNear(CachedManifest, Center, StreamRadiusCm);

	UE_LOG(LogTemp, Warning,
		TEXT("APB freeroam STREAM_SPAWN n=%d district=%s package=%s total_placements=%d keys=%d radius=%.0f bound=%d hit=%.2f in_radius=%d load_failed=%d near_metric=%d"),
		N, *CachedManifest.DistrictId, *CachedManifest.SourcePackage,
		CachedManifest.Placements.Num(), SpawnedPlacementKeys.Num(), StreamRadiusCm,
		CachedManifest.BoundCount, CachedManifest.HitRate, InRadius, LoadFailed, NearMetric);

	AppendFreeroamLog(FString::Printf(
		TEXT("STREAM_SPAWN district=%s package=%s chunks=%d spawned=%d total=%d radius=%.0f player_start=(%.0f,%.0f,%.0f) in_radius=%d load_failed=%d near_metric=%d\n"),
		*CachedManifest.DistrictId, *CachedManifest.SourcePackage, CachedManifest.StreamChunkCount,
		N, CachedManifest.Placements.Num(), StreamRadiusCm,
		CachedManifest.PlayerStart.X, CachedManifest.PlayerStart.Y, CachedManifest.PlayerStart.Z,
		InRadius, LoadFailed, NearMetric));
	// Honest mesh-bind line (spawnable bound count vs full manifest total)
	AppendFreeroamLog(FString::Printf(
		TEXT("BOUND_SPAWN district=%s bound=%d total_manifest=%d hit_rate=%.3f loaded_bound=%d spawned_near=%d load_failed=%d\n"),
		*CachedManifest.DistrictId, CachedManifest.BoundCount, CachedManifest.ManifestTotal,
		CachedManifest.HitRate, CachedManifest.bLoadedBoundManifest ? 1 : 0, N, LoadFailed));
	AppendFreeroamLog(FString::Printf(
		TEXT("MESH_LOAD district=%s attempted_in_radius=%d spawned=%d failed=%d skipped_dup=%d\n"),
		*CachedManifest.DistrictId, InRadius, N, LoadFailed, Skipped));

	// Hero preview (Financial contact as landmark)
	if (UStaticMesh* Hero = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha.m_contact_enforcement_larocha")))
	{
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(
			CachedManifest.PlayerStart + FVector(200.f, -200.f, 0.f), FRotator(0, 45, 0));
		if (A)
		{
			A->GetStaticMeshComponent()->SetStaticMesh(Hero);
			UAPBDistrictPlacementLoader::EnsureVisibleMeshMaterials(A->GetStaticMeshComponent());
			SpawnedActors.Add(A);
		}
	}

	AAPBDriveableVehicle* V = World->SpawnActor<AAPBDriveableVehicle>(CachedManifest.VehicleStart, FRotator::ZeroRotator);
	if (V)
	{
		V->CatalogVehicleId = TEXT("Vehicle_Car_A_UtilityEstate");
		V->ApplyCatalogVisualMesh();
		SpawnedActors.Add(V);
		++SpawnedVehicles;
	}
	SpawnPlayableWorldProps();
	// Second align after props (pawns may have possessed during prop spawn)
	AlignPlayerStartsAndTeleport(Center);
}

void AAPBFreeroamGameMode::SpawnPlayableWorldProps()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	const FVector Base = bManifestLoaded ? CachedManifest.PlayerStart : FVector(0, 0, 200);

	auto SpawnIx = [&](EAPBInteractableKind Kind, const FVector& Off) -> AAPBInteractable*
	{
		AAPBInteractable* A = World->SpawnActor<AAPBInteractable>(Base + Off, FRotator::ZeroRotator);
		if (A)
		{
			A->Kind = Kind;
			SpawnedActors.Add(A);
		}
		return A;
	};
	if (SpawnIx(EAPBInteractableKind::Mailbox, FVector(350, 0, 0))) ++SpawnedMailbox;
	if (SpawnIx(EAPBInteractableKind::AmmoBox, FVector(450, 120, 0))) ++SpawnedAmmo;
	if (SpawnIx(EAPBInteractableKind::Resupply, FVector(450, -120, 0))) ++SpawnedResupply;
	if (SpawnIx(EAPBInteractableKind::Contact, FVector(250, 200, 0))) { /* contact kiosk */ }

	// Opposition / ambient bots
	const int32 BotN = 6;
	for (int32 i = 0; i < BotN; ++i)
	{
		const float Ang = (2.f * PI * i) / BotN;
		const FVector Off(FMath::Cos(Ang) * 900.f, FMath::Sin(Ang) * 900.f, 50.f);
		AAPBBotNPC* Bot = World->SpawnActor<AAPBBotNPC>(Base + Off, FRotator(0, Ang * 57.3f, 0));
		if (Bot)
		{
			Bot->bHostile = (i % 2) == 0;
			Bot->DisplayLabel = Bot->bHostile ? TEXT("Opposition") : TEXT("Civilian");
			SpawnedActors.Add(Bot);
			++SpawnedNpc;
		}
	}

	// Extra driveable fleet near player
	static const TCHAR* Fleet[] = {
		TEXT("Vehicle_V_A_2DrCoupe"), TEXT("Vehicle_V_A_Taxi"), TEXT("Vehicle_V_A_SUV"), TEXT("Vehicle_V_A_2DrVan")
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Fleet); ++i)
	{
		const FVector Off(600.f + i * 350.f, -400.f, 40.f);
		AAPBDriveableVehicle* V = World->SpawnActor<AAPBDriveableVehicle>(Base + Off, FRotator(0, 90, 0));
		if (V)
		{
			V->CatalogVehicleId = Fleet[i];
			V->ApplyCatalogVisualMesh();
			SpawnedActors.Add(V);
			++SpawnedVehicles;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("APB PLAYABLE_PROPS bots=%d mailbox=%d ammo=%d resupply=%d vehicles=%d"),
		SpawnedNpc, SpawnedMailbox, SpawnedAmmo, SpawnedResupply, SpawnedVehicles);

	const FString Scratch = APB_ResolveFreeroamScratchDir();
	FFileHelper::SaveStringToFile(
		FString::Printf(TEXT("PLAYABLE_PROPS bots=%d mailbox=%d ammo=%d resupply=%d vehicles=%d\n"),
			SpawnedNpc, SpawnedMailbox, SpawnedAmmo, SpawnedResupply, SpawnedVehicles),
		*(Scratch + TEXT("playable_after_ui.log")),
		FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
}

void AAPBFreeroamGameMode::RefreshStreamAroundPlayers()
{
	if (!bManifestLoaded || !GetWorld()) return;
	FVector Center = CachedManifest.PlayerStart;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (APawn* P = PC->GetPawn()) Center = P->GetActorLocation();
	}
	int32 LoadFailed = 0, InRadius = 0, Skipped = 0;
	const int32 Added = UAPBDistrictPlacementLoader::SpawnFromManifestNearEx(
		GetWorld(), CachedManifest, Center, StreamRadiusCm, SpawnedActors, SpawnedPlacementKeys,
		&LoadFailed, &InRadius, &Skipped);
	if (Added > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("APB stream added %d near (%.0f,%.0f) keys=%d failed=%d"),
			Added, Center.X, Center.Y, SpawnedPlacementKeys.Num(), LoadFailed);
	}
}

void AAPBFreeroamGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client) return;
	StreamAccum += DeltaSeconds;
	if (StreamAccum >= 0.5f)
	{
		StreamAccum = 0.f;
		RefreshStreamAroundPlayers();
	}
}

void AAPBFreeroamGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (!HasAuthority()) return;

	// Always drop new players on district city start (not empty shell map origin)
	if (bManifestLoaded && NewPlayer)
	{
		const FVector Base = CachedManifest.PlayerStart;
		const FVector At(Base.X, Base.Y, FMath::Max(Base.Z, 120.f) + 120.f);
		if (APawn* P = NewPlayer->GetPawn())
		{
			P->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			NewPlayer->SetControlRotation(FRotator(-20.f, 45.f, 0.f));
		}
	}

	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB || !NewPlayer) return;

	const FString Name = NewPlayer->PlayerState ? NewPlayer->PlayerState->GetPlayerName() : TEXT("Peer");
	// Server Domain only: join peer into host district session
	if (APB->GetPhase() != TEXT("District"))
	{
		APB->CreateCharacter(Name, false);
		APB->JoinDistrict(DistrictId);
	}
	else
	{
		const FString Sid = SessionId.IsEmpty() ? APB->GetSessionId() : SessionId;
		APB->JoinDistrictAsPeer(Sid, Name);
	}
	// Push host Domain snapshot to ALL PlayerStates (including newly joined) for OnRep
	APB->PushDomainSnapshotToAllPlayerStates();
	// Deferred second push so late-joining clients still receive authority economy/mission
	if (UWorld* World = GetWorld())
	{
		FTimerHandle Th;
		World->GetTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [APB]()
		{
			if (APB) APB->PushDomainSnapshotToAllPlayerStates();
		}), 1.0f, false);
	}
}
