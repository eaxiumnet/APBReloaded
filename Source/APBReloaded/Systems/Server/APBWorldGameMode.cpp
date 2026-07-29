#include "APBWorldGameMode.h"
#include "APBPlayerController.h"
#include "APBPlayerState.h"
#include "APBServerControl.h"
#include "APBSecretProvider.h"
#include "APBHandoff.h"
#include "APBRelayProtocol.h"
#include "APBGameInstanceSubsystem.h"
#include "APBFriends.h"
#include "APBClan.h"
#include "APBGroup.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "APBSocial.h"
#include "APBTicket.h"
#include <string>

AAPBWorldGameMode::AAPBWorldGameMode()
{
	PlayerControllerClass = AAPBPlayerController::StaticClass();
	PlayerStateClass = AAPBPlayerState::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void AAPBWorldGameMode::BeginPlay()
{
	Super::BeginPlay();
	DataDir    = FPaths::ProjectContentDir() / TEXT("Data");
	PersistDir = FPaths::ProjectSavedDir()   / TEXT("DomainDB");
	SocialAuthority.InitFromDataDir(TCHAR_TO_UTF8(*DataDir));
	if (SocialAuthority.InitSocialPersistence(TCHAR_TO_UTF8(*PersistDir)))
	{
		UE_LOG(LogTemp, Log, TEXT("SOCIAL_PERSIST_INIT dir=%s"), *(PersistDir / TEXT("social")));
	}
	{
		const FString& TicketSecretHex = FAPBSecretProvider::TicketSecret();
		if (!TicketSecretHex.IsEmpty())
		{
			apb::TicketService::Global().SetSecret(TCHAR_TO_UTF8(*TicketSecretHex));
		}
	}
	ServerControl = NewObject<UAPBServerControl>(this);
	ServerControl->Init(this);
}

void AAPBWorldGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ServerControl)
	{
		ServerControl->Shutdown();
	}
	SocialAuthority.SaveSocialNow();
	Super::EndPlay(EndPlayReason);
}

void AAPBWorldGameMode::BeginDestroy()
{
	if (ServerControl)
	{
		ServerControl->Shutdown();
	}
	Super::BeginDestroy();
}

void AAPBWorldGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	const FString Key = PCKey(NewPlayer);
	if (!PlayerServices.Contains(Key))
	{
		auto Entry = MakeUnique<FAPBPlayerService>();
		Entry->Service = MakeUnique<apb::WorldService>();
		Entry->Service->InitFromDataDir(TCHAR_TO_UTF8(*DataDir));
		Entry->Service->InitPersistence(TCHAR_TO_UTF8(*PersistDir));
		PlayerServices.Add(Key, MoveTemp(Entry));
	}
}

void AAPBWorldGameMode::Logout(AController* Exiting)
{
	if (APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		const FString Key = PCKey(PC);
		if (TUniquePtr<FAPBPlayerService>* Found = PlayerServices.Find(Key))
		{
			if (*Found && (*Found)->Service) (*Found)->Service->SaveAllNow();
		}
		bool bTravelReservationActive = false;
		for (const TPair<FString, FTravelReservation>& Entry : TravelReservations)
		{
			if (Entry.Value.OwnerKey == Key)
			{
				bTravelReservationActive = true;
				break;
			}
		}
		if (!bTravelReservationActive && ServerControl)
		{
			const TArray<FString> ReleasedReservations = ServerControl->ReleaseLiveDistrictReservationsForPlayer(Key);
			for (const FString& ReservationId : ReleasedReservations)
			{
				ReleaseTravelReservationById(ReservationId);
			}
		}
		for (TPair<FString, FTravelReservation>& Entry : TravelReservations)
		{
			if (Entry.Value.OwnerKey == Key)
			{
				CancelTravelDomainReservation(Entry.Value);
			}
		}
		PlayerServices.Remove(Key);
	}
	Super::Logout(Exiting);
}

FAPBPlayerService* AAPBWorldGameMode::ServiceFor(APlayerController* PC) const
{
	if (!PC) return nullptr;
	const FString Key = PCKey(PC);
	const TUniquePtr<FAPBPlayerService>* Found = PlayerServices.Find(Key);
	return Found ? Found->Get() : nullptr;
}

FString AAPBWorldGameMode::PCKey(APlayerController* PC)
{
	if (!PC) return TEXT("");
	return FString::Printf(TEXT("%p"), (void*)PC);
}

