#include "APBVerifiedDistrictAssetRouting.h"
#include "APBVerifiedAssetRegistry.h"
#include "Domain/APBPlacementBinding.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "UObject/UObjectGlobals.h"

UAPBVerifiedAssetRegistry* UAPBVerifiedDistrictAssetRouting::GetRegistry(UWorld* World)
{
	if (!World || !World->GetGameInstance())
	{
		return nullptr;
	}
	return World->GetGameInstance()->GetSubsystem<UAPBVerifiedAssetRegistry>();
}

UStaticMesh* UAPBVerifiedDistrictAssetRouting::LoadStaticMesh(UWorld* World, const FString& ObjectPath, const FString& Domain, const FString& Context)
{
	UAPBVerifiedAssetRegistry* Registry = GetRegistry(World);
	if (!Registry)
	{
		UE_LOG(LogTemp, Error, TEXT("DISTRICT_ROUTE_REJECT domain=%s path=%s class=StaticMesh reason=verified_registry_unavailable context=%s"),
			*Domain, *ObjectPath, *Context);
		return nullptr;
	}
	UStaticMesh* Mesh = Registry->LoadStaticMesh(World, ObjectPath, Context);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("DISTRICT_ROUTE_REJECT domain=%s path=%s class=StaticMesh reason=registry_rejected context=%s"),
			*Domain, *ObjectPath, *Context);
		return nullptr;
	}
	UE_LOG(LogTemp, Verbose, TEXT("DISTRICT_ROUTE_ALLOW domain=%s path=%s class=StaticMesh context=%s"),
		*Domain, *ObjectPath, *Context);
	return Mesh;
}

USkeletalMesh* UAPBVerifiedDistrictAssetRouting::LoadSkeletalMesh(UWorld* World, const FString& ObjectPath, const FString& Domain, const FString& Context)
{
	UAPBVerifiedAssetRegistry* Registry = GetRegistry(World);
	if (!Registry)
	{
		UE_LOG(LogTemp, Error, TEXT("DISTRICT_ROUTE_REJECT domain=%s path=%s class=SkeletalMesh reason=verified_registry_unavailable context=%s"),
			*Domain, *ObjectPath, *Context);
		return nullptr;
	}
	USkeletalMesh* Mesh = Registry->LoadSkeletalMesh(World, ObjectPath, Context);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("DISTRICT_ROUTE_REJECT domain=%s path=%s class=SkeletalMesh reason=registry_rejected context=%s"),
			*Domain, *ObjectPath, *Context);
		return nullptr;
	}
	return Mesh;
}

UTexture2D* UAPBVerifiedDistrictAssetRouting::LoadTexture2D(UWorld* World, const FString& ObjectPath, const FString& Domain, const FString& Context)
{
	UAPBVerifiedAssetRegistry* Registry = GetRegistry(World);
	if (!Registry)
	{
		UE_LOG(LogTemp, Error, TEXT("DISTRICT_ROUTE_REJECT domain=%s path=%s class=Texture2D reason=verified_registry_unavailable context=%s"),
			*Domain, *ObjectPath, *Context);
		return nullptr;
	}
	UTexture2D* Texture = Registry->LoadTexture2D(World, ObjectPath, Context);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("DISTRICT_ROUTE_REJECT domain=%s path=%s class=Texture2D reason=registry_rejected context=%s"),
			*Domain, *ObjectPath, *Context);
		return nullptr;
	}
	return Texture;
}

UMaterialInterface* UAPBVerifiedDistrictAssetRouting::LoadMaterialInterface(UWorld* World, const FString& ObjectPath, const FString& Domain, const FString& Context)
{
	UAPBVerifiedAssetRegistry* Registry = GetRegistry(World);
	if (!Registry)
	{
		UE_LOG(LogTemp, Error, TEXT("DISTRICT_ROUTE_REJECT domain=%s path=%s class=MaterialInterface reason=verified_registry_unavailable context=%s"),
			*Domain, *ObjectPath, *Context);
		return nullptr;
	}
	UMaterialInterface* Material = Registry->LoadMaterialInterface(World, ObjectPath, Context);
	if (!Material)
	{
		UE_LOG(LogTemp, Error, TEXT("DISTRICT_ROUTE_REJECT domain=%s path=%s class=MaterialInterface reason=registry_rejected context=%s"),
			*Domain, *ObjectPath, *Context);
		return nullptr;
	}
	return Material;
}

