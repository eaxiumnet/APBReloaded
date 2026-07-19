#pragma once
#include "CoreMinimal.h"
#include "APBDistrictGameMode.h"
#include "APBDistrictPlacementLoader.h"
#include "APBFreeroamGameMode.generated.h"

/** District freeroam host: map→district manifest, chunked spawn, no session-loop theater. */
UCLASS()
class APBRELOADED_API AAPBFreeroamGameMode : public AAPBDistrictGameMode
{
	GENERATED_BODY()
public:
	AAPBFreeroamGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB|Freeroam")
	bool bLoadPlacementManifest = true;

	/** Streaming radius in cm for chunked placement load (San Paro multi-block spans). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB|Freeroam")
	float StreamRadiusCm = 60000.f;

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Spawn mailbox/ammo/resupply/bots/extra vehicles for playable freeroam. */
	void SpawnPlayableWorldProps();

private:
	void ResolveDistrictFromMap();
	void LoadDistrictContent();
	void RefreshStreamAroundPlayers();
	void EnsureDistrictLighting(const FVector& At);
	void AlignPlayerStartsAndTeleport(const FVector& At);
	void AppendFreeroamLog(const FString& Line);

	TArray<AActor*> SpawnedActors;
	TSet<FString> SpawnedPlacementKeys;
	FAPBDistrictManifest CachedManifest;
	bool bManifestLoaded = false;
	bool bLightingReady = false;
	float StreamAccum = 0.f;
	int32 SpawnedNpc = 0;
	int32 SpawnedMailbox = 0;
	int32 SpawnedAmmo = 0;
	int32 SpawnedResupply = 0;
	int32 SpawnedVehicles = 0;
	int32 LastMeshLoadFailed = 0;
	int32 LastInRadius = 0;
};