bool AAPBWorldGameMode::LoginPlayer(APlayerController* PC,
                                    const FString& User, const FString& Pass,
                                    FString& OutError)
{
	FAPBPlayerService* Svc = ServiceFor(PC);
	if (!Svc || !Svc->Service) { OutError = TEXT("no_service"); return false; }
	const std::string U = TCHAR_TO_UTF8(*User);
	const std::string P = TCHAR_TO_UTF8(*Pass);
	// Server-authoritative first-seen provisioning: registration must happen on the
	// authority, not the client (the client's in-process RegisterAccount never reaches
	// this per-player service). RegisterAccount is a no-op when the account already
	// exists, so existing accounts still validate their PBKDF2 password (and banned
	// accounts still fail) via LoginAccount below. Per-player isolation (R3) is preserved.
	Svc->Service->RegisterAccount(U, P);
	bool ok = Svc->Service->LoginAccount(U, P);
	if (!ok) { OutError = TEXT("login_failed"); return false; }

	// M14: bind the connection's social identity server-authoritatively. Direct world
	// clients (no district relay) must still be admitted + online for clan invites,
	// friend presence, and the name-keyed PushSocialStateToPlayerStates to reach them.
	// Identity = the domain character when one exists, else the password-verified account.
	FString Identity = User;
	FString FactionName = TEXT("Criminal");
	if (Svc->Service->character.has_value())
	{
		Identity = UTF8_TO_TCHAR(Svc->Service->character->name.c_str());
		FactionName = (Svc->Service->character->faction == apb::Faction::Enforcer)
			? TEXT("Enforcer") : TEXT("Criminal");
	}
	if (PC && PC->PlayerState)
	{
		PC->PlayerState->SetPlayerName(Identity);
	}
	if (!FindAdmittedPlayer(Identity))
	{
		FAPBAdmittedPlayer Entry;
		Entry.Account           = User;
		Entry.Character         = Identity;
		Entry.Faction           = FactionName;
		Entry.Jti               = TEXT("direct");
		Entry.DistrictNumericId = 0;
		Entry.bAdmitted         = true;
		AdmittedRoster.Add(Identity, MoveTemp(Entry));
	}
	SocialAuthority.friends_svc.SetOnline(TCHAR_TO_UTF8(*Identity), true);
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_DIRECT_BIND character=%s account=%s faction=%s"),
		*Identity, *User, *FactionName);
	PushSocialStateToPlayerStates();
	return true;
}

FString AAPBWorldGameMode::GetCharListJson(APlayerController* PC) const
{
	const FAPBPlayerService* Svc = ServiceFor(PC);
	if (!Svc || !Svc->Service) return TEXT("[]");
	const apb::WorldService* WS = Svc->Service.Get();
	if (WS->character.has_value())
	{
		const std::string& Name = WS->character->name;
		const std::string Faction = (WS->character->faction == apb::Faction::Criminal)
			? "Criminal" : "Enforcer";
		std::string Json = "[{\"name\":\"" + Name + "\",\"faction\":\"" + Faction + "\"}]";
		return FString(UTF8_TO_TCHAR(Json.c_str()));
	}
	return TEXT("[]");
}

FString AAPBWorldGameMode::GetDistrictListJson(APlayerController* PC) const
{
	const FAPBPlayerService* Svc = ServiceFor(PC);
	if (!Svc || !Svc->Service) return TEXT("[]");
	const apb::WorldService* WS = Svc->Service.Get();
	TMap<FString, FAPBDistrictPopulationSnapshot> LiveDistricts;
	if (ServerControl)
	{
		for (const FAPBDistrictPopulationSnapshot& Snapshot : ServerControl->GetLiveDistrictPopulationSnapshot())
		{
			LiveDistricts.Add(UTF8_TO_TCHAR(Snapshot.DistrictId.c_str()), Snapshot);
		}
	}
	const auto& Districts = WS->catalog.districts;
	std::string Json = "[";
	bool First = true;
	for (const auto& D : Districts)
	{
		if (!First) Json += ",";
		First = false;
		const FAPBDistrictPopulationSnapshot* Snapshot = LiveDistricts.Find(UTF8_TO_TCHAR(D.id.c_str()));
		const int32 InstanceCount = Snapshot ? Snapshot->InstanceCount : 0;
		const int32 Population = Snapshot ? Snapshot->Population : 0;
		Json += "{\"id\":\"" + D.id + "\",\"name\":\"" + D.name + "\",\"instanceCount\":" +
			std::to_string(InstanceCount) + ",\"population\":" + std::to_string(Population) + "}";
	}
	Json += "]";
	return FString(UTF8_TO_TCHAR(Json.c_str()));
}

