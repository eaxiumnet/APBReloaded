#include "APBDistrictGameMode.h"
#include "APBPlayerController.h"
#include "APBPlayerState.h"
#include "APBGameInstanceSubsystem.h"
#include "APBDistrictStreamer.h"
#include "APBServerControl.h"
#include "APBSecretProvider.h"
#include "Domain/APBCrypto.h"
#include "Engine/NetConnection.h"
#include "APBHandoff.h"
#include "APBChat.h"
#include "APBRelayProtocol.h"
#include "APBPorts.h"
#include "APBTicket.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

#include <string>
#include <vector>

namespace
{
	bool RequiresDistrictTicket()
	{
		bool bRequireTicket = true;
		GConfig->GetBool(TEXT("APBServer"), TEXT("RequireTicket"), bRequireTicket, GGameIni);
		return bRequireTicket || FParse::Param(FCommandLine::Get(), TEXT("RequireTicket"));
	}
}

AAPBDistrictGameMode::AAPBDistrictGameMode()
{
	PlayerControllerClass = AAPBPlayerController::StaticClass();
	PlayerStateClass = AAPBPlayerState::StaticClass();
	bUseSeamlessTravel = true;
	PrimaryActorTick.bCanEverTick = true;
}

AAPBDistrictGameMode::~AAPBDistrictGameMode() = default;

FString AAPBDistrictGameMode::TicketNetKey(const FUniqueNetIdRepl& UniqueId)
{
	return UniqueId.IsValid() ? UniqueId.ToString() : TEXT("");
}

FString AAPBDistrictGameMode::ExtractTicketFromOptions(const FString& Options)
{
	FString Raw = UGameplayStatics::ParseOption(Options, TEXT("APBTicket"));
	if (Raw.IsEmpty())
	{
		Raw = UGameplayStatics::ParseOption(Options, TEXT("ticket"));
	}
	if (Raw.IsEmpty())
	{
		return Raw;
	}

	// World-server IssueTicketJson is {"ticket":"payload.sig"}; accept raw token too.
	if (Raw.StartsWith(TEXT("{")))
	{
		const FString Needle = TEXT("\"ticket\":\"");
		int32 Pos = Raw.Find(Needle, ESearchCase::IgnoreCase);
		if (Pos != INDEX_NONE)
		{
			Pos += Needle.Len();
			const int32 End = Raw.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos);
			if (End != INDEX_NONE)
			{
				return Raw.Mid(Pos, End - Pos);
			}
		}
	}
	return Raw;
}

EAPBFaction AAPBDistrictGameMode::FactionFromTicketString(const FString& FactionStr)
{
	return FactionStr.Equals(TEXT("Enforcer"), ESearchCase::IgnoreCase)
		? EAPBFaction::Enforcer
		: EAPBFaction::Criminal;
}

bool AAPBDistrictGameMode::TakePendingTicket(const FUniqueNetIdRepl& UniqueId, FAPBVerifiedTicket& Out)
{
	const FString Key = TicketNetKey(UniqueId);
	if (FAPBVerifiedTicket* Found = PendingTicketsByNetId.Find(Key))
	{
		Out = *Found;
		PendingTicketsByNetId.Remove(Key);
		return Out.bValid;
	}
	return false;
}

FString AAPBDistrictGameMode::ResolveDistrictPlayerName(APlayerController* PC) const
{
	if (!PC || !PC->PlayerState)
	{
		return TEXT("Peer");
	}
	const FString Key = TicketNetKey(PC->PlayerState->GetUniqueId());
	if (const FAPBVerifiedTicket* Found = ActiveTicketsByNetId.Find(Key))
	{
		if (Found->bValid && !Found->Character.IsEmpty())
		{
			return Found->Character;
		}
	}
	return PC->PlayerState->GetPlayerName();
}

void AAPBDistrictGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	FString Err;
	FAPBSecretProvider::Initialize(Err);
	TicketSvc = MakeUnique<apb::TicketService>(TCHAR_TO_UTF8(*FAPBSecretProvider::TicketSecret()));
	
	SessionId = FString::Printf(TEXT("DS-%s-1"), *DistrictId);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			if (APB->GetPhase() != TEXT("District"))
			{
				APB->RegisterAccount(TEXT("host"), TEXT("host"));
				APB->Login(TEXT("host"), TEXT("host"));
				APB->EnterWorld(TEXT("W1"));
				APB->CreateCharacter(TEXT("Host"), false);
				APB->JoinDistrict(DistrictId);
			}
			SessionId = APB->GetSessionId();
			if (UWorld* World = GetWorld())
			{
				FActorSpawnParameters Sp;
				Sp.Name = TEXT("APBDistrictStreamer");
				World->SpawnActor<AAPBDistrictStreamer>(AAPBDistrictStreamer::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Sp);
			}
		}
	}
	DistrictEpoch = FGuid::NewGuid().ToString();
	RelayControl = NewObject<UAPBServerControl>(this);
	RelayControl->InitDistrict(DistrictId, DistrictEpoch);
	RelayControl->SetDistrictPopulation(GetRemotePlayerCount());
	UE_LOG(LogTemp, Log, TEXT("APB District session %s district=%s (listen-server freeroam)"), *SessionId, *DistrictId);
}

void AAPBDistrictGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RelayControl)
	{
		RelayControl->Shutdown();
	}
	Super::EndPlay(EndPlayReason);
}

void AAPBDistrictGameMode::BeginDestroy()
{
	if (RelayControl)
	{
		RelayControl->Shutdown();
	}
	Super::BeginDestroy();
}

void AAPBDistrictGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	if (!TicketSvc)
	{
		ErrorMessage = TEXT("Server configuration error");
		return;
	}

	const FString TokenRaw = ExtractTicketFromOptions(Options);
	if (TokenRaw.IsEmpty())
	{
		// Standalone / listen-server host / probe: no ticket on the URL (M4/M6 unchanged).
		if (RequiresDistrictTicket())
		{
			ErrorMessage = TEXT("APB ticket required");
			UE_LOG(LogTemp, Warning, TEXT("DISTRICT_TICKET_FAIL reason=missing"));
		}
		return;
	}

	apb::TicketClaims Claims;
	const std::string Token = TCHAR_TO_UTF8(*TokenRaw);
	const apb::TicketVerdict Verdict = TicketSvc->VerifyAndConsumeChecked(Token, Claims);
	if (Verdict != apb::TicketVerdict::Ok)
	{
		// A raw token already successfully redeemed on this district is a replay even after
		// it expires: VerifyAndConsumeChecked reports Invalid once the clock passes expiry,
		// before the replay-window is reached. RedeemedJtiByToken (never cleared for the
		// district's lifetime) is the expiry-independent replay signal; the Replay verdict
		// covers the not-yet-expired case. A replayed ticket is a distinct security event
		// from a forged/expired one.
		if (Verdict == apb::TicketVerdict::Replay || RedeemedJtiByToken.Find(TokenRaw) != nullptr)
		{
			ErrorMessage = TEXT("APB ticket already used");
			UE_LOG(LogTemp, Warning, TEXT("APB PreLogin reject: replay ticket from %s"), *Address);
			UE_LOG(LogTemp, Warning, TEXT("DISTRICT_TICKET_FAIL reason=replay"));
			return;
		}
		ErrorMessage = TEXT("Invalid or expired APB ticket");
		UE_LOG(LogTemp, Warning, TEXT("APB PreLogin reject: invalid ticket from %s"), *Address);
		UE_LOG(LogTemp, Warning, TEXT("DISTRICT_TICKET_FAIL reason=invalid"));
		return;
	}

	if (!DistrictId.IsEmpty() && !Claims.district.empty())
	{
		const FString ClaimDistrict(UTF8_TO_TCHAR(Claims.district.c_str()));
		if (!ClaimDistrict.Equals(DistrictId, ESearchCase::IgnoreCase))
		{
			ErrorMessage = TEXT("APB ticket district mismatch");
			UE_LOG(LogTemp, Warning, TEXT("APB PreLogin reject: ticket district=%s host=%s"),
				*ClaimDistrict, *DistrictId);
			UE_LOG(LogTemp, Warning, TEXT("DISTRICT_TICKET_FAIL reason=district_mismatch"));
			return;
		}
	}

	RedeemedJtiByToken.Add(TokenRaw, UTF8_TO_TCHAR(Claims.jti.c_str()));

	if (!Claims.target_district_epoch.empty() && !DistrictEpoch.IsEmpty())
	{
		const FString ClaimEpoch = UTF8_TO_TCHAR(Claims.target_district_epoch.c_str());
		if (ClaimEpoch != DistrictEpoch)
		{
			ErrorMessage = TEXT("APB ticket epoch mismatch");
			UE_LOG(LogTemp, Warning, TEXT("DISTRICT_EPOCH_RESTART_REFUSED expected=%s actual=%s"), *DistrictEpoch, *ClaimEpoch);
			return;
		}
	}

	FAPBVerifiedTicket Verified;
	Verified.Account = UTF8_TO_TCHAR(Claims.account.c_str());
	Verified.Character = UTF8_TO_TCHAR(Claims.character.c_str());
	Verified.Faction = UTF8_TO_TCHAR(Claims.faction.c_str());
	Verified.District = UTF8_TO_TCHAR(Claims.district.c_str());
	Verified.Jti = UTF8_TO_TCHAR(Claims.jti.c_str());
	Verified.bValid = true;

	const FString Key = TicketNetKey(UniqueId);
	if (Key.IsEmpty())
	{
		ErrorMessage = TEXT("APB ticket missing player id");
		UE_LOG(LogTemp, Warning, TEXT("DISTRICT_TICKET_FAIL reason=no_netid"));
		return;
	}
	PendingTicketsByNetId.Add(Key, Verified);
	UE_LOG(LogTemp, Log, TEXT("APB PreLogin ticket ok account=%s char=%s district=%s jti=%s"),
		*Verified.Account, *Verified.Character, *Verified.District, *Verified.Jti);
	UE_LOG(LogTemp, Log, TEXT("DISTRICT_TICKET_OK account=%s char=%s district=%s jti=%s"),
		*Verified.Account, *Verified.Character, *Verified.District, *Verified.Jti);
}

