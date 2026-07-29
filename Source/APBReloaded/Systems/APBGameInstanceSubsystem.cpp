#include "APBGameInstanceSubsystem.h"
#include "APBPlayerState.h"
#include "APBWorldService.h"
#include "APBPrivateServerOpcodes.h"
#include "APBTicket.h"
#include "Server/APBSecretProvider.h"
#include "Server/APBWorldGameMode.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "HAL/PlatformFilemanager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static apb::WorldService* Svc(void* P) { return reinterpret_cast<apb::WorldService*>(P); }
static const apb::WorldService* SvcC(const void* P) { return reinterpret_cast<const apb::WorldService*>(P); }

// Authority helper (plan 1b): social mutations must run against the single shared
// SocialAuthority on AAPBWorldGameMode, never against per-connection PlayerServices.
// Resolution order:
//   1. AAPBWorldGameMode present -> world authority, return its Social().
//   2. NM_Standalone (single-process editor-PIE / standalone world) -> fall back to
//      the local WorldService so probes and standalone flows continue to work.
//   3. All other modes (district listen/dedicated server, NM_Client, no World) ->
//      fail closed: return nullptr and emit a one-shot Warning so the district process
//      never silently mutates its own throwaway social state.
static apb::WorldService* SocialSvc(UWorld* World, void* Service)
{
	if (World)
	{
		if (AAPBWorldGameMode* GM = World->GetAuthGameMode<AAPBWorldGameMode>())
		{
			return &GM->Social();
		}
		if (World->GetNetMode() == NM_Standalone)
		{
			return Svc(Service);
		}
		// District server (listen/dedicated) or NM_Client: no social authority here.
		static bool bWarnedNoAuthority = false;
		if (!bWarnedNoAuthority)
		{
			bWarnedNoAuthority = true;
			UE_LOG(LogTemp, Warning,
				TEXT("SOCIAL_AUTHORITY_UNAVAILABLE netmode=%d ctx=district_process_no_world_gm"),
				static_cast<int32>(World->GetNetMode()));
		}
		return nullptr;
	}
	// No World at all (subsystem tear-down race, tool context, etc.).
	static bool bWarnedNoWorld = false;
	if (!bWarnedNoWorld)
	{
		bWarnedNoWorld = true;
		UE_LOG(LogTemp, Warning,
			TEXT("SOCIAL_AUTHORITY_UNAVAILABLE netmode=none ctx=no_world"));
	}
	return nullptr;
}

// Resolves the service whose character actually receives a mail payout. Mirrors
// SocialSvc's resolution order so standalone/PIE keeps using the local service,
// but on a world server it returns the per-connection service that owns the
// character rather than SocialAuthority, which never loads one.
static apb::WorldService* CharacterOwnerSvc(UWorld* World, void* Service, const FString& Character)
{
	if (World)
	{
		if (AAPBWorldGameMode* GM = World->GetAuthGameMode<AAPBWorldGameMode>())
		{
			if (apb::WorldService* Owner = GM->ServiceForCharacter(Character)) return Owner;
			if (World->GetNetMode() == NM_Standalone) return Svc(Service);
			return nullptr;
		}
		if (World->GetNetMode() == NM_Standalone) return Svc(Service);
	}
	return nullptr;
}

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
	PersistDir = FPaths::ProjectSavedDir() / TEXT("DomainDB");
	InitCatalogFromProjectData();
	DistrictTravelLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this,
		&UAPBGameInstanceSubsystem::HandleDistrictTravelMapLoaded);
	DistrictTravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this,
		&UAPBGameInstanceSubsystem::HandleDistrictTravelFailure);

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

	FString WSHost;
	if (FParse::Value(FCommandLine::Get(), TEXT("WorldServerHost="), WSHost) && !WSHost.IsEmpty())
	{
		bWorldServerMode = true;
		UE_LOG(LogTemp, Warning, TEXT("APBSubsystem world-server-client mode host=%s"), *WSHost);
	}

	const FString CommandLine(FCommandLine::Get());
	const FString MintKey(TEXT("APBMintTicket="));
	const int32 MintStart = CommandLine.Find(MintKey, ESearchCase::CaseSensitive);
	if (MintStart != INDEX_NONE)
	{
		const int32 ValueStart = MintStart + MintKey.Len();
		int32 ValueEnd = ValueStart;
		while (ValueEnd < CommandLine.Len() && !FChar::IsWhitespace(CommandLine[ValueEnd]))
		{
			++ValueEnd;
		}
		const FString MintRequest = CommandLine.Mid(ValueStart, ValueEnd - ValueStart);
		TArray<FString> Fields;
		MintRequest.ParseIntoArray(Fields, TEXT(","), true);
		if (Fields.Num() != 4)
		{
			UE_LOG(LogTemp, Error, TEXT("MINTED_TICKET_ERROR reason=invalid_request fields=%d"), Fields.Num());
			FPlatformMisc::RequestExit(false);
			return;
		}
		FString SecretError;
		if (!FAPBSecretProvider::Initialize(SecretError))
		{
			UE_LOG(LogTemp, Error, TEXT("MINTED_TICKET_ERROR reason=%s"), *SecretError);
			FPlatformMisc::RequestExit(false);
			return;
		}

		const FString& Secret = FAPBSecretProvider::TicketSecret();
		apb::TicketService::Global().SetSecret(TCHAR_TO_UTF8(*Secret));
		apb::TicketClaims Claims;
		Claims.account = TCHAR_TO_UTF8(*Fields[0]);
		Claims.character = TCHAR_TO_UTF8(*Fields[1]);
		Claims.faction = TCHAR_TO_UTF8(*Fields[2]);
		Claims.district = TCHAR_TO_UTF8(*Fields[3]);
		const std::string Token = apb::TicketService::Global().IssueTicket(Claims);
		UE_LOG(LogTemp, Log, TEXT("MINTED_TICKET=%s"), UTF8_TO_TCHAR(Token.c_str()));
		FPlatformMisc::RequestExit(false);
	}
}

void UAPBGameInstanceSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(DistrictTravelLoadedHandle);
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(DistrictTravelFailureHandle);
	}
	if (Service)
	{
		Svc(Service)->SaveAllNow(); // flush DomainDB on shutdown when persistence is active
		delete Svc(Service);
		Service = nullptr;
	}
	Super::Deinitialize();
}

bool UAPBGameInstanceSubsystem::InitCatalogFromProjectData()
{
	if (!Service) return false;
	const bool Ok = Svc(Service)->InitFromDataDir(TCHAR_TO_UTF8(*DataDir));
	const bool PersistOk = Svc(Service)->InitPersistence(TCHAR_TO_UTF8(*PersistDir));
	UE_LOG(LogTemp, Warning, TEXT("APB persistence init ok=%d dir=%s (Saved/DomainDB JSON)"),
		PersistOk ? 1 : 0, *PersistDir);
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

bool UAPBGameInstanceSubsystem::TickMission(float NowSec)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->TickMission(static_cast<double>(NowSec));
}

bool UAPBGameInstanceSubsystem::AdvanceOpposition(float Amount)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->AdvanceOpposition(static_cast<double>(Amount));
}

bool UAPBGameInstanceSubsystem::IsMissionActive() const
{
	if (!Service) return false;
	const auto& M = SvcC(Service)->mission;
	return M.has_value() && M->status == apb::MissionStatus::Active;
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
	if (!Service) return false;
	const UWorld* World = GetWorld();
	if (bWorldServerMode && World && World->GetNetMode() == NM_Client)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>())
				{
					PS->Server_LoginRequest(User, Pass);
					return true;
				}
			}
		}
		return false;
	}
	if (!CanMutateDomain()) return false;
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

TArray<FAPBClothingChoice> UAPBGameInstanceSubsystem::GetClothingForTab(int32 TabId, int32 MaxItems) const
{
	TArray<FAPBClothingChoice> Out;
	if (!Service) return Out;
	for (const auto& kv : SvcC(Service)->catalog.items)
	{
		if (kv.second.wardrobe_tab != TabId) continue;
		FAPBClothingChoice C;
		C.Id = UTF8_TO_TCHAR(kv.second.id.c_str());
		C.Name = UTF8_TO_TCHAR(kv.second.name.c_str());
		C.Slot = UTF8_TO_TCHAR(apb::CustomizationService::SlotForTab(TabId));
		C.ArmasPrice = kv.second.armas_price;
		Out.Add(C);
	}
	Out.Sort([](const FAPBClothingChoice& A, const FAPBClothingChoice& B) { return A.Id < B.Id; });
	if (Out.Num() > MaxItems) Out.SetNum(MaxItems);
	return Out;
}

FString UAPBGameInstanceSubsystem::GetSlotForTab(int32 TabId) const
{
	return UTF8_TO_TCHAR(apb::CustomizationService::SlotForTab(TabId));
}

bool UAPBGameInstanceSubsystem::EquipClothingColored(const FString& Slot, const FString& ItemId, int32 ColorPrimary, int32 ColorSecondary)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->EquipClothing(TCHAR_TO_UTF8(*Slot), TCHAR_TO_UTF8(*ItemId), ColorPrimary, ColorSecondary);
}

bool UAPBGameInstanceSubsystem::RandomizeAppearance(int32 Seed)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* W = Svc(Service);
	if (!W->character) return false;
	apb::CharacterAppearance App = W->customization.Randomize(W->character->faction, (uint32_t)Seed);
	const bool bOk = W->ApplyAppearance(App);
	if (bOk)
	{
		UE_LOG(LogTemp, Log, TEXT("APB RANDOMIZE seed=%d clothing=%d"), Seed, (int32)App.clothing.size());
	}
	return bOk;
}

TArray<FLinearColor> UAPBGameInstanceSubsystem::GetPaletteColors(const FString& PaletteName, int32 RowIndex) const
{
	TArray<FLinearColor> Out;
	const FString Path = DataDir / TEXT("palettes.json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) return Out;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return Out;
	const TSharedPtr<FJsonObject>* Pal = nullptr;
	if (!Root->TryGetObjectField(PaletteName, Pal) || !Pal) return Out;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!(*Pal)->TryGetArrayField(TEXT("rows"), Rows) || !Rows) return Out;
	if (RowIndex < 0 || RowIndex >= Rows->Num()) return Out;
	const TArray<TSharedPtr<FJsonValue>>& Colors = (*Rows)[RowIndex]->AsArray();
	for (const TSharedPtr<FJsonValue>& CV : Colors)
	{
		const TSharedPtr<FJsonObject> CO = CV->AsObject();
		if (!CO.IsValid()) continue;
		const float R = (float)CO->GetNumberField(TEXT("r")) / 255.f;
		const float G = (float)CO->GetNumberField(TEXT("g")) / 255.f;
		const float B = (float)CO->GetNumberField(TEXT("b")) / 255.f;
		Out.Add(FLinearColor(R, G, B, 1.f));
	}
	return Out;
}