FString AAPBWorldGameMode::IssueTicketJson(APlayerController* PC,
                                           const FString& CharName,
                                           const FString& DistrictId)
{
	FAPBPlayerService* Svc = ServiceFor(PC);
	if (!Svc || !Svc->Service || !ServerControl) return TEXT("{\"error\":\"no_live_node\"}");
	apb::WorldService* WS = Svc->Service.Get();
	// Plan intent (C4/C7): ticket issue requires IsLoggedIn. A freshly provisioned
	// account may not have a character yet, so faction falls back to Enforcer when no
	// character is loaded; the ticket is still a genuine HMAC payload.sig token.
	if (!WS->login.IsLoggedIn()) return TEXT("{\"error\":\"no_ticket\"}");
	if (!WS->character || WS->character->name != TCHAR_TO_UTF8(*CharName))
	{
		if (!WS->CreateCharacter(TCHAR_TO_UTF8(*CharName), apb::Faction::Enforcer))
		{
			return TEXT("{\"error\":\"character_unavailable\"}");
		}
	}
	FString Host;
	FString ReservationId;
	FString Error;
	int32 Port = 0;
	int32 NumericId = 0;
	const FString PlayerKey = PCKey(PC);
	TArray<FString> PreviousReservations;
	for (const TPair<FString, FTravelReservation>& Entry : TravelReservations)
	{
		if (Entry.Value.OwnerKey == PlayerKey)
		{
			PreviousReservations.Add(Entry.Key);
		}
	}
	for (const FString& PreviousReservationId : PreviousReservations)
	{
		ReleaseTravelReservationById(PreviousReservationId);
	}
	const apb::DistrictInfo* CatalogDistrict = nullptr;
	for (const apb::DistrictInfo& District : WS->catalog.districts)
	{
		if (DistrictId.Equals(UTF8_TO_TCHAR(District.id.c_str()), ESearchCase::IgnoreCase))
		{
			CatalogDistrict = &District;
			break;
		}
	}
	if (!CatalogDistrict || !CatalogDistrict->joinable)
	{
		return TEXT("{\"error\":\"unknown_district\"}");
	}
	if (!ServerControl->ResolveLiveDistrict(DistrictId, CatalogDistrict->max_players, Host, Port,
		NumericId, Error))
	{
		return FString::Printf(TEXT("{\"error\":\"%s\"}"), *Error);
	}
	const apb::DistrictReservation DomainReservation = WS->ReserveDistrict(TCHAR_TO_UTF8(*DistrictId),
		WS->character->name);
	if (DomainReservation.state != apb::DistrictQueueState::Reserved)
	{
		WS->CancelDistrictReservation(WS->character->name);
		return TEXT("{\"error\":\"over_capacity\"}");
	}
	if (!ServerControl->ReserveLiveDistrict(PlayerKey, DistrictId, CatalogDistrict->max_players, Host, Port,
		NumericId, ReservationId, Error))
	{
		WS->CancelDistrictReservation(WS->character->name);
		return FString::Printf(TEXT("{\"error\":\"%s\"}"), *Error);
	}
	apb::TicketClaims Claims;
	Claims.account   = WS->login.session->account_id;
	Claims.character = TCHAR_TO_UTF8(*CharName);
	Claims.faction   = (WS->character.has_value() &&
	                    WS->character->faction == apb::Faction::Criminal)
	                   ? "Criminal" : "Enforcer";
	Claims.district  = TCHAR_TO_UTF8(*DistrictId);
	std::string Token = apb::TicketService::Global().IssueTicket(Claims);
	if (Token.empty())
	{
		WS->CancelDistrictReservation(WS->character->name);
		ServerControl->ReleaseLiveDistrictReservation(ReservationId);
		return TEXT("{\"error\":\"no_ticket\"}");
	}
	apb::TicketClaims VerifiedClaims;
	if (!apb::TicketService::Global().VerifyTicket(Token, VerifiedClaims))
	{
		WS->CancelDistrictReservation(WS->character->name);
		ServerControl->ReleaseLiveDistrictReservation(ReservationId);
		return TEXT("{\"error\":\"no_ticket\"}");
	}
	apb::CharacterHandoff Handoff;
	Handoff.account = VerifiedClaims.account;
	Handoff.character = VerifiedClaims.character;
	Handoff.faction = VerifiedClaims.faction;
	Handoff.jti = VerifiedClaims.jti;
	Handoff.nonce = apb::random_hex(16);
	Handoff.sent_ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	Handoff.snapshot = WS->CaptureSnapshot();
	Handoff.snapshot.session_id = DomainReservation.session_id;
	Handoff.snapshot.district_id = TCHAR_TO_UTF8(*DistrictId);
	Handoff.snapshot.district_players = 1;
	const FString& HandoffSecret = FAPBSecretProvider::HandoffSecret();
	const FString& RelaySecret = FAPBSecretProvider::RelaySecret();
	if (HandoffSecret.IsEmpty() || RelaySecret.IsEmpty())
	{
		WS->CancelDistrictReservation(WS->character->name);
		ServerControl->ReleaseLiveDistrictReservation(ReservationId);
		return TEXT("{\"error\":\"no_ticket\"}");
	}
	const std::string SignedHandoff = apb::SignHandoff(Handoff, TCHAR_TO_UTF8(*HandoffSecret));
	apb::RelayMessage RelayHandoff = apb::RelayCodec::MakeHandoff(Handoff.account, Handoff.character, Handoff.faction, Handoff.jti, Handoff.nonce,
			SignedHandoff, NumericId, "handoff-" + Handoff.jti + "-" + Handoff.nonce, Handoff.sent_ms,
			TCHAR_TO_UTF8(*RelaySecret));
	FString TamperCharacter;
	FParse::Value(FCommandLine::Get(), TEXT("APBRelayTamperCharacter="), TamperCharacter);
	if (!TamperCharacter.IsEmpty() && TamperCharacter == CharName)
	{
		RelayHandoff.account += "-tampered";
	}
	if (SignedHandoff.empty() || !ServerControl->SendRelayToDistrict(NumericId, RelayHandoff))
	{
		WS->CancelDistrictReservation(WS->character->name);
		ServerControl->ReleaseLiveDistrictReservation(ReservationId);
		return TEXT("{\"error\":\"relay_unavailable\"}");
	}
	FTravelReservation Reservation;
	Reservation.OwnerKey = PlayerKey;
	Reservation.PlayerName = UTF8_TO_TCHAR(WS->character->name.c_str());
	Reservation.Account = UTF8_TO_TCHAR(Handoff.account.c_str());
	Reservation.PersistenceAccount = UTF8_TO_TCHAR(WS->login.session->username.c_str());
	Reservation.Character = UTF8_TO_TCHAR(Handoff.character.c_str());
	Reservation.Faction = UTF8_TO_TCHAR(Handoff.faction.c_str());
	Reservation.Jti = UTF8_TO_TCHAR(Handoff.jti.c_str());
	Reservation.HandoffNonce = UTF8_TO_TCHAR(Handoff.nonce.c_str());
	Reservation.DistrictNumericId = NumericId;
	Reservation.ExpiresMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + 15000;
	TravelReservations.Add(ReservationId, MoveTemp(Reservation));
	{
		FAPBAdmittedPlayer Pending;
		Pending.Account           = UTF8_TO_TCHAR(Handoff.account.c_str());
		Pending.Character         = UTF8_TO_TCHAR(Handoff.character.c_str());
		Pending.Faction           = UTF8_TO_TCHAR(Handoff.faction.c_str());
		Pending.Jti               = UTF8_TO_TCHAR(Handoff.jti.c_str());
		Pending.DistrictNumericId = NumericId;
		Pending.bAdmitted         = false;
		AdmittedRoster.Add(Pending.Character, MoveTemp(Pending));
	}
	UE_LOG(LogTemp, Log, TEXT("TRAVEL_RESERVATION_ISSUED id=%s district=%s port=%d"),
		*ReservationId, *DistrictId, Port);
	UE_LOG(LogTemp, Log, TEXT("CHAR_HANDOFF_SENT account=%s jti=%s nonce=%s faction=%s cash=%lld g1c=%lld threat=%.1f inv=%d mission=%s session=%s progress=%d"),
		*Reservation.Account, *Reservation.Jti, *Reservation.HandoffNonce, *Reservation.Faction, Handoff.snapshot.cash,
		Handoff.snapshot.g1c, Handoff.snapshot.threat_points, Handoff.snapshot.inventory_total_qty,
		UTF8_TO_TCHAR(Handoff.snapshot.mission_id.c_str()), UTF8_TO_TCHAR(Handoff.snapshot.session_id.c_str()),
		static_cast<int32>(Handoff.snapshot.contact_standings.size() + Handoff.snapshot.role_xp.size()));
	return FString::Printf(TEXT("{\"ticket\":\"%s\",\"host\":\"%s\",\"port\":%d,\"reservation_id\":\"%s\",\"session_id\":\"%s\"}"),
		UTF8_TO_TCHAR(Token.c_str()), *Host, Port, *ReservationId, UTF8_TO_TCHAR(Handoff.snapshot.session_id.c_str()));
}

