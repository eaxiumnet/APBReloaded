#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "APBVerifiedDistrictAssetRouting.generated.h"

class UMaterialInterface;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;
class UTexture2D;
class UWorld;
class UAPBVerifiedAssetRegistry;

/**
 * Task-19 fail-closed routing layer for every district, character, vehicle,
 * weapon, and interactable asset load. Every route resolves through
 * UAPBVerifiedAssetRegistry and never substitutes an alternate asset on
 * rejection: a denied path yields nullptr with a stable DISTRICTRoute_* log
 * reason. Placement routes keep apb::BuildPlacementBinding as district
 * identity validation before registry resolution.
 */
UCLASS()
class APBRELOADED_API UAPBVerifiedDistrictAssetRouting : public UObject
{
	GENERATED_BODY()

public:
	static UStaticMesh* LoadStaticMesh(UWorld* World, const FString& ObjectPath, const FString& Domain, const FString& Context);
	static USkeletalMesh* LoadSkeletalMesh(UWorld* World, const FString& ObjectPath, const FString& Domain, const FString& Context);
	static UTexture2D* LoadTexture2D(UWorld* World, const FString& ObjectPath, const FString& Domain, const FString& Context);
	static UMaterialInterface* LoadMaterialInterface(UWorld* World, const FString& ObjectPath, const FString& Domain, const FString& Context);
	static USoundBase* LoadSoundBase(UWorld* World, const FString& ObjectPath, const FString& Domain, const FString& Context);

	static UStaticMesh* RoutePlacementMesh(UWorld* World, const FString& MeshId, const FString& UePath,
		const FString& Package, const FString& ManifestDistrictId);
	static UStaticMesh* RouteFactionVisualMesh(UWorld* World, const FString& ObjectPath, const FString& Context);
	static UStaticMesh* RouteWardrobeMesh(UWorld* World, const FString& ObjectPath, const FString& Slot, const FString& ItemId);
	static UStaticMesh* RouteHeroLandmarkMesh(UWorld* World, const FString& ObjectPath, const FString& Context);
	static UStaticMesh* RouteVehicleMesh(UWorld* World, const FString& ObjectPath, const FString& Context);
	static UStaticMesh* RouteWeaponMesh(UWorld* World, const FString& ObjectPath, const FString& Context);
	static UStaticMesh* RouteInteractableMesh(UWorld* World, const FString& ObjectPath, const FString& Context);

private:
	static UAPBVerifiedAssetRegistry* GetRegistry(UWorld* World);
};
