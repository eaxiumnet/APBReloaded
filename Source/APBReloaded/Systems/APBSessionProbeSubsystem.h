#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "APBSessionProbeSubsystem.generated.h"

/** Probes via -APBProbe=client_loop|playable|mp_observe|frontend_flow. */
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

	FString Mode;
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
