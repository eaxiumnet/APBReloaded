#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "APBPlayerState.h"
#include "APBChat.h"
#include "APBRelayProtocol.h"
#include "APBDistrictGameMode.generated.h"

class UAPBServerControl;

/** Verified travel ticket claims stored between PreLogin and PostLogin. */
USTRUCT()
struct FAPBVerifiedTicket
{
	GENERATED_BODY()

	UPROPERTY()
	FString Account;

	UPROPERTY()
	FString Character;

	UPROPERTY()
	FString Faction;

	UPROPERTY()
	FString District;

	UPROPERTY()
	FString Jti;

	UPROPERTY()
	bool bValid = false;
};

/** Authoritative district session (freeroam multiplayer map host). */
UCLASS()
class APBRELOADED_API AAPBDistrictGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AAPBDistrictGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB")
	FString DistrictId = TEXT("Financial");

	UPROPERTY(BlueprintReadOnly, Category="APB")
	FString SessionId;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Character name from a redeemed ticket, or PlayerState name when no ticket. */
	FString ResolveDistrictPlayerName(APlayerController* PC) const;
	void SubmitChat(APlayerController* Sender, const FString& RawLine);	int32 GetRemotePlayerCount() const;

	/** Public accessor for the relay control (M14 social relay forwarding). */
	UAPBServerControl* GetRelayControl() const { return RelayControl; }

protected:
	/** PreLogin → PostLogin handoff keyed by UniqueNetId string. */
	TMap<FString, FAPBVerifiedTicket> PendingTicketsByNetId;

	/** PostLogin identity for peers that joined with a ticket. */
	TMap<FString, FAPBVerifiedTicket> ActiveTicketsByNetId;

	/** Exact issued tokens accepted in PreLogin, retained only to classify a later replay. */
	TMap<FString, FString> RedeemedJtiByToken;
	TSet<FString> AppliedHandoffNonces;
	TSet<FString> AppliedHandoffJtis;
	TSet<FString> ReturnedHandoffJtis;
	TMap<FString, apb::RelayMessage> PendingRelayHandoffsByJti;
	int64 HandoffProbeReturnAtMs = 0;
	apb::ChatService ChatService;
	int64 NextChatRelaySequence = 1;

	UPROPERTY()
	UAPBServerControl* RelayControl = nullptr;

	static FString TicketNetKey(const FUniqueNetIdRepl& UniqueId);
	static FString ExtractTicketFromOptions(const FString& Options);
	static EAPBFaction FactionFromTicketString(const FString& FactionStr);
	void InitTicketSecretFromProvider() const;
	bool TakePendingTicket(const FUniqueNetIdRepl& UniqueId, FAPBVerifiedTicket& Out);
	void ProcessRelayHandoffs();
	bool ApplyRelayHandoff(const apb::RelayMessage& Message);
	void ApplyRelayChat(const apb::RelayMessage& Message);
	void ApplySocialResult(const apb::RelayMessage& Message);
	void ApplySocialChat(const apb::RelayMessage& Message);
	void DeliverChat(const apb::ChatDelivery& Delivery);
	bool IsAdmittedPlayer(APlayerController* PlayerController) const;
	static const TCHAR* ChatChannelName(apb::ChatChannel Channel);
	static const TCHAR* ChatResultName(apb::ChatResult Result);
	void ApplyPendingRelayHandoff(const FString& Jti);
	bool SendRelayReturn(AController* Exiting);
	void RunHandoffProbeReturn();
};