bool UAPBGameInstanceSubsystem::AddSymbolLayer(int32 SymbolId, const FString& TargetSlot, float PosX, float PosY, float Rotation, float Scale, int32 ColorPrimary, int32 ColorSecondary)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* W = Svc(Service);
	if (!W->character) return false;
	apb::CharacterAppearance App = W->appearance;
	apb::SymbolLayer L;
	L.symbol_id = SymbolId; L.target_slot = TCHAR_TO_UTF8(*TargetSlot);
	L.pos_x = PosX; L.pos_y = PosY; L.rotation = Rotation; L.scale = Scale;
	L.color_primary = ColorPrimary; L.color_secondary = ColorSecondary;
	App.symbols.push_back(L);
	return W->ApplyAppearance(App);
}

int32 UAPBGameInstanceSubsystem::GetSymbolLayerCount() const
{
	if (!Service) return 0;
	return (int32)SvcC(Service)->appearance.symbols.size();
}

bool UAPBGameInstanceSubsystem::GetCameraFrameForTab(int32 TabId, float& OutPosY, float& OutPosZ, float& OutTargetZ, float& OutFov) const
{
	OutPosY = 280.f; OutPosZ = 95.f; OutTargetZ = 95.f; OutFov = 55.f;
	const FString Path = DataDir / TEXT("wardrobe_categories.json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) return false;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;
	const TSharedPtr<FJsonObject>* DefCam = nullptr;
	if (Root->TryGetObjectField(TEXT("default_camera"), DefCam) && DefCam)
	{
		OutPosY = (float)(*DefCam)->GetNumberField(TEXT("pos_y"));
		OutPosZ = (float)(*DefCam)->GetNumberField(TEXT("pos_z"));
		OutTargetZ = (float)(*DefCam)->GetNumberField(TEXT("target_z"));
		OutFov = (float)(*DefCam)->GetNumberField(TEXT("fov"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Cats = nullptr;
	if (!Root->TryGetArrayField(TEXT("categories"), Cats) || !Cats) return false;
	for (const TSharedPtr<FJsonValue>& CV : *Cats)
	{
		const TSharedPtr<FJsonObject> CO = CV->AsObject();
		if (!CO.IsValid() || (int32)CO->GetNumberField(TEXT("tab_id")) != TabId) continue;
		const TSharedPtr<FJsonObject>* Frame = nullptr;
		if (!CO->TryGetObjectField(TEXT("camera_frame"), Frame) || !Frame) return true;
		OutPosY = (float)(*Frame)->GetNumberField(TEXT("pos_y"));
		OutPosZ = (float)(*Frame)->GetNumberField(TEXT("pos_z"));
		OutTargetZ = (float)(*Frame)->GetNumberField(TEXT("target_z"));
		OutFov = (float)(*Frame)->GetNumberField(TEXT("fov"));
		return true;
	}
	return true;
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
	U.Mission.MissionTitle = U.MissionTitle;
	U.Mission.MissionStageIndex = S.mission_stage_index;
	U.Mission.MissionStageCount = S.mission_stage_count;
	U.Mission.MissionStageProgress = static_cast<float>(S.mission_stage_progress);
	U.Mission.MissionOppStageProgress = static_cast<float>(S.mission_opp_stage_progress);
	U.Mission.bMissionOppositionContesting = S.mission_opposition_contesting;
	U.Mission.bMissionOppositionWon = S.mission_opposition_won;
	U.Mission.bMissionTimedOut = S.mission_timed_out;
	U.Mission.MissionStageTimeLimitSec = static_cast<float>(S.mission_stage_time_limit_sec);
	U.Mission.MissionStageDeadlineServerSec = static_cast<float>(S.mission_stage_deadline_server_sec);
	for (const apb::SnapshotProgressEntry& Entry : S.contact_standings)
	{
		U.ProgressionState += FString::Printf(TEXT("C:%s=%lld;"), UTF8_TO_TCHAR(Entry.id.c_str()), Entry.value);
	}
	for (const apb::SnapshotProgressEntry& Entry : S.role_xp)
	{
		U.ProgressionState += FString::Printf(TEXT("R:%s=%lld;"), UTF8_TO_TCHAR(Entry.id.c_str()), Entry.value);
	}
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
		S.SessionId,
		S.ProgressionState,
		S.Mission);
}

bool UAPBGameInstanceSubsystem::ConnectToWorldServer(const FString& Host, int32 Port)
{
	bWorldServerMode = true;
	UE_LOG(LogTemp, Warning, TEXT("APBSubsystem ConnectToWorldServer host=%s port=%d"), *Host, Port);
	return true;
}

bool UAPBGameInstanceSubsystem::IsWorldServerConnected() const
{
	return bWorldServerMode;
}

FString UAPBGameInstanceSubsystem::GetIssuedTicket() const
{
	if (!bWorldServerMode) return FString();
	const UWorld* World = GetWorld();
	if (!World) return FString();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PC = It->Get())
		{
			if (const AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>())
			{
				if (!PS->IssuedTicketJson.IsEmpty()) return PS->IssuedTicketJson;
			}
		}
	}
	return FString();
}

void UAPBGameInstanceSubsystem::StartDistrictTravel(APlayerController* PlayerController,
	const FString& DistrictId, const FString& Host, const int32 Port, const FString& Ticket,
	const FString& ReservationId)
{
	if (!PlayerController || DistrictId.IsEmpty() || Host.IsEmpty() || Port < 1 || Ticket.IsEmpty() || ReservationId.IsEmpty())
	{
		return;
	}
	bDistrictTravelPending = true;
	PendingTravelDistrict = DistrictId;
	PendingTravelHost = Host;
	PendingTravelPort = Port;
	PendingTravelReservationId = ReservationId;
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().SetTimer(DistrictTravelTimeoutHandle,
			FTimerDelegate::CreateUObject(this, &UAPBGameInstanceSubsystem::HandleDistrictTravelTimeout), 10.0f, false);
	}
	PlayerController->ClientTravel(FString::Printf(TEXT("%s:%d?APBTicket=%s"), *Host, Port, *Ticket),
		ETravelType::TRAVEL_Absolute);
}

