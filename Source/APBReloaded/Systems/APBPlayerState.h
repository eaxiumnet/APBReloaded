#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "APBGameInstanceSubsystem.h"
#include "APBPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAPBMissionStateChangedDelegate);

UENUM(BlueprintType)
enum class EAPBFaction : uint8
{
	Enforcer UMETA(DisplayName="Enforcer"),
	Criminal UMETA(DisplayName="Criminal")
};

UCLASS()
class APBRELOADED_API AAPBPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	UPROPERTY(ReplicatedUsing=OnRep_Faction, BlueprintReadOnly, Category="APB")
	EAPBFaction Faction = EAPBFaction::Criminal;

	UPROPERTY(ReplicatedUsing=OnRep_Economy, BlueprintReadOnly, Category="APB")
	float ThreatPoints = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_Economy, BlueprintReadOnly, Category="APB")
	int64 Cash = 10000;

	UPROPERTY(ReplicatedUsing=OnRep_Economy, BlueprintReadOnly, Category="APB")
	int64 G1C = 5000;

	UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="APB")
	FString MissionTitle;

	UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="APB")
	int32 MissionStageIndex = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="APB")
	int32 MissionStageCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="APB|Mission")
	float MissionStageProgress = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="APB|Mission")
	float MissionOppStageProgress = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="APB|Mission")
	bool bMissionOppositionContesting = false;

	UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="APB|Mission")
	bool bMissionOppositionWon = false;

	UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="APB|Mission")
	bool bMissionTimedOut = false;

	UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="APB|Mission")
	float MissionStageTimeLimitSec = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_Mission, BlueprintReadOnly, Category="APB|Mission")
	float MissionStageDeadlineServerSec = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_Economy, BlueprintReadOnly, Category="APB")
	int32 InventoryItemCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Economy, BlueprintReadOnly, Category="APB")
	FString ProgressionState;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="APB")
	FString DistrictSessionId;

	UPROPERTY(BlueprintAssignable, Category="APB|Mission")
	FAPBMissionStateChangedDelegate OnMissionUpdated;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB")
	void ServerSetFaction(EAPBFaction NewFaction);

	UFUNCTION()
	void OnRep_Faction();

	UFUNCTION()
	void OnRep_Economy();

	UFUNCTION()
	void OnRep_Mission();

	void ApplyFactionAuthority(EAPBFaction NewFaction);

	void ApplyDomainSnapshot(float Threat, int64 InCash, int64 InG1C, int32 InvCount,
		const FString& Mission, int32 StageIdx, int32 StageCount, const FString& SessionId,
		const FString& InProgressionState, const FAPBMissionSnapshotUE& MissionSnap);

	// ── M6 world-server auth RPCs ─────────────────────────────────────────────

	UPROPERTY(ReplicatedUsing=OnRep_WorldAuth, BlueprintReadOnly, Category="APB|Auth")
	bool bWorldAuthOk = false;

	UPROPERTY(ReplicatedUsing=OnRep_WorldAuth, BlueprintReadOnly, Category="APB|Auth")
	FString CharListJson;

	UPROPERTY(ReplicatedUsing=OnRep_WorldAuth, BlueprintReadOnly, Category="APB|Auth")
	FString DistrictListJson;

	UPROPERTY(ReplicatedUsing=OnRep_WorldAuth, BlueprintReadOnly, Category="APB|Auth")
	FString IssuedTicketJson;

	UPROPERTY(ReplicatedUsing=OnRep_WorldAuth, BlueprintReadOnly, Category="APB|Auth")
	FString HandoffProbeJson;

	UFUNCTION()
	void OnRep_WorldAuth();

	// ── M14 social state ──────────────────────────────────────────────────────

	UPROPERTY(ReplicatedUsing=OnRep_Social, BlueprintReadOnly, Category="APB|Social")
	FString ClanId;

	UPROPERTY(ReplicatedUsing=OnRep_Social, BlueprintReadOnly, Category="APB|Social")
	FString ClanRole;

	UPROPERTY(ReplicatedUsing=OnRep_Social, BlueprintReadOnly, Category="APB|Social")
	FString GroupId;

	UPROPERTY(ReplicatedUsing=OnRep_Social, BlueprintReadOnly, Category="APB|Social")
	int32 OnlineFriendCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Social, BlueprintReadOnly, Category="APB|Social")
	bool bHasPendingClanInvite = false;

	UPROPERTY(ReplicatedUsing=OnRep_Social, BlueprintReadOnly, Category="APB|Social")
	bool bHasPendingGroupInvite = false;

	UPROPERTY(ReplicatedUsing=OnRep_Social, BlueprintReadOnly, Category="APB|Social")
	bool bGroupAllReady = false;

	UFUNCTION()
	void OnRep_Social();

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_LoginRequest(const FString& User, const FString& Pass);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_GetCharList();

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_GetDistrictList();

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_IssueTicket(const FString& CharName, const FString& DistrictId);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_ReleaseTravelReservation(const FString& ReservationId);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_PrepareHandoffProbe();

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_GetHandoffProbeState();

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Chat")
	void Server_SubmitChat(const FString& RawLine);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveChat(const FString& Channel, const FString& Sender, const FString& Recipient, const FString& Body);

	// ── M14 social RPCs (T13) ────────────────────────────────────────────────
	// Clients request social mutations via these validated Server RPCs. On the world
	// server, they dispatch to SocialAuthority via the UGI bridge. On a district process,
	// they forward as SocialRequest relay messages to the world. The owning character is
	// derived from the PlayerState's authenticated identity, never from a client arg.

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Social")
	void Server_SocialClan(const FString& Op, const FString& Arg1, const FString& Arg2);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Social")
	void Server_SocialFriend(const FString& Op, const FString& Target);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Social")
	void Server_SocialGroup(const FString& Op, const FString& Arg1, const FString& Arg2);

	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Social")
	void Server_SocialMail(const FString& Op, const FString& Arg1);

	UFUNCTION(Client, Reliable)
	void Client_SocialResult(const FString& Op, const FString& Status, const FString& Body);

};
