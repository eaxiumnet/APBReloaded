#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "APBSessionProbeSubsystem.generated.h"

/** Probes via -APBProbe=client_loop|playable|mp_observe|frontend_menu|frontend_flow|asset_allowlist|frontend_routing|social_probe.
 *  frontend_menu: 2011 menu gate — validates every UI stage + district select + travel
 *    dispatch, emits terminal FRONTEND_MENU_OK, then exits (M4 gate).
 *  frontend_flow: menu sequence THEN post-travel freeroam playables, emits terminal
 *    FRONTEND_FLOW_OK/FAIL, then exits (M9/M12 integration gate).
 *  social_probe: M14 social gate — emits a terminal role-specific verdict, then exits. */
UCLASS()
class APBRELOADED_API UAPBSessionProbeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	void ScheduleDevChat(int32 DelayMs, const FString& RawLine);
	void ScheduleChatDistrictTravel(int32 DelayMs, const FString& DistrictId);

private:
	void StartProbe(const FString& Mode);
	void AppendLog(const FString& Line);
	void TryArmProbeWhenWorldReady();
	void ArmProbeTimers(UWorld* World);
	void RunClientLoopProbe();
	void PlayableStep();
	void MpPoll();
	/** Phase 1: Frontend map — real UI stages, body, travel OpenLevel. */
	void RunFrontendFlowProbe();
	/** Phase 2: after travel to freeroam — props/walk/shoot/interact/vehicle. */
	void RunFrontendFlowPostTravel();
	FString FrontendVerdictPrefix() const;
	void FrontendFail(const FString& Reason);
	void EndFrontendProbe();

	/** M6: world-server authority probe — counts clients reaching auth/charlist/districtlist/ticket. */
	void RunWorldServerProbe();
	/** M6: world-server client probe — drives login→charlist→districtlist→ticket via Server RPCs. */
	void RunWorldServerClientProbe();
	void RunWorldTravelClientProbe();
	void RunLoadWorkloadStep();
	void RunWorldHandoffClientProbe();
	void RunWorldChatClientProbe();
	void RunSocialProbe();
	void RunVerifiedAssetAllowlistProbe();
	void RunFrontendRoutingProbe();
	void RunReplicationProbe();
	/** M11 network dispatch gate — a real network client sets its faction, enqueues via
	 *  Server_RequestMissionDispatch, and observes the mission the authority dispatches it into. */
	void RunMissionClientProbe();
	void QueueChatCommandsFromCommandLine();
	void ArmQueuedChatCommands();
	bool ParseHandoffProbeSnapshot(const FString& Json, int64& OutCash, int64& OutG1C, float& OutThreat,
		int32& OutInventorySlots, int32& OutInventoryQty, FString& OutFaction, FString& OutMission, FString& OutSession, FString& OutProgression) const;

	FString Mode;
	bool bTerminal = false;
	FString LogPath;
	int32 PlayablePhase = 0;
	FVector PlayableStart = FVector::ZeroVector;
	FVector DriveStart = FVector::ZeroVector;
	FTimerHandle PlayableTimer;
	FTimerHandle MpTimer;
	FTimerHandle FrontendTravelTimer;
	FTimerHandle WorldServerTimer;
	FTimerHandle LoadWorkloadTimer;
	bool bFrontendTravelPending = false;
	int32 FrontendEquippedSlots = 0;

	int32 WS_LoginCount = 0;
	int32 WS_CharListCount = 0;
	int32 WS_DistrictListCount = 0;
	int32 WS_TicketCount = 0;
	FString WSClientId;
	bool bWSClientDone = false;
	// Single-shot login guard: the world authority runs async (PBKDF2) auth that bumps a
	// per-connection generation nonce on every LoginPlayer call and rejects stale callbacks.
	// Re-sending login every tick would invalidate every in-flight auth, so send once and
	// only re-send after a generous interval as a lost-RPC safety net.
	bool bWSLoginSent = false;
	double WSLoginSentAt = 0.0;
	bool bLoadWorkloadEnabled = false;
	bool bLoadTravelDispatched = false;
	bool bLoadCompletionEmitted = false;
	bool bLoadMovementRequested = false;
	bool bLoadCombatRequested = false;
	bool bLoadVehicleRequested = false;
	bool bLoadMovementExecuted = false;
	bool bLoadCombatExecuted = false;
	bool bLoadVehicleEntered = false;
	bool bLoadVehicleThrottled = false;
	bool bLoadCombatSent = false;
	bool bLoadPerfEmitted = false;
	int32 LoadCombatStartShots = 0;
	int32 LoadWorkloadSteps = 0;
	double LoadWorkloadStartedAt = 0.0;
	double LoadLastPerfAt = 0.0;
	FString LoadWorkload;
	FString LoadMap;
	FString LoadPrimary;
	FString LoadIdentity;
	FString LoadAccount;
	bool bTravelLoginSent = false;
	bool bTravelTicketRequested = false;
	bool bTravelDispatchPending = false;
	FString TravelDistrictId;
	int64 TravelDispatchAtMs = 0;
	int32 HandoffPhase = 0;
	bool bHandoffLoginSent = false;
	bool bHandoffPrepareSent = false;
	bool bHandoffTicketSent = false;
	bool bHandoffDistrictLogged = false;
	bool bHandoffReachedDistrict = false;
	bool bHandoffStateRequested = false;
	int64 HandoffStateRequestAtMs = 0;
	int64 HandoffDeadlineMs = 0;
	int64 HandoffCash = 0;
	int64 HandoffG1C = 0;
	float HandoffThreat = 0.f;
	int32 HandoffInventorySlots = 0;
	int32 HandoffInventoryQty = 0;
	FString HandoffFaction;
	FString HandoffMission;
	FString HandoffSession;
	FString HandoffProgression;
	bool bChatLoginSent = false;
	bool bChatTicketRequested = false;
	bool bChatTravelDispatched = false;
	bool bChatArrivalLogged = false;
	bool bChatCommandsArmed = false;
	int64 ChatWorldReconnectReadyAtMs = 0;
	FString ChatCharacter;
	FString ChatDistrictId;
	TArray<FTimerHandle> ChatCommandTimers;
	FString SocialRole;
	bool bSocialLoginSent = false;
	bool bSocialWorldLoginSent = false;
	bool bSocialTicketRequested = false;
	bool bSocialTravelDispatched = false;
	bool bSocialArrivedInDistrict = false;
	bool bSocialClanOk = false;
	bool bSocialClanInviteOk = false;
	bool bSocialFriendsOk = false;
	bool bSocialGroupsOk = false;
	bool bSocialGroupInviteOk = false;
	bool bSocialMailOk = false;
	// Alice's single-flight social op awaiting its Client_SocialResult echo.
	FString SocialOpInFlight;
	int64 SocialProbeStartMs = 0;
	bool bSocialDone = false;
	FString ReplicationRole;
	int64 ReplicationProbeStartMs = 0;
	bool bReplicationDone = false;
	// M11 network dispatch client (-MissionRole=criminal|enforcer).
	FString MissionRole;
	int64 MissionClientStartMs = 0;
	bool bMissionFactionRequested = false;
	bool bMissionEnqueued = false;
	bool bMissionSeenQueued = false;
	bool bMissionClientDone = false;
	struct FChatGateCommand
	{
		int32 DelayMs = 0;
		FString RawLine;
		FString TravelDistrictId;
	};
	TArray<FChatGateCommand> QueuedChatCommands;
};
