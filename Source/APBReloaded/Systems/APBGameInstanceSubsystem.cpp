#include "APBGameInstanceSubsystem.h"
#include "APBPlayerState.h"
#include "APBWorldService.h"
#include "APBPrivateServerOpcodes.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

static apb::WorldService* Svc(void* P) { return reinterpret_cast<apb::WorldService*>(P); }
static const apb::WorldService* SvcC(const void* P) { return reinterpret_cast<const apb::WorldService*>(P); }

bool UAPBGameInstanceSubsystem::CanMutateDomain() const
{
	// Domain mutations are server-authoritative (standalone, listen host, dedicated).
	// Clients observe via replicated PlayerState only.
	if (const UWorld* World = GetWorld())
	{
		const ENetMode Mode = World->GetNetMode();
		if (Mode == NM_Client) return false;
	}
	return true;
}

void UAPBGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Service = new apb::WorldService();
	DataDir = FPaths::ProjectContentDir() / TEXT("Data");
	InitCatalogFromProjectData();

	// Default FPS lock 60 (config t.MaxFPS + GameUserSettings; F8 debug can change)
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		if (CVar->GetFloat() <= 0.f)
		{
			CVar->Set(60.f, ECVF_SetByCode);
		}
	}
	if (GEngine)
	{
		if (GEngine->GetMaxFPS() <= 0.f)
		{
			GEngine->SetMaxFPS(60.f);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("APB FPS default lock t.MaxFPS (prefer 60)"));
}

void UAPBGameInstanceSubsystem::Deinitialize()
{
	if (Service)
	{
		delete Svc(Service);
		Service = nullptr;
	}
	Super::Deinitialize();
}

bool UAPBGameInstanceSubsystem::InitCatalogFromProjectData()
{
	if (!Service) return false;
	const bool Ok = Svc(Service)->InitFromDataDir(TCHAR_TO_UTF8(*DataDir));
	// Steam-derived Content/Data (districts/catalogs from apbdb + placement JSON).
	const int32 DistrictN = Svc(Service)->ListDistricts().size();
	UE_LOG(LogTemp, Warning,
		TEXT("APB STEAM_DERIVED catalog_load ok=%d data_dir=%s districts=%d (Content/Data + placements)"),
		Ok ? 1 : 0, *DataDir, DistrictN);
	UE_LOG(LogTemp, Warning,
		TEXT("APB PRIVATESERVER_MAP lobby_ASK_LOGIN=0x%X world_ASK_DISTRICT_ENTER=0x%X (from ApbPrivateServer opcodes)"),
		static_cast<uint32>(apb::privateserver::lobby::Opcode::ASK_LOGIN),
		static_cast<uint32>(apb::privateserver::world::Opcode::ASK_DISTRICT_ENTER));
	return Ok;
}

bool UAPBGameInstanceSubsystem::CreateCharacter(const FString& Name, bool bEnforcer)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->CreateCharacter(TCHAR_TO_UTF8(*Name),
		bEnforcer ? apb::Faction::Enforcer : apb::Faction::Criminal);
}

TArray<FString> UAPBGameInstanceSubsystem::GetDistrictList() const
{
	TArray<FString> Out;
	if (!Service) return Out;
	for (const auto& d : SvcC(Service)->ListDistricts())
	{
		Out.Add(FString::Printf(TEXT("%s|%s|%s"), UTF8_TO_TCHAR(d.id.c_str()), UTF8_TO_TCHAR(d.name.c_str()), UTF8_TO_TCHAR(d.map_name.c_str())));
	}
	return Out;
}

bool UAPBGameInstanceSubsystem::JoinDistrict(const FString& DistrictId)
{
	if (!Service || !CanMutateDomain() || !Svc(Service)->character) return false;
	return Svc(Service)->JoinDistrict(TCHAR_TO_UTF8(*DistrictId), Svc(Service)->character->name);
}

bool UAPBGameInstanceSubsystem::JoinDistrictAsPeer(const FString& SessionId, const FString& PlayerName)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->JoinDistrictAsPeer(TCHAR_TO_UTF8(*SessionId), TCHAR_TO_UTF8(*PlayerName));
}

FString UAPBGameInstanceSubsystem::GetSessionId() const
{
	if (!Service || !SvcC(Service)->district) return FString();
	return UTF8_TO_TCHAR(SvcC(Service)->district->session_id.c_str());
}

