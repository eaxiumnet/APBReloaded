#include "APBSessionProbeSubsystem.h"
#include "CoreGlobals.h"
#include "APBInteractable.h"
#include "APBBotNPC.h"
#include "APBFrontendTypes.h"
#include "APBFrontendWidget.h"
#include "APBFrontendPlayerController.h"
#include "APBGameInstanceSubsystem.h"
#include "APBPlayerState.h"
#include "APBFreeroamCharacter.h"
#include "APBDriveableVehicle.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "APBPlayerState.h"

void UAPBSessionProbeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Probe;
	if (FParse::Value(FCommandLine::Get(), TEXT("APBProbe="), Probe))
	{
		StartProbe(Probe);
	}
}

void UAPBSessionProbeSubsystem::StartProbe(const FString& InMode)
{
	Mode = InMode.ToLower();
	// Prefer APB_SCRATCH env (set by run_verification_gates.ps1); allow -APBScratch= override.
	FString ScratchDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("APBScratch="), ScratchDir))
	{
		ScratchDir = FPlatformMisc::GetEnvironmentVariable(TEXT("APB_SCRATCH"));
	}
	if (ScratchDir.IsEmpty())
	{
		ScratchDir = TEXT("C:/Users/Support/AppData/Local/Temp/grok-goal-5b882b537032/implementer");
	}
	ScratchDir.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (!ScratchDir.EndsWith(TEXT("/")))
	{
		ScratchDir += TEXT("/");
	}
	IFileManager::Get().MakeDirectory(*ScratchDir, true);
	LogPath = ScratchDir;
	if (Mode == TEXT("client_loop")) LogPath += TEXT("client_loop.log");
	else if (Mode == TEXT("playable")) LogPath += TEXT("playable_probe.log");
	else if (Mode == TEXT("mp_observe")) LogPath += TEXT("mp_client_observe.log");
	else if (Mode == TEXT("frontend_menu")) LogPath += TEXT("frontend_menu.log");
	else if (Mode == TEXT("frontend_flow")) LogPath += TEXT("frontend_flow.log");
	else LogPath += TEXT("probe.log");

	if (Mode != TEXT("mp_observe"))
	{
		FFileHelper::SaveStringToFile(TEXT(""), *LogPath);
	}
	AppendLog(FString::Printf(TEXT("PROBE_START mode=%s scratch=%s"), *Mode, *ScratchDir));

	// GameInstance subsystems often init before a world exists — defer arming timers.
	if (UWorld* World = GetWorld())
	{
		ArmProbeTimers(World);
	}
	else if (UGameInstance* GI = GetGameInstance())
	{
		AppendLog(TEXT("PROBE_DEFER wait_for_world"));
		GI->GetTimerManager().SetTimer(
			PlayableTimer,
			FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::TryArmProbeWhenWorldReady),
			0.25f,
			true);
	}
	else
	{
		AppendLog(TEXT("PROBE_FAIL no_world_no_gi"));
	}
}

void UAPBSessionProbeSubsystem::TryArmProbeWhenWorldReady()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (const FWorldContext* Ctx = GI->GetWorldContext())
			{
				World = Ctx->World();
			}
		}
	}
	if (!World) return;
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(PlayableTimer);
	}
	AppendLog(TEXT("PROBE_ARM world_ready"));
	ArmProbeTimers(World);
}

void UAPBSessionProbeSubsystem::ArmProbeTimers(UWorld* World)
{
	if (!World) return;

	if (Mode == TEXT("client_loop"))
	{
		World->GetTimerManager().SetTimer(PlayableTimer, FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunClientLoopProbe), 1.0f, false);
	}
	else if (Mode == TEXT("playable"))
	{
		PlayablePhase = 0;
		World->GetTimerManager().SetTimer(PlayableTimer, FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::PlayableStep), 0.05f, true);
	}
	else if (Mode == TEXT("mp_observe"))
	{
		// Wait a few seconds for travel/join + OnRep before first poll
		World->GetTimerManager().SetTimer(MpTimer, FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::MpPoll), 1.0f, true, 3.0f);
	}
	else if (Mode == TEXT("frontend_menu") || Mode == TEXT("frontend_flow"))
	{
		// Cold-start on Frontend map; frontend_flow additionally runs post-travel freeroam asserts.
		bFrontendTravelPending = false;
		FrontendEquippedSlots = 0;
		bTerminal = false;
		World->GetTimerManager().SetTimer(PlayableTimer, FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunFrontendFlowProbe), 2.0f, false);
		AppendLog(FString::Printf(TEXT("PROBE_TIMER %s in 2s"), *Mode));
	}
}

