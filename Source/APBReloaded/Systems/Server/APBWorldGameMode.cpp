#include "APBWorldGameMode.h"
#include "APBPlayerState.h"
#include "APBServerControl.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "APBSocial.h"
#include "APBTicket.h"

AAPBWorldGameMode::AAPBWorldGameMode()
{
	PlayerStateClass = AAPBPlayerState::StaticClass();
}

void AAPBWorldGameMode::BeginPlay()
{
	Super::BeginPlay();
	DataDir    = FPaths::ProjectContentDir() / TEXT("Data");
	PersistDir = FPaths::ProjectSavedDir()   / TEXT("DomainDB");
	ServerControl = NewObject<UAPBServerControl>(this);
	ServerControl->Init(this);
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
	bool ok = Svc->Service->LoginAccount(TCHAR_TO_UTF8(*User), TCHAR_TO_UTF8(*Pass));
	if (!ok) { OutError = TEXT("login_failed"); return false; }
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
	const auto& Districts = WS->catalog.districts;
	std::string Json = "[";
	bool First = true;
	for (const auto& D : Districts)
	{
		if (!First) Json += ",";
		First = false;
		Json += "{\"id\":\"" + D.id + "\",\"name\":\"" + D.name + "\"}";
	}
	Json += "]";
	return FString(UTF8_TO_TCHAR(Json.c_str()));
}

FString AAPBWorldGameMode::IssueTicketJson(APlayerController* PC,
                                           const FString& CharName,
                                           const FString& DistrictId)
{
	FAPBPlayerService* Svc = ServiceFor(PC);
	if (!Svc || !Svc->Service) return TEXT("{}");
	apb::WorldService* WS = Svc->Service.Get();
	if (!WS->login.IsLoggedIn() || !WS->character.has_value()) return TEXT("{}");
	apb::TicketClaims Claims;
	Claims.account   = WS->login.session->account_id;
	Claims.character = TCHAR_TO_UTF8(*CharName);
	Claims.faction   = (WS->character->faction == apb::Faction::Criminal)
	                   ? "Criminal" : "Enforcer";
	Claims.district  = TCHAR_TO_UTF8(*DistrictId);
	std::string Token = apb::TicketService::Global().IssueTicket(Claims);
	std::string Json = "{\"ticket\":\"" + Token + "\"}";
	return FString(UTF8_TO_TCHAR(Json.c_str()));
}