void UAPBGameInstanceSubsystem::HandleDistrictTravelMapLoaded(UWorld* LoadedWorld)
{
	if (!bDistrictTravelPending || !LoadedWorld)
	{
		return;
	}
	bDistrictTravelPending = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(DistrictTravelTimeoutHandle);
	}
	UE_LOG(LogTemp, Log, TEXT("TRAVEL_OK district=%s host=%s port=%d"),
		*PendingTravelDistrict, *PendingTravelHost, PendingTravelPort);
	PendingTravelDistrict.Empty();
	PendingTravelHost.Empty();
	PendingTravelPort = 0;
	PendingTravelReservationId.Empty();
}

void UAPBGameInstanceSubsystem::HandleDistrictTravelFailure(UWorld* FailedWorld,
	ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!bDistrictTravelPending)
	{
		return;
	}
	if (FailedWorld)
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(FailedWorld, 0))
		{
			if (AAPBPlayerState* PlayerState = PlayerController->GetPlayerState<AAPBPlayerState>())
			{
				PlayerState->Server_ReleaseTravelReservation(PendingTravelReservationId);
			}
		}
	}
	bDistrictTravelPending = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(DistrictTravelTimeoutHandle);
	}
	UE_LOG(LogTemp, Warning, TEXT("TRAVEL_FAIL reason=travel_error"));
	PendingTravelDistrict.Empty();
	PendingTravelHost.Empty();
	PendingTravelPort = 0;
	PendingTravelReservationId.Empty();
}

void UAPBGameInstanceSubsystem::HandleDistrictTravelTimeout()
{
	if (!bDistrictTravelPending)
	{
		return;
	}
	bDistrictTravelPending = false;
	UE_LOG(LogTemp, Warning, TEXT("TRAVEL_FAIL reason=timeout"));
	PendingTravelDistrict.Empty();
	PendingTravelHost.Empty();
	PendingTravelPort = 0;
	PendingTravelReservationId.Empty();
}

void UAPBGameInstanceSubsystem::PushDomainSnapshotToAllPlayerStates()
{
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
				S.SessionId,
				S.ProgressionState,
				S.Mission);
				PS->ForceNetUpdate();
			}
		}
	}
}

bool UAPBGameInstanceSubsystem::ApplyHandoffSnapshot(const apb::DomainSnapshot& Snapshot)
{
	if (!Service || !CanMutateDomain()) return false;
	return Svc(Service)->ApplyHandoff(Snapshot);
}

bool UAPBGameInstanceSubsystem::ApplyHandoffProbeMutation()
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* WorldService = Svc(Service);
	if (!WorldService->character) return false;
	WorldService->character->cash += 77;
	WorldService->threat.points += 5.0;
	WorldService->SaveAllNow();
	return true;
}

