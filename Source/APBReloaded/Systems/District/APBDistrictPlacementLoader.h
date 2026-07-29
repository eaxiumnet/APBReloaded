#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "APBDistrictPlacementLoader.generated.h"

USTRUCT(BlueprintType)
struct FAPBPlacementEntry
{
	GENERATED_BODY()
	UPROPERTY() FString SourceId;
	UPROPERTY() FString MeshId;
	UPROPERTY() FString UePath;
	UPROPERTY() FString Actor;
	UPROPERTY() FString Edge;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() FVector Scale = FVector::OneVector;
	UPROPERTY() bool bRotationPresent = false;
	UPROPERTY() bool bScalePresent = false;
	UPROPERTY() FString Package;
};

USTRUCT(BlueprintType)
struct FAPBDistrictManifest
{
	GENERATED_BODY()
	UPROPERTY() FString DistrictId;
	UPROPERTY() FString SourcePackage;
	UPROPERTY() FString Provenance;
	UPROPERTY() FString SelectedManifestPath;
	UPROPERTY() FVector PlayerStart = FVector(-200.f, -1200.f, 120.f);
	UPROPERTY() FVector VehicleStart = FVector(400.f, -1200.f, 50.f);
	UPROPERTY() TArray<FAPBPlacementEntry> Placements;
	UPROPERTY() int32 StreamChunkCount = 0;
	/** Mesh-bind metrics (from *_bound.json / bind report). */
	UPROPERTY() int32 BoundCount = 0;
	UPROPERTY() int32 ManifestTotal = 0;
	UPROPERTY() float HitRate = 0.f;
	UPROPERTY() bool bLoadedBoundManifest = false;
	UPROPERTY() int32 RejectedRowCount = 0;
	UPROPERTY() int32 MissingSourceIdCount = 0;
	UPROPERTY() int32 NonRenderableRowCount = 0;
};

USTRUCT(BlueprintType)
struct FAPBLightEntry
{
	GENERATED_BODY()
	UPROPERTY() FString Actor;
	UPROPERTY() bool bSpot = false;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() float Radius = 1024.f;
	UPROPERTY() float Brightness = 1.f;
	UPROPERTY() float Falloff = 2.f;
	UPROPERTY() FLinearColor Color = FLinearColor::White;
	UPROPERTY() float OuterCone = 44.f;
	UPROPERTY() float InnerCone = 0.f;
};

USTRUCT(BlueprintType)
struct FAPBLightManifest
{
	GENERATED_BODY()
	UPROPERTY() FString DistrictId;
	UPROPERTY() TArray<FAPBLightEntry> Lights;
};

/** Spawns freeroam geometry only from Content/Data/district_placements/*.json (no BasicShapes cubes). */
UCLASS()
class APBRELOADED_API UAPBDistrictPlacementLoader : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="APB|District")
	static bool LoadManifestFromFile(const FString& AbsoluteJsonPath, FAPBDistrictManifest& OutManifest);

	UFUNCTION(BlueprintCallable, Category="APB|District")
	static bool LoadManifestForDistrict(const FString& ProjectContentDir, const FString& DistrictId, FAPBDistrictManifest& OutManifest);

	/** Returns number of actors spawned. Never loads /Engine/BasicShapes/Cube. */
	UFUNCTION(BlueprintCallable, Category="APB|District")
	static int32 SpawnFromManifest(UWorld* World, const FAPBDistrictManifest& Manifest, TArray<AActor*>& OutSpawned);

	/** Chunked spawn: only placements within Radius of Center (streaming freeroam). */
	UFUNCTION(BlueprintCallable, Category="APB|District")
	static int32 SpawnFromManifestNear(UWorld* World, const FAPBDistrictManifest& Manifest, FVector Center, float Radius, TArray<AActor*>& OutSpawned, TSet<FString>& InOutSpawnedMeshKeys);

	/**
	 * Chunked spawn with honest mesh-load counters.
	 * OutLoadFailed = placements in radius whose StaticMesh failed to load (not already spawned).
	 * OutSkippedAlready = already in InOutSpawnedMeshKeys.
	 * OutOutOfRadius = placements outside Radius (informational for full-manifest scans; 0 when only scanning).
	 */
	static int32 SpawnFromManifestNearEx(UWorld* World, const FAPBDistrictManifest& Manifest, FVector Center, float Radius,
		TArray<AActor*>& OutSpawned, TSet<FString>& InOutSpawnedMeshKeys,
		int32* OutLoadFailed, int32* OutInRadius, int32* OutSkippedAlready);

	UFUNCTION(BlueprintCallable, Category="APB|District")
	static bool LoadLightsForDistrict(const FString& ProjectContentDir, const FString& DistrictId, FAPBLightManifest& OutLights);

	UFUNCTION(BlueprintCallable, Category="APB|District")
	static int32 SpawnLightsFromManifest(UWorld* World, const FAPBLightManifest& Lights, FVector Center, float Radius, TArray<AActor*>& OutSpawned);

	UFUNCTION(BlueprintCallable, Category="APB|District")
	static bool ManifestUsesEngineCubes(const FAPBDistrictManifest& Manifest);

	UFUNCTION(BlueprintCallable, Category="APB|District")
	static FString ResolveDistrictIdFromMapName(const FString& MapName);

	/** Count placements within Radius of Center (no spawn) — stream bubble metric. */
	UFUNCTION(BlueprintCallable, Category="APB|District")
	static int32 CountPlacementsNear(const FAPBDistrictManifest& Manifest, FVector Center, float Radius);

	/** Ensure mesh component has a usable lit material (WorldGrid / DefaultLit) so geometry is not pure black. */
	static void EnsureVisibleMeshMaterials(UStaticMeshComponent* Comp);
};