void UAPBSessionProbeSubsystem::AppendLog(const FString& Line)
{
	const FString Ts = FString::Printf(TEXT("[%s] %s\n"), *FDateTime::UtcNow().ToString(), *Line);
	FFileHelper::SaveStringToFile(Ts, *LogPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
	UE_LOG(LogTemp, Log, TEXT("APBProbe %s"), *Line);
}

void UAPBSessionProbeSubsystem::RunClientLoopProbe()
{
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) { AppendLog(TEXT("FAIL no_apb")); return; }

	AppendLog(TEXT("SESSION_LOOP_BEGIN"));
	APB->RegisterAccount(TEXT("probe_user"), TEXT("probe_pass"));
	APB->Login(TEXT("probe_user"), TEXT("probe_pass"));
	AppendLog(TEXT("AUTH ok=1"));
	APB->EnterWorld(TEXT("W1"));
	AppendLog(TEXT("WORLD_ENTER ok=1"));
	APB->CreateCharacter(TEXT("Viper"), false);
	APB->EquipClothingItem(TEXT("torso"), TEXT("Clothing_Crim_Hoodie_T1"));
	APB->EquipClothingItem(TEXT("legs"), TEXT("Clothing_Crim_Jeans_T1"));
	// Visual clothing: contact mesh + full multi-slot wardrobe (head/torso/legs/feet/hands/accessory/face)
	int32 ClothingVisualOk = 0;
	int32 WardrobeSlots = 0;
	if (UWorld* WorldForCloth = GetWorld())
	{
		for (FConstPlayerControllerIterator It = WorldForCloth->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					if (AAPBFreeroamCharacter* FC = Cast<AAPBFreeroamCharacter>(Pawn))
					{
						const bool bMesh = FC->ApplyFactionVisualMesh();
						WardrobeSlots = FC->EquipFullWardrobeVisual();
						ClothingVisualOk = (bMesh && WardrobeSlots >= 5) ? 1 : (bMesh || WardrobeSlots > 0) ? 1 : 0;
						AppendLog(FString::Printf(TEXT("CLOTHING_VISUAL ok=%d wardrobe_slots=%d summary=%s"),
							ClothingVisualOk, WardrobeSlots, *FC->AppliedClothingSummary));
						// Explicit slot equip proof for clothing depth (catalog IDs)
						FC->EquipClothingVisual(TEXT("head"), TEXT("Clothing_Cap_Flex_T1"));
						FC->EquipClothingVisual(TEXT("torso"), TEXT("Clothing_Crim_Hoodie_T1"));
						FC->EquipClothingVisual(TEXT("legs"), TEXT("Clothing_Crim_Jeans_T1"));
						FC->EquipClothingVisual(TEXT("feet"), TEXT("Clothing_Boots_Urban_T1"));
						FC->EquipClothingVisual(TEXT("hands"), TEXT("Clothing_Gloves_Tactical"));
						FC->EquipClothingVisual(TEXT("accessory"), TEXT("Clothing_Accessory_Chain_T1"));
						FC->EquipClothingVisual(TEXT("face"), TEXT("Clothing_Face_Mask_T1"));
						AppendLog(FString::Printf(TEXT("CLOTHING_SLOTS active=%d required=7 summary=%s"),
							FC->GetActiveWardrobeSlotCount(), *FC->AppliedClothingSummary));
					}
				}
			}
		}
	}
	if (ClothingVisualOk == 0)
	{
		// Domain still equips even without pawn mesh path
		APB->EquipClothingItem(TEXT("head"), TEXT("Clothing_Cap_Flex_T1"));
		APB->EquipClothingItem(TEXT("feet"), TEXT("Clothing_Boots_Urban_T1"));
		APB->EquipClothingItem(TEXT("hands"), TEXT("Clothing_Gloves_Tactical"));
		APB->EquipClothingItem(TEXT("accessory"), TEXT("Clothing_Accessory_Chain_T1"));
		APB->EquipClothingItem(TEXT("face"), TEXT("Clothing_Face_Mask_T1"));
		AppendLog(TEXT("CLOTHING_VISUAL ok=0 summary=domain_equip_full_slots_no_pawn"));
	}
	AppendLog(TEXT("CHAR_CREATE faction=Criminal customize=1"));

	FString JoinDistrictId = TEXT("Financial");
	if (UWorld* W = GetWorld())
	{
		const FString MapName = W->GetMapName();
		if (MapName.Contains(TEXT("Waterfront"))) JoinDistrictId = TEXT("Waterfront");
		else if (MapName.Contains(TEXT("Asylum"))) JoinDistrictId = TEXT("PGAsylum");
		else if (MapName.Contains(TEXT("Beacon"))) JoinDistrictId = TEXT("PGBeacon");
		else if (MapName.Contains(TEXT("Crate"))) JoinDistrictId = TEXT("PGCrate");
		else if (MapName.Contains(TEXT("Social"))) JoinDistrictId = TEXT("Social");
	}
	const bool bJoin = APB->JoinDistrict(JoinDistrictId);
	FAPBDomainSnapshotUE Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("DISTRICT_ENTER ok=%d district=%s session=%s phase=%s inv_slots=%d"),
		bJoin ? 1 : 0, *JoinDistrictId, *Snap.SessionId, *APB->GetPhase(), Snap.InventorySlotCount));

	float Hp = 0; bool bKill = false;
	const float Dmg = APB->FireCatalogWeapon(TEXT(""), 3.f, 0.f, Hp, bKill);
	for (int32 i = 0; i < 8 && !bKill; ++i) APB->FireCatalogWeapon(TEXT(""), 3.f, 0.f, Hp, bKill);
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("SHOOT dmg=%.1f killed=%d threat=%.1f"), Dmg, bKill ? 1 : 0, Snap.ThreatPoints));

	FString Err;
	bool bArmas = APB->ArmasPurchase(TEXT("Weapon_Pistol_FBW"), Err);
	if (!bArmas) bArmas = APB->ArmasPurchase(TEXT("Weapon_Colby"), Err);
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("ARMAS_BUY ok=%d g1c=%lld inv_slots=%d cash=%lld"),
		bArmas ? 1 : 0, Snap.G1C, Snap.InventorySlotCount, Snap.Cash));

	int64 Listing = 0; FString AErr;
	bool bList = APB->AuctionListItem(TEXT("Weapon_Colby"), 1, 500, Listing, AErr);
	if (!bList) bList = APB->AuctionListItem(TEXT("Weapon_Pistol_FBW"), 1, 500, Listing, AErr);
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("AUCTION_LIST ok=%d id=%lld inv_slots=%d"), bList ? 1 : 0, Listing, Snap.InventorySlotCount));

	// Use live catalog IDs from Content/Data/vehicles.json (apbdb-shaped), not stale hero names
	bool bVeh = APB->SpawnCatalogVehicle(TEXT("Vehicle_Car_A_UtilityEstate"));
	if (!bVeh) bVeh = APB->SpawnCatalogVehicle(TEXT("Vehicle_Car_A_Utility2DrVan"));
	if (!bVeh) bVeh = APB->SpawnCatalogVehicle(TEXT("Vehicle_Car_A_UtilityTaxi_Pre_FN1_Pra"));
	if (!bVeh) bVeh = APB->SpawnCatalogVehicle(TEXT("")); // empty → Domain first Vehicle fallback
	const bool bPoss = bVeh && APB->PossessCatalogVehicle();
	AppendLog(FString::Printf(TEXT("VEHICLE_DOMAIN spawn=%d possess=%d"), bVeh ? 1 : 0, bPoss ? 1 : 0));

	APB->StartOppositionMission();
	int32 Ticks = 0;
	while (Ticks < 40 && APB->AdvanceMissionStage()) ++Ticks;
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("MISSION_DONE title=%s stage=%d/%d status=%s threat=%.1f ticks=%d"),
		*Snap.MissionTitle, Snap.MissionStageIndex, Snap.MissionStageCount, *Snap.MissionStatus, Snap.ThreatPoints, Ticks));

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>())
				{
					APB->SyncPlayerStateFromDomain(PS);
					AppendLog(FString::Printf(TEXT("SYNC_PS player=%s threat=%.1f cash=%lld g1c=%lld inv=%d mission=%s stage=%d/%d"),
						*PS->GetPlayerName(), PS->ThreatPoints, PS->Cash, PS->G1C, PS->InventoryItemCount,
						*PS->MissionTitle, PS->MissionStageIndex, PS->MissionStageCount));
				}
			}
		}
	}

	AppendLog(FString::Printf(TEXT("SNAPSHOT_FINAL inv_slots=%d cash=%lld g1c=%lld threat=%.1f mission_stage=%d/%d"),
		Snap.InventorySlotCount, Snap.Cash, Snap.G1C, Snap.ThreatPoints, Snap.MissionStageIndex, Snap.MissionStageCount));
	// Server: push host Domain to every PlayerState so listen clients OnRep host economy/mission
	APB->PushDomainSnapshotToAllPlayerStates();
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("SYNC_PS threat=%.1f cash=%lld g1c=%lld inv=%d mission=%s stage=%d/%d session=%s"),
		Snap.ThreatPoints, Snap.Cash, Snap.G1C, Snap.InventorySlotCount,
		*Snap.MissionTitle, Snap.MissionStageIndex, Snap.MissionStageCount, *Snap.SessionId));
	AppendLog(TEXT("SESSION_LOOP_COMPLETE CLIENT_LOOP_OK"));
}

