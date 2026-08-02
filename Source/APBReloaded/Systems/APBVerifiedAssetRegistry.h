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
	int32 GetMediaEntryCount() const { return MediaEntryCount; }
	bool IsAllowed(const FString& ObjectPath, const FName& ExpectedClass, FString* OutReason = nullptr) const;
	bool IsAllowedWithSourceBuild(const FString& ObjectPath, const FName& ExpectedClass,
		const FString& ExpectedSourceBuild, FString* OutReason = nullptr) const;
	bool IsMediaAllowed(const FString& MediaPath, const FString& Context, FString* OutReason = nullptr) const;
	bool GetFirstAllowedStaticMeshEntry(FString& OutObjectPath) const;
	UStaticMesh* LoadStaticMesh(UWorld* World, const FString& ObjectPath, const FString& Context,
		const FString& ExpectedSourceBuild = TEXT(""));
	USkeletalMesh* LoadSkeletalMesh(UWorld* World, const FString& ObjectPath, const FString& Context,
		const FString& ExpectedSourceBuild = TEXT(""));
	UTexture2D* LoadTexture2D(UWorld* World, const FString& ObjectPath, const FString& Context,
		const FString& ExpectedSourceBuild = TEXT(""));
	UMaterialInterface* LoadMaterialInterface(UWorld* World, const FString& ObjectPath, const FString& Context,
		const FString& ExpectedSourceBuild = TEXT(""));
	USoundBase* LoadSoundBase(UWorld* World, const FString& ObjectPath, const FString& Context,
		const FString& ExpectedSourceBuild = TEXT(""));

private:
	struct FAllowedEntry
	{
		FName Class;
		FString SourceBuild;
	};

	struct FMediaEntry
	{
		FString ObjectPath;
		FString FilePath;
		FString SourceBuild;
	};

	TMap<FString, FAllowedEntry> AllowedEntries;
	TMap<FString, FMediaEntry> MediaEntries;
	FString ManifestPath;
	int32 MediaEntryCount = 0;
	bool bManifestLoaded = false;
	bool bStrictEnforcement = false;
	bool bManifestOverride = false;
};