int32 AAPBDistrictGameMode::GetRemotePlayerCount() const
{
	// Districts run as listen servers (installed engine cannot build the dedicated Server
	// target). GetNumPlayers() counts the local listen-server host controller, inflating
	// reported population by +1 per instance. Report only remote (client) controllers so
	// AggregateByDistrict reflects real players.
	int32 Remote = 0;
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && !PC->IsLocalController())
			{
				++Remote;
			}
		}
	}
	return Remote;
}

void AAPBDistrictGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// M16 zero-trust: mirror the world authority's AES-GCM enable. Chat and other
	// client->district RPCs are encrypted by the client's ServerConnection key; without
	// the district-side key every such packet is dropped as "received encrypted packet
	// before key was set, ignoring." and delivery never reaches ChatService.
	if (NewPlayer && NewPlayer->NetConnection && !FParse::Param(FCommandLine::Get(), TEXT("DisableEncryption")))
	{
		const FString& Secret = FAPBSecretProvider::TicketSecret();
		if (!Secret.IsEmpty())
		{
			std::vector<uint8_t> Bytes = apb::hex_decode(TCHAR_TO_UTF8(*Secret));
			if (Bytes.size() == 32)
			{
				FEncryptionData Data;
				Data.Key.SetNum(32);
				FMemory::Memcpy(Data.Key.GetData(), Bytes.data(), 32);
				NewPlayer->NetConnection->EnableEncryption(Data);
			}
		}
	}

	FAPBVerifiedTicket Ticket;
	const bool bHasTicket = NewPlayer && NewPlayer->PlayerState
		&& TakePendingTicket(NewPlayer->PlayerState->GetUniqueId(), Ticket);

	FString PlayerName = NewPlayer && NewPlayer->PlayerState
		? NewPlayer->PlayerState->GetPlayerName()
		: TEXT("Peer");
	EAPBFaction Faction = EAPBFaction::Criminal;

	if (bHasTicket && Ticket.bValid)
	{
		const FString Key = TicketNetKey(NewPlayer->PlayerState->GetUniqueId());
		ActiveTicketsByNetId.Add(Key, Ticket);
		if (!Ticket.Character.IsEmpty())
		{
			PlayerName = Ticket.Character;
		}
		Faction = FactionFromTicketString(Ticket.Faction);
		ApplyPendingRelayHandoff(Ticket.Jti);
	}
	if (bHasTicket && Ticket.bValid && !PlayerName.IsEmpty())
	{
		ChatService.AddPlayer(TCHAR_TO_UTF8(*PlayerName));
		ChatService.SetFaction(TCHAR_TO_UTF8(*PlayerName), TCHAR_TO_UTF8(*Ticket.Faction));
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			APB->JoinDistrictAsPeer(SessionId, PlayerName);
			if (AAPBPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AAPBPlayerState>() : nullptr)
			{
				// D16b: meta state syncs only via the Domain bridge, never direct PlayerState writes.
				APB->SyncPlayerStateFromDomain(PS);
				if (bHasTicket && Ticket.bValid)
				{
					PS->ApplyFactionAuthority(Faction, true);
				}
			}
		}
	}
	if (RelayControl)
	{
		RelayControl->SetDistrictPopulation(GetRemotePlayerCount());
		if (bHasTicket && Ticket.bValid)
		{
			const FString& Secret = FAPBSecretProvider::RelaySecret();
			RelayControl->SendRelayToWorld(apb::RelayCodec::MakePlayerJoined(TCHAR_TO_UTF8(*Ticket.Account),
				TCHAR_TO_UTF8(*Ticket.Character), RelayControl->GetDistrictNumericId(), std::string("join-") + TCHAR_TO_UTF8(*Ticket.Jti),
				FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond(), TCHAR_TO_UTF8(*Secret)));
		}
	}
	if (bHasTicket && Ticket.bValid)
	{
		UE_LOG(LogTemp, Log, TEXT("DISTRICT_TICKET_ADMITTED account=%s char=%s faction=%s"),
			*Ticket.Account, *Ticket.Character, *Ticket.Faction);
	}
}

void AAPBDistrictGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	ProcessRelayHandoffs();
	RunHandoffProbeReturn();
}

void AAPBDistrictGameMode::ProcessRelayHandoffs()
{
	if (!RelayControl) return;
	apb::RelayMessage Message;
	while (RelayControl->DequeueDistrictRelayMessage(Message))
	{
		if (Message.verb == apb::RelayVerb::ChatRelay) ApplyRelayChat(Message);
		else if (Message.verb == apb::RelayVerb::SocialResult) ApplySocialResult(Message);
		else if (Message.verb == apb::RelayVerb::SocialChat) ApplySocialChat(Message);
		else ApplyRelayHandoff(Message);
	}
}

const TCHAR* AAPBDistrictGameMode::ChatChannelName(const apb::ChatChannel Channel)
{
	switch (Channel)
	{
	case apb::ChatChannel::Local: return TEXT("Local");
	case apb::ChatChannel::District: return TEXT("District");
	case apb::ChatChannel::Group: return TEXT("Group");
	case apb::ChatChannel::Whisper: return TEXT("Whisper");
	case apb::ChatChannel::Faction: return TEXT("Faction");
	case apb::ChatChannel::Clan: return TEXT("Clan");
	case apb::ChatChannel::Trade: return TEXT("Trade");
	case apb::ChatChannel::System: return TEXT("System");
	default: return TEXT("Unknown");
	}
}

const TCHAR* AAPBDistrictGameMode::ChatResultName(const apb::ChatResult Result)
{
	switch (Result)
	{
	case apb::ChatResult::Empty: return TEXT("Empty");
	case apb::ChatResult::Muted: return TEXT("Muted");
	case apb::ChatResult::RecipientOffline: return TEXT("RecipientOffline");
	case apb::ChatResult::RecipientBusy: return TEXT("RecipientBusy");
	case apb::ChatResult::BadChannel: return TEXT("BadChannel");
	case apb::ChatResult::Delivered: return TEXT("Delivered");
	default: return TEXT("Unknown");
	}
}

bool AAPBDistrictGameMode::IsAdmittedPlayer(APlayerController* PlayerController) const
{
	if (!PlayerController || !PlayerController->PlayerState) return false;
	const FAPBVerifiedTicket* Ticket = ActiveTicketsByNetId.Find(TicketNetKey(PlayerController->PlayerState->GetUniqueId()));
	return Ticket && Ticket->bValid && !Ticket->Character.IsEmpty();
}