FString AAPBWorldGameMode::PrepareHandoffProbeJson(APlayerController* PC)
{
	FString Probe;
	if (!FParse::Value(FCommandLine::Get(), TEXT("APBProbe="), Probe) || !Probe.Equals(TEXT("world_handoff_server"), ESearchCase::IgnoreCase)) return TEXT("");
	FAPBPlayerService* Svc = ServiceFor(PC);
	if (!Svc || !Svc->Service || !Svc->Service->login.IsLoggedIn()) return TEXT("");
	apb::WorldService* WS = Svc->Service.Get();
	if (!WS->character) WS->CreateCharacter("Operative", apb::Faction::Enforcer);
	if (!WS->character) return TEXT("");
	WS->character->name = "Operative";
	WS->character->faction = apb::Faction::Enforcer;
	WS->character->cash = 12345;
	WS->character->g1c = 4321;
	WS->threat.faction = apb::Faction::Enforcer;
	WS->threat.points = 42.5;
	WS->inventory.slots = {{"handoff_probe_a", 2}, {"handoff_probe_b", 3}};
	WS->progress.contact_standing.clear();
	WS->progress.role_xp.clear();
	WS->progress.contact_standing.emplace("Financial_C01", 250);
	WS->progress.role_xp.emplace("Enforcer", 90);
	WS->StartMission();
	WS->SaveAllNow();
	return FString(UTF8_TO_TCHAR(apb::SerializeSnapshot(WS->CaptureSnapshot()).c_str()));
}

FString AAPBWorldGameMode::GetHandoffProbeJson(APlayerController* PC) const
{
	FString Probe;
	if (!FParse::Value(FCommandLine::Get(), TEXT("APBProbe="), Probe) || !Probe.Equals(TEXT("world_handoff_server"), ESearchCase::IgnoreCase)) return TEXT("");
	const FAPBPlayerService* Svc = ServiceFor(PC);
	if (!Svc || !Svc->Service) return TEXT("");
	return FString(UTF8_TO_TCHAR(apb::SerializeSnapshot(Svc->Service->CaptureSnapshot()).c_str()));
}

