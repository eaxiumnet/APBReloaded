#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "APBPlayerState.generated.h"

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

	UPROPERTY(ReplicatedUsing=OnRep_Economy, BlueprintReadOnly, Category="APB")
	int32 InventoryItemCount = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="APB")
	FString DistrictSessionId;

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

	/** Authority: push domain snapshot for multipath observers. */
	void ApplyDomainSnapshot(float Threat, int64 InCash, int64 InG1C, int32 InvCount,
		const FString& Mission, int32 StageIdx, int32 StageCount, const FString& SessionId);

	// ── M6 world-server auth RPCs ─────────────────────────────────────────────

	/** Replicated results (world-server → client). */
	UPROPERTY(ReplicatedUsing=OnRep_WorldAuth, BlueprintReadOnly, Category="APB|Auth")
	bool bWorldAuthOk = false;

	UPROPERTY(ReplicatedUsing=OnRep_WorldAuth, BlueprintReadOnly, Category="APB|Auth")
	FString CharListJson;

	UPROPERTY(ReplicatedUsing=OnRep_WorldAuth, BlueprintReadOnly, Category="APB|Auth")
	FString DistrictListJson;

	UPROPERTY(ReplicatedUsing=OnRep_WorldAuth, BlueprintReadOnly, Category="APB|Auth")
	FString IssuedTicketJson;

	UFUNCTION()
	void OnRep_WorldAuth();

	/** Client → server: request login. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_LoginRequest(const FString& User, const FString& Pass);

	/** Client → server: fetch char list (requires prior successful login). */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_GetCharList();

	/** Client → server: fetch district list (requires prior successful login). */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_GetDistrictList();

	/** Client → server: issue a travel ticket for CharName + DistrictId. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category="APB|Auth")
	void Server_IssueTicket(const FString& CharName, const FString& DistrictId);
};
