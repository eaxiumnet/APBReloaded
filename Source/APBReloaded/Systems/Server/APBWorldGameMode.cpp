#include "APBWorldGameMode.h"
#include "APBPlayerController.h"
#include "APBPlayerState.h"
#include "APBServerControl.h"
#include "APBSecretProvider.h"
#include "Domain/APBCrypto.h"
#include "APBHandoff.h"
#include "APBRelayProtocol.h"
#include "Engine/NetConnection.h"
#include "APBGameInstanceSubsystem.h"
#include "APBAesEncryptionDelegate.h"
#include "APBFriends.h"
#include "APBClan.h"
#include "APBGroup.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "APBSocial.h"
#include "APBTicket.h"
#include "APBAuthExecutor.h"
#include "Async/Async.h"
#include <string>

AAPBWorldGameMode::AAPBWorldGameMode()
{
	PlayerControllerClass = AAPBPlayerController::StaticClass();
	PlayerStateClass = AAPBPlayerState::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

AAPBWorldGameMode::~AAPBWorldGameMode() = default;

void AAPBWorldGameMode::BeginPlay()
{
	Super::BeginPlay();
	FAPBAesEncryptionDelegate::Bind(true);
	
	AuthExecutor = MakeUnique<apb::AuthExecutor>();
	AuthExecutor->Initialize(2, 64);
	
	DataDir    = FPaths::ProjectContentDir() / TEXT("Data");
	PersistDir = FPaths::ProjectSavedDir()   / TEXT("DomainDB");
	SocialAuthority.InitFromDataDir(TCHAR_TO_UTF8(*DataDir));
	if (SocialAuthority.InitSocialPersistence(TCHAR_TO_UTF8(*PersistDir)))
	{
		UE_LOG(LogTemp, Log, TEXT("SOCIAL_PERSIST_INIT dir=%s"), *(PersistDir / TEXT("social")));
	}
	// M12: the world authority owns auction.json (single writer) and settles
	// bids/buyouts/expiry through its own mailboxes.
	if (SocialAuthority.InitAuctionPersistence(TCHAR_TO_UTF8(*PersistDir)))
	{
		UE_LOG(LogTemp, Log, TEXT("AUCTION_PERSIST_INIT dir=%s listings=%d"),
			*PersistDir, static_cast<int32>(SocialAuthority.auction.listings.size()));
	}
	{
		const FString& TicketSecretHex = FAPBSecretProvider::TicketSecret();
		TicketSvc = MakeUnique<apb::TicketService>(TCHAR_TO_UTF8(*TicketSecretHex));
	}
	WorldEpoch = FGuid::NewGuid().ToString();
	ServerControl = NewObject<UAPBServerControl>(this);
	ServerControl->Init(this);

	// DIAG (p17): catch any engine-level network/travel failure on the world server that
	// could request engine exit ~3-5s after the sole remote client disconnects.
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddWeakLambda(this, [](UWorld*, UNetDriver*, ENetworkFailure::Type FailureType, const FString& Msg)
		{
			UE_LOG(LogTemp, Warning, TEXT("WS_NET_FAILURE type=%d msg=%s"), (int32)FailureType, *Msg);
		});
		GEngine->OnTravelFailure().AddWeakLambda(this, [](UWorld*, ETravelFailure::Type FailureType, const FString& Msg)
		{
			UE_LOG(LogTemp, Warning, TEXT("WS_TRAVEL_FAILURE type=%d msg=%s"), (int32)FailureType, *Msg);
		});
	}
}

void AAPBWorldGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ServerControl)
	{
		ServerControl->Shutdown();
	}
	if (AuthExecutor)
	{
		AuthExecutor->Shutdown();
	}
	SocialAuthority.SaveSocialNow();
	SocialAuthority.SaveAuctionNow();
	Super::EndPlay(EndPlayReason);
}