void AAPBWorldGameMode::CancelTravelDomainReservation(FTravelReservation& Reservation)
{
	if (Reservation.bDomainReservationReleased)
	{
		return;
	}
	if (TUniquePtr<FAPBPlayerService>* Service = PlayerServices.Find(Reservation.OwnerKey))
	{
		if (*Service && (*Service)->Service)
		{
			(*Service)->Service->CancelDistrictReservation(TCHAR_TO_UTF8(*Reservation.PlayerName));
		}
	}
	Reservation.bDomainReservationReleased = true;
}

void AAPBWorldGameMode::ReleaseTravelReservation(APlayerController* PC, const FString& ReservationId)
{
	if (PC)
	{
		if (const FTravelReservation* Reservation = TravelReservations.Find(ReservationId))
		{
			if (Reservation->OwnerKey == PCKey(PC))
			{
				ReleaseTravelReservationById(ReservationId);
			}
		}
	}
}

void AAPBWorldGameMode::ReleaseTravelReservationById(const FString& ReservationId)
{
	if (FTravelReservation* Reservation = TravelReservations.Find(ReservationId))
	{
		const FString Character = Reservation->Character;
		const bool bWasAdmitted = Reservation->bDistrictAdmitted;
		CancelTravelDomainReservation(*Reservation);
		if (ServerControl)
		{
			ServerControl->ReleaseLiveDistrictReservation(ReservationId);
		}
		UE_LOG(LogTemp, Log, TEXT("TRAVEL_RESERVATION_RELEASED id=%s"), *ReservationId);
		TravelReservations.Remove(ReservationId);
		if (!bWasAdmitted && !Character.IsEmpty())
		{
			if (const FAPBAdmittedPlayer* Entry = AdmittedRoster.Find(Character))
			{
				if (!Entry->bAdmitted)
				{
					AdmittedRoster.Remove(Character);
				}
			}
		}
	}
}

void AAPBWorldGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ProcessRelayReturns();
	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	TArray<FString> ExpiredReservations;
	for (const TPair<FString, FTravelReservation>& Entry : TravelReservations)
	{
		if (!Entry.Value.bDistrictAdmitted && Entry.Value.ExpiresMs <= NowMs)
		{
			ExpiredReservations.Add(Entry.Key);
		}
	}
	for (const FString& ReservationId : ExpiredReservations)
	{
		ReleaseTravelReservationById(ReservationId);
		UE_LOG(LogTemp, Log, TEXT("TRAVEL_RESERVATION_EXPIRED id=%s"), *ReservationId);
	}
}

void AAPBWorldGameMode::ProcessRelayReturns()
{
	if (!ServerControl) return;
	apb::RelayMessage Message;
	while (ServerControl->DequeueWorldRelayMessage(Message))
	{
		if (Message.verb == apb::RelayVerb::PlayerJoined) MarkRelayPlayerJoined(Message);
		else if (Message.verb == apb::RelayVerb::PlayerLeft) MarkRelayPlayerLeft(Message);
		else if (Message.verb == apb::RelayVerb::ChatRelay) ForwardRelayChat(Message);
		else if (Message.verb == apb::RelayVerb::SocialRequest) HandleSocialRequest(Message);
		else ApplyRelayReturn(Message);
	}
}

void AAPBWorldGameMode::MarkRelayPlayerJoined(const apb::RelayMessage& Message)
{
	const FString Character(UTF8_TO_TCHAR(Message.character.c_str()));
	if (FAPBAdmittedPlayer* Existing = AdmittedRoster.Find(Character))
	{
		if (Existing->bAdmitted)
		{
			// T14: mark presence online idempotently even on duplicate join.
			SocialAuthority.friends_svc.SetOnline(TCHAR_TO_UTF8(*Character), true);
			return;
		}
		Existing->bAdmitted = true;
	}
	else
	{
		FAPBAdmittedPlayer Entry;
		Entry.Account           = UTF8_TO_TCHAR(Message.account.c_str());
		Entry.Character         = Character;
		Entry.Faction           = UTF8_TO_TCHAR(Message.faction.c_str());
		Entry.Jti               = UTF8_TO_TCHAR(Message.jti.c_str());
		Entry.DistrictNumericId = Message.numeric_id;
		Entry.bAdmitted         = true;
		AdmittedRoster.Add(Character, MoveTemp(Entry));
	}
	// T14: update friend presence on the social authority.
	SocialAuthority.friends_svc.SetOnline(TCHAR_TO_UTF8(*Character), true);
	UE_LOG(LogTemp, Log, TEXT("FRIEND_PRESENCE character=%s online=1"), *Character);
	PushSocialStateToPlayerStates();
	for (TPair<FString, FTravelReservation>& Entry : TravelReservations)
	{
		FTravelReservation& Reservation = Entry.Value;
		if (Reservation.Account == UTF8_TO_TCHAR(Message.account.c_str()) &&
			Reservation.Character == Character &&
			Reservation.DistrictNumericId == Message.numeric_id)
		{
			Reservation.bDistrictAdmitted = true;
			UE_LOG(LogTemp, Log, TEXT("CHAR_HANDOFF_ADMITTED account=%s jti=%s"), *Reservation.Account, *Reservation.Jti);
			return;
		}
	}
}