void AAPBDistrictGameMode::DeliverChat(const apb::ChatDelivery& Delivery)
{
	const FString Recipient(UTF8_TO_TCHAR(Delivery.recipient.c_str()));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!IsAdmittedPlayer(PlayerController) || !ResolveDistrictPlayerName(PlayerController).Equals(Recipient, ESearchCase::CaseSensitive)) continue;
		if (AAPBPlayerState* PlayerState = PlayerController->GetPlayerState<AAPBPlayerState>())
		{
			const FString Channel(ChatChannelName(Delivery.message.channel));
			const FString Sender(UTF8_TO_TCHAR(Delivery.message.sender.c_str()));
			PlayerState->Client_ReceiveChat(Channel, Sender, Recipient, UTF8_TO_TCHAR(Delivery.message.body.c_str()));
			UE_LOG(LogTemp, Log, TEXT("CHAT_DELIVERED channel=%s from=%s to=%s"), *Channel, *Sender, *Recipient);
		}
		return;
	}
}

void AAPBDistrictGameMode::SubmitChat(APlayerController* Sender, const FString& RawLine)
{
	if (!HasAuthority() || !IsAdmittedPlayer(Sender))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=Unauthenticated"));
		return;
	}
	const FString SenderName = ResolveDistrictPlayerName(Sender);
	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	const apb::ParsedChat Parsed = apb::ChatService::ParseCommand(TCHAR_TO_UTF8(*RawLine), apb::ChatChannel::District);
	const apb::SubmitResult Result = ChatService.SubmitRaw(TCHAR_TO_UTF8(*SenderName), TCHAR_TO_UTF8(*RawLine), apb::ChatChannel::District, NowMs);
	if (Result.status == apb::ChatResult::RecipientOffline && Parsed.ok && Parsed.channel == apb::ChatChannel::Whisper && RelayControl)
	{
		const FString& Secret = FAPBSecretProvider::RelaySecret();
		if (!Secret.IsEmpty())
		{
			const std::string RequestId = "chat-" + std::to_string(RelayControl->GetDistrictNumericId()) + "-" + std::to_string(NextChatRelaySequence++);
			const apb::RelayMessage Message = apb::RelayCodec::MakeChatRelay(TCHAR_TO_UTF8(*SenderName), Parsed.target,
				Parsed.body, RelayControl->GetDistrictNumericId(), RequestId, NowMs, TCHAR_TO_UTF8(*Secret));
			if (RelayControl->SendRelayToWorld(Message))
			{
				UE_LOG(LogTemp, Log, TEXT("CHAT_RELAY_FORWARD to=%s"), UTF8_TO_TCHAR(Parsed.target.c_str()));
				return;
			}
		}
	}
	if (Result.status != apb::ChatResult::Delivered)
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=%s"), ChatResultName(Result.status));
		return;
	}
	// Local is roster-wide until the district layer owns a server-authoritative spatial query.
	for (const apb::ChatDelivery& Delivery : Result.deliveries) DeliverChat(Delivery);
}

void AAPBDistrictGameMode::ApplyRelayChat(const apb::RelayMessage& Message)
{
	if (Message.from.empty() || Message.to.empty() || Message.body.empty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=RecipientOffline-after-relay-miss"));
		return;
	}
	const apb::SubmitResult Result = ChatService.Submit(Message.from, apb::ChatChannel::Whisper, Message.to,
		Message.body, Message.sent_ms);
	if (Result.status != apb::ChatResult::Delivered)
	{
		const TCHAR* Reason = Result.status == apb::ChatResult::RecipientOffline
			? TEXT("RecipientOffline-after-relay-miss")
			: ChatResultName(Result.status);
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=%s"), Reason);
		return;
	}
	for (const apb::ChatDelivery& Delivery : Result.deliveries) DeliverChat(Delivery);
}

void AAPBDistrictGameMode::ApplyPendingRelayHandoff(const FString& Jti)
{
	if (apb::RelayMessage* Pending = PendingRelayHandoffsByJti.Find(Jti))
	{
		const apb::RelayMessage Message = *Pending;
		PendingRelayHandoffsByJti.Remove(Jti);
		ApplyRelayHandoff(Message);
	}
}