void AAPBWorldGameMode::BeginDestroy()
{
	if (ServerControl)
	{
		ServerControl->Shutdown();
	}
	if (AuthExecutor)
	{
		AuthExecutor->Shutdown();
	}
	Super::BeginDestroy();
}

void AAPBWorldGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

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

	uint64 Key = 0;
	if (AAPBPlayerController* APBPC = Cast<AAPBPlayerController>(NewPlayer))
	{
		if (APBPC->PlayerSessionId == 0)
		{
			static uint64 NextSessionId = 1;
			APBPC->PlayerSessionId = NextSessionId++;
		}
		Key = APBPC->PlayerSessionId;
	}

	if (Key != 0 && !PlayerServices.Contains(Key))
	{
		auto Entry = MakeUnique<FAPBPlayerService>();
		Entry->Service = MakeUnique<apb::WorldService>();
		Entry->Service->InitFromDataDir(TCHAR_TO_UTF8(*DataDir));
		Entry->Service->InitPersistence(TCHAR_TO_UTF8(*PersistDir));
		// M12 single-writer: the authority owns auction.json; this connection's copy
		// is read-only so its SaveAllNow at Logout cannot clobber authority listings.
		Entry->Service->auction_write_enabled = false;
		PlayerServices.Add(Key, MoveTemp(Entry));
	}
}

void AAPBWorldGameMode::Logout(AController* Exiting)
{
	const double LogoutT0 = FPlatformTime::Seconds();
	if (AAPBPlayerController* PC = Cast<AAPBPlayerController>(Exiting))
	{
		const uint64 Key = PC->PlayerSessionId;
		UE_LOG(LogTemp, Warning, TEXT("LOGOUT_BEGIN session=%llu"), Key);
		if (TUniquePtr<FAPBPlayerService>* Found = PlayerServices.Find(Key))
		{
			if (*Found && (*Found)->Service) (*Found)->Service->SaveAllNow();
		}
		UE_LOG(LogTemp, Warning, TEXT("LOGOUT_AFTER_SAVE dt=%.3f"), FPlatformTime::Seconds() - LogoutT0);
		bool bTravelReservationActive = false;
		for (const TPair<FString, FTravelReservation>& Entry : TravelReservations)
		{
			if (Entry.Value.OwnerSessionId == Key)
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
			if (Entry.Value.OwnerSessionId == Key)
			{
				CancelTravelDomainReservation(Entry.Value);
			}
		}
		PlayerServices.Remove(Key);
		UE_LOG(LogTemp, Warning, TEXT("LOGOUT_AFTER_RESV dt=%.3f"), FPlatformTime::Seconds() - LogoutT0);
	}
	Super::Logout(Exiting);
	UE_LOG(LogTemp, Warning, TEXT("LOGOUT_END dt=%.3f"), FPlatformTime::Seconds() - LogoutT0);
}

FAPBPlayerService* AAPBWorldGameMode::ServiceFor(APlayerController* PC) const
{
	if (AAPBPlayerController* APBPC = Cast<AAPBPlayerController>(PC))
	{
		const uint64 Key = APBPC->PlayerSessionId;
		const TUniquePtr<FAPBPlayerService>* Found = PlayerServices.Find(Key);
		return Found ? Found->Get() : nullptr;
	}
	return nullptr;
}