bool UAPBGameInstanceSubsystem::SocialClanCreate(const FString& ClanId, const FString& Name, const FString& Tag, const FString& Leader, bool bEnforcer)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::Faction F = bEnforcer ? apb::Faction::Enforcer : apb::Faction::Criminal;
	const apb::ClanResult R = S->clans.CreateClan(TCHAR_TO_UTF8(*ClanId), TCHAR_TO_UTF8(*Name),
		TCHAR_TO_UTF8(*Tag), F, TCHAR_TO_UTF8(*Leader));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_CREATE clan=%s leader=%s ok=%d"), *ClanId, *Leader, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialClanInvite(const FString& Inviter, const FString& Invitee)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	apb::Faction InviteeFaction = apb::Faction::Criminal;
	if (UWorld* World = GetWorld())
	{
		if (AAPBWorldGameMode* GM = World->GetAuthGameMode<AAPBWorldGameMode>())
		{
			const FAPBAdmittedPlayer* Entry = GM->FindAdmittedPlayer(Invitee);
			if (!Entry)
			{
				UE_LOG(LogTemp, Warning, TEXT("SOCIAL_CLAN_INVITE_FAIL reason=invitee_not_admitted invitee=%s"), *Invitee);
				return false;
			}
			InviteeFaction = Entry->Faction.Equals(TEXT("Enforcer"), ESearchCase::CaseSensitive)
				? apb::Faction::Enforcer : apb::Faction::Criminal;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SOCIAL_CLAN_INVITE_FAIL reason=invitee_not_admitted invitee=%s"), *Invitee);
			return false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SOCIAL_CLAN_INVITE_FAIL reason=invitee_not_admitted invitee=%s"), *Invitee);
		return false;
	}
	const apb::ClanResult R = S->clans.Invite(TCHAR_TO_UTF8(*Inviter), TCHAR_TO_UTF8(*Invitee), InviteeFaction);
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_INVITE inviter=%s invitee=%s ok=%d src=roster"), *Inviter, *Invitee, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialClanAcceptInvite(const FString& Invitee)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::ClanResult R = S->clans.AcceptInvite(TCHAR_TO_UTF8(*Invitee));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_ACCEPT invitee=%s ok=%d"), *Invitee, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialClanDeclineInvite(const FString& Invitee)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::ClanResult R = S->clans.DeclineInvite(TCHAR_TO_UTF8(*Invitee));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_DECLINE invitee=%s ok=%d"), *Invitee, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialClanKick(const FString& Actor, const FString& Target)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::ClanResult R = S->clans.Kick(TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Target));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_KICK actor=%s target=%s ok=%d"), *Actor, *Target, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialClanSetMotd(const FString& Actor, const FString& Text)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::ClanResult R = S->clans.SetMotd(TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Text));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_SETMOTD actor=%s ok=%d"), *Actor, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialClanAddRank(const FString& Actor, const FString& RankName, int32 Permissions)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::ClanResult R = S->clans.AddRank(TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*RankName), static_cast<uint32_t>(Permissions));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_ADDRANK actor=%s rank=%s ok=%d"), *Actor, *RankName, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialClanSetMemberRank(const FString& Actor, const FString& Target, int32 RankIndex)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::ClanResult R = S->clans.SetMemberRank(TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Target), RankIndex);
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_SETMEMBERRANK actor=%s target=%s rank=%d ok=%d"), *Actor, *Target, RankIndex, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialClanLeave(const FString& Player)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::ClanResult R = S->clans.Leave(TCHAR_TO_UTF8(*Player));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_LEAVE player=%s ok=%d"), *Player, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialClanDisband(const FString& Leader)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::ClanResult R = S->clans.Disband(TCHAR_TO_UTF8(*Leader));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_DISBAND leader=%s ok=%d"), *Leader, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialClanTransferLeader(const FString& Leader, const FString& Target)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::ClanResult R = S->clans.TransferLeader(TCHAR_TO_UTF8(*Leader), TCHAR_TO_UTF8(*Target));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_CLAN_TRANSFER leader=%s target=%s ok=%d"), *Leader, *Target, R == apb::ClanResult::Ok ? 1 : 0);
	return R == apb::ClanResult::Ok;
}

FAPBClanInfoUE UAPBGameInstanceSubsystem::GetClanInfo(const FString& ClanId) const
{
	FAPBClanInfoUE Out;
	const apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return Out;
	const apb::Clan* C = S->clans.Find(TCHAR_TO_UTF8(*ClanId));
	if (!C) return Out;
	Out.Id         = UTF8_TO_TCHAR(C->id.c_str());
	Out.Name       = UTF8_TO_TCHAR(C->name.c_str());
	Out.Tag        = UTF8_TO_TCHAR(C->tag.c_str());
	Out.Motd       = UTF8_TO_TCHAR(C->motd.c_str());
	Out.LeaderName = UTF8_TO_TCHAR(C->leader.c_str());
	for (const auto& M : C->members)
		Out.Members.Add(UTF8_TO_TCHAR(M.player.c_str()));
	return Out;
}