FString UAPBGameInstanceSubsystem::GetPhase() const
{
	if (!Service) return TEXT("None");
	switch (SvcC(Service)->phase)
	{
	case apb::SessionPhase::Boot: return TEXT("Boot");
	case apb::SessionPhase::LoggedIn: return TEXT("LoggedIn");
	case apb::SessionPhase::WorldLobby: return TEXT("WorldLobby");
	case apb::SessionPhase::District: return TEXT("District");
	}
	return TEXT("Unknown");
}

bool UAPBGameInstanceSubsystem::ArmasPurchase(const FString& ItemId, FString& OutError)
{
	if (!Service || !CanMutateDomain()) { OutError = TEXT("no_service_or_client"); return false; }
	auto r = Svc(Service)->ArmasBuy(TCHAR_TO_UTF8(*ItemId));
	OutError = UTF8_TO_TCHAR(r.error.c_str());
	return r.ok;
}

bool UAPBGameInstanceSubsystem::AuctionListItem(const FString& ItemId, int32 Qty, int64 Price, int64& OutListingId, FString& OutError)
{
	if (!Service || !CanMutateDomain()) { OutError = TEXT("no_service_or_client"); return false; }
	auto r = Svc(Service)->AuctionList(TCHAR_TO_UTF8(*ItemId), Qty, Price);
	OutListingId = r.listing_id;
	OutError = UTF8_TO_TCHAR(r.error.c_str());
	return r.ok;
}

void UAPBGameInstanceSubsystem::StartOppositionMission()
{
	if (Service && CanMutateDomain()) Svc(Service)->StartMission();
}

bool UAPBGameInstanceSubsystem::AdvanceMissionStage()
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->AdvanceMission(1.0);
}

float UAPBGameInstanceSubsystem::GetThreatPoints() const
{
	if (!Service) return 0.f;
	return static_cast<float>(SvcC(Service)->threat.points);
}

int32 UAPBGameInstanceSubsystem::GetThreatBotCount() const
{
	if (!Service) return 0;
	return SvcC(Service)->threat.CurrentTier().bot_count;
}

bool UAPBGameInstanceSubsystem::Login(const FString& User, const FString& Pass)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->LoginAccount(TCHAR_TO_UTF8(*User), TCHAR_TO_UTF8(*Pass));
}

bool UAPBGameInstanceSubsystem::RegisterAccount(const FString& User, const FString& Pass)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->RegisterAccount(TCHAR_TO_UTF8(*User), TCHAR_TO_UTF8(*Pass));
}

bool UAPBGameInstanceSubsystem::EnterWorld(const FString& WorldId)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->EnterWorld(TCHAR_TO_UTF8(*WorldId));
}

bool UAPBGameInstanceSubsystem::SpawnCatalogVehicle(const FString& VehicleId)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->SpawnVehicle(TCHAR_TO_UTF8(*VehicleId));
}

bool UAPBGameInstanceSubsystem::PossessCatalogVehicle()
{
	if (!Service || !CanMutateDomain() || !Svc(Service)->character) return false;
	return Svc(Service)->PossessVehicle(Svc(Service)->character->name);
}

TArray<FString> UAPBGameInstanceSubsystem::GetStreamChunksNear(float X, float Y) const
{
	TArray<FString> Out;
	if (!Service) return Out;
	for (const auto& id : SvcC(Service)->StreamChunksNear(X, Y))
	{
		Out.Add(UTF8_TO_TCHAR(id.c_str()));
	}
	return Out;
}

float UAPBGameInstanceSubsystem::GetOppositionPressure() const
{
	if (!Service) return 1.f;
	return static_cast<float>(SvcC(Service)->OppositionPressure());
}

float UAPBGameInstanceSubsystem::FireCatalogWeapon(const FString& WeaponId, float AimX, float AimY, float& OutTargetHealth, bool& bKilled)
{
	OutTargetHealth = 0.f;
	bKilled = false;
	if (!Service || !CanMutateDomain() || !Svc(Service)->character) return 0.f;
	apb::WorldService* W = Svc(Service);
	std::string wid = TCHAR_TO_UTF8(*WeaponId);
	if (wid.empty())
	{
		for (const auto& kv : W->catalog.items)
		{
			if (kv.second.category == "Weapon") { wid = kv.first; break; }
		}
	}
	// Persist opposition target across shots for kill/threat path
	static apb::CombatantState Target{ "Opposition", apb::Faction::Enforcer, 200, 2, 0, true };
	if (!Target.alive || Target.health <= 0)
	{
		Target = apb::CombatantState{ "Opposition", apb::Opposing(W->character->faction), 200, 2, 0, true };
	}
	apb::CombatantState shooter{ W->character->name, W->character->faction, 1000, 0, 0, true };
	auto shot = W->FireWeapon(wid, shooter, Target, AimX, AimY);
	OutTargetHealth = static_cast<float>(Target.health);
	bKilled = shot.killed || !Target.alive;
	return static_cast<float>(shot.damage);
}