bool AAPBDistrictGameMode::ApplyRelayHandoff(const apb::RelayMessage& Message)
{
	if (Message.verb != apb::RelayVerb::Handoff || Message.nonce.empty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_HANDOFF_REJECT reason=bad_sig"));
		return false;
	}
	if (AppliedHandoffNonces.Contains(UTF8_TO_TCHAR(Message.nonce.c_str())))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_HANDOFF_REJECT reason=nonce_replay"));
		return false;
	}
	const FString& Secret = FAPBSecretProvider::HandoffSecret();
	if (Secret.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_HANDOFF_REJECT reason=bad_sig"));
		return false;
	}
	apb::CharacterHandoff Handoff;
	if (!apb::VerifyHandoff(Message.body, TCHAR_TO_UTF8(*Secret), Handoff))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_HANDOFF_REJECT reason=bad_sig"));
		return false;
	}
	if (Handoff.nonce != Message.nonce)
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_HANDOFF_REJECT reason=stale"));
		return false;
	}
	if (Handoff.account != Message.account || Handoff.character != Message.character || Handoff.jti != Message.jti)
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_HANDOFF_REJECT reason=account_mismatch"));
		return false;
	}
	if (Handoff.faction != Message.faction)
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_HANDOFF_REJECT reason=faction_mismatch"));
		return false;
	}
	FAPBVerifiedTicket* Ticket = nullptr;
	for (TPair<FString, FAPBVerifiedTicket>& Entry : ActiveTicketsByNetId)
	{
		if (Entry.Value.bValid && Entry.Value.Jti == UTF8_TO_TCHAR(Handoff.jti.c_str()))
		{
			Ticket = &Entry.Value;
			break;
		}
	}
	if (!Ticket || Ticket->Account != UTF8_TO_TCHAR(Handoff.account.c_str()) || Ticket->Character != UTF8_TO_TCHAR(Handoff.character.c_str()))
	{
		if (PendingRelayHandoffsByJti.Num() < static_cast<int32>(apb::kRelayMaxQueueDepth))
		{
			PendingRelayHandoffsByJti.Add(UTF8_TO_TCHAR(Handoff.jti.c_str()), Message);
			return false;
		}
		UE_LOG(LogTemp, Warning, TEXT("CHAR_HANDOFF_REJECT reason=account_mismatch"));
		return false;
	}
	if (!Ticket->Faction.Equals(UTF8_TO_TCHAR(Handoff.faction.c_str()), ESearchCase::IgnoreCase) ||
		Handoff.snapshot.faction != (Ticket->Faction.Equals(TEXT("Enforcer"), ESearchCase::IgnoreCase) ? apb::Faction::Enforcer : apb::Faction::Criminal))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_HANDOFF_REJECT reason=faction_mismatch"));
		return false;
	}
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB || !APB->ApplyHandoffSnapshot(Handoff.snapshot))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_HANDOFF_REJECT reason=stale"));
		return false;
	}
	AppliedHandoffNonces.Add(UTF8_TO_TCHAR(Handoff.nonce.c_str()));
	AppliedHandoffJtis.Add(UTF8_TO_TCHAR(Handoff.jti.c_str()));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>()) APB->SyncPlayerStateFromDomain(PS);
		}
	}
	APB->PushDomainSnapshotToAllPlayerStates();
	UE_LOG(LogTemp, Log, TEXT("CHAR_HANDOFF_APPLIED account=%s faction=%s cash=%lld threat=%.1f"),
		*Ticket->Account, *Ticket->Faction, Handoff.snapshot.cash, Handoff.snapshot.threat_points);
	// M13: the Domain now holds any persisted session vehicle (RestoreHandoff); let
	// the district respawn its actor + paint.
	OnHandoffApplied();
	FString Probe;
	if (FParse::Value(FCommandLine::Get(), TEXT("APBProbe="), Probe) && Probe.Equals(TEXT("world_handoff_district"), ESearchCase::IgnoreCase))
	{
		HandoffProbeReturnAtMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond() + 2000;
	}
	return true;
}

