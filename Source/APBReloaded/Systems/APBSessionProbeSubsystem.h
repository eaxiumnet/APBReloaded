#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "APBSessionProbeSubsystem.generated.h"

/** Probes via -APBProbe=client_loop|playable|mp_observe|frontend_menu|frontend_flow.
 *  frontend_menu: 2011 menu gate — validates every UI stage + district select + travel
 *    dispatch, emits terminal FRONTEND_MENU_OK, then exits (M4 gate).
 *  frontend_flow: menu sequence THEN post-travel freeroam playables, emits terminal
 *    FRONTEND_FLOW_OK/FAIL, then exits (M9/M12 integration gate). */
UCLASS()
class APBRELOADED_API UAPBSessionProbeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

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

	FString Mode;
	bool bTerminal = false;
	FString LogPath;
	int32 PlayablePhase = 0;
	FVector PlayableStart = FVector::ZeroVector;
	FVector DriveStart = FVector::ZeroVector;
	FTimerHandle PlayableTimer;
	FTimerHandle MpTimer;
	FTimerHandle FrontendTravelTimer;
	bool bFrontendTravelPending = false;
	int32 FrontendEquippedSlots = 0;
};