bool UAPBGameInstanceSubsystem::EquipClothingItem(const FString& Slot, const FString& ItemId)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->EquipClothing(TCHAR_TO_UTF8(*Slot), TCHAR_TO_UTF8(*ItemId));
}

bool UAPBGameInstanceSubsystem::ApplyBodyProfile(float Height, float Bulk, int32 SkinTone, int32 FacePreset)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* W = Svc(Service);
	if (!W->character) return false;
	apb::CharacterAppearance App = W->appearance;
	App.body.height = FMath::Clamp(Height, 0.8f, 1.2f);
	App.body.bulk = FMath::Clamp(Bulk, 0.8f, 1.2f);
	// Map UI 0.8–1.2 bulk into Domain 0–1 style bulk if needed; store as given for fidelity
	App.body.skin_tone = SkinTone;
	App.body.face_preset = FacePreset;
	const bool bOk = W->ApplyAppearance(App);
	if (bOk)
	{
		UE_LOG(LogTemp, Log, TEXT("APB BODY height=%.3f bulk=%.3f skin=%d face=%d"),
			App.body.height, App.body.bulk, SkinTone, FacePreset);
	}
	return bOk;
}

bool UAPBGameInstanceSubsystem::GetBodyProfile(float& OutHeight, float& OutBulk) const
{
	OutHeight = 1.f;
	OutBulk = 1.f;
	if (!Service) return false;
	const apb::WorldService* W = SvcC(Service);
	if (!W->character) return false;
	OutHeight = W->appearance.body.height;
	OutBulk = W->appearance.body.bulk;
	return true;
}