bool AAPBWorldGameMode::LoginPlayer(APlayerController* PC,
                                    const FString& User, const FString& Pass,
                                    FString& OutError)
{
	if (PC && PC->NetConnection && !PC->NetConnection->IsEncryptionEnabled()
	    && !FParse::Param(FCommandLine::Get(), TEXT("DisableEncryption")))
	{
		OutError = TEXT("auth_refused_plaintext");
		UE_LOG(LogTemp, Warning, TEXT("LOGIN_REFUSED reason=auth_refused_plaintext"));
		return false;
	}

	FAPBPlayerService* Svc = ServiceFor(PC);
	if (!Svc || !Svc->Service) { OutError = TEXT("no_service"); return false; }
	
	if (!AuthExecutor) { OutError = TEXT("no_executor"); return false; }
	
	const uint32_t ConnectionId = PC->GetUniqueID();
	if (!AuthExecutor->CheckRateLimit(ConnectionId)) {
		OutError = TEXT("rate_limited");
		return false;
	}

	const std::string U = TCHAR_TO_UTF8(*User);
	const std::string P = TCHAR_TO_UTF8(*Pass);
	
	apb::AccountRecord record_snapshot;
	if (Svc->Service->login.accounts.count(U)) {
		record_snapshot = Svc->Service->login.accounts[U];
	} else {
		record_snapshot.account_id = "ACC-" + U;
		record_snapshot.username = U;
		record_snapshot.password_scheme = "pbkdf2";
		record_snapshot.password_iterations = 600000;
		record_snapshot.password_dk_bytes = 32;
		record_snapshot.password_salt = apb::random_hex(16);
	}
	
	Svc->GenerationNonce++;
	const uint32_t Nonce = Svc->GenerationNonce;
	
	TWeakObjectPtr<AAPBWorldGameMode> WeakThis(this);
	TWeakObjectPtr<AAPBPlayerController> WeakPC(Cast<AAPBPlayerController>(PC));
	uint64 SessionId = WeakPC.IsValid() ? WeakPC->PlayerSessionId : 0;
	
	auto Callback = [WeakThis, WeakPC, SessionId, U](uint32_t CId, const std::string& Username, uint32_t ReturnedNonce, apb::AuthResult Result, const apb::AccountRecord& Record)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, WeakPC, SessionId, U, ReturnedNonce, Result, Record]() {
			AAPBWorldGameMode* GM = WeakThis.Get();
			if (!GM) { UE_LOG(LogTemp, Warning, TEXT("CB_FAIL_GM")); return; }
			
			AAPBPlayerController* PC = WeakPC.Get();
			if (!PC || PC->PlayerSessionId != SessionId) { UE_LOG(LogTemp, Warning, TEXT("CB_FAIL_PC")); return; }
			
			FAPBPlayerService* Svc = GM->ServiceFor(PC);
			if (!Svc || !Svc->Service || Svc->GenerationNonce != ReturnedNonce) { UE_LOG(LogTemp, Warning, TEXT("CB_FAIL_SVC")); return; }
			
			UE_LOG(LogTemp, Warning, TEXT("CB_RESULT=%d"), (int32)Result); if (Result == apb::AuthResult::Rejected) {
				return;
			}
			
			if (Result == apb::AuthResult::Registered || Result == apb::AuthResult::AuthenticatedNeedsRehash) {
				Svc->Service->login.accounts[U] = Record;
				if (Svc->Service->store.IsActive()) {
					Svc->Service->store.SaveAccounts(Svc->Service->login);
				}
			}
			
			// Enforce one session per account
			for (const TPair<uint64, TUniquePtr<FAPBPlayerService>>& Entry : GM->PlayerServices) {
				if (Entry.Value && Entry.Value->Service && Entry.Key != SessionId) {
					if (Entry.Value->Service->login.IsLoggedIn() && Entry.Value->Service->login.session->username == U) {
						UE_LOG(LogTemp, Warning, TEXT("ACCOUNT_IN_USE_KICK username=%s"), UTF8_TO_TCHAR(U.c_str()));
						Entry.Value->Service->LogoutAccount();
					}
				}
			}
			
			// Adopt via the Domain so LoginAccount's post-verify bookkeeping runs —
			// critically TryLoadPersistedCharacter(), otherwise a returning account gets a
			// fabricated default character that clobbers its persisted one at Logout.
			Svc->Service->AdoptAuthenticatedSession(Record);
			
			if (!Svc->Service->character.has_value()) {
				Svc->Service->CreateCharacter(U, apb::Faction::Criminal);
			}
			FString Identity = UTF8_TO_TCHAR(U.c_str());
			FString FactionName = TEXT("Criminal");
			if (Svc->Service->character.has_value()) {
				Identity = UTF8_TO_TCHAR(Svc->Service->character->name.c_str());
				FactionName = (Svc->Service->character->faction == apb::Faction::Enforcer) ? TEXT("Enforcer") : TEXT("Criminal");
			}
			if (PC->PlayerState) {
				PC->PlayerState->SetPlayerName(Identity);
			}
			if (!GM->FindAdmittedPlayer(Identity)) {
				FAPBAdmittedPlayer Entry;
				Entry.Account = UTF8_TO_TCHAR(U.c_str());
				Entry.Character = Identity;
				Entry.Faction = FactionName;
				Entry.Jti = TEXT("direct");
				Entry.DistrictNumericId = 0;
				Entry.bAdmitted = true;
				GM->AdmittedRoster.Add(Identity, MoveTemp(Entry));
			}
			GM->SocialAuthority.friends_svc.SetOnline(TCHAR_TO_UTF8(*Identity), true);
			UE_LOG(LogTemp, Log, TEXT("SOCIAL_DIRECT_BIND character=%s account=%s faction=%s"), *Identity, UTF8_TO_TCHAR(U.c_str()), *FactionName);
			GM->PushSocialStateToPlayerStates();
			
			if (AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>()) {
				UE_LOG(LogTemp, Warning, TEXT("SETTING_BWORLDAUTHOK"));
				PS->bWorldAuthOk = true;
				PS->ForceNetUpdate();
			} else {
				UE_LOG(LogTemp, Warning, TEXT("PS_IS_NULL"));
			}
		});
	};
	
	if (!AuthExecutor->DispatchAuth(ConnectionId, U, P, record_snapshot, Nonce, Callback)) {
		OutError = TEXT("auth_queue_full");
		return false;
	}
	
	OutError = TEXT("pending");
	return false;
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
	FAuthenticatedPlayer Auth = RequireAuthenticatedPlayer(PC);
	if (!Auth.IsValid()) return TEXT("{\"error\":\"no_ticket\"}");
	if (Auth.Character != CharName)
	{
		UE_LOG(LogTemp, Warning, TEXT("TICKET_ISSUE_REJECTED character_mismatch requested=%s auth=%s"), *CharName, *Auth.Character);
		return TEXT("{\"error\":\"character_unavailable\"}");
	}
	FString Host;
	FString ReservationId;
	FString Error;
	int32 Port = 0;
	int32 NumericId = 0;
	uint64 PlayerSessionId = 0;
	if (AAPBPlayerController* APBPC = Cast<AAPBPlayerController>(PC))
	{
		PlayerSessionId = APBPC->PlayerSessionId;
	}
	TArray<FString> PreviousReservations;
	for (const TPair<FString, FTravelReservation>& Entry : TravelReservations)
	{
		if (Entry.Value.OwnerSessionId == PlayerSessionId)
		{
			PreviousReservations.Add(Entry.Key);
		}
	}
	for (const FString& PreviousReservationId : PreviousReservations)
	{
		ReleaseTravelReservationById(PreviousReservationId);
	}
	apb::WorldService* WS = static_cast<apb::WorldService*>(Auth.Service);
	if (!WS || !ServerControl) return TEXT("{\"error\":\"no_live_node\"}");
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
	FString TargetDistrictEpoch;
	if (!ServerControl->ResolveLiveDistrict(DistrictId, CatalogDistrict->max_players, Host, Port,
		NumericId, TargetDistrictEpoch, Error))
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
	if (!ServerControl->ReserveLiveDistrict(PlayerSessionId, DistrictId, CatalogDistrict->max_players, Host, Port,
		NumericId, TargetDistrictEpoch, ReservationId, Error))
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
	Claims.issuer_world_epoch = TCHAR_TO_UTF8(*GetWorldEpoch());
	Claims.target_district_epoch = TCHAR_TO_UTF8(*TargetDistrictEpoch);
	std::string Token = TicketSvc->IssueTicket(Claims);
	if (Token.empty())
	{
		WS->CancelDistrictReservation(WS->character->name);
		ServerControl->ReleaseLiveDistrictReservation(ReservationId);
		return TEXT("{\"error\":\"no_ticket\"}");
	}
	apb::TicketClaims VerifiedClaims;
	if (!TicketSvc->VerifyAndConsume(Token, VerifiedClaims))
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
	Reservation.OwnerSessionId = PlayerSessionId;
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
		UTF8_TO_TCHAR(Handoff.account.c_str()), UTF8_TO_TCHAR(Handoff.jti.c_str()), UTF8_TO_TCHAR(Handoff.nonce.c_str()), UTF8_TO_TCHAR(Handoff.faction.c_str()), Handoff.snapshot.cash,
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
	if (TUniquePtr<FAPBPlayerService>* Service = PlayerServices.Find(Reservation.OwnerSessionId))
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
			if (AAPBPlayerController* APBPC = Cast<AAPBPlayerController>(PC))
			{
				if (Reservation->OwnerSessionId == APBPC->PlayerSessionId)
				{
					ReleaseTravelReservationById(ReservationId);
				}
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
	if (AuthExecutor)
	{
		AuthExecutor->TickRateLimits(DeltaSeconds);
	}
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
	// M12: the authority settles expired listings on a 5s cadence. Settlement
	// delivers item/cash via SocialAuthority.mail, so persist both stores and
	// refresh replicated mail badges when anything settled.
	if (NowMs >= NextAuctionSettleMs)
	{
		NextAuctionSettleMs = NowMs + 5000;
		const int32 Settled = SocialAuthority.auction.SettleExpired(NowMs / 1000);
		if (Settled > 0)
		{
			SocialAuthority.SaveAuctionNow();
			SocialAuthority.SaveSocialNow();
			PushSocialStateToPlayerStates();
			UE_LOG(LogTemp, Log, TEXT("AUCTION_SETTLED count=%d"), Settled);
		}
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
	FAuthenticatedPlayer AuthActor;
	AuthActor.Character = Character;

	// Clan ops
	if (Op == TEXT("clan.create"))
	{
		Status = APB->SocialClanCreate(Arg1, Arg1, Arg2, AuthActor, false) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("clan.invite"))
	{
		Status = APB->SocialClanInvite(AuthActor, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("clan.accept"))
	{
		Status = APB->SocialClanAcceptInvite(AuthActor) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("clan.decline"))
	{
		Status = APB->SocialClanDeclineInvite(AuthActor) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("clan.leave"))
	{
		Status = APB->SocialClanLeave(AuthActor) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("clan.disband"))
	{
		Status = APB->SocialClanDisband(AuthActor) ? TEXT("ok") : TEXT("domain_rejected");
	}
	// Friend ops
	else if (Op == TEXT("friend.request"))
	{
		Status = APB->SocialFriendRequest(AuthActor, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("friend.accept"))
	{
		Status = APB->SocialFriendAccept(AuthActor, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("friend.decline"))
	{
		Status = APB->SocialFriendDecline(AuthActor, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("friend.remove"))
	{
		Status = APB->SocialFriendRemove(AuthActor, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("friend.ignore"))
	{
		Status = APB->SocialFriendIgnore(AuthActor, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("friend.unignore"))
	{
		Status = APB->SocialFriendUnignore(AuthActor, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	// Group ops
	else if (Op == TEXT("group.create"))
	{
		FString OutId;
		Status = APB->SocialGroupCreate(AuthActor, OutId) ? FString::Printf(TEXT("ok:%s"), *OutId) : TEXT("domain_rejected");
	}
	else if (Op == TEXT("group.invite"))
	{
		Status = APB->SocialGroupInvite(AuthActor, Arg1) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("group.accept"))
	{
		Status = APB->SocialGroupAccept(AuthActor) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("group.leave"))
	{
		Status = APB->SocialGroupLeave(AuthActor) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("group.setready"))
	{
		Status = APB->SocialGroupSetReady(AuthActor, Arg1 == TEXT("1") || Arg1 == TEXT("true")) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("group.disband"))
	{
		Status = APB->SocialGroupDisband(AuthActor) ? TEXT("ok") : TEXT("domain_rejected");
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
		Status = APB->SocialMailSend(AuthActor, Arg1, Subject, BodyText, 0) ? TEXT("ok") : TEXT("domain_rejected");
	}
	else if (Op == TEXT("mail.markread"))
	{
		Status = APB->SocialMailMarkRead(AuthActor, Arg1) == EAPBMailResult::Ok ? TEXT("ok") : TEXT("domain_rejected");
	}
	// Auction ops (M12) — same shapes as DispatchSocialOpDirect so a district player
	// gets identical semantics to a lobby player. Fails closed when the character has
	// no live service on this process (the owning session lives here on the world).
	else if (Op == TEXT("auction.list"))
	{
		// Arg1 = "ItemId|Qty|Buyout|StartPrice|DurationSec"
		TArray<FString> Parts;
		Arg1.ParseIntoArray(Parts, TEXT("|"), true);
		if (Parts.Num() < 2)
		{
			Status = TEXT("bad_args");
		}
		else
		{
			const int64 BuyoutPrice = Parts.Num() >= 3 ? FCString::Atoi64(*Parts[2]) : 0;
			const int64 StartPrice  = Parts.Num() >= 4 ? FCString::Atoi64(*Parts[3]) : 0;
			const int32 DurationSec = Parts.Num() >= 5 ? FCString::Atoi(*Parts[4]) : 0;
			int64 OutId = 0; FString Err;
			Status = APB->AuctionListItemAuth(AuthActor, Parts[0], FCString::Atoi(*Parts[1]),
					BuyoutPrice, StartPrice, DurationSec, OutId, Err)
				? FString::Printf(TEXT("ok:%lld"), static_cast<long long>(OutId))
				: (TEXT("domain_rejected:") + Err);
		}
	}
	else if (Op == TEXT("auction.bid"))
	{
		FString Err;
		Status = APB->AuctionBid(AuthActor, FCString::Atoi64(*Arg1), FCString::Atoi64(*Arg2), Err)
			? TEXT("ok") : (TEXT("domain_rejected:") + Err);
	}
	else if (Op == TEXT("auction.buyout"))
	{
		FString Err;
		Status = APB->AuctionBuyout(AuthActor, FCString::Atoi64(*Arg1), Err)
			? TEXT("ok") : (TEXT("domain_rejected:") + Err);
	}
	else if (Op == TEXT("auction.cancel"))
	{
		FString Err;
		Status = APB->AuctionCancelListing(AuthActor, FCString::Atoi64(*Arg1), Err)
			? TEXT("ok") : (TEXT("domain_rejected:") + Err);
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

FAuthenticatedPlayer AAPBWorldGameMode::RequireAuthenticatedPlayer(APlayerController* PC) const
{
	FAuthenticatedPlayer Auth;
	if (!PC) return Auth;
	if (AAPBPlayerController* APC = Cast<AAPBPlayerController>(PC))
	{
		Auth.SessionId = APC->PlayerSessionId;
		if (const TUniquePtr<FAPBPlayerService>* SvcPtr = PlayerServices.Find(Auth.SessionId))
		{
			if (*SvcPtr && (*SvcPtr)->Service)
			{
				Auth.Service = (*SvcPtr)->Service.Get();
				if (apb::WorldService* WS = static_cast<apb::WorldService*>(Auth.Service))
				{
					if (WS->character.has_value() && !WS->character->name.empty())
					{
						Auth.Character = UTF8_TO_TCHAR(WS->character->name.c_str());
					}
				}
			}
		}
	}
	return Auth;
}
