#include "APBDistrictPlacementLoader.h"
#include "Domain/APBPlacementBinding.h"
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
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"

bool UAPBDistrictPlacementLoader::LoadManifestFromFile(const FString& AbsoluteJsonPath, FAPBDistrictManifest& OutManifest)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *AbsoluteJsonPath)) return false;
	FTCHARToUTF8 Utf8Text(*Text);
	apb::DistrictManifestPure Parsed;
	if (!apb::ParsePlacementManifestJson(
		std::string(Utf8Text.Get(), static_cast<size_t>(Utf8Text.Length())), Parsed))
	{
		UE_LOG(LogTemp, Error, TEXT("PLACEMENT_MANIFEST_REJECT path=%s reason=%s"),
			*AbsoluteJsonPath, UTF8_TO_TCHAR(Parsed.parse_error.c_str()));
		return false;
	}

	OutManifest = FAPBDistrictManifest();
	OutManifest.DistrictId = UTF8_TO_TCHAR(Parsed.district_id.c_str());
	OutManifest.SourcePackage = UTF8_TO_TCHAR(Parsed.source_package.c_str());
	OutManifest.Provenance = UTF8_TO_TCHAR(Parsed.provenance.c_str());
	OutManifest.PlayerStart = FVector(Parsed.player_start.x, Parsed.player_start.y, Parsed.player_start.z);
	OutManifest.VehicleStart = FVector(Parsed.vehicle_start.x, Parsed.vehicle_start.y, Parsed.vehicle_start.z);
	OutManifest.StreamChunkCount = Parsed.stream_chunk_count;
	OutManifest.BoundCount = Parsed.bound_count;
	OutManifest.ManifestTotal = Parsed.manifest_total;
	OutManifest.HitRate = static_cast<float>(Parsed.hit_rate);
	OutManifest.RejectedRowCount = static_cast<int32>(Parsed.rejected_rows.size());
	for (const apb::PlacementRejectedRow& Rejection : Parsed.rejected_rows)
	{
		if (Rejection.reason == "MissingSourceId") ++OutManifest.MissingSourceIdCount;
		else ++OutManifest.NonRenderableRowCount;
	}
	for (const apb::PlacementEntryPure& ParsedEntry : Parsed.placements)
	{
		FAPBPlacementEntry E;
		E.SourceId = UTF8_TO_TCHAR(ParsedEntry.source_id.c_str());
		E.MeshId = UTF8_TO_TCHAR(ParsedEntry.mesh_id.c_str());
		E.UePath = UTF8_TO_TCHAR(ParsedEntry.ue_path.c_str());
		E.Package = UTF8_TO_TCHAR(ParsedEntry.package.c_str());
		E.Actor = UTF8_TO_TCHAR(ParsedEntry.actor.c_str());
		E.Edge = UTF8_TO_TCHAR(ParsedEntry.edge.c_str());
		E.Location = FVector(ParsedEntry.location.x, ParsedEntry.location.y, ParsedEntry.location.z);
		E.bRotationPresent = ParsedEntry.rotation_present;
		if (E.bRotationPresent)
			E.Rotation = FRotator(ParsedEntry.rotation.x, ParsedEntry.rotation.y, ParsedEntry.rotation.z);
		E.bScalePresent = ParsedEntry.scale_present;
		if (E.bScalePresent)
			E.Scale = FVector(ParsedEntry.scale.x, ParsedEntry.scale.y, ParsedEntry.scale.z);
		OutManifest.Placements.Add(E);
	}
	UE_LOG(LogTemp, Warning,
		TEXT("PLACEMENT_MANIFEST_ROWS path=%s renderable=%d rejected=%d missing_source_id=%d non_renderable=%d"),
		*AbsoluteJsonPath, OutManifest.Placements.Num(), OutManifest.RejectedRowCount,
		OutManifest.MissingSourceIdCount, OutManifest.NonRenderableRowCount);
	return true;
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

	const std::vector<apb::ManifestCandidate> Candidates =
		apb::PlacementManifestCandidates(TCHAR_TO_UTF8(*DistrictId));
	for (const apb::ManifestCandidate& Candidate : Candidates)
	{
		const FString Name = UTF8_TO_TCHAR(Candidate.file_name.c_str());
		const FString Path = FPaths::Combine(ProjectContentDir, TEXT("Data/district_placements"), Name);
		if (!LoadManifestFromFile(Path, OutManifest)) continue;
		OutManifest.SelectedManifestPath = Path;
		OutManifest.bLoadedBoundManifest = Candidate.synthetic;
		if (Candidate.synthetic)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("PLACEMENT_MANIFEST_SYNTHETIC district=%s path=%s reason=%s"),
				*DistrictId, *Path, UTF8_TO_TCHAR(Candidate.reason_code.c_str()));
		}
		UE_LOG(LogTemp, Warning,
			TEXT("PLACEMENT_MANIFEST_CHOSEN district=%s path=%s provenance=%s renderable=%d rejected=%d"),
			*DistrictId, *Path, *OutManifest.Provenance, OutManifest.Placements.Num(),
			OutManifest.RejectedRowCount);
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("PLACEMENT_MANIFEST_MISSING district=%s base=%s"), *DistrictId, *Base);
	return false;
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