void AAPBDistrictGameMode::RunHandoffProbeReturn()
{
	if (HandoffProbeReturnAtMs == 0) return;
	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	if (NowMs < HandoffProbeReturnAtMs) return;
	HandoffProbeReturnAtMs = 0;
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB || !APB->ApplyHandoffProbeMutation()) return;
	APB->PushDomainSnapshotToAllPlayerStates();
	const FAPBDomainSnapshotUE Mutated = APB->CaptureDomainSnapshot();
	UE_LOG(LogTemp, Log, TEXT("CHAR_HANDOFF_PROBE_MUTATED cash=%lld threat=%.1f"), Mutated.Cash, Mutated.ThreatPoints);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->PlayerState) continue;
		const FString Key = TicketNetKey(PC->PlayerState->GetUniqueId());
		const FAPBVerifiedTicket* Ticket = ActiveTicketsByNetId.Find(Key);
		if (!Ticket || !Ticket->bValid || !SendRelayReturn(PC)) continue;
		int32 WorldPort = apb::ports::World;
		FParse::Value(FCommandLine::Get(), TEXT("WorldPort="), WorldPort);
		PC->ClientTravel(FString::Printf(TEXT("127.0.0.1:%d"), WorldPort), ETravelType::TRAVEL_Absolute);
	}
}

bool AAPBDistrictGameMode::SendRelayReturn(AController* Exiting)
{
	if (!RelayControl || !Exiting || !Exiting->PlayerState) return false;
	const FString Key = TicketNetKey(Exiting->PlayerState->GetUniqueId());
	const FAPBVerifiedTicket* Ticket = ActiveTicketsByNetId.Find(Key);
	if (!Ticket || !Ticket->bValid || !AppliedHandoffJtis.Contains(Ticket->Jti)) return false;
	if (ReturnedHandoffJtis.Contains(Ticket->Jti)) return true;
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	const FString& HandoffSecret = FAPBSecretProvider::HandoffSecret();
	const FString& RelaySecret = FAPBSecretProvider::RelaySecret();
	if (!APB || HandoffSecret.IsEmpty() || RelaySecret.IsEmpty()) return false;
	apb::CharacterHandoff Handoff;
	Handoff.account = TCHAR_TO_UTF8(*Ticket->Account);
	Handoff.character = TCHAR_TO_UTF8(*Ticket->Character);
	Handoff.faction = TCHAR_TO_UTF8(*Ticket->Faction);
	Handoff.jti = TCHAR_TO_UTF8(*Ticket->Jti);
	Handoff.nonce = apb::random_hex(16);
	Handoff.sent_ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	Handoff.snapshot = reinterpret_cast<apb::WorldService*>(APB->Service)->CaptureSnapshot();
	const std::string Signed = apb::SignHandoff(Handoff, TCHAR_TO_UTF8(*HandoffSecret));
	const apb::RelayMessage Message = apb::RelayCodec::MakeReturn(Handoff.account, Handoff.character, Handoff.faction,
		Handoff.jti, Handoff.nonce, Signed, RelayControl->GetDistrictNumericId(), "return-" + Handoff.jti + "-" + Handoff.nonce,
		Handoff.sent_ms, TCHAR_TO_UTF8(*RelaySecret));
	if (!RelayControl->SendRelayToWorld(Message)) return false;
	ReturnedHandoffJtis.Add(Ticket->Jti);
	UE_LOG(LogTemp, Log, TEXT("CHAR_RETURN_SENT account=%s nonce=%s"), *Ticket->Account, UTF8_TO_TCHAR(Handoff.nonce.c_str()));
	return true;
}