bool UAPBGameInstanceSubsystem::SocialFriendRequest(const FString& From, const FString& To)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::FriendResult R = S->friends_svc.SendRequest(TCHAR_TO_UTF8(*From), TCHAR_TO_UTF8(*To));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_FRIEND_REQUEST from=%s to=%s ok=%d"), *From, *To, R == apb::FriendResult::Ok ? 1 : 0);
	return R == apb::FriendResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialFriendAccept(const FString& Invitee, const FString& Inviter)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::FriendResult R = S->friends_svc.AcceptRequest(TCHAR_TO_UTF8(*Invitee), TCHAR_TO_UTF8(*Inviter));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_FRIEND_ACCEPT invitee=%s inviter=%s ok=%d"), *Invitee, *Inviter, R == apb::FriendResult::Ok ? 1 : 0);
	return R == apb::FriendResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialFriendDecline(const FString& Invitee, const FString& Inviter)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::FriendResult R = S->friends_svc.DeclineRequest(TCHAR_TO_UTF8(*Invitee), TCHAR_TO_UTF8(*Inviter));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_FRIEND_DECLINE invitee=%s inviter=%s ok=%d"), *Invitee, *Inviter, R == apb::FriendResult::Ok ? 1 : 0);
	return R == apb::FriendResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialFriendRemove(const FString& Player, const FString& Other)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::FriendResult R = S->friends_svc.RemoveFriend(TCHAR_TO_UTF8(*Player), TCHAR_TO_UTF8(*Other));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_FRIEND_REMOVE player=%s other=%s ok=%d"), *Player, *Other, R == apb::FriendResult::Ok ? 1 : 0);
	return R == apb::FriendResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialFriendIgnore(const FString& Player, const FString& Target)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::FriendResult R = S->friends_svc.Ignore(TCHAR_TO_UTF8(*Player), TCHAR_TO_UTF8(*Target));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_FRIEND_IGNORE player=%s target=%s ok=%d"), *Player, *Target, R == apb::FriendResult::Ok ? 1 : 0);
	return R == apb::FriendResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialFriendUnignore(const FString& Player, const FString& Target)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::FriendResult R = S->friends_svc.Unignore(TCHAR_TO_UTF8(*Player), TCHAR_TO_UTF8(*Target));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_FRIEND_UNIGNORE player=%s target=%s ok=%d"), *Player, *Target, R == apb::FriendResult::Ok ? 1 : 0);
	return R == apb::FriendResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialAreFriends(const FString& A, const FString& B) const
{
	const apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	return S->friends_svc.AreFriends(TCHAR_TO_UTF8(*A), TCHAR_TO_UTF8(*B));
}

bool UAPBGameInstanceSubsystem::SocialIsIgnoring(const FString& Player, const FString& Target) const
{
	const apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	return S->friends_svc.IsIgnoring(TCHAR_TO_UTF8(*Player), TCHAR_TO_UTF8(*Target));
}

TArray<FAPBFriendEntryUE> UAPBGameInstanceSubsystem::SocialGetFriendList(const FString& Player) const
{
	TArray<FAPBFriendEntryUE> Out;
	const apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return Out;
	for (const std::string& N : S->friends_svc.FriendsOf(TCHAR_TO_UTF8(*Player)))
	{
		FAPBFriendEntryUE E;
		E.Name    = UTF8_TO_TCHAR(N.c_str());
		E.bOnline = S->friends_svc.IsOnline(N);
		Out.Add(E);
	}
	return Out;
}

TArray<FString> UAPBGameInstanceSubsystem::SocialGetIncomingRequests(const FString& Player) const
{
	TArray<FString> Out;
	const apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return Out;
	for (const std::string& N : S->friends_svc.IncomingRequests(TCHAR_TO_UTF8(*Player)))
		Out.Add(UTF8_TO_TCHAR(N.c_str()));
	return Out;
}

TArray<FString> UAPBGameInstanceSubsystem::SocialGetIgnoreList(const FString& Player) const
{
	TArray<FString> Out;
	const apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return Out;
	for (const std::string& N : S->friends_svc.IgnoredBy(TCHAR_TO_UTF8(*Player)))
		Out.Add(UTF8_TO_TCHAR(N.c_str()));
	return Out;
}

bool UAPBGameInstanceSubsystem::SocialGroupCreate(const FString& Leader, FString& OutGroupId)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	std::string GroupIdStr;
	const apb::GroupResult R = S->groups.CreateGroup(TCHAR_TO_UTF8(*Leader), GroupIdStr);
	OutGroupId = UTF8_TO_TCHAR(GroupIdStr.c_str());
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_GROUP_CREATE leader=%s id=%s ok=%d"), *Leader, *OutGroupId, R == apb::GroupResult::Ok ? 1 : 0);
	return R == apb::GroupResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialGroupInvite(const FString& Inviter, const FString& Invitee)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::GroupResult R = S->groups.Invite(TCHAR_TO_UTF8(*Inviter), TCHAR_TO_UTF8(*Invitee));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_GROUP_INVITE inviter=%s invitee=%s ok=%d"), *Inviter, *Invitee, R == apb::GroupResult::Ok ? 1 : 0);
	return R == apb::GroupResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialGroupAccept(const FString& Invitee)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::GroupResult R = S->groups.AcceptInvite(TCHAR_TO_UTF8(*Invitee));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_GROUP_ACCEPT invitee=%s ok=%d"), *Invitee, R == apb::GroupResult::Ok ? 1 : 0);
	return R == apb::GroupResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialGroupLeave(const FString& Player)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::GroupResult R = S->groups.Leave(TCHAR_TO_UTF8(*Player));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_GROUP_LEAVE player=%s ok=%d"), *Player, R == apb::GroupResult::Ok ? 1 : 0);
	return R == apb::GroupResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialGroupKick(const FString& Leader, const FString& Target)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::GroupResult R = S->groups.Kick(TCHAR_TO_UTF8(*Leader), TCHAR_TO_UTF8(*Target));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_GROUP_KICK leader=%s target=%s ok=%d"), *Leader, *Target, R == apb::GroupResult::Ok ? 1 : 0);
	return R == apb::GroupResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialGroupDisband(const FString& Leader)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::GroupResult R = S->groups.Disband(TCHAR_TO_UTF8(*Leader));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_GROUP_DISBAND leader=%s ok=%d"), *Leader, R == apb::GroupResult::Ok ? 1 : 0);
	return R == apb::GroupResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialGroupTransferLeader(const FString& Leader, const FString& Target)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::GroupResult R = S->groups.TransferLeader(TCHAR_TO_UTF8(*Leader), TCHAR_TO_UTF8(*Target));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_GROUP_TRANSFER leader=%s target=%s ok=%d"), *Leader, *Target, R == apb::GroupResult::Ok ? 1 : 0);
	return R == apb::GroupResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialGroupSetReady(const FString& Player, bool bReady)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::GroupResult R = S->groups.SetReady(TCHAR_TO_UTF8(*Player), bReady);
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_GROUP_SETREADY player=%s ready=%d ok=%d"), *Player, bReady ? 1 : 0, R == apb::GroupResult::Ok ? 1 : 0);
	return R == apb::GroupResult::Ok;
}

