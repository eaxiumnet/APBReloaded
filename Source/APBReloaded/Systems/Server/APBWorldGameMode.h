#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "APBRelayProtocol.h"
#include "APBWorldService.h"
#include "APBWorldGameMode.generated.h"

class UAPBServerControl;

/** One entry in the world-authority admitted-player roster.
 *  bAdmitted=false: ticket issued, relay join not yet confirmed (pending).
 *  bAdmitted=true:  relay PlayerJoined received — player is live in the district. */
struct FAPBAdmittedPlayer
{
	FString Account;
	FString Character;
	FString Faction;
	FString Jti;
	int32   DistrictNumericId = 0;
	bool    bAdmitted         = false;
};

struct FAPBPlayerService {
	TUniquePtr<apb::WorldService> Service;
};

UCLASS()
class APBRELOADED_API AAPBWorldGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AAPBWorldGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UPROPERTY()
	UAPBServerControl* ServerControl = nullptr;

	bool LoginPlayer(APlayerController* PC,
	                 const FString& User, const FString& Pass,
	                 FString& OutError);
	FString GetCharListJson(APlayerController* PC) const;
	FString GetDistrictListJson(APlayerController* PC) const;
	FString IssueTicketJson(APlayerController* PC,
	                        const FString& CharName, const FString& DistrictId);
	FString PrepareHandoffProbeJson(APlayerController* PC);
	FString GetHandoffProbeJson(APlayerController* PC) const;
	void ReleaseTravelReservation(APlayerController* PC, const FString& ReservationId);

	apb::WorldService& Social() { return SocialAuthority; }
	const apb::WorldService& Social() const { return SocialAuthority; }

	const FAPBAdmittedPlayer* FindAdmittedPlayer(const FString& Character) const;
	const TMap<FString, FAPBAdmittedPlayer>& GetAdmittedRoster() const { return AdmittedRoster; }

	/** The per-connection authoritative service that currently owns Character, or
	 *  nullptr when no logged-in connection holds it. Mail claims must credit cash
	 *  here: SocialAuthority owns the mailbox but never loads a character, so
	 *  crediting the social service would drop the payout on the floor. */
	apb::WorldService* ServiceForCharacter(const FString& Character) const;

private:
	TMap<FString, TUniquePtr<FAPBPlayerService>> PlayerServices;
	apb::WorldService SocialAuthority;
	FString PersistDir;
	FString DataDir;
	struct FTravelReservation
	{
		FString OwnerKey;
		FString PlayerName;
		FString Account;
		FString PersistenceAccount;
		FString Character;
		FString Faction;
		FString Jti;
		FString HandoffNonce;
		int32 DistrictNumericId = 0;
		int64 ExpiresMs = 0;
		bool bDistrictAdmitted = false;
		bool bDomainReservationReleased = false;
	};
	TMap<FString, FTravelReservation> TravelReservations;
	TMap<FString, FAPBAdmittedPlayer> AdmittedRoster;
	TSet<FString> ConsumedReturnNonces;
	void CancelTravelDomainReservation(FTravelReservation& Reservation);
	void ReleaseTravelReservationById(const FString& ReservationId);
	void ProcessRelayReturns();
	bool ApplyRelayReturn(const apb::RelayMessage& Message);
	void MarkRelayPlayerJoined(const apb::RelayMessage& Message);
	void MarkRelayPlayerLeft(const apb::RelayMessage& Message);
	void ForwardRelayChat(const apb::RelayMessage& Message);
	void HandleSocialRequest(const apb::RelayMessage& Message);
	void PushSocialStateToPlayerStates();
	FAPBPlayerService* ServiceFor(APlayerController* PC) const;
	static FString PCKey(APlayerController* PC);
};