USoundBase* UAPBVerifiedDistrictAssetRouting::LoadSoundBase(UWorld* World, const FString& ObjectPath, const FString& Domain, const FString& Context)
{
	UAPBVerifiedAssetRegistry* Registry = GetRegistry(World);
	if (!Registry)
	{
		UE_LOG(LogTemp, Error, TEXT("DISTRICT_ROUTE_REJECT domain=%s path=%s class=SoundBase reason=verified_registry_unavailable context=%s"),
			*Domain, *ObjectPath, *Context);
		return nullptr;
	}
	USoundBase* Sound = Registry->LoadSoundBase(World, ObjectPath, Context);
	if (!Sound)
	{
		UE_LOG(LogTemp, Error, TEXT("DISTRICT_ROUTE_REJECT domain=%s path=%s class=SoundBase reason=registry_rejected context=%s"),
			*Domain, *ObjectPath, *Context);
		return nullptr;
	}
	return Sound;
}

UStaticMesh* UAPBVerifiedDistrictAssetRouting::RoutePlacementMesh(UWorld* World, const FString& MeshId, const FString& UePath,
	const FString& Package, const FString& ManifestDistrictId)
{
	const apb::PlacementBinding Binding = apb::BuildPlacementBinding(
		TCHAR_TO_UTF8(*MeshId),
		TCHAR_TO_UTF8(*UePath),
		TCHAR_TO_UTF8(*Package),
		TCHAR_TO_UTF8(*ManifestDistrictId));
	const FString ExpectedFolder = UTF8_TO_TCHAR(Binding.expected_folder.c_str());
	if (!Binding.valid)
	{
		const FString Reason = UTF8_TO_TCHAR(Binding.reason_code.c_str());
		UE_LOG(LogTemp, Error,
			TEXT("PLACEMENT_MESH_BINDING_FAIL mesh_id=%s expected_folder=%s reason=%s package=%s manifest_district=%s"),
			*MeshId, *ExpectedFolder, *Reason, *Package, *ManifestDistrictId);
		return nullptr;
	}
	if (UePath.Contains(TEXT("BasicShapes/Cube")))
	{
		UE_LOG(LogTemp, Error,
			TEXT("PLACEMENT_MESH_BINDING_FAIL mesh_id=%s expected_folder=%s reason=forbidden_basic_shape"),
			*MeshId, *ExpectedFolder);
		return nullptr;
	}
	UAPBVerifiedAssetRegistry* Registry = GetRegistry(World);
	for (const std::string& Candidate : Binding.candidate_paths)
	{
		const FString Path = UTF8_TO_TCHAR(Candidate.c_str());
		if (!Path.StartsWith(TEXT("/Game/Imported/Districts/"))) continue;
		if (!Registry)
		{
			UE_LOG(LogTemp, Error,
				TEXT("PLACEMENT_MESH_BINDING_FAIL mesh_id=%s expected_folder=%s reason=verified_registry_unavailable"),
				*MeshId, *ExpectedFolder);
			return nullptr;
		}
		if (UStaticMesh* Mesh = Registry->LoadStaticMesh(World, Path, TEXT("district_placement"))) return Mesh;
	}
	UE_LOG(LogTemp, Error,
		TEXT("PLACEMENT_MESH_BINDING_FAIL mesh_id=%s expected_folder=%s reason=mesh_asset_not_found"),
		*MeshId, *ExpectedFolder);
	return nullptr;
}

UStaticMesh* UAPBVerifiedDistrictAssetRouting::RouteFactionVisualMesh(UWorld* World, const FString& ObjectPath, const FString& Context)
{
	return LoadStaticMesh(World, ObjectPath, TEXT("character"), Context);
}

UStaticMesh* UAPBVerifiedDistrictAssetRouting::RouteWardrobeMesh(UWorld* World, const FString& ObjectPath, const FString& Slot, const FString& ItemId)
{
	return LoadStaticMesh(World, ObjectPath, TEXT("character"), FString::Printf(TEXT("wardrobe slot=%s item=%s"), *Slot, *ItemId));
}

UStaticMesh* UAPBVerifiedDistrictAssetRouting::RouteHeroLandmarkMesh(UWorld* World, const FString& ObjectPath, const FString& Context)
{
	return LoadStaticMesh(World, ObjectPath, TEXT("character"), Context);
}

UStaticMesh* UAPBVerifiedDistrictAssetRouting::RouteVehicleMesh(UWorld* World, const FString& ObjectPath, const FString& Context)
{
	return LoadStaticMesh(World, ObjectPath, TEXT("vehicle"), Context);
}

UStaticMesh* UAPBVerifiedDistrictAssetRouting::RouteWeaponMesh(UWorld* World, const FString& ObjectPath, const FString& Context)
{
	return LoadStaticMesh(World, ObjectPath, TEXT("weapon"), Context);
}

UStaticMesh* UAPBVerifiedDistrictAssetRouting::RouteInteractableMesh(UWorld* World, const FString& ObjectPath, const FString& Context)
{
	return LoadStaticMesh(World, ObjectPath, TEXT("interactable"), Context);
}
