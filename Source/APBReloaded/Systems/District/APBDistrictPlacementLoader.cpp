#include "APBDistrictPlacementLoader.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/PlayerStart.h"
#include "Containers/Set.h"

bool UAPBDistrictPlacementLoader::LoadManifestFromFile(const FString& AbsoluteJsonPath, FAPBDistrictManifest& OutManifest)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *AbsoluteJsonPath)) return false;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;

	OutManifest = FAPBDistrictManifest();
	OutManifest.DistrictId = Root->GetStringField(TEXT("district_id"));
	OutManifest.SourcePackage = Root->GetStringField(TEXT("source_package"));
	const TArray<TSharedPtr<FJsonValue>>* Starts;
	if (Root->TryGetArrayField(TEXT("player_start"), Starts) && Starts->Num() >= 3)
	{
		OutManifest.PlayerStart = FVector(
			(*Starts)[0]->AsNumber(), (*Starts)[1]->AsNumber(), (*Starts)[2]->AsNumber());
	}
	if (Root->TryGetArrayField(TEXT("vehicle_start"), Starts) && Starts->Num() >= 3)
	{
		OutManifest.VehicleStart = FVector(
			(*Starts)[0]->AsNumber(), (*Starts)[1]->AsNumber(), (*Starts)[2]->AsNumber());
	}
	const TArray<TSharedPtr<FJsonValue>>* Chunks;
	if (Root->TryGetArrayField(TEXT("stream_chunks"), Chunks))
	{
		OutManifest.StreamChunkCount = Chunks->Num();
	}
	const TArray<TSharedPtr<FJsonValue>>* Placements;
	if (!Root->TryGetArrayField(TEXT("placements"), Placements)) return false;
	for (const TSharedPtr<FJsonValue>& V : *Placements)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;
		FAPBPlacementEntry E;
		E.MeshId = O->GetStringField(TEXT("mesh_id"));
		E.UePath = O->GetStringField(TEXT("ue_path"));
		E.Package = O->GetStringField(TEXT("package"));
		const TArray<TSharedPtr<FJsonValue>>* Loc;
		if (O->TryGetArrayField(TEXT("location"), Loc) && Loc->Num() >= 3)
		{
			E.Location = FVector((*Loc)[0]->AsNumber(), (*Loc)[1]->AsNumber(), (*Loc)[2]->AsNumber());
		}
		const TArray<TSharedPtr<FJsonValue>>* Rot;
		if (O->TryGetArrayField(TEXT("rotation"), Rot) && Rot->Num() >= 3)
		{
			E.Rotation = FRotator((*Rot)[0]->AsNumber(), (*Rot)[1]->AsNumber(), (*Rot)[2]->AsNumber());
		}
		const TArray<TSharedPtr<FJsonValue>>* Scl;
		if (O->TryGetArrayField(TEXT("scale"), Scl) && Scl->Num() >= 3)
		{
			E.Scale = FVector((*Scl)[0]->AsNumber(), (*Scl)[1]->AsNumber(), (*Scl)[2]->AsNumber());
		}
		OutManifest.Placements.Add(E);
	}
	return OutManifest.Placements.Num() > 0;
}

bool UAPBDistrictPlacementLoader::LoadManifestForDistrict(const FString& ProjectContentDir, const FString& DistrictId, FAPBDistrictManifest& OutManifest)
{
	// Prefer district-specific manifest; never silently substitute Financial for Waterfront if missing
	FString Base = TEXT("Financial_Block09");
	if (DistrictId.Contains(TEXT("Waterfront"))) Base = TEXT("Waterfront_Block05");
	else if (DistrictId.Contains(TEXT("Asylum")) || DistrictId.Contains(TEXT("PGAsylum"))) Base = TEXT("Asylum_Block");
	else if (DistrictId.Contains(TEXT("Beacon"))) Base = TEXT("Beacon_Block");
	else if (DistrictId.Contains(TEXT("Crate"))) Base = TEXT("Crate_Block");
	else if (DistrictId.Contains(TEXT("Social"))) Base = TEXT("Social_Block");

	// Prefer mesh-bound spawn list (spawnable only) then full manifest
	const FString BoundName = Base + TEXT("_bound.json");
	const FString FullName = Base + TEXT(".json");
	const FString BoundPath = FPaths::Combine(ProjectContentDir, TEXT("Data/district_placements"), BoundName);
	const FString FullPath = FPaths::Combine(ProjectContentDir, TEXT("Data/district_placements"), FullName);

	bool bBound = LoadManifestFromFile(BoundPath, OutManifest);
	if (bBound)
	{
		OutManifest.bLoadedBoundManifest = true;
		// Total/hit_rate from bound file fields if present
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *BoundPath))
		{
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
			if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
			{
				OutManifest.BoundCount = Root->HasField(TEXT("bound_count"))
					? static_cast<int32>(Root->GetNumberField(TEXT("bound_count")))
					: OutManifest.Placements.Num();
				OutManifest.ManifestTotal = Root->HasField(TEXT("manifest_total"))
					? static_cast<int32>(Root->GetNumberField(TEXT("manifest_total")))
					: OutManifest.Placements.Num();
				OutManifest.HitRate = Root->HasField(TEXT("hit_rate"))
					? static_cast<float>(Root->GetNumberField(TEXT("hit_rate")))
					: 1.f;
			}
		}
		if (OutManifest.BoundCount <= 0) OutManifest.BoundCount = OutManifest.Placements.Num();
		return true;
	}

	if (!LoadManifestFromFile(FullPath, OutManifest))
	{
		UE_LOG(LogTemp, Warning, TEXT("APB: missing placement manifest %s for district %s"), *FullName, *DistrictId);
		return false;
	}
	OutManifest.bLoadedBoundManifest = false;
	OutManifest.BoundCount = OutManifest.Placements.Num();
	OutManifest.ManifestTotal = OutManifest.Placements.Num();
	OutManifest.HitRate = 1.f;
	UE_LOG(LogTemp, Warning,
		TEXT("APB STEAM_DERIVED placement_load district=%s path=%s n=%d (Content/Data/district_placements)"),
		*DistrictId, *FullPath, OutManifest.Placements.Num());
	return true;
}