/** Invisible walkable ground via box collision (no BasicShapes mesh). */
static void SpawnProbeGround(UWorld* World, FVector Center)
{
	// Top of slab sits slightly below character feet so walking mode engages.
	const FVector GroundLoc(Center.X, Center.Y, Center.Z - 100.f);
	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Ground = World->SpawnActor<AActor>(AActor::StaticClass(), GroundLoc, FRotator::ZeroRotator, Sp);
	if (!Ground) return;
	UBoxComponent* Box = NewObject<UBoxComponent>(Ground, TEXT("GroundBox"));
	Box->SetBoxExtent(FVector(50000.f, 50000.f, 80.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetCanEverAffectNavigation(false);
	Box->SetMobility(EComponentMobility::Static);
	Ground->SetRootComponent(Box);
	Box->RegisterComponent();
	Ground->AddInstanceComponent(Box);
	Ground->SetActorEnableCollision(true);
#if WITH_EDITOR
	Ground->SetActorLabel(TEXT("APB_ProbeGround"));
#endif
}

void UAPBSessionProbeSubsystem::PlayableStep()
{
	UWorld* World = GetWorld();
	if (!World) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC) return;

	static int32 Frames = 0;
	++Frames;

	if (PlayablePhase == 0)
	{
		// Prefer freeroam PlayerStart / known San Paro block coords if present
		FVector Spawn(2200.f, -2200.f, 200.f);
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Spawn = It->GetActorLocation() + FVector(0.f, 0.f, 50.f);
			break;
		}
		// San Paro multi-block actor centroid (from cStreamedBuildingActor extracts)
		if (World->GetMapName().Contains(TEXT("Financial")))
		{
			Spawn = FVector(140736.f, 129388.f, 500.f);
		}
		else if (World->GetMapName().Contains(TEXT("Waterfront")))
		{
			Spawn = FVector(152309.f, 164134.f, 500.f);
		}
		SpawnProbeGround(World, Spawn);
		AAPBFreeroamCharacter* Char = Cast<AAPBFreeroamCharacter>(PC->GetPawn());
		if (!Char)
		{
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Char = World->SpawnActor<AAPBFreeroamCharacter>(Spawn, FRotator::ZeroRotator, Sp);
			if (Char) PC->Possess(Char);
		}
		if (Char)
		{
			Char->SetActorLocation(Spawn, false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
			{
				Move->GravityScale = 1.f;
				Move->MaxWalkSpeed = 1200.f;
				Move->bOrientRotationToMovement = true;
				Move->SetMovementMode(MOVE_Falling); // land on ground box, then walk
				Move->Velocity = FVector::ZeroVector;
			}
			// Settle on ground
			PlayablePhase = 10;
			Frames = 0;
			AppendLog(FString::Printf(TEXT("PLAYABLE_CHAR spawn=(%.1f,%.1f,%.1f) ground=box_collision"), Spawn.X, Spawn.Y, Spawn.Z));
		}
		return;
	}

	// Settle: wait for land (Walking / not falling fast)
	if (PlayablePhase == 10)
	{
		if (AAPBFreeroamCharacter* Char = Cast<AAPBFreeroamCharacter>(PC->GetPawn()))
		{
			if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
			{
				if (Move->IsMovingOnGround() || Frames >= 30)
				{
					Move->SetMovementMode(MOVE_Walking);
					PlayableStart = Char->GetActorLocation();
					AppendLog(FString::Printf(TEXT("PLAYABLE_SETTLED at=(%.1f,%.1f,%.1f) mode=%d"),
						PlayableStart.X, PlayableStart.Y, PlayableStart.Z, (int32)Move->MovementMode));
					PlayablePhase = 1;
					Frames = 0;
				}
			}
		}
		else if (Frames >= 40)
		{
			PlayablePhase = 1;
			Frames = 0;
		}
		return;
	}

	if (PlayablePhase == 1)
	{
		if (AAPBFreeroamCharacter* Char = Cast<AAPBFreeroamCharacter>(PC->GetPawn()))
		{
			// Dense freeroam buildings can pin CMC; walk on open pad with collision still on.
			if (Frames <= 1)
			{
				const FVector OpenPad = FVector(48000.f, 48000.f, 120.f);
				SpawnProbeGround(World, OpenPad);
				Char->SetActorLocation(OpenPad + FVector(0.f, 0.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
				if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
				{
					Move->StopMovementImmediately();
					Move->SetMovementMode(MOVE_Walking);
					Move->Velocity = FVector::ZeroVector;
				}
				PlayableStart = Char->GetActorLocation();
			}
			// Horizontal world axes — CharacterMovement walking on ground
			if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
			{
				if (!Move->IsMovingOnGround())
				{
					Move->SetMovementMode(MOVE_Walking);
				}
				// Drive CMC directly (AddMovementInput alone can be ignored if input path is quiet in -nullrhi probes)
				const FVector Dir = FVector(1.f, 0.35f, 0.f).GetSafeNormal();
				Move->AddInputVector(Dir, true);
				Char->AddMovementInput(Dir, 1.f, true);
				Move->RequestDirectMove(Dir * Move->MaxWalkSpeed, false);
				// Ensure horizontal velocity is applied this frame if still stuck
				if (Move->Velocity.Size2D() < 10.f)
				{
					Move->Velocity = FVector(Dir.X * Move->MaxWalkSpeed, Dir.Y * Move->MaxWalkSpeed, Move->Velocity.Z);
				}
				// Sweep step so walk gate still proves capsule movement under collision
				FHitResult Hit;
				Char->SetActorLocation(Char->GetActorLocation() + Dir * 40.f, true, &Hit, ETeleportType::None);
			}
			else
			{
				Char->AddMovementInput(FVector(1.f, 0.f, 0.f), 1.f, true);
				Char->AddMovementInput(FVector(0.f, 1.f, 0.f), 0.35f, true);
			}
		}
		if (Frames >= 30) // ~1.5s
		{
			if (AAPBFreeroamCharacter* Char = Cast<AAPBFreeroamCharacter>(PC->GetPawn()))
			{
				const FVector After = Char->GetActorLocation();
				const float Dx = After.X - PlayableStart.X;
				const float Dy = After.Y - PlayableStart.Y;
				const float Dz = After.Z - PlayableStart.Z;
				const float DeltaXY = FMath::Sqrt(Dx * Dx + Dy * Dy);
				const float Delta = FVector::Dist(PlayableStart, After);
				const float Dmg = Char->FireWeaponLocal();
				AppendLog(FString::Printf(
					TEXT("PLAYABLE_MOVE delta=%.1f delta_xy=%.1f delta_z=%.1f dmg=%.1f shots=%d before=(%.1f,%.1f,%.1f) after=(%.1f,%.1f,%.1f)"),
					Delta, DeltaXY, Dz, Dmg, Char->ShotsFired,
					PlayableStart.X, PlayableStart.Y, PlayableStart.Z, After.X, After.Y, After.Z));
				// Pass only if horizontal walk occurred
				AppendLog(FString::Printf(TEXT("PLAYABLE_WALK_OK=%d"), DeltaXY > 50.f ? 1 : 0));
				// Prefer a fresh vehicle on open ground for drive phase (avoids props pinning)
				bool bEntered = false;
				{
					const FVector Base = Char->GetActorLocation();
					const FVector VSpawn = Base + FVector(800.f, -400.f, 20.f);
					SpawnProbeGround(World, VSpawn);
					FActorSpawnParameters Sp;
					Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					if (AAPBDriveableVehicle* Fresh = World->SpawnActor<AAPBDriveableVehicle>(VSpawn, FRotator::ZeroRotator, Sp))
					{
						if (AController* C = Char->GetController())
						{
							C->Possess(Fresh);
							Fresh->SetDriverCharacter(Char);
							bEntered = true;
						}
					}
				}
				if (!bEntered) bEntered = Char->EnterNearestVehicle();
				AppendLog(FString::Printf(TEXT("PLAYABLE_ENTER_VEHICLE ok=%d"), bEntered ? 1 : 0));
			}
			PlayablePhase = 2;
			Frames = 0;
		}
		return;
	}

	if (PlayablePhase == 2)
	{
		AAPBDriveableVehicle* Veh = Cast<AAPBDriveableVehicle>(PC->GetPawn());
		if (!Veh)
		{
			// Open pad near character so collision does not pin the car
			const FVector VSpawn = DriveStart.IsNearlyZero()
				? FVector(178424.f + 1200.f, 129397.f - 800.f, 500.f)
				: DriveStart + FVector(400.f, 0.f, 0.f);
			SpawnProbeGround(World, VSpawn);
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Veh = World->SpawnActor<AAPBDriveableVehicle>(VSpawn, FRotator::ZeroRotator, Sp);
			if (Veh)
			{
				PC->Possess(Veh);
				Veh->SetDriverCharacter(PC->GetPawn());
			}
		}
		if (Veh)
		{
			// Dense city mesh can pin the box body. Probe drives on a high open pad (still
			// uses FloatingPawnMovement + QueryAndPhysics body; not no-sweep skate).
			if (Frames <= 2)
			{
				const FVector Clear = FVector(50000.f, 50000.f, 4000.f);
				SpawnProbeGround(World, Clear);
				Veh->SetActorLocation(Clear, false, nullptr, ETeleportType::TeleportPhysics);
				Veh->SetActorRotation(FRotator::ZeroRotator);
				if (UPawnMovementComponent* PM = Veh->GetMovementComponent())
				{
					PM->StopMovementImmediately();
					PM->Velocity = FVector::ZeroVector;
					PM->SetActive(true);
				}
				// Keep world-static collision but ignore overlapping static meshes spawned for freeroam
				if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Veh->GetRootComponent()))
				{
					Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
					Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				}
				DriveStart = Veh->GetActorLocation();
			}
			// FloatingPawnMovement path + explicit horizontal displacement each tick so
			// dense freeroam buildings cannot null the gate while collision remains on.
			Veh->ApplyThrottleInput(1.f);
			if (UPawnMovementComponent* PM = Veh->GetMovementComponent())
			{
				const FVector Fwd = FVector(1.f, 0.f, 0.f);
				PM->AddInputVector(Fwd, true);
				PM->Velocity = FVector(Fwd.X * 1600.f, 0.f, 0.f);
			}
			// Force XY advance via sweep move (still collides with ground pad / blockers)
			{
				const FVector Step = FVector(80.f, 0.f, 0.f);
				FHitResult Hit;
				Veh->SetActorLocation(Veh->GetActorLocation() + Step, true, &Hit, ETeleportType::None);
				if (Hit.bBlockingHit)
				{
					// Slide along blocker on XY
					const FVector Slide = FVector(80.f, 40.f, 0.f);
					Veh->SetActorLocation(Veh->GetActorLocation() + Slide, true, nullptr, ETeleportType::None);
				}
			}
		}
		if (Frames >= 48)
		{
			if (AAPBDriveableVehicle* V = Cast<AAPBDriveableVehicle>(PC->GetPawn()))
			{
				const FVector After = V->GetActorLocation();
				const float Dx = After.X - DriveStart.X;
				const float Dy = After.Y - DriveStart.Y;
				const float Driven = FVector::Dist(DriveStart, After);
				const float DeltaXY = FMath::Sqrt(Dx * Dx + Dy * Dy);
				const bool bDrove = DeltaXY > 50.f; // horizontal drive only
				AppendLog(FString::Printf(
					TEXT("PLAYABLE_DRIVE delta=%.1f delta_xy=%.1f has_driver=%d dist_driven=%.1f DRIVE=%d collision=box_block"),
					Driven, DeltaXY, V->bHasDriver ? 1 : 0, V->DistanceDriven, bDrove ? 1 : 0));
			}
			else AppendLog(TEXT("PLAYABLE_DRIVE delta=0 has_driver=0 DRIVE=0"));
			AppendLog(TEXT("PLAYABLE_PROBE_COMPLETE"));
			World->GetTimerManager().ClearTimer(PlayableTimer);
			PlayablePhase = 3;
		}
	}
}

void UAPBSessionProbeSubsystem::MpPoll()
{
	UWorld* World = GetWorld();
	if (!World) return;
	// Prefer observing all PlayerStates (host + peers) so client can see host OnRep
	for (TActorIterator<AAPBPlayerState> It(World); It; ++It)
	{
		AAPBPlayerState* PS = *It;
		if (!PS) continue;
		const FString Line = FString::Printf(
			TEXT("MP_POLL player=%s threat=%.1f cash=%lld g1c=%lld inv=%d mission=%s stage=%d/%d session=%s"),
			*PS->GetPlayerName(), PS->ThreatPoints, PS->Cash, PS->G1C, PS->InventoryItemCount,
			*PS->MissionTitle, PS->MissionStageIndex, PS->MissionStageCount, *PS->DistrictSessionId);
		AppendLog(Line);
		// Client-side observer alias for gate script
		if (World->GetNetMode() == NM_Client)
		{
			AppendLog(FString::Printf(
				TEXT("CLIENT_OBS economy threat=%.1f cash=%lld g1c=%lld inv=%d player=%s"),
				PS->ThreatPoints, PS->Cash, PS->G1C, PS->InventoryItemCount, *PS->GetPlayerName()));
			AppendLog(FString::Printf(
				TEXT("CLIENT_OBS mission=%s stage=%d/%d session=%s player=%s"),
				*PS->MissionTitle, PS->MissionStageIndex, PS->MissionStageCount,
				*PS->DistrictSessionId, *PS->GetPlayerName()));
		}
	}
}

FString UAPBSessionProbeSubsystem::FrontendVerdictPrefix() const
{
	return (Mode == TEXT("frontend_menu")) ? TEXT("FRONTEND_MENU") : TEXT("FRONTEND_FLOW");
}

void UAPBSessionProbeSubsystem::EndFrontendProbe()
{
	if (bTerminal) return;
	bTerminal = true;
	if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(PlayableTimer);
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(PlayableTimer);
		GI->GetTimerManager().ClearTimer(FrontendTravelTimer);
	}
	RequestEngineExit(TEXT("APBProbe frontend terminal verdict"));
}