bool UAPBDistrictPlacementLoader::LoadLightsForDistrict(const FString& ProjectContentDir, const FString& DistrictId, FAPBLightManifest& OutLights)
{
	FString Base = TEXT("Financial_Block09");
	if (DistrictId.Contains(TEXT("Waterfront"))) Base = TEXT("Waterfront_Block05");
	else if (DistrictId.Contains(TEXT("Asylum")) || DistrictId.Contains(TEXT("PGAsylum"))) Base = TEXT("Asylum_Block");
	else if (DistrictId.Contains(TEXT("Beacon"))) Base = TEXT("Beacon_Block");
	else if (DistrictId.Contains(TEXT("Crate"))) Base = TEXT("Crate_Block");
	else if (DistrictId.Contains(TEXT("Social"))) Base = TEXT("Social_Block");

	const FString Path = FPaths::Combine(ProjectContentDir, TEXT("Data/district_placements"), Base + TEXT("_lights.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) return false;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;

	OutLights = FAPBLightManifest();
	OutLights.DistrictId = Root->GetStringField(TEXT("district_id"));
	const TArray<TSharedPtr<FJsonValue>>* Arr;
	if (!Root->TryGetArrayField(TEXT("lights"), Arr)) return false;
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;
		FAPBLightEntry L;
		L.Actor = O->GetStringField(TEXT("actor"));
		L.bSpot = O->GetStringField(TEXT("light_type")) == TEXT("spot");
		const TArray<TSharedPtr<FJsonValue>>* Loc;
		if (O->TryGetArrayField(TEXT("location"), Loc) && Loc->Num() >= 3)
			L.Location = FVector((*Loc)[0]->AsNumber(), (*Loc)[1]->AsNumber(), (*Loc)[2]->AsNumber());
		const TArray<TSharedPtr<FJsonValue>>* Rot;
		if (O->TryGetArrayField(TEXT("rotation"), Rot) && Rot->Num() >= 3)
			L.Rotation = FRotator((*Rot)[0]->AsNumber(), (*Rot)[1]->AsNumber(), (*Rot)[2]->AsNumber());
		const TArray<TSharedPtr<FJsonValue>>* Col;
		if (O->TryGetArrayField(TEXT("color"), Col) && Col->Num() >= 3)
			L.Color = FLinearColor((*Col)[0]->AsNumber(), (*Col)[1]->AsNumber(), (*Col)[2]->AsNumber(), 1.f);
		L.Radius = O->HasField(TEXT("radius")) ? static_cast<float>(O->GetNumberField(TEXT("radius"))) : 1024.f;
		L.Brightness = O->HasField(TEXT("brightness")) ? static_cast<float>(O->GetNumberField(TEXT("brightness"))) : 1.f;
		L.Falloff = O->HasField(TEXT("falloff")) ? static_cast<float>(O->GetNumberField(TEXT("falloff"))) : 2.f;
		L.OuterCone = O->HasField(TEXT("outer_cone")) ? static_cast<float>(O->GetNumberField(TEXT("outer_cone"))) : 44.f;
		L.InnerCone = O->HasField(TEXT("inner_cone")) ? static_cast<float>(O->GetNumberField(TEXT("inner_cone"))) : 0.f;
		OutLights.Lights.Add(L);
	}
	return OutLights.Lights.Num() > 0;
}

int32 UAPBDistrictPlacementLoader::SpawnLightsFromManifest(UWorld* World, const FAPBLightManifest& Lights, FVector Center, float Radius, TArray<AActor*>& OutSpawned)
{
	if (!World) return 0;
	const float R2 = Radius * Radius;
	int32 Count = 0;
	for (const FAPBLightEntry& L : Lights.Lights)
	{
		if (FVector::DistSquared(L.Location, Center) > R2) continue;
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// APB 0..1 brightness -> UE candela, with a floor so default-inherited lights still read.
		const float Intensity = FMath::Max(L.Brightness, 0.1f) * 5000.f;
		if (L.bSpot)
		{
			ASpotLight* A = World->SpawnActor<ASpotLight>(L.Location, L.Rotation, Sp);
			if (!A) continue;
			A->SetMobility(EComponentMobility::Movable);
			if (USpotLightComponent* C = Cast<USpotLightComponent>(A->GetLightComponent()))
			{
				C->SetAttenuationRadius(L.Radius);
				C->SetIntensity(Intensity);
				C->SetLightColor(L.Color);
				C->SetOuterConeAngle(FMath::Clamp(L.OuterCone, 1.f, 80.f));
				C->SetInnerConeAngle(FMath::Clamp(L.InnerCone, 0.f, L.OuterCone));
				C->SetCastShadows(false);
			}
			OutSpawned.Add(A);
			++Count;
		}
		else
		{
			APointLight* A = World->SpawnActor<APointLight>(L.Location, L.Rotation, Sp);
			if (!A) continue;
			A->SetMobility(EComponentMobility::Movable);
			if (UPointLightComponent* C = Cast<UPointLightComponent>(A->GetLightComponent()))
			{
				C->SetAttenuationRadius(L.Radius);
				C->SetIntensity(Intensity);
				C->SetLightColor(L.Color);
				C->SetCastShadows(false);
			}
			OutSpawned.Add(A);
			++Count;
		}
	}
	return Count;
}

static UStaticMesh* LoadPlacementMesh(const FAPBPlacementEntry& E, const FString& ManifestDistrictId)
{
	const apb::PlacementBinding Binding = apb::BuildPlacementBinding(
		TCHAR_TO_UTF8(*E.MeshId),
		TCHAR_TO_UTF8(*E.UePath),
		TCHAR_TO_UTF8(*E.Package),
		TCHAR_TO_UTF8(*ManifestDistrictId));
	const FString ExpectedFolder = UTF8_TO_TCHAR(Binding.expected_folder.c_str());
	if (!Binding.valid)
	{
		const FString Reason = UTF8_TO_TCHAR(Binding.reason_code.c_str());
		UE_LOG(LogTemp, Error,
			TEXT("PLACEMENT_MESH_BINDING_FAIL mesh_id=%s expected_folder=%s reason=%s package=%s manifest_district=%s"),
			*E.MeshId, *ExpectedFolder, *Reason, *E.Package, *ManifestDistrictId);
		return nullptr;
	}
	if (E.UePath.Contains(TEXT("BasicShapes/Cube")))
	{
		UE_LOG(LogTemp, Error,
			TEXT("PLACEMENT_MESH_BINDING_FAIL mesh_id=%s expected_folder=%s reason=forbidden_basic_shape"),
			*E.MeshId, *ExpectedFolder);
		return nullptr;
	}
	for (const std::string& Candidate : Binding.candidate_paths)
	{
		const FString Path = UTF8_TO_TCHAR(Candidate.c_str());
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path)) return Mesh;
	}
	UE_LOG(LogTemp, Error,
		TEXT("PLACEMENT_MESH_BINDING_FAIL mesh_id=%s expected_folder=%s reason=mesh_asset_not_found"),
		*E.MeshId, *ExpectedFolder);
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

	// Preserve real baked APB materials (MICs assigned offline by assign_district_materials.py);
	// only stamp the readable fallback onto slots that shipped empty/WorldGrid from the umodel import.
	const int32 Num = FMath::Max(1, Comp->GetNumMaterials());
	for (int32 i = 0; i < Num; ++i)
	{
		const UMaterialInterface* Existing = Comp->GetMaterial(i);
		const bool bMissing = (Existing == nullptr)
			|| (Existing == UMaterial::GetDefaultMaterial(MD_Surface))
			|| Existing->GetName().Contains(TEXT("WorldGrid"));
		if (bMissing)
		{
			Comp->SetMaterial(i, Fallback);
		}
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
		const float Dist2 = FVector::DistSquared(E.Location, Center);
		if (Dist2 > R2) continue;
		++InRadius;
		const apb::PlacementIdentity Identity = apb::PlacementDedupKey(TCHAR_TO_UTF8(*E.SourceId));
		if (!Identity.valid)
		{
			++LoadFailed;
			UE_LOG(LogTemp, Error,
				TEXT("PLACEMENT_IDENTITY_REJECT mesh_id=%s package=%s reason=%s"),
				*E.MeshId, *E.Package, UTF8_TO_TCHAR(Identity.reason_code.c_str()));
			continue;
		}
		const FString Key = UTF8_TO_TCHAR(Identity.key.c_str());
		if (InOutSpawnedMeshKeys.Contains(Key))
		{
			++Skipped;
			continue;
		}
		UStaticMesh* Mesh = LoadPlacementMesh(E, Manifest.DistrictId);
		if (!Mesh)
		{
			++LoadFailed;
			continue;
		}
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(E.Location, E.Rotation, Sp);
		if (!A) continue;
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
		SMC->SetCastShadow(true);
		// Static must be set AFTER all transform calls above, else UE warns about moving
		// a Static component. Static gives the city shell baked-quality shadows + GI.
		A->SetMobility(EComponentMobility::Static);
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