void AAPBWorldGameMode::MarkRelayPlayerLeft(const apb::RelayMessage& Message)
{
	const FString Character(UTF8_TO_TCHAR(Message.character.c_str()));
	if (const FAPBAdmittedPlayer* Entry = AdmittedRoster.Find(Character))
	{
		if (Entry->DistrictNumericId == Message.numeric_id)
		{
			AdmittedRoster.Remove(Character);
			// T14: mark presence offline.
			SocialAuthority.friends_svc.SetOnline(TCHAR_TO_UTF8(*Character), false);
			UE_LOG(LogTemp, Log, TEXT("FRIEND_PRESENCE character=%s online=0"), *Character);
			PushSocialStateToPlayerStates();
		}
	}
}

void AAPBWorldGameMode::ForwardRelayChat(const apb::RelayMessage& Message)
{
	if (!ServerControl || Message.from.empty() || Message.to.empty() || Message.body.empty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=RecipientOffline-after-relay-miss"));
		return;
	}
	const FString Sender(UTF8_TO_TCHAR(Message.from.c_str()));
	if (const FAPBAdmittedPlayer* SenderEntry = AdmittedRoster.Find(Sender))
	{
		if (!SenderEntry->bAdmitted || SenderEntry->DistrictNumericId != Message.numeric_id)
		{
			UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=Unauthenticated"));
			return;
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=Unauthenticated"));
		return;
	}
	const FString Recipient(UTF8_TO_TCHAR(Message.to.c_str()));
	const FAPBAdmittedPlayer* RecipientEntry = AdmittedRoster.Find(Recipient);
	const int32 TargetDistrictId = RecipientEntry ? RecipientEntry->DistrictNumericId : 0;
	const FString& Secret = FAPBSecretProvider::RelaySecret();
	if (TargetDistrictId <= 0 || Secret.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=RecipientOffline-after-relay-miss"));
		return;
	}
	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	const std::string RequestId = "chatfwd-" + std::to_string(TargetDistrictId) + "-" + Message.request_id;
	const apb::RelayMessage Forwarded = apb::RelayCodec::MakeChatRelay(Message.from, Message.to, Message.body,
		TargetDistrictId, RequestId, NowMs, TCHAR_TO_UTF8(*Secret));
	if (!ServerControl->SendRelayToDistrict(TargetDistrictId, Forwarded))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=RecipientOffline-after-relay-miss"));
	}
}

const FAPBAdmittedPlayer* AAPBWorldGameMode::FindAdmittedPlayer(const FString& Character) const
{
	const FAPBAdmittedPlayer* Entry = AdmittedRoster.Find(Character);
	if (Entry && Entry->bAdmitted) return Entry;
	return nullptr;
}

apb::WorldService* AAPBWorldGameMode::ServiceForCharacter(const FString& Character) const
{
	if (Character.IsEmpty()) return nullptr;
	const std::string Wanted = TCHAR_TO_UTF8(*Character);
	for (const TPair<FString, TUniquePtr<FAPBPlayerService>>& Entry : PlayerServices)
	{
		if (!Entry.Value || !Entry.Value->Service) continue;
		apb::WorldService* Candidate = Entry.Value->Service.Get();
		if (Candidate->character.has_value() && Candidate->character->name == Wanted)
			return Candidate;
	}
	return nullptr;
}

bool AAPBWorldGameMode::ApplyRelayReturn(const apb::RelayMessage& Message)
{
	if (Message.verb != apb::RelayVerb::Return) return false;
	const FString& Secret = FAPBSecretProvider::HandoffSecret();
	if (Secret.IsEmpty()) return false;
	apb::CharacterHandoff Handoff;
	if (!apb::VerifyHandoff(Message.body, TCHAR_TO_UTF8(*Secret), Handoff))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_RETURN_REJECT reason=bad_sig"));
		return false;
	}
	if (ConsumedReturnNonces.Contains(UTF8_TO_TCHAR(Handoff.nonce.c_str())))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_RETURN_REJECT reason=nonce_replay"));
		return false;
	}
	FTravelReservation* Reservation = nullptr;
	for (TPair<FString, FTravelReservation>& Entry : TravelReservations)
	{
		if (Entry.Value.Jti == UTF8_TO_TCHAR(Handoff.jti.c_str())) { Reservation = &Entry.Value; break; }
	}
	const FString HandoffAccount = UTF8_TO_TCHAR(Handoff.account.c_str());
	const FString HandoffCharacter = UTF8_TO_TCHAR(Handoff.character.c_str());
	const FString HandoffFaction = UTF8_TO_TCHAR(Handoff.faction.c_str());
	const FString HandoffJti = UTF8_TO_TCHAR(Handoff.jti.c_str());
	if (!Reservation || !Reservation->bDistrictAdmitted || Reservation->Account != HandoffAccount || Reservation->Character != HandoffCharacter ||
		Reservation->Faction != HandoffFaction || Message.nonce != Handoff.nonce || Message.account != Handoff.account || Message.character != Handoff.character ||
		Message.faction != Handoff.faction || Message.jti != Handoff.jti || Message.numeric_id != Reservation->DistrictNumericId)
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_RETURN_REJECT reason=account_mismatch"));
		return false;
	}
	apb::WorldService Persisted;
	const std::string DataDirectory(TCHAR_TO_UTF8(*this->DataDir));
	const std::string PersistenceDirectory(TCHAR_TO_UTF8(*this->PersistDir));
	Persisted.InitFromDataDir(DataDirectory);
	Persisted.InitPersistence(PersistenceDirectory);
	if (!Persisted.ApplyHandoffForAccount(Handoff.snapshot, TCHAR_TO_UTF8(*Reservation->PersistenceAccount)))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAR_RETURN_REJECT reason=stale"));
		return false;
	}
	ConsumedReturnNonces.Add(UTF8_TO_TCHAR(Handoff.nonce.c_str()));
	UE_LOG(LogTemp, Log, TEXT("CHAR_RETURN_APPLIED account=%s cash=%lld threat=%.1f"),
		UTF8_TO_TCHAR(Handoff.account.c_str()), Handoff.snapshot.cash, Handoff.snapshot.threat_points);
	return true;
}