bool UAPBGameInstanceSubsystem::SocialGroupAssignMission(const FString& Leader, const FString& MissionId)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	const apb::GroupResult R = S->groups.AssignMission(TCHAR_TO_UTF8(*Leader), TCHAR_TO_UTF8(*MissionId));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_GROUP_ASSIGNMISSION leader=%s mission=%s ok=%d"), *Leader, *MissionId, R == apb::GroupResult::Ok ? 1 : 0);
	return R == apb::GroupResult::Ok;
}

FAPBGroupInfoUE UAPBGameInstanceSubsystem::GetGroupInfo(const FString& GroupId) const
{
	FAPBGroupInfoUE Out;
	const apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return Out;
	const apb::Group* G = S->groups.Find(TCHAR_TO_UTF8(*GroupId));
	if (!G) return Out;
	Out.Id        = UTF8_TO_TCHAR(G->id.c_str());
	Out.Leader    = UTF8_TO_TCHAR(G->leader.c_str());
	Out.MissionId = UTF8_TO_TCHAR(G->mission_id.c_str());
	Out.bAllReady = S->groups.AllReady(TCHAR_TO_UTF8(*GroupId));
	for (const auto& M : G->members)
		Out.Members.Add(UTF8_TO_TCHAR(M.player.c_str()));
	return Out;
}

bool UAPBGameInstanceSubsystem::SocialMailSend(const FString& Character, const FString& To, const FString& Subject, const FString& Body, int64 Cash)
{
	if (!Service || !CanMutateDomain()) return false;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return false;
	// The claimed sender is validated against the service that authoritatively owns
	// that character. Reading the per-connection service instead would trust a local
	// field that is empty on a world server, and would let a client spoof the sender.
	apb::WorldService* Sender = CharacterOwnerSvc(GetWorld(), Service, Character);
	if (!Sender || !Sender->character.has_value()) return false;
	const std::string From = Sender->character->name;
	if (From.empty() || From != std::string(TCHAR_TO_UTF8(*Character))) return false;
	const bool bOk = S->mail.SendMail(From, TCHAR_TO_UTF8(*To), TCHAR_TO_UTF8(*Subject),
		TCHAR_TO_UTF8(*Body), static_cast<int64_t>(Cash));
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_MAIL_SEND from=%s to=%s ok=%d"), UTF8_TO_TCHAR(From.c_str()), *To, bOk ? 1 : 0);
	return bOk;
}

TArray<FAPBMailMessageUE> UAPBGameInstanceSubsystem::SocialMailGetInbox(const FString& Character) const
{
	TArray<FAPBMailMessageUE> Out;
	const apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return Out;
	for (const apb::MailMessage* M : S->mail.InboxFor(TCHAR_TO_UTF8(*Character)))
	{
		FAPBMailMessageUE E;
		E.Id       = FString::Printf(TEXT("%lld"), static_cast<long long>(M->id));
		E.From     = UTF8_TO_TCHAR(M->from.c_str());
		E.Subject  = UTF8_TO_TCHAR(M->subject.c_str());
		E.Body     = UTF8_TO_TCHAR(M->body.c_str());
		E.bRead    = M->read;
		E.bClaimed = M->claimed;
		for (const auto& A : M->attachments) E.Cash += static_cast<int64>(A.cash);
		Out.Add(E);
	}
	return Out;
}

int32 UAPBGameInstanceSubsystem::SocialMailUnreadCount(const FString& Character) const
{
	const apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return 0;
	return S->mail.UnreadCount(TCHAR_TO_UTF8(*Character));
}

EAPBMailResult UAPBGameInstanceSubsystem::SocialMailMarkRead(const FString& Character, const FString& MailId)
{
	if (!Service || !CanMutateDomain()) return EAPBMailResult::AuthorityUnavailable;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return EAPBMailResult::AuthorityUnavailable;
	const int64_t Id = static_cast<int64_t>(FCString::Atoi64(*MailId));
	const apb::MailMessage* Msg = S->mail.Find(Id);
	if (!Msg) return EAPBMailResult::NotFound;
	if (Msg->to != std::string(TCHAR_TO_UTF8(*Character))) return EAPBMailResult::NotOwner;
	S->mail.MarkRead(Id);
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_MAIL_MARKREAD character=%s id=%s"), *Character, *MailId);
	return EAPBMailResult::Ok;
}