void AAPBDistrictGameMode::ApplySocialResult(const apb::RelayMessage& Message)
{
	// A SocialResult arrived from the world authority — the social op was executed.
	// Log the outcome and notify the requesting player via Client_SocialResult.
	const FString Character(UTF8_TO_TCHAR(Message.character.c_str()));
	const FString OpId(UTF8_TO_TCHAR(Message.operation_id.c_str()));
	const FString Status(UTF8_TO_TCHAR(Message.social_status.c_str()));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_RESULT_RECEIVED character=%s op_id=%s status=%s"),
		*Character, *OpId, *Status);

	// Find the player and deliver the result + push updated social state.
	if (!GetWorld()) return;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		if (!IsAdmittedPlayer(PC)) continue;
		if (!ResolveDistrictPlayerName(PC).Equals(Character, ESearchCase::CaseSensitive)) continue;
		if (AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>())
		{
			// Parse op from operation_id (format: soc-<numeric_id>-<op>-<ts>).
			// The op itself contains dots (e.g. "clan.invite") but no dashes.
			FString OpName;
			// Find the three dashes by searching progressively from the previous position.
			int32 D1 = OpId.Find(TEXT("-"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 0);
			if (D1 != INDEX_NONE)
			{
				int32 D2 = OpId.Find(TEXT("-"), ESearchCase::CaseSensitive, ESearchDir::FromStart, D1 + 1);
				if (D2 != INDEX_NONE)
				{
					int32 D3 = OpId.Find(TEXT("-"), ESearchCase::CaseSensitive, ESearchDir::FromStart, D2 + 1);
					if (D3 != INDEX_NONE)
					{
						OpName = OpId.Mid(D2 + 1, D3 - D2 - 1);
					}
				}
			}
			PS->Client_SocialResult(OpName, Status, UTF8_TO_TCHAR(Message.body.c_str()));

			// Update replicated social fields from the result status.
			// Clan invite pending is implied by the op type + status.
			if (OpName.Contains(TEXT("clan.invite")) && Status == TEXT("ok"))
			{
				PS->bHasPendingClanInvite = true;
			}
			else if (OpName.Contains(TEXT("clan.accept")) && Status == TEXT("ok"))
			{
				PS->bHasPendingClanInvite = false;
			}
			else if (OpName.Contains(TEXT("group.invite")) && Status == TEXT("ok"))
			{
				PS->bHasPendingGroupInvite = true;
			}
			else if (OpName.Contains(TEXT("group.accept")) && Status == TEXT("ok"))
			{
				PS->bHasPendingGroupInvite = false;
			}
			PS->OnRep_Social();
			PS->ForceNetUpdate();
		}
		return;
	}
}

void AAPBDistrictGameMode::ApplySocialChat(const apb::RelayMessage& Message)
{
	// Social-channel chat fan-out from the world (clan/group chat relay).
	// Route into this district's ChatService for delivery to local members.
	if (Message.from.empty() || Message.body.empty()) return;
	const FString Channel(UTF8_TO_TCHAR(Message.social_op.c_str()));
	const apb::ChatChannel Ch = Channel.Contains(TEXT("clan"), ESearchCase::IgnoreCase)
		? apb::ChatChannel::Clan
		: Channel.Contains(TEXT("group"), ESearchCase::IgnoreCase)
		? apb::ChatChannel::Group
		: apb::ChatChannel::District;
	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	const apb::SubmitResult Result = ChatService.Submit(Message.from, Ch,
		Message.to, Message.body, NowMs);
	if (Result.status == apb::ChatResult::Delivered)
	{
		for (const apb::ChatDelivery& Delivery : Result.deliveries) DeliverChat(Delivery);
	}
}

void AAPBDistrictGameMode::Logout(AController* Exiting)
{
	if (HasAuthority()) SendRelayReturn(Exiting);
	if (Exiting && Exiting->PlayerState)
	{
		const FString Key = TicketNetKey(Exiting->PlayerState->GetUniqueId());
		if (const FAPBVerifiedTicket* Ticket = ActiveTicketsByNetId.Find(Key))
		{
			ChatService.RemovePlayer(TCHAR_TO_UTF8(*Ticket->Character));
			AppliedHandoffJtis.Remove(Ticket->Jti);
			if (RelayControl)
			{
				const FString& Secret = FAPBSecretProvider::RelaySecret();
				if (!Secret.IsEmpty())
				{
					const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
					const std::string RequestId = "leave-" + std::string(TCHAR_TO_UTF8(*Ticket->Jti));
					RelayControl->SendRelayToWorld(apb::RelayCodec::MakePlayerLeft(TCHAR_TO_UTF8(*Ticket->Account),
						TCHAR_TO_UTF8(*Ticket->Character), RelayControl->GetDistrictNumericId(), RequestId, NowMs,
						TCHAR_TO_UTF8(*Secret)));
				}
			}
		}
		ActiveTicketsByNetId.Remove(Key);
	}
	Super::Logout(Exiting);
	if (RelayControl)
	{
		RelayControl->SetDistrictPopulation(GetRemotePlayerCount());
	}
}
