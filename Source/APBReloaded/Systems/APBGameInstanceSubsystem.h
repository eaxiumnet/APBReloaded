#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "APBFrontendTypes.h"
#include "APBWorldService.h"
#include "APBGameInstanceSubsystem.generated.h"

class AAPBPlayerState;
class APlayerController;
class UWorld;

USTRUCT(BlueprintType)
struct FAPBMissionSnapshotUE
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString MissionTitle;
	UPROPERTY(BlueprintReadOnly) int32 MissionStageIndex = 0;
	UPROPERTY(BlueprintReadOnly) int32 MissionStageCount = 0;
	UPROPERTY(BlueprintReadOnly) float MissionStageProgress = 0.f;
	UPROPERTY(BlueprintReadOnly) float MissionOppStageProgress = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bMissionOppositionContesting = false;
	UPROPERTY(BlueprintReadOnly) bool bMissionOppositionWon = false;
	UPROPERTY(BlueprintReadOnly) bool bMissionTimedOut = false;
	UPROPERTY(BlueprintReadOnly) float MissionStageTimeLimitSec = 0.f;
	UPROPERTY(BlueprintReadOnly) float MissionStageDeadlineServerSec = 0.f;
};

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
	UPROPERTY(BlueprintReadOnly) FString ProgressionState;
	UPROPERTY(BlueprintReadOnly) FString SessionId;
	UPROPERTY(BlueprintReadOnly) FString DistrictId;
	UPROPERTY(BlueprintReadOnly) int32 DistrictPlayers = 0;
	UPROPERTY(BlueprintReadOnly) FAPBMissionSnapshotUE Mission;
};

USTRUCT(BlueprintType)
struct FAPBFriendEntryUE
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString Name = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") bool bOnline = false;
};

USTRUCT(BlueprintType)
struct FAPBClanInfoUE
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString Id = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString Name = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString Tag = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString Motd = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString LeaderName = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") TArray<FString> Members = {};
};

USTRUCT(BlueprintType)
struct FAPBMailMessageUE
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString Id = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString From = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString Subject = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString Body = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") bool bRead = false;
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") bool bClaimed = false;
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") int64 Cash = 0;
};

USTRUCT(BlueprintType)
struct FAPBGroupInfoUE
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString Id = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString Leader = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") FString MissionId = TEXT("");
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") bool bAllReady = false;
	UPROPERTY(BlueprintReadOnly, Category="APB|Social") TArray<FString> Members = {};
};

UENUM(BlueprintType)
enum class EAPBMailResult : uint8
{
	Ok                   UMETA(DisplayName="Ok"),
	NotOwner             UMETA(DisplayName="NotOwner"),
	NotFound             UMETA(DisplayName="NotFound"),
	AlreadyClaimed       UMETA(DisplayName="AlreadyClaimed"),
	Unclaimed            UMETA(DisplayName="Unclaimed"),
	GrantFailed          UMETA(DisplayName="GrantFailed"),
	AuthorityUnavailable UMETA(DisplayName="AuthorityUnavailable"),
	/** Attachment payload this build cannot grant (item mail before inventory
	 *  integration). Distinct from GrantFailed: nothing was mutated, so the
	 *  message stays unclaimed, undeletable and reclaimable. */
	UnsupportedAttachment UMETA(DisplayName="UnsupportedAttachment"),
};

/** Transport-level outcome a UI needs to distinguish a legitimate domain rejection
 *  from an infrastructure failure (no social authority reachable in this process).
 *  Declared here for future RPC transport wiring; not yet wired to any RPC path. */
UENUM(BlueprintType)
enum class EAPBSocialTransportResult : uint8
{
	Ok                   UMETA(DisplayName="Ok"),
	DomainRejected       UMETA(DisplayName="DomainRejected"),
	AuthorityUnavailable UMETA(DisplayName="AuthorityUnavailable"),
	Timeout              UMETA(DisplayName="Timeout"),
	StaleSession         UMETA(DisplayName="StaleSession"),
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

	/** Server mission clock: fails the active mission when its current stage deadline passes.
	 *  Caller supplies now_sec (must be the GameState server-world time the HUD counts against). */
	UFUNCTION(BlueprintCallable, Category="APB|Mission")
	bool TickMission(float NowSec);

	UFUNCTION(BlueprintCallable, Category="APB|Mission")
	bool AdvanceOpposition(float Amount = 1.0f);

	UFUNCTION(BlueprintCallable, Category="APB|Mission")
	bool IsMissionActive() const;

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

	/** M5: clothing rows for a wardrobe tab (1..15) from the parsed Domain catalog. */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	TArray<FAPBClothingChoice> GetClothingForTab(int32 TabId, int32 MaxItems = 60) const;

	/** M5: domain equip-slot key for a wardrobe tab (1..15). */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	FString GetSlotForTab(int32 TabId) const;

	/** M5: equip with palette color indices (EquipClothingItem drops color). */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	bool EquipClothingColored(const FString& Slot, const FString& ItemId, int32 ColorPrimary, int32 ColorSecondary);

	/** M5: randomize the whole appearance for the current character's faction. */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	bool RandomizeAppearance(int32 Seed);

	/** M5: palette row colors (Clothing/Hair/Symbols) from palettes.json. */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	TArray<FLinearColor> GetPaletteColors(const FString& PaletteName, int32 RowIndex) const;

	/** M5: append a symbol/tattoo layer (stub; full editor is M17). */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	bool AddSymbolLayer(int32 SymbolId, const FString& TargetSlot, float PosX, float PosY, float Rotation, float Scale, int32 ColorPrimary, int32 ColorSecondary);

	/** M5: current symbol/tattoo layer count. */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	int32 GetSymbolLayerCount() const;

