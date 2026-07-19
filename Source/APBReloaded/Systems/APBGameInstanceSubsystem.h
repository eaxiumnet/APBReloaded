#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "APBFrontendTypes.h"
#include "APBGameInstanceSubsystem.generated.h"

class AAPBPlayerState;

USTRUCT(BlueprintType)
struct FAPBDomainSnapshotUE
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bHasCharacter = false;
	UPROPERTY(BlueprintReadOnly) FString CharacterName;
	UPROPERTY(BlueprintReadOnly) bool bEnforcer = false;
	UPROPERTY(BlueprintReadOnly) int64 Cash = 0;
	UPROPERTY(BlueprintReadOnly) int64 G1C = 0;
	UPROPERTY(BlueprintReadOnly) int32 InventorySlotCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 InventoryTotalQty = 0;
	UPROPERTY(BlueprintReadOnly) float ThreatPoints = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 ThreatBots = 0;
	UPROPERTY(BlueprintReadOnly) FString MissionId;
	UPROPERTY(BlueprintReadOnly) FString MissionTitle;
	UPROPERTY(BlueprintReadOnly) int32 MissionStageIndex = 0;
	UPROPERTY(BlueprintReadOnly) int32 MissionStageCount = 0;
	UPROPERTY(BlueprintReadOnly) FString MissionStatus;
	UPROPERTY(BlueprintReadOnly) FString SessionId;
	UPROPERTY(BlueprintReadOnly) FString DistrictId;
	UPROPERTY(BlueprintReadOnly) int32 DistrictPlayers = 0;
};

UCLASS()
class APBRELOADED_API UAPBGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="APB|World")
	bool InitCatalogFromProjectData();

	UFUNCTION(BlueprintCallable, Category="APB|World")
	bool CreateCharacter(const FString& Name, bool bEnforcer);

	UFUNCTION(BlueprintCallable, Category="APB|World")
	TArray<FString> GetDistrictList() const;

	UFUNCTION(BlueprintCallable, Category="APB|World")
	bool JoinDistrict(const FString& DistrictId);

	UFUNCTION(BlueprintCallable, Category="APB|World")
	bool JoinDistrictAsPeer(const FString& SessionId, const FString& PlayerName);

	UFUNCTION(BlueprintCallable, Category="APB|World")
	FString GetSessionId() const;

	UFUNCTION(BlueprintCallable, Category="APB|World")
	FString GetPhase() const;

	UFUNCTION(BlueprintCallable, Category="APB|Economy")
	bool ArmasPurchase(const FString& ItemId, FString& OutError);

	UFUNCTION(BlueprintCallable, Category="APB|Economy")
	bool AuctionListItem(const FString& ItemId, int32 Qty, int64 Price, int64& OutListingId, FString& OutError);

	UFUNCTION(BlueprintCallable, Category="APB|District")
	void StartOppositionMission();

	UFUNCTION(BlueprintCallable, Category="APB|District")
	bool AdvanceMissionStage();

	UFUNCTION(BlueprintCallable, Category="APB|District")
	float GetThreatPoints() const;

	UFUNCTION(BlueprintCallable, Category="APB|District")
	int32 GetThreatBotCount() const;

	UFUNCTION(BlueprintCallable, Category="APB|Auth")
	bool Login(const FString& User, const FString& Pass);

	UFUNCTION(BlueprintCallable, Category="APB|Auth")
	bool RegisterAccount(const FString& User, const FString& Pass);

	UFUNCTION(BlueprintCallable, Category="APB|World")
	bool EnterWorld(const FString& WorldId);

	UFUNCTION(BlueprintCallable, Category="APB|District")
	bool SpawnCatalogVehicle(const FString& VehicleId);

	UFUNCTION(BlueprintCallable, Category="APB|District")
	bool PossessCatalogVehicle();

	UFUNCTION(BlueprintCallable, Category="APB|District")
	TArray<FString> GetStreamChunksNear(float X, float Y) const;

	UFUNCTION(BlueprintCallable, Category="APB|District")
	float GetOppositionPressure() const;

	UFUNCTION(BlueprintCallable, Category="APB|Combat")
	float FireCatalogWeapon(const FString& WeaponId, float AimX, float AimY, float& OutTargetHealth, bool& bKilled);

	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	bool EquipClothingItem(const FString& Slot, const FString& ItemId);

	/** Apply body height/bulk (and optional skin/face) into Domain CharacterAppearance. */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	bool ApplyBodyProfile(float Height, float Bulk, int32 SkinTone = 1, int32 FacePreset = 1);

	/** Read current Domain body profile (after ApplyBodyProfile / create). */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	bool GetBodyProfile(float& OutHeight, float& OutBulk) const;

	/** Clothing catalog rows for a slot (from Content/Data/clothing.json). */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	TArray<FAPBClothingChoice> GetClothingForSlot(const FString& Slot, int32 MaxItems = 40) const;

	/** Map soft path for a district id (Lvl_APB_*). */
	UFUNCTION(BlueprintCallable, Category="APB|World")
	FString GetDistrictMapName(const FString& DistrictId) const;

	UFUNCTION(BlueprintCallable, Category="APB|District")
	int32 GetDistrictPlayerCount() const;

	/** Pure readers from shipped Domain WorldService::CaptureSnapshot(). */
	UFUNCTION(BlueprintCallable, Category="APB|Domain")
	FAPBDomainSnapshotUE CaptureDomainSnapshot() const;

	UFUNCTION(BlueprintCallable, Category="APB|Domain")
	int32 GetInventorySlotCount() const;

	UFUNCTION(BlueprintCallable, Category="APB|Domain")
	int64 GetCharacterCash() const;

	UFUNCTION(BlueprintCallable, Category="APB|Domain")
	int64 GetCharacterG1C() const;

	UFUNCTION(BlueprintCallable, Category="APB|Domain")
	FString GetMissionTitle() const;

	UFUNCTION(BlueprintCallable, Category="APB|Domain")
	int32 GetMissionStageIndex() const;

	UFUNCTION(BlueprintCallable, Category="APB|Domain")
	int32 GetMissionStageCount() const;

	/** Single Domain→PlayerState bridge — always call after Domain mutations (authority only). */
	UFUNCTION(BlueprintCallable, Category="APB|Domain")
	void SyncPlayerStateFromDomain(AAPBPlayerState* PlayerState);

	/** Server: push host Domain snapshot to every PlayerState and ForceNetUpdate (MP OnRep path). */
	UFUNCTION(BlueprintCallable, Category="APB|Domain")
	void PushDomainSnapshotToAllPlayerStates();

	/** True when local net mode may mutate Domain (standalone / listen-server / dedicated). */
	bool CanMutateDomain() const;

	FString DataDir;
	/** Domain JSON persistence root: <ProjectSavedDir>/DomainDB (M2). */
	FString PersistDir;
	void* Service = nullptr;
};
