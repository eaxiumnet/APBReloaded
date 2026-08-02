#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "APBVerifiedAssetRegistry.generated.h"

class UMaterialInterface;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;
class UTexture2D;
class UWorld;

UCLASS()
class APBRELOADED_API UAPBVerifiedAssetRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool IsStrictEnforcementEnabled() const { return bStrictEnforcement; }
	bool IsManifestLoaded() const { return bManifestLoaded; }
	int32 GetAllowedEntryCount() const { return AllowedEntries.Num(); }
	bool IsAllowed(const FString& ObjectPath, const FName& ExpectedClass, FString* OutReason = nullptr) const;
	bool GetFirstAllowedStaticMeshEntry(FString& OutObjectPath) const;
	UStaticMesh* LoadStaticMesh(UWorld* World, const FString& ObjectPath, const FString& Context);
	USkeletalMesh* LoadSkeletalMesh(UWorld* World, const FString& ObjectPath, const FString& Context);
	UTexture2D* LoadTexture2D(UWorld* World, const FString& ObjectPath, const FString& Context);
	UMaterialInterface* LoadMaterialInterface(UWorld* World, const FString& ObjectPath, const FString& Context);
	USoundBase* LoadSoundBase(UWorld* World, const FString& ObjectPath, const FString& Context);

private:
	struct FAllowedEntry
	{
		FName Class;
		FString SourceBuild;
	};

	TMap<FString, FAllowedEntry> AllowedEntries;
	FString ManifestPath;
	bool bManifestLoaded = false;
	bool bStrictEnforcement = false;
};