EAPBMailResult UAPBGameInstanceSubsystem::SocialMailClaimAttachments(const FString& Character, const FString& MailId)
{
	if (!Service || !CanMutateDomain()) return EAPBMailResult::AuthorityUnavailable;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return EAPBMailResult::AuthorityUnavailable;
	const int64_t Id = static_cast<int64_t>(FCString::Atoi64(*MailId));
	const apb::MailMessage* Msg = S->mail.Find(Id);
	if (!Msg) return EAPBMailResult::NotFound;
	if (Msg->to != std::string(TCHAR_TO_UTF8(*Character))) return EAPBMailResult::NotOwner;
	if (Msg->claimed) return EAPBMailResult::AlreadyClaimed;
	if (Msg->attachments.empty()) return EAPBMailResult::Ok;

	// Fail closed before touching anything: an item payload cannot be granted until
	// inventory integration exists, and silently dropping it would destroy the item.
	if (S->mail.HasItemAttachments(Id))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SOCIAL_MAIL_CLAIM_UNSUPPORTED_ATTACHMENT character=%s id=%s ctx=no_inventory_grant"),
			*Character, *MailId);
		return EAPBMailResult::UnsupportedAttachment;
	}

	apb::WorldService* Owner = CharacterOwnerSvc(GetWorld(), Service, Character);
	if (!Owner || !Owner->character.has_value()) return EAPBMailResult::GrantFailed;

	int64_t CashTotal = 0;
	for (const auto& A : Msg->attachments) CashTotal += A.cash;
	const std::string Who = TCHAR_TO_UTF8(*Character);

	// Without an active journal (standalone/PIE with no social persistence) there is
	// nothing durable to reconcile against, so claim directly and keep that path working.
	if (!S->claim_journal.IsActive())
	{
		const std::vector<apb::MailAttachment> Atts = S->mail.ClaimAttachments(Id);
		if (Atts.empty()) return EAPBMailResult::GrantFailed;
		Owner->character->cash += CashTotal;
		UE_LOG(LogTemp, Log, TEXT("SOCIAL_MAIL_CLAIM character=%s id=%s cash=%lld ctx=unjournaled"),
			*Character, *MailId, static_cast<long long>(CashTotal));
		return EAPBMailResult::Ok;
	}

	// Recovery: a crash after the cash receipt but before the mail flag leaves the
	// message unclaimed with its cash already paid. Finish the flag, never re-credit.
	if (S->claim_journal.CashAlreadyCredited(Who, Id))
	{
		if (S->mail.CommitClaimed(Id))
		{
			S->claim_journal.CommitMail(Who, Id);
			S->SaveSocialNow();
			UE_LOG(LogTemp, Warning,
				TEXT("SOCIAL_MAIL_CLAIM_RECOVERED character=%s id=%s ctx=commit_mail_only"),
				*Character, *MailId);
			return EAPBMailResult::Ok;
		}
		return EAPBMailResult::AlreadyClaimed;
	}

	// Ordered commit: journal Prepared -> credit cash + durable receipt -> mail flag.
	const int64_t NowUtc = static_cast<int64_t>(FDateTime::UtcNow().ToUnixTimestamp());
	if (!S->claim_journal.Prepare(Who, Id, CashTotal, NowUtc)) return EAPBMailResult::GrantFailed;

	Owner->character->cash += CashTotal;
	if (!S->claim_journal.CommitCharacter(Who, Id))
	{
		Owner->character->cash -= CashTotal;
		return EAPBMailResult::GrantFailed;
	}
	Owner->SaveAllNow();

	if (!S->mail.CommitClaimed(Id)) return EAPBMailResult::GrantFailed;
	S->claim_journal.CommitMail(Who, Id);
	S->SaveSocialNow();

	UE_LOG(LogTemp, Log, TEXT("SOCIAL_MAIL_CLAIM character=%s id=%s cash=%lld ctx=journaled"),
		*Character, *MailId, static_cast<long long>(CashTotal));
	return EAPBMailResult::Ok;
}

EAPBMailResult UAPBGameInstanceSubsystem::SocialMailDelete(const FString& Character, const FString& MailId)
{
	if (!Service || !CanMutateDomain()) return EAPBMailResult::AuthorityUnavailable;
	apb::WorldService* S = SocialSvc(GetWorld(), Service);
	if (!S) return EAPBMailResult::AuthorityUnavailable;
	const int64_t Id = static_cast<int64_t>(FCString::Atoi64(*MailId));
	const apb::MailMessage* Msg = S->mail.Find(Id);
	if (!Msg) return EAPBMailResult::NotFound;
	if (Msg->to != std::string(TCHAR_TO_UTF8(*Character))) return EAPBMailResult::NotOwner;
	if (S->mail.HasUnclaimedAttachments(Id)) return EAPBMailResult::Unclaimed;
	S->mail.Delete(Id);
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_MAIL_DELETE character=%s id=%s"), *Character, *MailId);
	return EAPBMailResult::Ok;
}