void AAPBWorldGameMode::HandleSocialRequest(const apb::RelayMessage& Message)
{
	// Cross-process social relay: a district forwarded a SocialRequest from one of its
	// players. We execute the op against SocialAuthority via the UGI bridge, then send
	// a SocialResult relay message back to the originating district. The district then
	// pushes the updated social state to the requesting player's PlayerState.
	const FString Character(UTF8_TO_TCHAR(Message.character.c_str()));
	const FString SocialOp(UTF8_TO_TCHAR(Message.social_op.c_str()));
	if (Character.IsEmpty() || SocialOp.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("SOCIAL_RELAY_REJECT reason=missing_fields"));
		return;
	}

	// Parse arg1/arg2 from the body JSON (compact flat: {"arg1":"...","arg2":"..."}).
	FString Arg1, Arg2;
	const FString Body(UTF8_TO_TCHAR(Message.body.c_str()));
	{
		const FString Key1 = TEXT("\"arg1\":\"");
		int32 P1 = Body.Find(Key1);
		if (P1 != INDEX_NONE)
		{
			P1 += Key1.Len();
			int32 E1 = Body.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, P1);
			if (E1 != INDEX_NONE) Arg1 = Body.Mid(P1, E1 - P1);
		}
		const FString Key2 = TEXT("\"arg2\":\"");
		int32 P2 = Body.Find(Key2);
		if (P2 != INDEX_NONE)
		{
			P2 += Key2.Len();
			int32 E2 = Body.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, P2);
			if (E2 != INDEX_NONE) Arg2 = Body.Mid(P2, E2 - P2);
		}
	}

	// Dispatch through the UGI bridge against SocialAuthority.
	UAPBGameInstanceSubsystem* APB = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>();
	}
	if (!APB)
	{
		UE_LOG(LogTemp, Warning, TEXT("SOCIAL_RELAY_REJECT reason=no_bridge"));
		return;
	}

	// The UGI bridge's SocialSvc resolver finds SocialAuthority because we are the world GM.
	const bool bCanMutate = APB->CanMutateDomain();
	if (!bCanMutate)
	{
		UE_LOG(LogTemp, Warning, TEXT("SOCIAL_RELAY_REJECT reason=cannot_mutate"));
		return;
	}

	FString Status = TEXT("unknown_op");
	const FString Op = SocialOp.ToLower();

	// Clan ops
	if (Op == TEXT("clan.create"))
	{
		Status = APB->SocialClanCreate(Arg1, Arg1, Arg2, Character, false) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("clan.invite"))
	{
		Status = APB->SocialClanInvite(Character, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("clan.accept"))
	{
		Status = APB->SocialClanAcceptInvite(Character) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("clan.decline"))
	{
		Status = APB->SocialClanDeclineInvite(Character) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("clan.leave"))
	{
		Status = APB->SocialClanLeave(Character) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("clan.disband"))
	{
		Status = APB->SocialClanDisband(Character) ? TEXT("ok") : TEXT("domain_rejected");
	}
	// Friend ops
	else if (Op == TEXT("friend.request"))
	{
		Status = APB->SocialFriendRequest(Character, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("friend.accept"))
	{
		Status = APB->SocialFriendAccept(Character, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("friend.decline"))
	{
		Status = APB->SocialFriendDecline(Character, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("friend.remove"))
	{
		Status = APB->SocialFriendRemove(Character, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("friend.ignore"))
	{
		Status = APB->SocialFriendIgnore(Character, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("friend.unignore"))
	{
		Status = APB->SocialFriendUnignore(Character, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	// Group ops
	else if (Op == TEXT("group.create"))
	{
		FString OutId;
		Status = APB->SocialGroupCreate(Character, OutId) ? FString::Printf(TEXT("ok:%s"), *OutId) : TEXT("domain_rejected");
	}
	else if (Op == TEXT("group.invite"))
	{
		Status = APB->SocialGroupInvite(Character, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("group.accept"))
	{
		Status = APB->SocialGroupAccept(Character) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("group.leave"))
	{
		Status = APB->SocialGroupLeave(Character) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("group.setready"))
	{
		Status = APB->SocialGroupSetReady(Character, Arg1 == TEXT("1") || Arg1 == TEXT("true")) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("group.disband"))
	{
		Status = APB->SocialGroupDisband(Character) ? TEXT("ok") : TEXT("domain_rejected");
	}
	// Mail ops
	else if (Op == TEXT("mail.send"))
	{
		FString Subject, BodyText;
		if (Arg2.Contains(TEXT("|")))
		{
			Subject = Arg2.Left(Arg2.Find(TEXT("|")));
			BodyText = Arg2.RightChop(Arg2.Find(TEXT("|")) + 1);
		}
		else
		{
			Subject = Arg2;
		}
		Status = APB->SocialMailSend(Character, Arg1, Subject, BodyText, 0) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("mail.markread"))
	{
		Status = APB->SocialMailMarkRead(Character, Arg1) == EAPBMailResult::Ok ? TEXT("ok") : TEXT("domain_rejected");
	}

	UE_LOG(LogTemp, Log, TEXT("SOCIAL_RELAY_OP character=%s op=%s status=%s op_id=%s"),
		*Character, *SocialOp, *Status, UTF8_TO_TCHAR(Message.operation_id.c_str()));

	// Send SocialResult back to the originating district.
	const FString& RelaySecret = FAPBSecretProvider::RelaySecret();
	if (ServerControl && !RelaySecret.IsEmpty() && Message.numeric_id > 0)
	{
		const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
		const apb::RelayMessage Result = apb::RelayCodec::MakeSocialResult(
			TCHAR_TO_UTF8(*Character), Message.operation_id, TCHAR_TO_UTF8(*Status),
			0, std::string("epoch-1"), std::string("{}"), Message.numeric_id,
			Message.operation_id + "-result", NowMs, TCHAR_TO_UTF8(*RelaySecret));
		ServerControl->SendRelayToDistrict(Message.numeric_id, Result);
		UE_LOG(LogTemp, Log, TEXT("SOCIAL_RESULT_SENT district=%d op_id=%s"),
			Message.numeric_id, UTF8_TO_TCHAR(Message.operation_id.c_str()));
	}

	// Push updated social state to all world-side PlayerStates (for lobby clients).
	PushSocialStateToPlayerStates();

	// Emit gate markers for social operations.
	if (Op == TEXT("clan.create") && Status == TEXT("ok"))
	{
		UE_LOG(LogTemp, Log, TEXT("SOCIAL_GATE_CLAN_OK"));
	}
	if (Op == TEXT("friend.request") && Status == TEXT("ok"))
	{
		UE_LOG(LogTemp, Log, TEXT("SOCIAL_GATE_FRIENDS_OK"));
	}
	if (Op == TEXT("group.create") && Status.StartsWith(TEXT("ok")))
	{
		UE_LOG(LogTemp, Log, TEXT("SOCIAL_GATE_GROUP_OK"));
	}
}

void AAPBWorldGameMode::PushSocialStateToPlayerStates()
{
	// After a social mutation or presence change, push the updated social state to each
	// connected player's replicated fields. This reads from SocialAuthority.
	UWorld* World = GetWorld();
	if (!World) return;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>();
		if (!PS || !PS->HasAuthority()) continue;

		const FString Character = PS->GetPlayerName();
		const std::string CharName = TCHAR_TO_UTF8(*Character);

		// Clan state — ClanOf returns the clan id string; Find gets the Clan struct.
		const std::string ClanIdStr = SocialAuthority.clans.ClanOf(CharName);
		const apb::Clan* Clan = ClanIdStr.empty() ? nullptr : SocialAuthority.clans.Find(ClanIdStr);
		PS->ClanId = Clan ? UTF8_TO_TCHAR(Clan->id.c_str()) : FString();
		PS->ClanRole = Clan ? UTF8_TO_TCHAR(SocialAuthority.clans.RankOf(CharName).c_str()) : FString();
		PS->bHasPendingClanInvite = !SocialAuthority.clans.PendingInvitesFor(CharName).empty();

		// Group state — GroupOf returns the group id string; Find gets the Group struct.
		const std::string GroupIdStr = SocialAuthority.groups.GroupOf(CharName);
		const apb::Group* Group = GroupIdStr.empty() ? nullptr : SocialAuthority.groups.Find(GroupIdStr);
		PS->GroupId = Group ? UTF8_TO_TCHAR(Group->id.c_str()) : FString();
		PS->bHasPendingGroupInvite = !SocialAuthority.groups.PendingInvitesFor(CharName).empty();
		PS->bGroupAllReady = Group ? SocialAuthority.groups.AllReady(Group->id) : false;

		// Friend state
		PS->OnlineFriendCount = static_cast<int32>(SocialAuthority.friends_svc.OnlineFriendsOf(CharName).size());

		// Mail state — unread count is the client-visible inbox badge.
		PS->MailUnreadCount = SocialAuthority.mail.UnreadCount(CharName);

		PS->OnRep_Social();
		PS->ForceNetUpdate();
	}
}