TArray<FAPBClothingChoice> UAPBGameInstanceSubsystem::GetClothingForSlot(const FString& Slot, int32 MaxItems) const
{
	TArray<FAPBClothingChoice> Out;
	const FString Path = DataDir / TEXT("clothing.json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) return Out;
	// Lightweight scan: objects with "slot":"<Slot>"
	const FString SlotKey = FString::Printf(TEXT("\"slot\": \"%s\""), *Slot.ToLower());
	const FString SlotKey2 = FString::Printf(TEXT("\"slot\":\"%s\""), *Slot.ToLower());
	int32 SearchFrom = 0;
	while (Out.Num() < MaxItems)
	{
		int32 Hit = Text.Find(SlotKey, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
		if (Hit == INDEX_NONE) Hit = Text.Find(SlotKey2, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
		if (Hit == INDEX_NONE) break;
		// walk back to nearest '{'
		int32 ObjStart = Text.Find(TEXT("{"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, Hit);
		int32 ObjEnd = Text.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Hit);
		if (ObjStart == INDEX_NONE || ObjEnd == INDEX_NONE) { SearchFrom = Hit + 1; continue; }
		const FString Obj = Text.Mid(ObjStart, ObjEnd - ObjStart + 1);
		auto Grab = [&](const TCHAR* Key) -> FString
		{
			const FString K1 = FString::Printf(TEXT("\"%s\": \""), Key);
			const FString K2 = FString::Printf(TEXT("\"%s\":\""), Key);
			int32 P = Obj.Find(K1, ESearchCase::IgnoreCase);
			int32 KeyLen = K1.Len();
			if (P == INDEX_NONE) { P = Obj.Find(K2, ESearchCase::IgnoreCase); KeyLen = K2.Len(); }
			if (P == INDEX_NONE) return FString();
			P += KeyLen;
			int32 E = Obj.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, P);
			return E == INDEX_NONE ? FString() : Obj.Mid(P, E - P);
		};
		FAPBClothingChoice C;
		C.Id = Grab(TEXT("id"));
		C.Name = Grab(TEXT("name"));
		C.Slot = Slot;
		if (!C.Id.IsEmpty()) Out.Add(C);
		SearchFrom = ObjEnd + 1;
	}
	// Fallbacks for empty slots so editor always has choices
	if (Out.Num() == 0)
	{
		FAPBClothingChoice C;
		C.Slot = Slot;
		C.Id = FString::Printf(TEXT("Clothing_%s_Default"), *Slot);
		C.Name = FString::Printf(TEXT("Default %s"), *Slot);
		Out.Add(C);
	}
	return Out;
}

FString UAPBGameInstanceSubsystem::GetDistrictMapName(const FString& DistrictId) const
{
	if (!Service) return TEXT("Lvl_APB_Financial_Freeroam");
	for (const auto& d : SvcC(Service)->ListDistricts())
	{
		if (DistrictId.Equals(UTF8_TO_TCHAR(d.id.c_str()), ESearchCase::IgnoreCase))
		{
			return UTF8_TO_TCHAR(d.map_name.c_str());
		}
	}
	return TEXT("Lvl_APB_Financial_Freeroam");
}

int32 UAPBGameInstanceSubsystem::GetDistrictPlayerCount() const
{
	if (!Service || !SvcC(Service)->district) return 0;
	return static_cast<int32>(SvcC(Service)->district->players.size());
}

FAPBDomainSnapshotUE UAPBGameInstanceSubsystem::CaptureDomainSnapshot() const
{
	FAPBDomainSnapshotUE U;
	if (!Service) return U;
	const apb::DomainSnapshot S = SvcC(Service)->CaptureSnapshot();
	U.bHasCharacter = S.has_character;
	U.CharacterName = UTF8_TO_TCHAR(S.character_name.c_str());
	U.bEnforcer = S.faction == apb::Faction::Enforcer;
	U.Cash = S.cash;
	U.G1C = S.g1c;
	U.InventorySlotCount = S.inventory_slot_count;
	U.InventoryTotalQty = S.inventory_total_qty;
	U.ThreatPoints = static_cast<float>(S.threat_points);
	U.ThreatBots = S.threat_bots;
	U.MissionId = UTF8_TO_TCHAR(S.mission_id.c_str());
	U.MissionTitle = UTF8_TO_TCHAR(S.mission_title.c_str());
	U.MissionStageIndex = S.mission_stage_index;
	U.MissionStageCount = S.mission_stage_count;
	U.MissionStatus = UTF8_TO_TCHAR(S.mission_status.c_str());
	U.SessionId = UTF8_TO_TCHAR(S.session_id.c_str());
	U.DistrictId = UTF8_TO_TCHAR(S.district_id.c_str());
	U.DistrictPlayers = S.district_players;
	return U;
}

int32 UAPBGameInstanceSubsystem::GetInventorySlotCount() const
{
	return CaptureDomainSnapshot().InventorySlotCount;
}

int64 UAPBGameInstanceSubsystem::GetCharacterCash() const
{
	return CaptureDomainSnapshot().Cash;
}

int64 UAPBGameInstanceSubsystem::GetCharacterG1C() const
{
	return CaptureDomainSnapshot().G1C;
}

FString UAPBGameInstanceSubsystem::GetMissionTitle() const
{
	return CaptureDomainSnapshot().MissionTitle;
}

int32 UAPBGameInstanceSubsystem::GetMissionStageIndex() const
{
	return CaptureDomainSnapshot().MissionStageIndex;
}

int32 UAPBGameInstanceSubsystem::GetMissionStageCount() const
{
	return CaptureDomainSnapshot().MissionStageCount;
}

void UAPBGameInstanceSubsystem::SyncPlayerStateFromDomain(AAPBPlayerState* PlayerState)
{
	if (!PlayerState || !PlayerState->HasAuthority()) return;
	const FAPBDomainSnapshotUE S = CaptureDomainSnapshot();
	PlayerState->ApplyFactionAuthority(S.bEnforcer ? EAPBFaction::Enforcer : EAPBFaction::Criminal);
	PlayerState->ApplyDomainSnapshot(
		S.ThreatPoints,
		S.Cash,
		S.G1C,
		S.InventorySlotCount,
		S.MissionTitle.IsEmpty() ? S.MissionStatus : S.MissionTitle,
		S.MissionStageIndex,
		S.MissionStageCount,
		S.SessionId);
}

void UAPBGameInstanceSubsystem::PushDomainSnapshotToAllPlayerStates()
{
	// Server / listen-host only: one Domain capture → all PlayerStates → OnRep on clients
	if (!CanMutateDomain()) return;
	UWorld* World = GetWorld();
	if (!World) return;
	const FAPBDomainSnapshotUE S = CaptureDomainSnapshot();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>())
			{
				if (!PS->HasAuthority()) continue;
				PS->ApplyFactionAuthority(S.bEnforcer ? EAPBFaction::Enforcer : EAPBFaction::Criminal);
				PS->ApplyDomainSnapshot(
					S.ThreatPoints,
					S.Cash,
					S.G1C,
					S.InventorySlotCount,
					S.MissionTitle.IsEmpty() ? S.MissionStatus : S.MissionTitle,
					S.MissionStageIndex,
					S.MissionStageCount,
					S.SessionId);
				PS->ForceNetUpdate();
			}
		}
	}
}