	/** M5: per-tab preview camera frame from wardrobe_categories.json (retail APBLCC), default_camera fallback. */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	bool GetCameraFrameForTab(int32 TabId, float& OutPosY, float& OutPosZ, float& OutTargetZ, float& OutFov) const;

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
	bool ApplyHandoffSnapshot(const apb::DomainSnapshot& Snapshot);
	bool ApplyHandoffProbeMutation();

	/** True when local net mode may mutate Domain (standalone / listen-server / dedicated). */
	bool CanMutateDomain() const;

	// ── M14 Social — Clan (T09) ───────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanCreate(const FString& ClanId, const FString& Name, const FString& Tag, const FString& Leader, bool bEnforcer);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanInvite(const FString& Inviter, const FString& Invitee);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanAcceptInvite(const FString& Invitee);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanDeclineInvite(const FString& Invitee);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanKick(const FString& Actor, const FString& Target);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanSetMotd(const FString& Actor, const FString& Text);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanAddRank(const FString& Actor, const FString& RankName, int32 Permissions);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanSetMemberRank(const FString& Actor, const FString& Target, int32 RankIndex);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanLeave(const FString& Player);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanDisband(const FString& Leader);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	bool SocialClanTransferLeader(const FString& Leader, const FString& Target);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Clan")
	FAPBClanInfoUE GetClanInfo(const FString& ClanId) const;

	// ── M14 Social — Friends (T10) ────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	bool SocialFriendRequest(const FString& From, const FString& To);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	bool SocialFriendAccept(const FString& Invitee, const FString& Inviter);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	bool SocialFriendDecline(const FString& Invitee, const FString& Inviter);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	bool SocialFriendRemove(const FString& Player, const FString& Other);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	bool SocialFriendIgnore(const FString& Player, const FString& Target);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	bool SocialFriendUnignore(const FString& Player, const FString& Target);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	bool SocialAreFriends(const FString& A, const FString& B) const;

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	bool SocialIsIgnoring(const FString& Player, const FString& Target) const;

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	TArray<FAPBFriendEntryUE> SocialGetFriendList(const FString& Player) const;

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	TArray<FString> SocialGetIncomingRequests(const FString& Player) const;

	UFUNCTION(BlueprintCallable, Category="APB|Social|Friends")
	TArray<FString> SocialGetIgnoreList(const FString& Player) const;

	// ── M14 Social — Group (T11) ──────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category="APB|Social|Group")
	bool SocialGroupCreate(const FString& Leader, FString& OutGroupId);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Group")
	bool SocialGroupInvite(const FString& Inviter, const FString& Invitee);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Group")
	bool SocialGroupAccept(const FString& Invitee);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Group")
	bool SocialGroupLeave(const FString& Player);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Group")
	bool SocialGroupKick(const FString& Leader, const FString& Target);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Group")
	bool SocialGroupDisband(const FString& Leader);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Group")
	bool SocialGroupTransferLeader(const FString& Leader, const FString& Target);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Group")
	bool SocialGroupSetReady(const FString& Player, bool bReady);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Group")
	bool SocialGroupAssignMission(const FString& Leader, const FString& MissionId);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Group")
	FAPBGroupInfoUE GetGroupInfo(const FString& GroupId) const;

	UFUNCTION(BlueprintCallable, Category="APB|Social|Mail")
	bool SocialMailSend(const FString& Character, const FString& To, const FString& Subject, const FString& Body, int64 Cash);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Mail")
	TArray<FAPBMailMessageUE> SocialMailGetInbox(const FString& Character) const;

	UFUNCTION(BlueprintCallable, Category="APB|Social|Mail")
	int32 SocialMailUnreadCount(const FString& Character) const;

	UFUNCTION(BlueprintCallable, Category="APB|Social|Mail")
	EAPBMailResult SocialMailMarkRead(const FString& Character, const FString& MailId);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Mail")
	EAPBMailResult SocialMailClaimAttachments(const FString& Character, const FString& MailId);

	UFUNCTION(BlueprintCallable, Category="APB|Social|Mail")
	EAPBMailResult SocialMailDelete(const FString& Character, const FString& MailId);

	FString DataDir;
	/** Domain JSON persistence root: <ProjectSavedDir>/DomainDB (M2). */
	FString PersistDir;
	void* Service = nullptr;

	// ── M6 world-server client mode ───────────────────────────────────────────

	/** True when this client connected to a dedicated world-server process. */
	UPROPERTY(BlueprintReadOnly, Category="APB|Auth")
	bool bWorldServerMode = false;

	/** Connect to a world-server at host:port. Sets bWorldServerMode on success. */
	UFUNCTION(BlueprintCallable, Category="APB|Auth")
	bool ConnectToWorldServer(const FString& Host, int32 Port);

	UFUNCTION(BlueprintCallable, Category="APB|Auth")
	bool IsWorldServerConnected() const;

	/** Last ticket string issued by the world server (for district travel URL). */
	UFUNCTION(BlueprintCallable, Category="APB|Auth")
	FString GetIssuedTicket() const;
	void StartDistrictTravel(APlayerController* PlayerController, const FString& DistrictId,
		const FString& Host, int32 Port, const FString& Ticket, const FString& ReservationId);

private:
	void HandleDistrictTravelMapLoaded(UWorld* LoadedWorld);
	FDelegateHandle DistrictTravelLoadedHandle;
	FDelegateHandle DistrictTravelFailureHandle;
	FTimerHandle DistrictTravelTimeoutHandle;
	bool bDistrictTravelPending = false;
	FString PendingTravelDistrict;
	FString PendingTravelHost;
	int32 PendingTravelPort = 0;
	FString PendingTravelReservationId;
	void HandleDistrictTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType,
		const FString& ErrorString);
	void HandleDistrictTravelTimeout();
};