bool UAPBDistrictPlacementLoader::ManifestUsesEngineCubes(const FAPBDistrictManifest& Manifest)
{
	for (const FAPBPlacementEntry& E : Manifest.Placements)
	{
		if (E.UePath.Contains(TEXT("BasicShapes/Cube")) || E.MeshId.Contains(TEXT("Cube")))
			return true;
	}
	return false;
}

static UStaticMesh* LoadPlacementMesh(const FAPBPlacementEntry& E)
{
	if (E.UePath.Contains(TEXT("BasicShapes/Cube"))) return nullptr;
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *E.UePath);
	if (Mesh) return Mesh;
	// Infer district folder + LOD0 stem variants (umodel export naming)
	TArray<FString> Folders = { TEXT("Financial"), TEXT("Waterfront"), TEXT("Asylum"), TEXT("Beacon"), TEXT("Crate"), TEXT("Social") };
	TArray<FString> Stems;
	Stems.Add(E.MeshId);
	if (!E.MeshId.EndsWith(TEXT("_LOD_0"))) Stems.Add(E.MeshId + TEXT("_LOD_0"));
	if (!E.MeshId.EndsWith(TEXT("_LOD0"))) Stems.Add(E.MeshId + TEXT("_LOD0"));
	Stems.Add(E.MeshId.Replace(TEXT("_LOD_0"), TEXT("")));
	Stems.Add(E.MeshId.Replace(TEXT("_LOD0"), TEXT("")));
	for (const FString& Folder : Folders)
	{
		for (const FString& Stem : Stems)
		{
			if (Stem.IsEmpty()) continue;
			const FString Alt = FString::Printf(TEXT("/Game/Imported/Districts/%s/%s.%s"), *Folder, *Stem, *Stem);
			Mesh = LoadObject<UStaticMesh>(nullptr, *Alt);
			if (Mesh) return Mesh;
		}
	}
	return nullptr;
}

