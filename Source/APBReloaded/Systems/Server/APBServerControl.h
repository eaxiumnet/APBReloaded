#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "APBDistrictPopulationSnapshot.h"
#include "APBRelayProtocol.h"
#include "APBServerControl.generated.h"

class AAPBWorldGameMode;
class APlayerController;
class FAPBRelayListener;
class FAPBRelayDistrictClient;

/** Role selector + auth-gated serving surface for the world-server process.
 *  Owned by AAPBWorldGameMode; created in BeginPlay on the authority. */
UCLASS()
class APBRELOADED_API UAPBServerControl : public UObject
{
	GENERATED_BODY()
public:
	void Init(AAPBWorldGameMode* InMode);
	void InitDistrict(const FString& ResolvedDistrictId, const FString& DistrictEpoch);
	void Shutdown();
	void SetDistrictPopulation(int32 PlayerCount);

	virtual void BeginDestroy() override;

	bool IsWorldServerRole() const { return bWorldServerRole; }

	bool LoginRequest(APlayerController* PC,
	                  const FString& User, const FString& Pass,
	                  FString& OutError);

	FString GetCharListJson(APlayerController* PC) const;
	FString GetDistrictListJson(APlayerController* PC) const;

	/** Only callable when PC is already logged in. */
	FString IssueTicketJson(APlayerController* PC,
	                        const FString& CharName, const FString& DistrictId);
	bool ResolveLiveDistrict(const FString& DistrictId, int32 MaxPlayers, FString& OutHost, int32& OutPort,
		int32& OutNumericId, FString& OutEpoch, FString& OutError) const;
	std::vector<FAPBDistrictPopulationSnapshot> GetLiveDistrictPopulationSnapshot() const;
	bool ReserveLiveDistrict(uint64 PlayerSessionId, const FString& DistrictId,
		int32 MaxPlayers, FString& OutHost, int32& OutPort, int32& OutNumericId, FString& OutEpoch, FString& OutReservationId,
		FString& OutError);
	void ReleaseLiveDistrictReservation(const FString& ReservationId);
	bool SendRelayToDistrict(int32 NumericId, const apb::RelayMessage& Message);
	bool SendRelayToWorld(const apb::RelayMessage& Message);
	bool DequeueDistrictRelayMessage(apb::RelayMessage& OutMessage);
	bool DequeueWorldRelayMessage(apb::RelayMessage& OutMessage);
	int32 GetDistrictNumericId() const;
	TArray<FString> ReleaseLiveDistrictReservationsForPlayer(uint64 PlayerSessionId);

private:
	UPROPERTY()
	AAPBWorldGameMode* Mode = nullptr;

	bool bWorldServerRole = false;
	int32 DistrictNumericId = 0;
	FAPBRelayListener* RelayListener = nullptr;
	FAPBRelayDistrictClient* RelayDistrictClient = nullptr;
	TMap<uint64, FString> ReservationByPlayer;
	TMap<FString, int32> ReservationNodeById;
	TMap<int32, int32> PendingReservationsByNode;
	int64 NextReservationId = 1;
};