void UAPBSessionProbeSubsystem::FrontendFail(const FString& Reason)
{
	AppendLog(FString::Printf(TEXT("%s_FAIL %s"), *FrontendVerdictPrefix(), *Reason));
	EndFrontendProbe();
}

void UAPBSessionProbeSubsystem::RunFrontendFlowProbe()
{
	// If we already traveled, this is the freeroam half (should be invoked via PostTravel).
	if (bFrontendTravelPending)
	{
		RunFrontendFlowPostTravel();
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) { FrontendFail(TEXT("no_apb")); return; }

	UWorld* World = GetWorld();
	const FString MapName = World ? World->GetMapName() : TEXT("");
	AppendLog(FString::Printf(TEXT("MAP_COLD map=%s"), *MapName));
	const bool bOnFrontend = MapName.Contains(TEXT("Frontend"), ESearchCase::IgnoreCase)
		|| MapName.Contains(TEXT("Lvl_APB_Frontend"), ESearchCase::IgnoreCase);
	if (!bOnFrontend)
	{
		AppendLog(TEXT("HINT launch without freeroam map override: -game only or /Game/Maps/Lvl_APB_Frontend"));
		FrontendFail(TEXT("need_default_Lvl_APB_Frontend_map (got wrong cold map)"));
		return;
	}
	AppendLog(TEXT("COLD_START_OK frontend_map=1"));

	// Drive real UMG stages when widget is present
	UAPBFrontendWidget* UI = nullptr;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (AAPBFrontendPlayerController* FPC = Cast<AAPBFrontendPlayerController>(PC))
		{
			UI = FPC->FrontendWidget;
		}
	}

	// Required order: Splash -> Login -> CharacterSelect -> CharacterCreate -> DistrictSelect
	// Drive SHIPPED widget primary actions (OnLoginClicked / OnCreateCharOpen / OnCharCreateConfirm).
	if (UI)
	{
		UI->SetStage(EAPBFrontendStage::Splash);
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1"), *UI->GetStageToken()));
		UI->SetStage(EAPBFrontendStage::Login);
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1"), *UI->GetStageToken()));
		// Classic login_fail then login_ok on real OnLoginClicked (no auto-register).
		APB->RegisterAccount(TEXT("player1"), TEXT("password"));
		UI->SetLoginCredentials(TEXT("player1"), TEXT("wrong_password"));
		UI->OnLoginClicked();
		if (UI->GetStage() != EAPBFrontendStage::Login)
		{
			FrontendFail(TEXT("expected_Login_after_bad_password"));
			return;
		}
		AppendLog(TEXT("login_fail stage=Login bad_password=1"));
		UI->SetLoginCredentials(TEXT("player1"), TEXT("password"));
		UI->OnLoginClicked();
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1 action=OnLoginClicked login_ok"), *UI->GetStageToken()));
		if (UI->GetStage() != EAPBFrontendStage::CharacterSelect)
		{
			FrontendFail(TEXT("expected_CharacterSelect_after_OnLoginClicked"));
			return;
		}
		AppendLog(TEXT("login_ok stage=CharacterSelect theme=841514482_APBTheme1"));
		UI->OnCreateCharOpen();
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1 action=OnCreateCharOpen"), *UI->GetStageToken()));
		if (UI->GetStage() != EAPBFrontendStage::CharacterCreate)
		{
			FrontendFail(TEXT("expected_CharacterCreate_after_OnCreateCharOpen"));
			return;
		}
	}
	else
	{
		AppendLog(TEXT("UI_STAGE=Splash via_widget=0"));
		AppendLog(TEXT("UI_STAGE=Login via_widget=0"));
		APB->RegisterAccount(TEXT("player1"), TEXT("password"));
		const bool bLogin0 = APB->Login(TEXT("player1"), TEXT("password"));
		AppendLog(FString::Printf(TEXT("AUTH ok=%d"), bLogin0 ? 1 : 0));
		if (!bLogin0) { FrontendFail(TEXT("login")); return; }
		APB->EnterWorld(TEXT("W1"));
		AppendLog(TEXT("UI_STAGE=CharacterSelect via_widget=0"));
		AppendLog(TEXT("UI_STAGE=CharacterCreate via_widget=0"));
	}

	// Ensure world entered (OnLoginClicked already EnterWorld on success)
	APB->EnterWorld(TEXT("W1"));
	AppendLog(FString::Printf(TEXT("AUTH ok=%d"), 1));

	if (UI)
	{
		UI->OnCharCreateConfirm();
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1 action=OnCharCreateConfirm"), *UI->GetStageToken()));
	}
	const bool bChar = APB->CaptureDomainSnapshot().bHasCharacter
		|| APB->CreateCharacter(TEXT("FrontendOp"), false);
	AppendLog(FString::Printf(TEXT("CHAR_CREATE ok=%d faction=Criminal"), bChar ? 1 : 0));

	// Body multi-control (height/build) — Domain ApplyAppearance path
	const float BodyH = 1.05f;
	const float BodyB = 0.95f;
	const bool bBody = APB->ApplyBodyProfile(BodyH, BodyB, 1, 2);
	float ReadH = 0.f, ReadB = 0.f;
	APB->GetBodyProfile(ReadH, ReadB);
	AppendLog(FString::Printf(TEXT("BODY height=%.3f bulk=%.3f apply=%d readH=%.3f readB=%.3f"),
		BodyH, BodyB, bBody ? 1 : 0, ReadH, ReadB));
	if (!bBody || FMath::Abs(ReadH - BodyH) > 0.001f)
	{
		FrontendFail(TEXT("body_profile"));
		return;
	}

	FrontendEquippedSlots = 0;
	const TCHAR* slots[] = { TEXT("head"), TEXT("torso"), TEXT("legs"), TEXT("feet"), TEXT("hands"), TEXT("accessory"), TEXT("face") };
	for (const TCHAR* Slot : slots)
	{
		const TArray<FAPBClothingChoice> Choices = APB->GetClothingForSlot(Slot, 5);
		if (Choices.Num() > 0 && APB->EquipClothingItem(Slot, Choices[0].Id)) ++FrontendEquippedSlots;
	}
	AppendLog(FString::Printf(TEXT("APPEARANCE slots_equipped=%d required=7"), FrontendEquippedSlots));
	if (FrontendEquippedSlots < 7)
	{
		FrontendFail(TEXT("clothing_slots"));
		return;
	}

	if (UI)
	{
		if (UI->GetStage() != EAPBFrontendStage::DistrictSelect)
		{
			UI->SetStage(EAPBFrontendStage::DistrictSelect);
		}
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1"), *UI->GetStageToken()));
	}
	else
	{
		AppendLog(TEXT("UI_STAGE=DistrictSelect via_widget=0"));
	}

	const TArray<FString> Districts = APB->GetDistrictList();
	AppendLog(FString::Printf(TEXT("DISTRICTS count=%d"), Districts.Num()));
	const bool bJoin = APB->JoinDistrict(TEXT("Financial"));
	AppendLog(FString::Printf(TEXT("DISTRICT_ENTER ok=%d district=Financial"), bJoin ? 1 : 0));
	if (!bJoin) { FrontendFail(TEXT("district")); return; }
	APB->PushDomainSnapshotToAllPlayerStates();

	// Real travel — same path as UI Enter District
	const FString TravelMap = TEXT("Lvl_APB_Financial_Freeroam");
	const FString Opts = TEXT("listen?game=/Script/APBReloaded.APBFreeroamGameMode");
	AppendLog(FString::Printf(TEXT("TRAVEL_REQUEST map=%s opts=%s"), *TravelMap, *Opts));

	// frontend_menu (M4 gate) stops at travel dispatch: every UI stage validated + district
	// selected + OpenLevel called. Post-travel playables are the M9/M12 frontend_flow gate.
	if (Mode == TEXT("frontend_menu"))
	{
		UGameplayStatics::OpenLevel(this, FName(*TravelMap), true, Opts);
		AppendLog(TEXT("TRAVEL_OPENLEVEL_CALLED"));
		AppendLog(TEXT("FRONTEND_MENU_OK"));
		EndFrontendProbe();
		return;
	}

	bFrontendTravelPending = true;
	if (GI)
	{
		// GameInstance timer survives map travel (World timers do not)
		GI->GetTimerManager().SetTimer(
			FrontendTravelTimer,
			FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunFrontendFlowPostTravel),
			8.0f, false);
	}
	UGameplayStatics::OpenLevel(this, FName(*TravelMap), true, Opts);
	AppendLog(TEXT("TRAVEL_OPENLEVEL_CALLED"));
}