void UAPBDistrictPlacementLoader::EnsureVisibleMeshMaterials(UStaticMeshComponent* Comp)
{
	if (!Comp) return;

	// Prefer materials that stay readable without baked lightmaps / full Lumen bounce.
	// umodel imports only ship WorldGrid slots — under black sky + low ambient they look pure black.
	static const TCHAR* Candidates[] = {
		TEXT("/Engine/EngineDebugMaterials/LevelColorationUnlitMaterial.LevelColorationUnlitMaterial"),
		TEXT("/Engine/EngineDebugMaterials/LevelColorationLitMaterial.LevelColorationLitMaterial"),
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"),
		TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"),
	};
	UMaterialInterface* Fallback = nullptr;
	for (const TCHAR* Path : Candidates)
	{
		Fallback = LoadObject<UMaterialInterface>(nullptr, Path);
		if (Fallback) break;
	}
	if (!Fallback)
	{
		Fallback = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	if (!Fallback) return;

	// Always force a known-good material on every slot (do not trust empty/black import slots).
	const int32 Num = FMath::Max(1, Comp->GetNumMaterials());
	for (int32 i = 0; i < Num; ++i)
	{
		Comp->SetMaterial(i, Fallback);
	}
	Comp->SetVisibility(true);
	Comp->SetHiddenInGame(false);
	Comp->SetCastShadow(true);
	// Two-sided via custom depth not available; ensure bounds not culled to zero
	Comp->bUseDefaultCollision = true;
}

int32 UAPBDistrictPlacementLoader::CountPlacementsNear(const FAPBDistrictManifest& Manifest, FVector Center, float Radius)
{
	const float R2 = Radius * Radius;
	int32 N = 0;
	for (const FAPBPlacementEntry& E : Manifest.Placements)
	{
		if (FVector::DistSquared(E.Location, Center) <= R2) ++N;
	}
	return N;
}

int32 UAPBDistrictPlacementLoader::SpawnFromManifest(UWorld* World, const FAPBDistrictManifest& Manifest, TArray<AActor*>& OutSpawned)
{
	TSet<FString> Keys;
	return SpawnFromManifestNear(World, Manifest, FVector::ZeroVector, 1.0e12f, OutSpawned, Keys);
}

int32 UAPBDistrictPlacementLoader::SpawnFromManifestNear(UWorld* World, const FAPBDistrictManifest& Manifest, FVector Center, float Radius, TArray<AActor*>& OutSpawned, TSet<FString>& InOutSpawnedMeshKeys)
{
	return SpawnFromManifestNearEx(World, Manifest, Center, Radius, OutSpawned, InOutSpawnedMeshKeys, nullptr, nullptr, nullptr);
}

int32 UAPBDistrictPlacementLoader::SpawnFromManifestNearEx(UWorld* World, const FAPBDistrictManifest& Manifest, FVector Center, float Radius,
	TArray<AActor*>& OutSpawned, TSet<FString>& InOutSpawnedMeshKeys,
	int32* OutLoadFailed, int32* OutInRadius, int32* OutSkippedAlready)
{
	if (!World) return 0;
	const float R2 = Radius * Radius;
	int32 Count = 0;
	int32 LoadFailed = 0;
	int32 InRadius = 0;
	int32 Skipped = 0;
	for (const FAPBPlacementEntry& E : Manifest.Placements)
	{
		const FString Key = E.MeshId + TEXT("@") + E.Location.ToString();
		const float Dist2 = FVector::DistSquared(E.Location, Center);
		if (Dist2 > R2) continue;
		++InRadius;
		if (InOutSpawnedMeshKeys.Contains(Key))
		{
			++Skipped;
			continue;
		}
		UStaticMesh* Mesh = LoadPlacementMesh(E);
		if (!Mesh)
		{
			++LoadFailed;
			continue;
		}
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(E.Location, E.Rotation, Sp);
		if (!A) continue;
		// Movable: Static mobility + no lightmaps + Lumen often renders umodel meshes pure black
		A->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* SMC = A->GetStaticMeshComponent();
		SMC->SetMobility(EComponentMobility::Movable);
		SMC->SetStaticMesh(Mesh);
		A->SetActorScale3D(E.Scale);
		// Query-only for bulk city: full physics on 2k actors buries the player inside solids
		SMC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		SMC->SetCollisionResponseToAllChannels(ECR_Block);
		SMC->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		SMC->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		EnsureVisibleMeshMaterials(SMC);
		SMC->SetCastShadow(false); // perf + avoid self-shadow black piles
		OutSpawned.Add(A);
		InOutSpawnedMeshKeys.Add(Key);
		++Count;
	}
	// PlayerStart once per world session (if not already among OutSpawned)
	bool bHasStart = false;
	for (AActor* Existing : OutSpawned)
	{
		if (Existing && Existing->IsA<APlayerStart>()) { bHasStart = true; break; }
	}
	if (!bHasStart && Count > 0)
	{
		FActorSpawnParameters Sp;
		APlayerStart* PS = World->SpawnActor<APlayerStart>(Manifest.PlayerStart, FRotator::ZeroRotator, Sp);
		if (PS) OutSpawned.Add(PS);
	}
	if (OutLoadFailed) *OutLoadFailed = LoadFailed;
	if (OutInRadius) *OutInRadius = InRadius;
	if (OutSkippedAlready) *OutSkippedAlready = Skipped;
	return Count;
}

FString UAPBDistrictPlacementLoader::ResolveDistrictIdFromMapName(const FString& MapName)
{
	const FString M = MapName;
	if (M.Contains(TEXT("Waterfront"))) return TEXT("Waterfront");
	if (M.Contains(TEXT("Asylum")) || M.Contains(TEXT("Abington"))) return TEXT("PGAsylum");
	if (M.Contains(TEXT("Beacon"))) return TEXT("PGBeacon");
	if (M.Contains(TEXT("Crate"))) return TEXT("PGCrate");
	if (M.Contains(TEXT("Social")) || M.Contains(TEXT("Breakwater"))) return TEXT("Social");
	if (M.Contains(TEXT("FinancialChaos"))) return TEXT("FinancialChaos");
	if (M.Contains(TEXT("FinancialRiot"))) return TEXT("FinancialRiot");
	if (M.Contains(TEXT("Financial"))) return TEXT("Financial");
	return TEXT("Financial");
}