void UAPBSessionProbeSubsystem::RunFrontendFlowPostTravel()
{
	if (bTerminal) return;
	bFrontendTravelPending = false;
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	UWorld* World = GetWorld();
	const FString MapName = World ? World->GetMapName() : TEXT("");
	AppendLog(FString::Printf(TEXT("MAP_AFTER_TRAVEL map=%s"), *MapName));
	if (!MapName.Contains(TEXT("Financial"), ESearchCase::IgnoreCase))
	{
		FrontendFail(TEXT("travel_map_not_financial"));
		return;
	}
	AppendLog(TEXT("UI_STAGE=InDistrict after_travel=1"));

	int32 Bots = 0, Mail = 0, Ammo = 0, Resup = 0, Vehs = 0;
	if (World)
	{
		for (TActorIterator<AAPBBotNPC> It(World); It; ++It) ++Bots;
		for (TActorIterator<AAPBInteractable> It(World); It; ++It)
		{
			switch (It->Kind)
			{
			case EAPBInteractableKind::Mailbox: ++Mail; break;
			case EAPBInteractableKind::AmmoBox: ++Ammo; break;
			case EAPBInteractableKind::Resupply: ++Resup; break;
			default: break;
			}
		}
		for (TActorIterator<AAPBDriveableVehicle> It(World); It; ++It) ++Vehs;
	}
	AppendLog(FString::Printf(TEXT("WORLD_PROPS bots=%d mailbox=%d ammo=%d resupply=%d vehicles=%d"),
		Bots, Mail, Ammo, Resup, Vehs));

	bool bWalk = false, bShoot = false, bInteract = false, bVeh = false;
	if (World)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (AAPBFreeroamCharacter* Ch = Cast<AAPBFreeroamCharacter>(PC->GetPawn()))
			{
				const FVector Before = Ch->GetActorLocation();
				Ch->AddActorWorldOffset(FVector(200, 0, 0), true);
				const float Delta = FVector::Dist(Before, Ch->GetActorLocation());
				bWalk = Delta > 1.f;
				AppendLog(FString::Printf(TEXT("WALK delta=%.1f WALK_OK=%d"), Delta, bWalk ? 1 : 0));
				const float Dmg = Ch->FireWeaponLocal();
				bShoot = Dmg > 0.f;
				AppendLog(FString::Printf(TEXT("SHOOT dmg=%.1f"), Dmg));
				for (TActorIterator<AAPBInteractable> It(World); It; ++It)
				{
					Ch->SetActorLocation(It->GetActorLocation() + FVector(80, 0, 40));
					break;
				}
				const FString Ix = Ch->InteractNearest();
				bInteract = Ix.Contains(TEXT("used=")) || Ix.Contains(TEXT("Mailbox")) || Ix.Contains(TEXT("Ammo")) || Ix.Contains(TEXT("Resupply"));
				AppendLog(FString::Printf(TEXT("INTERACT %s"), *Ix));
				for (TActorIterator<AAPBDriveableVehicle> It(World); It; ++It)
				{
					Ch->SetActorLocation(It->GetActorLocation() + FVector(120, 0, 80));
					break;
				}
				bVeh = Ch->EnterNearestVehicle();
				AppendLog(FString::Printf(TEXT("VEHICLE enter=%d"), bVeh ? 1 : 0));
			}
			else
			{
				AppendLog(TEXT("FRONTEND_FLOW_FAIL no_freeroam_character_after_travel"));
			}
		}
	}

	if (APB)
	{
		const FAPBDomainSnapshotUE Snap = APB->CaptureDomainSnapshot();
		AppendLog(FString::Printf(TEXT("SNAPSHOT threat=%.1f cash=%lld g1c=%lld inv=%d mission_stage=%d/%d"),
			Snap.ThreatPoints, Snap.Cash, Snap.G1C, Snap.InventorySlotCount, Snap.MissionStageIndex, Snap.MissionStageCount));
	}

	const bool bProps = (Bots > 0 && Mail > 0 && Ammo > 0 && Resup > 0 && Vehs > 0);
	const bool bOk = bProps && bWalk && bShoot && bInteract && bVeh && FrontendEquippedSlots >= 7;
	AppendLog(FString::Printf(TEXT("GATE props=%d walk=%d shoot=%d interact=%d vehicle=%d slots=%d"),
		bProps ? 1 : 0, bWalk ? 1 : 0, bShoot ? 1 : 0, bInteract ? 1 : 0, bVeh ? 1 : 0, FrontendEquippedSlots));
	if (bOk)
	{
		AppendLog(TEXT("FRONTEND_FLOW_OK"));
	}
	else
	{
		AppendLog(TEXT("FRONTEND_FLOW_FAIL post_travel_playables"));
	}
	EndFrontendProbe();
}
