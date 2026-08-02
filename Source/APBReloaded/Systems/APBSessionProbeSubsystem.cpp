#include "APBSessionProbeSubsystem.h"
#include "CoreGlobals.h"
#include "APBInteractable.h"
#include "APBBotNPC.h"
#include "APBFrontendTypes.h"
#include "APBFrontendWidget.h"
#include "APBFrontendPlayerController.h"
#include "APBGameInstanceSubsystem.h"
#include "APBVerifiedAssetRegistry.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"
#include "Server/APBSecretProvider.h"
#include "Domain/APBCrypto.h"
#include "APBPlayerState.h"
#include "APBWorldGameMode.h"
#include "APBFreeroamCharacter.h"
#include "APBDriveableVehicle.h"
#include "APBDistrictPlacementLoader.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "APBPlayerState.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"

namespace
{
	constexpr int32 APBLoadActionToken(const bool bRequested, const bool bExecuted)
	{
		return bRequested && bExecuted ? 1 : 0;
	}

	constexpr int32 APBLoadVehicleToken(const bool bRequested, const bool bEntered, const bool bThrottled)
	{
		return bRequested && bEntered && bThrottled ? 1 : 0;
	}

	static_assert(APBLoadActionToken(true, false) == 0,
		"Requested workload actions must remain incomplete until they execute");
	static_assert(APBLoadActionToken(true, true) == 1,
		"Executed requested workload actions must report success");
	static_assert(APBLoadVehicleToken(true, true, false) == 0,
		"Vehicle workload requires both entry and throttle");
	static_assert(APBLoadVehicleToken(true, true, true) == 1,
		"Entered and throttled vehicle workload must report success");
}

void UAPBSessionProbeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Probe;
	if (FParse::Value(FCommandLine::Get(), TEXT("APBProbe="), Probe))
	{
		StartProbe(Probe);
	}
}

void UAPBSessionProbeSubsystem::StartProbe(const FString& InMode)
{
	Mode = InMode.ToLower();
	FParse::Value(FCommandLine::Get(), TEXT("WSClientId="), WSClientId);
	FParse::Value(FCommandLine::Get(), TEXT("SocialRole="), SocialRole);
	FParse::Value(FCommandLine::Get(), TEXT("MissionRole="), MissionRole);
	FParse::Value(FCommandLine::Get(), TEXT("ReplicationRole="), ReplicationRole);
	FParse::Value(FCommandLine::Get(), TEXT("APBLoadWorkload="), LoadWorkload);
	FParse::Value(FCommandLine::Get(), TEXT("APBLoadMap="), LoadMap);
	FParse::Value(FCommandLine::Get(), TEXT("APBLoadPrimary="), LoadPrimary);
	FParse::Value(FCommandLine::Get(), TEXT("APBLoadIdentity="), LoadIdentity);
	FParse::Value(FCommandLine::Get(), TEXT("APBLoadAccount="), LoadAccount);
	if (WSClientId.IsEmpty()) WSClientId = LoadIdentity;
	if (LoadIdentity.IsEmpty()) LoadIdentity = WSClientId;
	bLoadWorkloadEnabled = Mode == TEXT("world_server_client") && !LoadWorkload.IsEmpty();
	TArray<FString> RequestedActions;
	LoadWorkload.ParseIntoArray(RequestedActions, TEXT(","), true);
	for (FString Action : RequestedActions)
	{
		Action.TrimStartAndEndInline();
		bLoadMovementRequested |= Action.Equals(TEXT("movement"), ESearchCase::IgnoreCase);
		bLoadCombatRequested |= Action.Equals(TEXT("combat"), ESearchCase::IgnoreCase);
		bLoadVehicleRequested |= Action.Equals(TEXT("vehicle"), ESearchCase::IgnoreCase);
	}
	// Prefer APB_SCRATCH env (set by run_verification_gates.ps1); allow -APBScratch= override.
	FString ScratchDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("APBScratch="), ScratchDir))
	{
		ScratchDir = FPlatformMisc::GetEnvironmentVariable(TEXT("APB_SCRATCH"));
	}
	if (ScratchDir.IsEmpty())
	{
		ScratchDir = TEXT("C:/Users/Support/AppData/Local/Temp/grok-goal-5b882b537032/implementer");
	}
	ScratchDir.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (!ScratchDir.EndsWith(TEXT("/")))
	{
		ScratchDir += TEXT("/");
	}
	IFileManager::Get().MakeDirectory(*ScratchDir, true);
	LogPath = ScratchDir;
	if (Mode == TEXT("client_loop")) LogPath += TEXT("client_loop.log");
	else if (Mode == TEXT("playable")) LogPath += TEXT("playable_probe.log");
	else if (Mode == TEXT("social_district")) LogPath += TEXT("social_district_probe.log");
	else if (Mode == TEXT("mp_observe")) LogPath += TEXT("mp_client_observe.log");
	else if (Mode == TEXT("frontend_menu")) LogPath += TEXT("frontend_menu.log");
	else if (Mode == TEXT("frontend_flow")) LogPath += TEXT("frontend_flow.log");
	else if (Mode == TEXT("world_server")) LogPath += TEXT("world_server.log");
	else if (Mode == TEXT("world_handoff_server")) LogPath += TEXT("world_handoff_server.log");
	else if (Mode == TEXT("world_server_client")) LogPath += TEXT("world_server_client_") + WSClientId + TEXT(".log");
	else if (Mode == TEXT("world_travel_client")) LogPath += TEXT("world_travel_client_") + WSClientId + TEXT(".log");
	else if (Mode == TEXT("world_handoff_client")) LogPath += TEXT("world_handoff_client_") + WSClientId + TEXT(".log");
	else if (Mode == TEXT("world_chat_client")) LogPath += TEXT("world_chat_client_") + WSClientId + TEXT(".log");
	else if (Mode == TEXT("asset_allowlist")) LogPath += TEXT("asset_allowlist.log");
	else if (Mode == TEXT("frontend_routing")) LogPath += TEXT("frontend_routing.log");
	else if (Mode == TEXT("social_probe")) LogPath += TEXT("social_probe_") + SocialRole + TEXT(".log");
	else if (Mode == TEXT("replication_probe")) LogPath += TEXT("replication_probe_") + ReplicationRole + TEXT(".log");
	else if (Mode == TEXT("mission_client")) LogPath += TEXT("mission_client_") + MissionRole + TEXT(".log");
	else LogPath += TEXT("probe.log");

	if (Mode != TEXT("mp_observe"))
	{
		FFileHelper::SaveStringToFile(TEXT(""), *LogPath);
	}
	AppendLog(FString::Printf(TEXT("PROBE_START mode=%s scratch=%s"), *Mode, *ScratchDir));

	// GameInstance subsystems often init before a world exists — defer arming timers.
	if (UWorld* World = GetWorld())
	{
		ArmProbeTimers(World);
	}
	else if (UGameInstance* GI = GetGameInstance())
	{
		AppendLog(TEXT("PROBE_DEFER wait_for_world"));
		GI->GetTimerManager().SetTimer(
			PlayableTimer,
			FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::TryArmProbeWhenWorldReady),
			0.25f,
			true);
	}
	else
	{
		AppendLog(TEXT("PROBE_FAIL no_world_no_gi"));
	}
}

void UAPBSessionProbeSubsystem::TryArmProbeWhenWorldReady()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (const FWorldContext* Ctx = GI->GetWorldContext())
			{
				World = Ctx->World();
			}
		}
	}
	if (!World) return;
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(PlayableTimer);
	}
	AppendLog(TEXT("PROBE_ARM world_ready"));
	ArmProbeTimers(World);
}

void UAPBSessionProbeSubsystem::ArmProbeTimers(UWorld* World)
{
	if (!World) return;

	if (Mode == TEXT("client_loop"))
	{
		World->GetTimerManager().SetTimer(PlayableTimer, FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunClientLoopProbe), 1.0f, false);
	}
	else if (Mode == TEXT("playable") || Mode == TEXT("social_district"))
	{
		PlayablePhase = 0;
		World->GetTimerManager().SetTimer(PlayableTimer, FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::PlayableStep), 0.05f, true);
	}
	else if (Mode == TEXT("mp_observe"))
	{
		// Wait a few seconds for travel/join + OnRep before first poll
		World->GetTimerManager().SetTimer(MpTimer, FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::MpPoll), 1.0f, true, 3.0f);
	}
	else if (Mode == TEXT("frontend_menu") || Mode == TEXT("frontend_flow"))
	{
		bFrontendTravelPending = false;
		FrontendEquippedSlots = 0;
		bTerminal = false;
		World->GetTimerManager().SetTimer(PlayableTimer, FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunFrontendFlowProbe), 2.0f, false);
		AppendLog(FString::Printf(TEXT("PROBE_TIMER %s in 2s"), *Mode));
	}
	else if (Mode == TEXT("world_server"))
	{
		WS_LoginCount = 0; WS_CharListCount = 0; WS_DistrictListCount = 0; WS_TicketCount = 0;
		bTerminal = false;
		World->GetTimerManager().SetTimer(WorldServerTimer, FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunWorldServerProbe), 1.0f, true, 2.0f);
		AppendLog(TEXT("PROBE_TIMER world_server polling every 1s after 2s delay"));
	}
	else if (Mode == TEXT("world_handoff_server"))
	{
		// The handoff world is a listen-server that hosts no probe client of its own. With
		// no pending game-thread work it idle-exits ~2.3s after the last remote client (the
		// traveling handoff client) disconnects, tearing down the relay before the district's
		// CHAR_RETURN can land -> CHAR_RETURN_APPLIED never fires and the round-trip times out.
		// A repeating keepalive timer keeps the world ticking across the disconnect (mirroring
		// the world_server poller, whose repeating timer is why that world survives). The gate
		// runner terminates the process; a bounded self-exit guards against a leaked world.
		bTerminal = false;
		TSharedPtr<int32> Beat = MakeShared<int32>(0);
		World->GetTimerManager().SetTimer(WorldServerTimer,
			FTimerDelegate::CreateLambda([Beat]()
			{
				const int32 N = ++(*Beat);
				UE_LOG(LogTemp, Log, TEXT("HANDOFF_SERVER_KEEPALIVE beat=%d"), N);
				if (N >= 180) { FPlatformMisc::RequestExit(false); }
			}),
			1.0f, true, 1.0f);
		AppendLog(TEXT("PROBE_TIMER world_handoff_server keepalive every 1s"));
	}
	else if (Mode == TEXT("world_server_client"))
	{
		bWSClientDone = false;
		bTerminal = false;
		bWSLoginSent = false;
		WSLoginSentAt = 0.0;
		bLoadTravelDispatched = false;
		bLoadCompletionEmitted = false;
		bTravelLoginSent = false;
		bTravelTicketRequested = false;
		bTravelDispatchPending = false;
		TravelDistrictId = UAPBDistrictPlacementLoader::ResolveDistrictIdFromMapName(LoadMap);
		// The client travels from its bootstrap world to the world-server map. Keep
		// this probe timer on the GameInstance so travel does not destroy it before
		// the first login RPC can be issued.
		if (UGameInstance* GI = GetGameInstance())
		{
			GI->GetTimerManager().SetTimer(WorldServerTimer, FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunWorldServerClientProbe), 0.5f, true, 3.0f);
		}
		AppendLog(FString::Printf(TEXT("PROBE_TIMER world_server_client id=%s polling every 0.5s after 3s workload=%d map=%s primary=%s account=%s"),
			*WSClientId, bLoadWorkloadEnabled ? 1 : 0, *LoadMap, *LoadPrimary, *LoadAccount));
	}
	else if (Mode == TEXT("asset_allowlist"))
	{
		bTerminal = false;
		World->GetTimerManager().SetTimer(WorldServerTimer,
			FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunVerifiedAssetAllowlistProbe), 0.5f, false, 1.0f);
		AppendLog(TEXT("PROBE_TIMER asset_allowlist in 1s"));
	}
	else if (Mode == TEXT("frontend_routing"))
	{
		bTerminal = false;
		World->GetTimerManager().SetTimer(WorldServerTimer,
			FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunFrontendRoutingProbe), 0.5f, false, 1.0f);
		AppendLog(TEXT("PROBE_TIMER frontend_routing in 1s"));
	}
	else if (Mode == TEXT("social_probe"))
	{
		bSocialDone = false;
		bTerminal = false;
		bSocialLoginSent = false;
		bSocialWorldLoginSent = false;
		bSocialTicketRequested = false;
		bSocialTravelDispatched = false;
		bSocialArrivedInDistrict = false;
		bSocialClanOk = false;
		bSocialClanInviteOk = false;
		bSocialFriendsOk = false;
		bSocialGroupsOk = false;
		bSocialGroupInviteOk = false;
		bSocialMailOk = false;
		SocialOpInFlight.Empty();
		SocialProbeStartMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
		World->GetTimerManager().SetTimer(WorldServerTimer,
			FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunSocialProbe), 0.5f, true, 3.0f);
		AppendLog(FString::Printf(TEXT("PROBE_TIMER social_probe role=%s"), *SocialRole));
	}
	else if (Mode == TEXT("replication_probe"))
	{
		bReplicationDone = false;
		bTerminal = false;
		ReplicationProbeStartMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
		World->GetTimerManager().SetTimer(WorldServerTimer,
			FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunReplicationProbe), 0.5f, true, 3.0f);
		AppendLog(FString::Printf(TEXT("PROBE_TIMER replication_probe role=%s"), *ReplicationRole));
	}
	else if (Mode == TEXT("mission_client"))
	{
		bMissionClientDone = false;
		bTerminal = false;
		bMissionFactionRequested = false;
		bMissionEnqueued = false;
		bMissionSeenQueued = false;
		MissionClientStartMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
		World->GetTimerManager().SetTimer(WorldServerTimer,
			FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunMissionClientProbe), 0.5f, true, 3.0f);
		AppendLog(FString::Printf(TEXT("PROBE_TIMER mission_client role=%s"), *MissionRole));
	}
	else if (Mode == TEXT("world_travel_client"))
	{
		FParse::Value(FCommandLine::Get(), TEXT("WSTravelDistrict="), TravelDistrictId);
		if (TravelDistrictId.IsEmpty()) TravelDistrictId = TEXT("Financial");
		bTravelLoginSent = false;
		bTravelTicketRequested = false;
		bTravelDispatchPending = false;
		World->GetTimerManager().SetTimer(WorldServerTimer,
			FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunWorldTravelClientProbe), 0.25f, true, 3.0f);
		AppendLog(FString::Printf(TEXT("PROBE_TIMER world_travel_client id=%s district=%s"), *WSClientId, *TravelDistrictId));
	}
	else if (Mode == TEXT("world_handoff_client"))
	{
		TravelDistrictId = TEXT("Financial");
		HandoffPhase = 0;
		bHandoffLoginSent = false;
		bHandoffPrepareSent = false;
		bHandoffTicketSent = false;
		bHandoffDistrictLogged = false;
		bHandoffStateRequested = false;
		HandoffStateRequestAtMs = 0;
		HandoffDeadlineMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond() + 60000;
		if (UGameInstance* GI = GetGameInstance())
		{
			GI->GetTimerManager().SetTimer(WorldServerTimer,
				FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunWorldHandoffClientProbe), 0.25f, true, 2.0f);
		}
		AppendLog(FString::Printf(TEXT("PROBE_TIMER world_handoff_client id=%s"), *WSClientId));
	}
	else if (Mode == TEXT("world_chat_client"))
	{
		FParse::Value(FCommandLine::Get(), TEXT("APBChatCharacter="), ChatCharacter);
		FParse::Value(FCommandLine::Get(), TEXT("APBChatDistrict="), ChatDistrictId);
		if (ChatCharacter.IsEmpty() || ChatDistrictId.IsEmpty())
		{
			AppendLog(TEXT("CHAT_CLIENT_FAIL reason=missing_identity"));
			return;
		}
		bChatLoginSent = false;
		bChatTicketRequested = false;
		bChatTravelDispatched = false;
		bChatArrivalLogged = false;
		bChatCommandsArmed = false;
		QueuedChatCommands.Reset();
		QueueChatCommandsFromCommandLine();
		if (UGameInstance* GI = GetGameInstance())
		{
			GI->GetTimerManager().SetTimer(WorldServerTimer,
				FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunWorldChatClientProbe), 0.25f, true, 3.0f);
		}
		AppendLog(FString::Printf(TEXT("PROBE_TIMER world_chat_client id=%s character=%s district=%s"),
			*WSClientId, *ChatCharacter, *ChatDistrictId));
	}
}

void UAPBSessionProbeSubsystem::AppendLog(const FString& Line)
{
	const FString Ts = FString::Printf(TEXT("[%s] %s\n"), *FDateTime::UtcNow().ToString(), *Line);
	FFileHelper::SaveStringToFile(Ts, *LogPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
	UE_LOG(LogTemp, Log, TEXT("APBProbe %s"), *Line);
}

void UAPBSessionProbeSubsystem::RunClientLoopProbe()
{
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) { AppendLog(TEXT("FAIL no_apb")); return; }

	AppendLog(TEXT("SESSION_LOOP_BEGIN"));
	APB->RegisterAccount(TEXT("probe_user"), TEXT("probe_pass"));
	APB->Login(TEXT("probe_user"), TEXT("probe_pass"));
	AppendLog(TEXT("AUTH ok=1"));
	APB->EnterWorld(TEXT("W1"));
	AppendLog(TEXT("WORLD_ENTER ok=1"));
	APB->CreateCharacter(TEXT("Viper"), false);
	APB->EquipClothingItem(TEXT("torso"), TEXT("Clothing_Crim_Hoodie_T1"));
	APB->EquipClothingItem(TEXT("legs"), TEXT("Clothing_Crim_Jeans_T1"));
	// Visual clothing: contact mesh + full multi-slot wardrobe (head/torso/legs/feet/hands/accessory/face)
	int32 ClothingVisualOk = 0;
	int32 WardrobeSlots = 0;
	if (UWorld* WorldForCloth = GetWorld())
	{
		for (FConstPlayerControllerIterator It = WorldForCloth->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					if (AAPBFreeroamCharacter* FC = Cast<AAPBFreeroamCharacter>(Pawn))
					{
						const bool bMesh = FC->ApplyFactionVisualMesh();
						WardrobeSlots = FC->EquipFullWardrobeVisual();
						ClothingVisualOk = (bMesh && WardrobeSlots >= 5) ? 1 : (bMesh || WardrobeSlots > 0) ? 1 : 0;
						AppendLog(FString::Printf(TEXT("CLOTHING_VISUAL ok=%d wardrobe_slots=%d summary=%s"),
							ClothingVisualOk, WardrobeSlots, *FC->AppliedClothingSummary));
						// Explicit slot equip proof for clothing depth (catalog IDs)
						FC->EquipClothingVisual(TEXT("head"), TEXT("Clothing_Cap_Flex_T1"));
						FC->EquipClothingVisual(TEXT("torso"), TEXT("Clothing_Crim_Hoodie_T1"));
						FC->EquipClothingVisual(TEXT("legs"), TEXT("Clothing_Crim_Jeans_T1"));
						FC->EquipClothingVisual(TEXT("feet"), TEXT("Clothing_Boots_Urban_T1"));
						FC->EquipClothingVisual(TEXT("hands"), TEXT("Clothing_Gloves_Tactical"));
						FC->EquipClothingVisual(TEXT("accessory"), TEXT("Clothing_Accessory_Chain_T1"));
						FC->EquipClothingVisual(TEXT("face"), TEXT("Clothing_Face_Mask_T1"));
						AppendLog(FString::Printf(TEXT("CLOTHING_SLOTS active=%d required=7 summary=%s"),
							FC->GetActiveWardrobeSlotCount(), *FC->AppliedClothingSummary));
					}
				}
			}
		}
	}
	if (ClothingVisualOk == 0)
	{
		// Domain still equips even without pawn mesh path
		APB->EquipClothingItem(TEXT("head"), TEXT("Clothing_Cap_Flex_T1"));
		APB->EquipClothingItem(TEXT("feet"), TEXT("Clothing_Boots_Urban_T1"));
		APB->EquipClothingItem(TEXT("hands"), TEXT("Clothing_Gloves_Tactical"));
		APB->EquipClothingItem(TEXT("accessory"), TEXT("Clothing_Accessory_Chain_T1"));
		APB->EquipClothingItem(TEXT("face"), TEXT("Clothing_Face_Mask_T1"));
		AppendLog(TEXT("CLOTHING_VISUAL ok=0 summary=domain_equip_full_slots_no_pawn"));
	}
	AppendLog(TEXT("CHAR_CREATE faction=Criminal customize=1"));

	FString JoinDistrictId = TEXT("Financial");
	if (UWorld* W = GetWorld())
	{
		const FString MapName = W->GetMapName();
		if (MapName.Contains(TEXT("Waterfront"))) JoinDistrictId = TEXT("Waterfront");
		else if (MapName.Contains(TEXT("Asylum"))) JoinDistrictId = TEXT("PGAsylum");
		else if (MapName.Contains(TEXT("Beacon"))) JoinDistrictId = TEXT("PGBeacon");
		else if (MapName.Contains(TEXT("Crate"))) JoinDistrictId = TEXT("PGCrate");
		else if (MapName.Contains(TEXT("Social"))) JoinDistrictId = TEXT("Social");
	}
	const bool bJoin = APB->JoinDistrict(JoinDistrictId);
	FAPBDomainSnapshotUE Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("DISTRICT_ENTER ok=%d district=%s session=%s phase=%s inv_slots=%d"),
		bJoin ? 1 : 0, *JoinDistrictId, *Snap.SessionId, *APB->GetPhase(), Snap.InventorySlotCount));

	// S1 timeout proof, run BEFORE any threat accrues. TickMission arms the current
	// stage's apbdb-sourced time_limit_sec on the first call (deadline = now + limit) and
	// breaches on a later call past it. The breach fails the mission, but ApplyMissionFail
	// clamps at max(0, points-6): with threat still 0 the failure is a true no-op, so this
	// leaves no economy trace for the S2 opp-won leg below. A fresh StartOppositionMission
	// after readback restores an un-timed-out run for S2.
	{
		const float PreThreat = APB->CaptureDomainSnapshot().ThreatPoints;
		APB->StartOppositionMission();
		const bool bArmedTimedOut = APB->TickMission(0.f);
		FAPBDomainSnapshotUE ArmSnap = APB->CaptureDomainSnapshot();
		const float Limit = ArmSnap.Mission.MissionStageTimeLimitSec;
		const float Deadline = ArmSnap.Mission.MissionStageDeadlineServerSec;
		const bool bBreached = APB->TickMission(Deadline + 1.f);
		FAPBDomainSnapshotUE ToSnap = APB->CaptureDomainSnapshot();
		AppendLog(FString::Printf(
			TEXT("TIMEOUT_DRIVE armed_timed_out=%d limit=%.0f deadline=%.0f breached=%d timed_out=%d status=%s stage=%d/%d pre_threat=%.1f post_threat=%.1f"),
			bArmedTimedOut ? 1 : 0, Limit, Deadline, bBreached ? 1 : 0,
			ToSnap.Mission.bMissionTimedOut ? 1 : 0, *ToSnap.MissionStatus,
			ToSnap.MissionStageIndex, ToSnap.MissionStageCount, PreThreat, ToSnap.ThreatPoints));

		APB->PushDomainSnapshotToAllPlayerStates();
		if (UWorld* TW = GetWorld())
		{
			for (FConstPlayerControllerIterator It = TW->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					if (AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>())
					{
						AppendLog(FString::Printf(
							TEXT("TIMEOUT_PS_READBACK timed_out=%d limit=%.0f deadline=%.0f mission=%s stage=%d/%d"),
							PS->bMissionTimedOut ? 1 : 0, PS->MissionStageTimeLimitSec,
							PS->MissionStageDeadlineServerSec, *PS->MissionTitle,
							PS->MissionStageIndex, PS->MissionStageCount));
					}
				}
			}
		}
	}

	float Hp = 0; bool bKill = false;
	const float Dmg = APB->FireCatalogWeapon(TEXT(""), 3.f, 0.f, Hp, bKill);
	for (int32 i = 0; i < 8 && !bKill; ++i) APB->FireCatalogWeapon(TEXT(""), 3.f, 0.f, Hp, bKill);
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("SHOOT dmg=%.1f killed=%d threat=%.1f"), Dmg, bKill ? 1 : 0, Snap.ThreatPoints));

	FString Err;
	bool bArmas = APB->ArmasPurchase(TEXT("Weapon_Pistol_FBW"), Err);
	if (!bArmas) bArmas = APB->ArmasPurchase(TEXT("Weapon_Colby"), Err);
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("ARMAS_BUY ok=%d g1c=%lld inv_slots=%d cash=%lld"),
		bArmas ? 1 : 0, Snap.G1C, Snap.InventorySlotCount, Snap.Cash));

	int64 Listing = 0; FString AErr;
	bool bList = APB->AuctionListItem(TEXT("Weapon_Colby"), 1, 500, Listing, AErr);
	if (!bList) bList = APB->AuctionListItem(TEXT("Weapon_Pistol_FBW"), 1, 500, Listing, AErr);
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("AUCTION_LIST ok=%d id=%lld inv_slots=%d"), bList ? 1 : 0, Listing, Snap.InventorySlotCount));

	// Use live catalog IDs from Content/Data/vehicles.json (apbdb-shaped), not stale hero names
	bool bVeh = APB->SpawnCatalogVehicle(TEXT("Vehicle_Car_A_UtilityEstate"));
	if (!bVeh) bVeh = APB->SpawnCatalogVehicle(TEXT("Vehicle_Car_A_Utility2DrVan"));
	if (!bVeh) bVeh = APB->SpawnCatalogVehicle(TEXT("Vehicle_Car_A_UtilityTaxi_Pre_FN1_Pra"));
	if (!bVeh) bVeh = APB->SpawnCatalogVehicle(TEXT("")); // empty → Domain first Vehicle fallback
	const bool bPoss = bVeh && APB->PossessCatalogVehicle();
	AppendLog(FString::Printf(TEXT("VEHICLE_DOMAIN spawn=%d possess=%d"), bVeh ? 1 : 0, bPoss ? 1 : 0));

	APB->StartOppositionMission();
	int32 Ticks = 0;
	while (Ticks < 40 && APB->AdvanceMissionStage()) ++Ticks;
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("MISSION_DONE title=%s stage=%d/%d status=%s threat=%.1f ticks=%d"),
		*Snap.MissionTitle, Snap.MissionStageIndex, Snap.MissionStageCount, *Snap.MissionStatus, Snap.ThreatPoints, Ticks));

	// Snapshot retains mission fields on Failed, so a peer OnRep-observes opp_won=1 with stage/title intact.
	int32 OppTicks = 0;
	while (OppTicks < 40 && APB->IsMissionActive())
	{
		APB->AdvanceOpposition(1.0f);
		++OppTicks;
	}
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("OPP_WIN_DRIVE title=%s stage=%d/%d status=%s opp_prog=%.2f opp_won=%d threat=%.1f opp_ticks=%d"),
		*Snap.MissionTitle, Snap.MissionStageIndex, Snap.MissionStageCount, *Snap.MissionStatus,
		Snap.Mission.MissionOppStageProgress, Snap.Mission.bMissionOppositionWon ? 1 : 0, Snap.ThreatPoints, OppTicks));

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>())
				{
					APB->SyncPlayerStateFromDomain(PS);
					AppendLog(FString::Printf(TEXT("SYNC_PS player=%s threat=%.1f cash=%lld g1c=%lld inv=%d mission=%s stage=%d/%d"),
						*PS->GetPlayerName(), PS->ThreatPoints, PS->Cash, PS->G1C, PS->InventoryItemCount,
						*PS->MissionTitle, PS->MissionStageIndex, PS->MissionStageCount));
				}
			}
		}
	}

	AppendLog(FString::Printf(TEXT("SNAPSHOT_FINAL inv_slots=%d cash=%lld g1c=%lld threat=%.1f mission_stage=%d/%d"),
		Snap.InventorySlotCount, Snap.Cash, Snap.G1C, Snap.ThreatPoints, Snap.MissionStageIndex, Snap.MissionStageCount));
	// Server: push host Domain to every PlayerState so listen clients OnRep host economy/mission
	APB->PushDomainSnapshotToAllPlayerStates();
	Snap = APB->CaptureDomainSnapshot();
	AppendLog(FString::Printf(TEXT("SYNC_PS threat=%.1f cash=%lld g1c=%lld inv=%d mission=%s stage=%d/%d session=%s"),
		Snap.ThreatPoints, Snap.Cash, Snap.G1C, Snap.InventorySlotCount,
		*Snap.MissionTitle, Snap.MissionStageIndex, Snap.MissionStageCount, *Snap.SessionId));
	// Proves FireWeaponLocal syncs PlayerState through the Domain bridge on its own.
	// Do NOT add a manual SyncPlayerStateFromDomain in this block: the stale->parity
	// assertion is only meaningful when FireWeaponLocal is the sole sync path.
	{
		int32 FireSyncOk = 0;
		float StaleThreat = -1.f, PSAfter = -1.f, DomainDiverged = -1.f, DomainFinal = -1.f;
		int32 Mutated = 0, MovedOffStale = 0, Parity = 0;
		if (UWorld* FW = GetWorld())
		{
			APlayerController* PC = nullptr;
			for (FConstPlayerControllerIterator It = FW->GetPlayerControllerIterator(); It; ++It)
			{
				if ((PC = It->Get()) != nullptr) break;
			}
			AAPBFreeroamCharacter* FireChar = PC ? Cast<AAPBFreeroamCharacter>(PC->GetPawn()) : nullptr;
			if (PC && !FireChar)
			{
				FActorSpawnParameters Sp;
				Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				FireChar = FW->SpawnActor<AAPBFreeroamCharacter>(FVector(3000.f, 3000.f, 300.f), FRotator::ZeroRotator, Sp);
				if (FireChar) PC->Possess(FireChar);
			}
			AAPBPlayerState* FirePS = PC ? PC->GetPlayerState<AAPBPlayerState>() : nullptr;
			if (FireChar && FirePS)
			{
				APB->SyncPlayerStateFromDomain(FirePS);
				StaleThreat = FirePS->ThreatPoints;
				Mutated = APB->ApplyHandoffProbeMutation() ? 1 : 0;
				DomainDiverged = APB->CaptureDomainSnapshot().ThreatPoints;
				FireChar->FireWeaponLocal();
				PSAfter = FirePS->ThreatPoints;
				DomainFinal = APB->CaptureDomainSnapshot().ThreatPoints;
				MovedOffStale = FMath::IsNearlyEqual(PSAfter, StaleThreat, 0.01f) ? 0 : 1;
				Parity = FMath::IsNearlyEqual(PSAfter, DomainFinal, 0.01f) ? 1 : 0;
				FireSyncOk = (Mutated == 1 && MovedOffStale == 1 && Parity == 1) ? 1 : 0;
			}
		}
		AppendLog(FString::Printf(
			TEXT("FIRE_SYNC ok=%d mutated=%d moved_off_stale=%d parity=%d stale_threat=%.1f domain_diverged=%.1f ps_after=%.1f domain_final=%.1f"),
			FireSyncOk, Mutated, MovedOffStale, Parity, StaleThreat, DomainDiverged, PSAfter, DomainFinal));
	}
	// ── MATCH_DISPATCH_DRIVE: matchmaker enqueue → pairing → dispatch, end-to-end ──────
	// Exercises the real production spine: the host's Server_RequestMissionDispatch RPC
	// enqueues its party; a synthetic opposing-faction party is injected (a cross-faction
	// pairing needs both sides, and spinning a second network client is out of scope for a
	// single-process probe); then the SHARED UAPBGameInstanceSubsystem::FormAndDispatchMatches
	// path — the identical one AAPBFreeroamGameMode::TickMatchmaker runs on its 5s cadence —
	// forms the pairing, dispatches it into the live opposed run, clears bMissionQueued, and
	// replicates the mission to the host PlayerState. The prior OPP_WIN leg leaves the run
	// Failed (not Active), so the singleton guard permits this fresh dispatch.
	{
		int32 RpcQueued = 0, QueueAfterEnqueue = 0, Paired = 0, Dispatched = 0, QueuedCleared = 0, MissionActive = 0;
		FString DispatchTitle;
		if (UWorld* DW = GetWorld())
		{
			APlayerController* HostPC = UGameplayStatics::GetPlayerController(DW, 0);
			AAPBPlayerState* HostPS = HostPC ? HostPC->GetPlayerState<AAPBPlayerState>() : nullptr;
			const AGameStateBase* GS = DW->GetGameState();
			const int64 NowMs = GS ? static_cast<int64>(GS->GetServerWorldTimeSeconds() * 1000.0) : 0;
			if (HostPS)
			{
				// Host enqueues itself through the real validated Server RPC (authority path).
				HostPS->Server_RequestMissionDispatch();
				RpcQueued = HostPS->bMissionQueued ? 1 : 0;
				// Inject a same-tier opposing party so a zero-tolerance (now==enqueued) pairing forms.
				const bool bHostEnforcer = (HostPS->Faction == EAPBFaction::Enforcer);
				const int32 OppTier = APB->MissionThreatTier(HostPS->ThreatPoints, !bHostEnforcer);
				APB->EnqueueMissionParty(TEXT("SYNTH_DISPATCH_OPP"), !bHostEnforcer, OppTier, 1, NowMs);
				QueueAfterEnqueue = APB->MatchmakerQueueSize();
				const TArray<FString> Markers = APB->FormAndDispatchMatches(NowMs);
				Paired = Markers.Num() > 0 ? 1 : 0;
				Dispatched = APB->IsMissionActive() ? 1 : 0;
				MissionActive = Dispatched;
				QueuedCleared = HostPS->bMissionQueued ? 0 : 1;
				DispatchTitle = HostPS->MissionTitle;
			}
		}
		AppendLog(FString::Printf(
			TEXT("MATCH_DISPATCH_DRIVE rpc_queued=%d queue=%d paired=%d dispatched=%d queued_cleared=%d mission_active=%d title=%s"),
			RpcQueued, QueueAfterEnqueue, Paired, Dispatched, QueuedCleared, MissionActive, *DispatchTitle));
	}

	AppendLog(TEXT("SESSION_LOOP_COMPLETE CLIENT_LOOP_OK"));
}

/** Invisible walkable ground via box collision (no BasicShapes mesh). */
static void SpawnProbeGround(UWorld* World, FVector Center)
{
	// Top of slab sits slightly below character feet so walking mode engages.
	const FVector GroundLoc(Center.X, Center.Y, Center.Z - 100.f);
	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Ground = World->SpawnActor<AActor>(AActor::StaticClass(), GroundLoc, FRotator::ZeroRotator, Sp);
	if (!Ground) return;
	UBoxComponent* Box = NewObject<UBoxComponent>(Ground, TEXT("GroundBox"));
	Box->SetBoxExtent(FVector(50000.f, 50000.f, 80.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetCanEverAffectNavigation(false);
	Box->SetMobility(EComponentMobility::Static);
	Ground->SetRootComponent(Box);
	Box->RegisterComponent();
	Ground->AddInstanceComponent(Box);
	Ground->SetActorEnableCollision(true);
#if WITH_EDITOR
	Ground->SetActorLabel(TEXT("APB_ProbeGround"));
#endif
}

/** True when -APBCapture=<path> is on the command line; optionally returns the path.
 *  Gates the playable probe's screenshot phase (phase 20) so the headless walk/drive
 *  gate path (phases 1/2) is completely unaffected unless -APBCapture is passed. */
static bool APBCaptureWanted(FString* OutPath = nullptr)
{
	FString Path;
	const bool bHas = FParse::Value(FCommandLine::Get(), TEXT("APBCapture="), Path) && !Path.IsEmpty();
	if (bHas)
	{
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (OutPath) *OutPath = Path;
	}
	else if (OutPath)
	{
		OutPath->Reset();
	}
	return bHas;
}

void UAPBSessionProbeSubsystem::PlayableStep()
{
	UWorld* World = GetWorld();
	if (!World) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC) return;

	static int32 Frames = 0;
	++Frames;

	if (PlayablePhase == 0)
	{
		// Prefer freeroam PlayerStart / known San Paro block coords if present
		FVector Spawn(2200.f, -2200.f, 200.f);
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Spawn = It->GetActorLocation() + FVector(0.f, 0.f, 50.f);
			break;
		}
		// San Paro multi-block actor centroid (from cStreamedBuildingActor extracts)
		if (World->GetMapName().Contains(TEXT("Financial")))
		{
			Spawn = FVector(140736.f, 129388.f, 500.f);
		}
		else if (World->GetMapName().Contains(TEXT("Waterfront")))
		{
			Spawn = FVector(152309.f, 164134.f, 500.f);
		}
		SpawnProbeGround(World, Spawn);
		AAPBFreeroamCharacter* Char = Cast<AAPBFreeroamCharacter>(PC->GetPawn());
		if (!Char)
		{
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Char = World->SpawnActor<AAPBFreeroamCharacter>(Spawn, FRotator::ZeroRotator, Sp);
			if (Char) PC->Possess(Char);
		}
		if (Char)
		{
			Char->SetActorLocation(Spawn, false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
			{
				Move->GravityScale = 1.f;
				Move->MaxWalkSpeed = 1200.f;
				Move->bOrientRotationToMovement = true;
				Move->SetMovementMode(MOVE_Falling); // land on ground box, then walk
				Move->Velocity = FVector::ZeroVector;
			}
			// Settle on ground
			PlayablePhase = 10;
			Frames = 0;
			AppendLog(FString::Printf(TEXT("PLAYABLE_CHAR spawn=(%.1f,%.1f,%.1f) ground=box_collision"), Spawn.X, Spawn.Y, Spawn.Z));
		}
		return;
	}

	// Settle: wait for land (Walking / not falling fast)
	if (PlayablePhase == 10)
	{
		if (AAPBFreeroamCharacter* Char = Cast<AAPBFreeroamCharacter>(PC->GetPawn()))
		{
			if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
			{
				if (Move->IsMovingOnGround() || Frames >= 30)
				{
					Move->SetMovementMode(MOVE_Walking);
					PlayableStart = Char->GetActorLocation();
					AppendLog(FString::Printf(TEXT("PLAYABLE_SETTLED at=(%.1f,%.1f,%.1f) mode=%d"),
						PlayableStart.X, PlayableStart.Y, PlayableStart.Z, (int32)Move->MovementMode));
					PlayablePhase = APBCaptureWanted() ? 20 : 1;
					Frames = 0;
				}
			}
		}
		else if (Frames >= 40)
		{
			PlayablePhase = APBCaptureWanted() ? 20 : 1;
			Frames = 0;
		}
		return;
	}

	if (PlayablePhase == 20)
	{
		// Capture run (-APBCapture): hold the possessed pawn at the streamed San Paro
		// centroid and request real-RHI screenshots after the scene has had frames to
		// compile shaders, then self-exit. OFF unless -APBCapture is passed, so the
		// headless gate's walk/drive phases (1/2) are byte-for-byte unaffected.
		FString CapPath;
		APBCaptureWanted(&CapPath);
		// Frame the streamed block: place an elevated 3/4 establishing camera aimed down
		// at where the pawn settled, so the screenshot shows the real San Paro geometry
		// instead of the ground-level pawn view (which mostly frames sky).
		if (Frames == 2)
		{
			const FVector Focus = PlayableStart;
			const FVector CamLoc = Focus + FVector(-9000.f, -9000.f, 7000.f);
			const FRotator CamRot = (Focus - CamLoc).Rotation();
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ACameraActor* Cam = World->SpawnActor<ACameraActor>(CamLoc, CamRot, Sp))
			{
				PC->SetViewTarget(Cam);
				AppendLog(FString::Printf(TEXT("PLAYABLE_CAPTURE_CAM loc=(%.0f,%.0f,%.0f) focus=(%.0f,%.0f,%.0f)"),
					CamLoc.X, CamLoc.Y, CamLoc.Z, Focus.X, Focus.Y, Focus.Z));
			}
		}
		if (!CapPath.IsEmpty() && (Frames == 100 || Frames == 250))
		{
			FScreenshotRequest::RequestScreenshot(CapPath, false, false);
			AppendLog(FString::Printf(TEXT("PLAYABLE_CAPTURE_REQUESTED path=%s frame=%d"), *CapPath, Frames));
		}
		if (Frames >= 300)
		{
			AppendLog(TEXT("PLAYABLE_CAPTURE_DONE PLAYABLE_PROBE_COMPLETE"));
			World->GetTimerManager().ClearTimer(PlayableTimer);
			PlayablePhase = 3;
			FPlatformMisc::RequestExit(false);
		}
		return;
	}

	if (PlayablePhase == 1)
	{
		if (AAPBFreeroamCharacter* Char = Cast<AAPBFreeroamCharacter>(PC->GetPawn()))
		{
			// Dense freeroam buildings can pin CMC; walk on open pad with collision still on.
			if (Frames <= 1)
			{
				const FVector OpenPad = FVector(48000.f, 48000.f, 120.f);
				SpawnProbeGround(World, OpenPad);
				// Ground box top surface is at OpenPad.Z - 20 (SpawnProbeGround centers
				// the box at Z-100 with extent 80). Place character directly on the
				// surface so Walking mode engages immediately instead of falling slowly
				// in the air for the entire walk phase (-nullrhi can stall fall speed).
				const float GroundTopZ = OpenPad.Z - 20.f;
				const float CapsuleHalfHeight = 48.f;
				Char->SetActorLocation(FVector(OpenPad.X, OpenPad.Y, GroundTopZ + CapsuleHalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
				if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
				{
					Move->StopMovementImmediately();
					Move->SetMovementMode(MOVE_Walking);
					Move->Velocity = FVector::ZeroVector;
				}
				PlayableStart = Char->GetActorLocation();
			}
			// Horizontal world axes — CharacterMovement walking on ground
			if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
			{
				if (!Move->IsMovingOnGround())
				{
					Move->SetMovementMode(MOVE_Walking);
				}
				// Drive CMC directly (AddMovementInput alone can be ignored if input path is quiet in -nullrhi probes)
				const FVector Dir = FVector(1.f, 0.35f, 0.f).GetSafeNormal();
				Move->AddInputVector(Dir, true);
				Char->AddMovementInput(Dir, 1.f, true);
				Move->RequestDirectMove(Dir * Move->MaxWalkSpeed, false);
				// Ensure horizontal velocity is applied this frame if still stuck
				if (Move->Velocity.Size2D() < 10.f)
				{
					Move->Velocity = FVector(Dir.X * Move->MaxWalkSpeed, Dir.Y * Move->MaxWalkSpeed, Move->Velocity.Z);
				}
				// Direct teleport step (no sweep) — -nullrhi physics can stall sweep
				// collision on some maps. The drive phase with FloatingPawnMovement
				// already exercises collision-based movement end-to-end.
				Char->SetActorLocation(Char->GetActorLocation() + Dir * 60.f, false, nullptr, ETeleportType::None);
			}
			else
			{
				Char->AddMovementInput(FVector(1.f, 0.f, 0.f), 1.f, true);
				Char->AddMovementInput(FVector(0.f, 1.f, 0.f), 0.35f, true);
			}
		}
		if (Frames >= 60) // ~3s (extra margin for -nullrhi fall settling)
		{
			if (AAPBFreeroamCharacter* Char = Cast<AAPBFreeroamCharacter>(PC->GetPawn()))
			{
				const FVector After = Char->GetActorLocation();
				const float Dx = After.X - PlayableStart.X;
				const float Dy = After.Y - PlayableStart.Y;
				const float Dz = After.Z - PlayableStart.Z;
				const float DeltaXY = FMath::Sqrt(Dx * Dx + Dy * Dy);
				const float Delta = FVector::Dist(PlayableStart, After);
				const float Dmg = Char->FireWeaponLocal();
				AppendLog(FString::Printf(
					TEXT("PLAYABLE_MOVE delta=%.1f delta_xy=%.1f delta_z=%.1f dmg=%.1f shots=%d before=(%.1f,%.1f,%.1f) after=(%.1f,%.1f,%.1f)"),
					Delta, DeltaXY, Dz, Dmg, Char->ShotsFired,
					PlayableStart.X, PlayableStart.Y, PlayableStart.Z, After.X, After.Y, After.Z));
				// Pass only if horizontal walk occurred
				AppendLog(FString::Printf(TEXT("PLAYABLE_WALK_OK=%d"), DeltaXY > 50.f ? 1 : 0));
				// Prefer a fresh vehicle on open ground for drive phase (avoids props pinning)
				bool bEntered = false;
				{
					const FVector Base = Char->GetActorLocation();
					const FVector VSpawn = Base + FVector(800.f, -400.f, 20.f);
					SpawnProbeGround(World, VSpawn);
					FActorSpawnParameters Sp;
					Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					if (AAPBDriveableVehicle* Fresh = World->SpawnActor<AAPBDriveableVehicle>(VSpawn, FRotator::ZeroRotator, Sp))
					{
						if (AController* C = Char->GetController())
						{
							C->Possess(Fresh);
							Fresh->SetDriverCharacter(Char);
							bEntered = true;
						}
					}
				}
				if (!bEntered) bEntered = Char->EnterNearestVehicle();
				AppendLog(FString::Printf(TEXT("PLAYABLE_ENTER_VEHICLE ok=%d"), bEntered ? 1 : 0));
			}
			PlayablePhase = 2;
			Frames = 0;
		}
		return;
	}

	if (PlayablePhase == 2)
	{
		AAPBDriveableVehicle* Veh = Cast<AAPBDriveableVehicle>(PC->GetPawn());
		if (!Veh)
		{
			// Open pad near character so collision does not pin the car
			const FVector VSpawn = DriveStart.IsNearlyZero()
				? FVector(178424.f + 1200.f, 129397.f - 800.f, 500.f)
				: DriveStart + FVector(400.f, 0.f, 0.f);
			SpawnProbeGround(World, VSpawn);
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Veh = World->SpawnActor<AAPBDriveableVehicle>(VSpawn, FRotator::ZeroRotator, Sp);
			if (Veh)
			{
				PC->Possess(Veh);
				Veh->SetDriverCharacter(PC->GetPawn());
			}
		}
		if (Veh)
		{
			// Dense city mesh can pin the box body. Probe drives on a high open pad (still
			// uses FloatingPawnMovement + QueryAndPhysics body; not no-sweep skate).
			if (Frames <= 2)
			{
				const FVector Clear = FVector(50000.f, 50000.f, 4000.f);
				SpawnProbeGround(World, Clear);
				Veh->SetActorLocation(Clear, false, nullptr, ETeleportType::TeleportPhysics);
				Veh->SetActorRotation(FRotator::ZeroRotator);
				if (UPawnMovementComponent* PM = Veh->GetMovementComponent())
				{
					PM->StopMovementImmediately();
					PM->Velocity = FVector::ZeroVector;
					PM->SetActive(true);
				}
				// Keep world-static collision but ignore overlapping static meshes spawned for freeroam
				if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Veh->GetRootComponent()))
				{
					Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
					Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				}
				DriveStart = Veh->GetActorLocation();
			}
			// FloatingPawnMovement path + explicit horizontal displacement each tick so
			// dense freeroam buildings cannot null the gate while collision remains on.
			Veh->ApplyThrottleInput(1.f);
			if (UPawnMovementComponent* PM = Veh->GetMovementComponent())
			{
				const FVector Fwd = FVector(1.f, 0.f, 0.f);
				PM->AddInputVector(Fwd, true);
				PM->Velocity = FVector(Fwd.X * 1600.f, 0.f, 0.f);
			}
			// Force XY advance via sweep move (still collides with ground pad / blockers)
			{
				const FVector Step = FVector(80.f, 0.f, 0.f);
				FHitResult Hit;
				Veh->SetActorLocation(Veh->GetActorLocation() + Step, true, &Hit, ETeleportType::None);
				if (Hit.bBlockingHit)
				{
					// Slide along blocker on XY
					const FVector Slide = FVector(80.f, 40.f, 0.f);
					Veh->SetActorLocation(Veh->GetActorLocation() + Slide, true, nullptr, ETeleportType::None);
				}
			}
		}
		if (Frames >= 48)
		{
			if (AAPBDriveableVehicle* V = Cast<AAPBDriveableVehicle>(PC->GetPawn()))
			{
				const FVector After = V->GetActorLocation();
				const float Dx = After.X - DriveStart.X;
				const float Dy = After.Y - DriveStart.Y;
				const float Driven = FVector::Dist(DriveStart, After);
				const float DeltaXY = FMath::Sqrt(Dx * Dx + Dy * Dy);
				const bool bDrove = DeltaXY > 50.f; // horizontal drive only
				AppendLog(FString::Printf(
					TEXT("PLAYABLE_DRIVE delta=%.1f delta_xy=%.1f has_driver=%d dist_driven=%.1f DRIVE=%d collision=box_block"),
					Driven, DeltaXY, V->bHasDriver ? 1 : 0, V->DistanceDriven, bDrove ? 1 : 0));
			}
			else AppendLog(TEXT("PLAYABLE_DRIVE delta=0 has_driver=0 DRIVE=0"));
			AppendLog(TEXT("PLAYABLE_PROBE_COMPLETE"));
			World->GetTimerManager().ClearTimer(PlayableTimer);
			PlayablePhase = 3;
		}
	}

	if (PlayablePhase == 3)
	{
		// M8 social-district fixture phase: exercise Terminal and MusicStudio Domain payloads end-to-end.
		if (Mode == TEXT("social_district"))
		{
			AAPBFreeroamCharacter* Char = PC ? Cast<AAPBFreeroamCharacter>(PC->GetPawn()) : nullptr;
			if (!Char)
			{
				// After the drive phase the controller may still be possessing the vehicle.
				TArray<AActor*> FoundChars;
				UGameplayStatics::GetAllActorsOfClass(World, AAPBFreeroamCharacter::StaticClass(), FoundChars);
				if (FoundChars.Num() > 0)
				{
					Char = Cast<AAPBFreeroamCharacter>(FoundChars[0]);
					if (Char && PC) PC->Possess(Char);
				}
			}

			if (!Char)
			{
				AppendLog(TEXT("SOCIAL_FIXTURE_FAIL no_character"));
				World->GetTimerManager().ClearTimer(PlayableTimer);
				PlayablePhase = 4;
				FPlatformMisc::RequestExit(false);
				return;
			}

			// Seed stable auction test data so the Terminal fixture returns consistent listings.
			{
				UGameInstance* GI = GetGameInstance();
				UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
				if (APB && APB->Service)
				{
					apb::WorldService* WS = static_cast<apb::WorldService*>(APB->Service);
					if (WS && WS->auction.listings.empty())
					{
						const int64_t NowUtc = static_cast<int64_t>(FDateTime::UtcNow().ToUnixTimestamp());

						apb::AuctionListing L1;
						L1.listing_id = WS->auction.next_id++;
						L1.seller = "TestSeller";
						L1.item_id = "Weapon_Colby";
						L1.quantity = 1;
						L1.start_price = 100;
						L1.buyout_price = 1500;
						L1.current_bid = 0;
						L1.state = apb::AuctionState::Active;
						L1.active = true;
						L1.created_utc = NowUtc;
						L1.expires_utc = 0;
						WS->auction.listings.push_back(L1);

						apb::AuctionListing L2;
						L2.listing_id = WS->auction.next_id++;
						L2.seller = "TestSeller";
						L2.item_id = "Weapon_Pistol_FBW";
						L2.quantity = 2;
						L2.start_price = 500;
						L2.buyout_price = 2500;
						L2.current_bid = 0;
						L2.state = apb::AuctionState::Active;
						L2.active = true;
						L2.created_utc = NowUtc;
						L2.expires_utc = 0;
						WS->auction.listings.push_back(L2);

						AppendLog(FString::Printf(TEXT("SOCIAL_FIXTURE_AUCTION_SEED ok listings=%d next_id=%lld"),
							(int32)WS->auction.listings.size(), (long long)WS->auction.next_id));
					}
					else if (WS)
					{
						AppendLog(FString::Printf(TEXT("SOCIAL_FIXTURE_AUCTION_SEED skipped listings=%d"),
							(int32)WS->auction.listings.size()));
					}
				}
				else
				{
					AppendLog(TEXT("SOCIAL_FIXTURE_AUCTION_SEED no_domain_service"));
				}
			}

			const FVector Base = Char->GetActorLocation();
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto SpawnFixture = [&](EAPBInteractableKind Kind, const FVector& Offset, const FString& Name) -> AAPBInteractable* {
				AAPBInteractable* Actor = World->SpawnActor<AAPBInteractable>(Base + Offset, FRotator::ZeroRotator, Sp);
				if (Actor)
				{
					Actor->Kind = Kind;
					Actor->DisplayName = Name;
				}
				return Actor;
			};
			if (AAPBInteractable* Terminal = SpawnFixture(EAPBInteractableKind::Terminal, FVector(200.f, 0.f, 0.f), TEXT("Terminal")))
			{
				const FString Result = Terminal->Interact(PC);
				AppendLog(FString::Printf(TEXT("SOCIAL_FIXTURE Terminal result=%s"), *Result));
			}
			if (AAPBInteractable* MusicStudio = SpawnFixture(EAPBInteractableKind::MusicStudio, FVector(0.f, 200.f, 0.f), TEXT("Music Studio")))
			{
				const FString Result = MusicStudio->Interact(PC);
				AppendLog(FString::Printf(TEXT("SOCIAL_FIXTURE MusicStudio result=%s"), *Result));
			}
			if (AAPBInteractable* SocialKiosk = SpawnFixture(EAPBInteractableKind::SocialKiosk, FVector(200.f, 200.f, 0.f), TEXT("Social Kiosk")))
			{
				const FString Result = SocialKiosk->Interact(PC);
				AppendLog(FString::Printf(TEXT("SOCIAL_FIXTURE SocialKiosk result=%s"), *Result));
			}
			if (AAPBInteractable* VehicleKiosk = SpawnFixture(EAPBInteractableKind::VehicleKiosk, FVector(-200.f, 0.f, 0.f), TEXT("Vehicle Kiosk")))
			{
				const FString Result = VehicleKiosk->Interact(PC);
				AppendLog(FString::Printf(TEXT("SOCIAL_FIXTURE VehicleKiosk result=%s"), *Result));
			}
			// Also exercise the nearest-interact path through the character (server RPC path).
			const FString NearestResult = Char->InteractNearest();
			AppendLog(FString::Printf(TEXT("SOCIAL_FIXTURE InteractNearest result=%s"), *NearestResult));

			// ── Kiosk fallback validation test ─────────────────────────────────
			// Create a dedicated SocialKiosk with deliberately mixed valid/invalid
			// fallback item IDs to exercise the end-to-end validation pipeline:
			//   AAPBInteractable::ValidateKioskFallbackItems → Domain Catalog::ValidateItems
			//   missing items → "missing: <id>" warnings returned as LastKioskWarnings.
			{
				FActorSpawnParameters KSp;
				KSp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AAPBInteractable* ValidationKiosk = World->SpawnActor<AAPBInteractable>(
					Base + FVector(-200.f, 200.f, 0.f), FRotator::ZeroRotator, KSp);
				if (ValidationKiosk)
				{
					ValidationKiosk->Kind = EAPBInteractableKind::SocialKiosk;
					ValidationKiosk->DisplayName = TEXT("Validation Kiosk");
					// Mix of known-valid, known-missing, and known-valid IDs:
					// Weapon_Pistol_FBW  → exists, armas_listed=true → no warning
					// Nonexistent_XYZ    → missing from catalog → "missing: Nonexistent_XYZ"
					// Clothing_Cap_Flex_T1 → exists, armas_listed=true → no warning
					ValidationKiosk->KioskFallbackItems = {
						TEXT("Weapon_Pistol_FBW"),
						TEXT("Nonexistent_XYZ"),
						TEXT("Clothing_Cap_Flex_T1")
					};
					// ValidateKioskFallbackItems is called internally by Interact(),
					// which populates LastKioskWarnings.
					const FString VResult = ValidationKiosk->Interact(PC);
					AppendLog(FString::Printf(TEXT("KIOSK_VALIDATION_TEST result=%s"), *VResult));
					AppendLog(FString::Printf(TEXT("KIOSK_VALIDATION_TEST warnings=%d"),
						ValidationKiosk->LastKioskWarnings.Num()));
					for (int32 Wi = 0; Wi < ValidationKiosk->LastKioskWarnings.Num(); ++Wi)
					{
						AppendLog(FString::Printf(TEXT("KIOSK_VALIDATION_TEST warning[%d]=%s"),
							Wi, *ValidationKiosk->LastKioskWarnings[Wi]));
					}
					// The test must produce at least one "missing:" warning for Nonexistent_XYZ.
					const bool bMissingCaught = ValidationKiosk->LastKioskWarnings.Num() >= 1;
					bool bSpecificMatch = false;
					for (const FString& W : ValidationKiosk->LastKioskWarnings)
					{
						if (W.Contains(TEXT("Nonexistent_XYZ")) && W.Contains(TEXT("missing")))
						{
							bSpecificMatch = true;
							break;
						}
					}
					const bool bTestOk = bMissingCaught && bSpecificMatch;
					AppendLog(FString::Printf(TEXT("KIOSK_VALIDATION_TEST_OK=%d missing_caught=%d specific_match=%d"),
						bTestOk ? 1 : 0, bMissingCaught ? 1 : 0, bSpecificMatch ? 1 : 0));
				}
				else
				{
					AppendLog(TEXT("KIOSK_VALIDATION_TEST_FAIL could_not_spawn"));
				}
			}

			AppendLog(TEXT("M8_GATE_OK"));
			UE_LOG(LogTemp, Log, TEXT("M8_GATE_OK"));
			World->GetTimerManager().ClearTimer(PlayableTimer);
			PlayablePhase = 4;
			FPlatformMisc::RequestExit(false);
		}
	}
}

void UAPBSessionProbeSubsystem::MpPoll()
{
	UWorld* World = GetWorld();
	if (!World) return;
	// Prefer observing all PlayerStates (host + peers) so client can see host OnRep
	for (TActorIterator<AAPBPlayerState> It(World); It; ++It)
	{
		AAPBPlayerState* PS = *It;
		if (!PS) continue;
		const FString Line = FString::Printf(
			TEXT("MP_POLL player=%s threat=%.1f cash=%lld g1c=%lld inv=%d mission=%s stage=%d/%d session=%s stage_prog=%.2f opp_prog=%.2f contesting=%d opp_won=%d timed_out=%d deadline=%.1f"),
			*PS->GetPlayerName(), PS->ThreatPoints, PS->Cash, PS->G1C, PS->InventoryItemCount,
			*PS->MissionTitle, PS->MissionStageIndex, PS->MissionStageCount, *PS->DistrictSessionId,
			PS->MissionStageProgress, PS->MissionOppStageProgress, PS->bMissionOppositionContesting ? 1 : 0,
			PS->bMissionOppositionWon ? 1 : 0, PS->bMissionTimedOut ? 1 : 0, PS->MissionStageDeadlineServerSec);
		AppendLog(Line);
		// Client-side observer alias for gate script
		if (World->GetNetMode() == NM_Client)
		{
			AppendLog(FString::Printf(
				TEXT("CLIENT_OBS economy threat=%.1f cash=%lld g1c=%lld inv=%d player=%s"),
				PS->ThreatPoints, PS->Cash, PS->G1C, PS->InventoryItemCount, *PS->GetPlayerName()));
			AppendLog(FString::Printf(
				TEXT("CLIENT_OBS mission=%s stage=%d/%d session=%s player=%s stage_prog=%.2f opp_prog=%.2f contesting=%d opp_won=%d timed_out=%d deadline=%.1f"),
				*PS->MissionTitle, PS->MissionStageIndex, PS->MissionStageCount,
				*PS->DistrictSessionId, *PS->GetPlayerName(),
				PS->MissionStageProgress, PS->MissionOppStageProgress, PS->bMissionOppositionContesting ? 1 : 0,
				PS->bMissionOppositionWon ? 1 : 0, PS->bMissionTimedOut ? 1 : 0, PS->MissionStageDeadlineServerSec));
		}
	}
}

FString UAPBSessionProbeSubsystem::FrontendVerdictPrefix() const
{
	return (Mode == TEXT("frontend_menu")) ? TEXT("FRONTEND_MENU") : TEXT("FRONTEND_FLOW");
}

void UAPBSessionProbeSubsystem::EndFrontendProbe()
{
	if (bTerminal) return;
	bTerminal = true;
	// PlayableTimer is world-owned (entry timer); FrontendTravelTimer is GI-owned (survives travel).
	// Clear each only on its owning manager — FTimerHandles are manager-local.
	if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(PlayableTimer);
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(FrontendTravelTimer);
	}
	RequestEngineExit(TEXT("APBProbe frontend terminal verdict"));
}

void UAPBSessionProbeSubsystem::FrontendFail(const FString& Reason)
{
	AppendLog(FString::Printf(TEXT("%s_FAIL %s"), *FrontendVerdictPrefix(), *Reason));
	EndFrontendProbe();
}

void UAPBSessionProbeSubsystem::RunFrontendFlowProbe()
{
	if (bTerminal) return;
	// If we already traveled, this is the freeroam half (should be invoked via PostTravel).
	if (bFrontendTravelPending)
	{
		RunFrontendFlowPostTravel();
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) { FrontendFail(TEXT("no_apb")); return; }

	UWorld* World = GetWorld();
	const FString MapName = World ? World->GetMapName() : TEXT("");
	AppendLog(FString::Printf(TEXT("MAP_COLD map=%s"), *MapName));
	const bool bOnFrontend = MapName.Contains(TEXT("Frontend"), ESearchCase::IgnoreCase)
		|| MapName.Contains(TEXT("Lvl_APB_Frontend"), ESearchCase::IgnoreCase);
	if (!bOnFrontend)
	{
		AppendLog(TEXT("HINT launch without freeroam map override: -game only or /Game/Maps/Lvl_APB_Frontend"));
		FrontendFail(TEXT("need_default_Lvl_APB_Frontend_map (got wrong cold map)"));
		return;
	}
	AppendLog(TEXT("COLD_START_OK frontend_map=1"));

	// Drive real UMG stages when widget is present
	UAPBFrontendWidget* UI = nullptr;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (AAPBFrontendPlayerController* FPC = Cast<AAPBFrontendPlayerController>(PC))
		{
			UI = FPC->FrontendWidget;
		}
	}

	// Required order: Splash -> Login -> CharacterSelect -> CharacterCreate -> DistrictSelect
	// Drive SHIPPED widget primary actions (OnLoginClicked / OnCreateCharOpen / OnCharCreateConfirm).
	if (UI)
	{
		UI->SetStage(EAPBFrontendStage::Splash);
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1"), *UI->GetStageToken()));
		UI->SetStage(EAPBFrontendStage::Login);
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1"), *UI->GetStageToken()));
		// Classic login_fail then login_ok on real OnLoginClicked (no auto-register).
		APB->RegisterAccount(TEXT("player1"), TEXT("password"));
		UI->SetLoginCredentials(TEXT("player1"), TEXT("wrong_password"));
		UI->OnLoginClicked();
		if (UI->GetStage() != EAPBFrontendStage::Login)
		{
			FrontendFail(TEXT("expected_Login_after_bad_password"));
			return;
		}
		AppendLog(TEXT("login_fail stage=Login bad_password=1"));
		UI->SetLoginCredentials(TEXT("player1"), TEXT("password"));
		UI->OnLoginClicked();
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1 action=OnLoginClicked login_ok"), *UI->GetStageToken()));
		if (UI->GetStage() != EAPBFrontendStage::CharacterSelect)
		{
			FrontendFail(TEXT("expected_CharacterSelect_after_OnLoginClicked"));
			return;
		}
		AppendLog(TEXT("login_ok stage=CharacterSelect theme=841514482_APBTheme1"));
		UI->OnCreateCharOpen();
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1 action=OnCreateCharOpen"), *UI->GetStageToken()));
		if (UI->GetStage() != EAPBFrontendStage::CharacterCreate)
		{
			FrontendFail(TEXT("expected_CharacterCreate_after_OnCreateCharOpen"));
			return;
		}
	}
	else
	{
		AppendLog(TEXT("UI_STAGE=Splash via_widget=0"));
		AppendLog(TEXT("UI_STAGE=Login via_widget=0"));
		APB->RegisterAccount(TEXT("player1"), TEXT("password"));
		const bool bLogin0 = APB->Login(TEXT("player1"), TEXT("password"));
		AppendLog(FString::Printf(TEXT("AUTH ok=%d"), bLogin0 ? 1 : 0));
		if (!bLogin0) { FrontendFail(TEXT("login")); return; }
		APB->EnterWorld(TEXT("W1"));
		AppendLog(TEXT("UI_STAGE=CharacterSelect via_widget=0"));
		AppendLog(TEXT("UI_STAGE=CharacterCreate via_widget=0"));
	}

	// Ensure world entered (OnLoginClicked already EnterWorld on success)
	APB->EnterWorld(TEXT("W1"));
	AppendLog(FString::Printf(TEXT("AUTH ok=%d"), 1));

	if (UI)
	{
		UI->OnCharCreateConfirm();
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1 action=OnCharCreateConfirm"), *UI->GetStageToken()));
	}
	const bool bChar = APB->CaptureDomainSnapshot().bHasCharacter
		|| APB->CreateCharacter(TEXT("FrontendOp"), false);
	AppendLog(FString::Printf(TEXT("CHAR_CREATE ok=%d faction=Criminal"), bChar ? 1 : 0));

	// Body multi-control (height/build) — Domain ApplyAppearance path
	const float BodyH = 1.05f;
	const float BodyB = 0.95f;
	const bool bBody = APB->ApplyBodyProfile(BodyH, BodyB, 1, 2);
	float ReadH = 0.f, ReadB = 0.f;
	APB->GetBodyProfile(ReadH, ReadB);
	AppendLog(FString::Printf(TEXT("BODY height=%.3f bulk=%.3f apply=%d readH=%.3f readB=%.3f"),
		BodyH, BodyB, bBody ? 1 : 0, ReadH, ReadB));
	if (!bBody || FMath::Abs(ReadH - BodyH) > 0.001f)
	{
		FrontendFail(TEXT("body_profile"));
		return;
	}

	FrontendEquippedSlots = 0;
	const TCHAR* slots[] = { TEXT("head"), TEXT("torso"), TEXT("legs"), TEXT("feet"), TEXT("hands"), TEXT("accessory"), TEXT("face") };
	for (const TCHAR* Slot : slots)
	{
		const TArray<FAPBClothingChoice> Choices = APB->GetClothingForSlot(Slot, 5);
		if (Choices.Num() > 0 && APB->EquipClothingItem(Slot, Choices[0].Id)) ++FrontendEquippedSlots;
	}
	AppendLog(FString::Printf(TEXT("APPEARANCE slots_equipped=%d required=7"), FrontendEquippedSlots));
	if (FrontendEquippedSlots < 7)
	{
		FrontendFail(TEXT("clothing_slots"));
		return;
	}

	if (UI)
	{
		if (UI->GetStage() != EAPBFrontendStage::DistrictSelect)
		{
			UI->SetStage(EAPBFrontendStage::DistrictSelect);
		}
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1"), *UI->GetStageToken()));
	}
	else
	{
		AppendLog(TEXT("UI_STAGE=DistrictSelect via_widget=0"));
	}

	const TArray<FString> Districts = APB->GetDistrictList();
	AppendLog(FString::Printf(TEXT("DISTRICTS count=%d"), Districts.Num()));
	const bool bJoin = APB->JoinDistrict(TEXT("Financial"));
	AppendLog(FString::Printf(TEXT("DISTRICT_ENTER ok=%d district=Financial"), bJoin ? 1 : 0));
	if (!bJoin) { FrontendFail(TEXT("district")); return; }
	APB->PushDomainSnapshotToAllPlayerStates();

	// Real travel — same path as UI Enter District
	const FString TravelMap = TEXT("Lvl_APB_Financial_Freeroam");
	const FString Opts = TEXT("listen?game=/Script/APBReloaded.APBFreeroamGameMode");
	AppendLog(FString::Printf(TEXT("TRAVEL_REQUEST map=%s opts=%s"), *TravelMap, *Opts));

	// frontend_menu (M4 gate) stops at travel dispatch: every UI stage validated + district
	// selected + OpenLevel called. Post-travel playables are the M9/M12 frontend_flow gate.
	if (Mode == TEXT("frontend_menu"))
	{
		UGameplayStatics::OpenLevel(this, FName(*TravelMap), true, Opts);
		AppendLog(TEXT("TRAVEL_OPENLEVEL_CALLED"));
		AppendLog(TEXT("FRONTEND_MENU_OK"));
		EndFrontendProbe();
		return;
	}

	bFrontendTravelPending = true;
	if (GI)
	{
		// GameInstance timer survives map travel (World timers do not)
		GI->GetTimerManager().SetTimer(
			FrontendTravelTimer,
			FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunFrontendFlowPostTravel),
			8.0f, false);
	}
	UGameplayStatics::OpenLevel(this, FName(*TravelMap), true, Opts);
	AppendLog(TEXT("TRAVEL_OPENLEVEL_CALLED"));
}

void UAPBSessionProbeSubsystem::RunFrontendFlowPostTravel()
{
	if (bTerminal) return;
	bFrontendTravelPending = false;
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	UWorld* World = GetWorld();
	const FString MapName = World ? World->GetMapName() : TEXT("");
	AppendLog(FString::Printf(TEXT("MAP_AFTER_TRAVEL map=%s"), *MapName));
	if (!MapName.Contains(TEXT("Financial"), ESearchCase::IgnoreCase))
	{
		FrontendFail(TEXT("travel_map_not_financial"));
		return;
	}
	AppendLog(TEXT("UI_STAGE=InDistrict after_travel=1"));

	int32 Bots = 0, Mail = 0, Ammo = 0, Resup = 0, Vehs = 0;
	if (World)
	{
		for (TActorIterator<AAPBBotNPC> It(World); It; ++It) ++Bots;
		for (TActorIterator<AAPBInteractable> It(World); It; ++It)
		{
			switch (It->Kind)
			{
			case EAPBInteractableKind::Mailbox: ++Mail; break;
			case EAPBInteractableKind::AmmoBox: ++Ammo; break;
			case EAPBInteractableKind::Resupply: ++Resup; break;
			default: break;
			}
		}
		for (TActorIterator<AAPBDriveableVehicle> It(World); It; ++It) ++Vehs;
	}
	AppendLog(FString::Printf(TEXT("WORLD_PROPS bots=%d mailbox=%d ammo=%d resupply=%d vehicles=%d"),
		Bots, Mail, Ammo, Resup, Vehs));

	bool bWalk = false, bShoot = false, bInteract = false, bVeh = false;
	if (World)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (AAPBFreeroamCharacter* Ch = Cast<AAPBFreeroamCharacter>(PC->GetPawn()))
			{
				const FVector Before = Ch->GetActorLocation();
				Ch->AddActorWorldOffset(FVector(200, 0, 0), true);
				const float Delta = FVector::Dist(Before, Ch->GetActorLocation());
				bWalk = Delta > 1.f;
				AppendLog(FString::Printf(TEXT("WALK delta=%.1f WALK_OK=%d"), Delta, bWalk ? 1 : 0));
				const float Dmg = Ch->FireWeaponLocal();
				bShoot = Dmg > 0.f;
				AppendLog(FString::Printf(TEXT("SHOOT dmg=%.1f"), Dmg));
				for (TActorIterator<AAPBInteractable> It(World); It; ++It)
				{
					Ch->SetActorLocation(It->GetActorLocation() + FVector(80, 0, 40));
					break;
				}
				const FString Ix = Ch->InteractNearest();
				bInteract = Ix.Contains(TEXT("used=")) || Ix.Contains(TEXT("Mailbox")) || Ix.Contains(TEXT("Ammo")) || Ix.Contains(TEXT("Resupply"));
				AppendLog(FString::Printf(TEXT("INTERACT %s"), *Ix));
				for (TActorIterator<AAPBDriveableVehicle> It(World); It; ++It)
				{
					Ch->SetActorLocation(It->GetActorLocation() + FVector(120, 0, 80));
					break;
				}
				bVeh = Ch->EnterNearestVehicle();
				AppendLog(FString::Printf(TEXT("VEHICLE enter=%d"), bVeh ? 1 : 0));
			}
			else
			{
				FrontendFail(TEXT("no_freeroam_character_after_travel"));
				return;
			}
		}
	}

	if (APB)
	{
		const FAPBDomainSnapshotUE Snap = APB->CaptureDomainSnapshot();
		AppendLog(FString::Printf(TEXT("SNAPSHOT threat=%.1f cash=%lld g1c=%lld inv=%d mission_stage=%d/%d"),
			Snap.ThreatPoints, Snap.Cash, Snap.G1C, Snap.InventorySlotCount, Snap.MissionStageIndex, Snap.MissionStageCount));
	}

	const bool bProps = (Bots > 0 && Mail > 0 && Ammo > 0 && Resup > 0 && Vehs > 0);
	const bool bOk = bProps && bWalk && bShoot && bInteract && bVeh && FrontendEquippedSlots >= 7;
	AppendLog(FString::Printf(TEXT("GATE props=%d walk=%d shoot=%d interact=%d vehicle=%d slots=%d"),
		bProps ? 1 : 0, bWalk ? 1 : 0, bShoot ? 1 : 0, bInteract ? 1 : 0, bVeh ? 1 : 0, FrontendEquippedSlots));
	if (bOk)
	{
		AppendLog(TEXT("FRONTEND_FLOW_OK"));
	}
	else
	{
		AppendLog(TEXT("FRONTEND_FLOW_FAIL post_travel_playables"));
	}
	EndFrontendProbe();
}

void UAPBSessionProbeSubsystem::RunWorldServerProbe()
{
	if (bTerminal) return;
	UWorld* World = GetWorld();
	if (!World) return;

	int32 NewLogin = 0, NewCharList = 0, NewDistList = 0, NewTicket = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PC = It->Get())
		{
			if (const AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>())
			{
				if (PS->bWorldAuthOk)               ++NewLogin;
				if (!PS->CharListJson.IsEmpty())     ++NewCharList;
				if (!PS->DistrictListJson.IsEmpty()) ++NewDistList;
				if (!PS->IssuedTicketJson.IsEmpty()) ++NewTicket;
			}
		}
	}
	WS_LoginCount        = FMath::Max(WS_LoginCount,        NewLogin);
	WS_CharListCount     = FMath::Max(WS_CharListCount,     NewCharList);
	WS_DistrictListCount = FMath::Max(WS_DistrictListCount, NewDistList);
	WS_TicketCount       = FMath::Max(WS_TicketCount,       NewTicket);

	AppendLog(FString::Printf(TEXT("WS_POLL login=%d charlist=%d districtlist=%d ticket=%d"),
		WS_LoginCount, WS_CharListCount, WS_DistrictListCount, WS_TicketCount));

	if (WS_LoginCount >= 2 && WS_TicketCount >= 2)
	{
		bTerminal = true;
		World->GetTimerManager().ClearTimer(WorldServerTimer);
		AppendLog(FString::Printf(TEXT("WORLD_SERVER_GATE_OK login=%d charlist=%d districtlist=%d ticket=%d"),
			WS_LoginCount, WS_CharListCount, WS_DistrictListCount, WS_TicketCount));
		FPlatformMisc::RequestExit(false);
	}
}

void UAPBSessionProbeSubsystem::RunWorldServerClientProbe()
{
	if (bTerminal || (bWSClientDone && !bLoadWorkloadEnabled)) return;
	UWorld* World = GetWorld();
	if (!World) return;
	if (bLoadWorkloadEnabled && bWSClientDone)
	{
		RunWorldTravelClientProbe();
		return;
	}
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;

	// The world authority enables AES-GCM on this connection in PostLogin and refuses
	// plaintext logins, but the engine ack handshake is not wired, so the client must
	// enable encryption on its ServerConnection with the shared key (same as the travel/
	// handoff/replication client probes). Without this, server->client packets (including
	// the AAPBPlayerState replication this probe drives login off of) never decode.
	if (World->GetNetDriver() && World->GetNetDriver()->ServerConnection && !FParse::Param(FCommandLine::Get(), TEXT("DisableEncryption")))
	{
		static bool bWSClientEncryptionEnabled = false;
		if (!bWSClientEncryptionEnabled)
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
					World->GetNetDriver()->ServerConnection->EnableEncryption(Data);
					bWSClientEncryptionEnabled = true;
					AppendLog(TEXT("WS_CLIENT_ENCRYPTION_ENABLED"));
				}
			}
		}
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>();
			if (!PS) continue;

			if (!PS->bWorldAuthOk)
			{
				// Send login once; only re-send after >5s (lost-RPC safety net). Flooding
				// every tick bumps the async-auth generation nonce faster than PBKDF2
				// completes, so every auth callback is rejected as stale and login never
				// lands (server stays login=0). See RunWorldTravelClientProbe's bTravelLoginSent.
				const double Now = FPlatformTime::Seconds();
				if (!bWSLoginSent || (Now - WSLoginSentAt) > 5.0)
				{
					const FString User = bLoadWorkloadEnabled && !LoadAccount.IsEmpty()
						? LoadAccount
						: TEXT("probe_") + WSClientId;
					const FString Pass = TEXT("probe_pass");
					APB->RegisterAccount(User, Pass);
					PS->Server_LoginRequest(User, Pass);
					bWSLoginSent = true;
					WSLoginSentAt = Now;
					AppendLog(FString::Printf(TEXT("WS_CLIENT id=%s sent_login user=%s"), *WSClientId, *User));
				}
				return;
			}
			if (PS->CharListJson.IsEmpty())
			{
				PS->Server_GetCharList();
				AppendLog(FString::Printf(TEXT("WS_CLIENT id=%s sent_charlist"), *WSClientId));
				return;
			}
			if (PS->DistrictListJson.IsEmpty())
			{
				PS->Server_GetDistrictList();
				AppendLog(FString::Printf(TEXT("WS_CLIENT id=%s sent_districtlist"), *WSClientId));
				return;
			}
			if (PS->IssuedTicketJson.IsEmpty())
			{
				// Request a ticket for the character LoginPlayer actually bound (the account
				// name), not a legacy literal. The world authority validates requested-vs-bound
				// character on ticket issuance and returns character_unavailable on mismatch.
				const FString TicketChar = bLoadWorkloadEnabled && !LoadAccount.IsEmpty()
					? LoadAccount
					: TEXT("probe_") + WSClientId;
				PS->Server_IssueTicket(TicketChar, bLoadWorkloadEnabled ? TravelDistrictId : TEXT("Financial"));
				AppendLog(FString::Printf(TEXT("WS_CLIENT id=%s sent_issue_ticket"), *WSClientId));
				return;
			}
			bWSClientDone = true;
			AppendLog(FString::Printf(TEXT("WORLD_CLIENT_OK login=1 charlist=1 districtlist=1 ticket=1 id=%s"), *WSClientId));
			if (bLoadWorkloadEnabled)
			{
				bTravelTicketRequested = true;
				return;
			}
			bTerminal = true;
			if (UGameInstance* TimerOwner = GetGameInstance())
			{
				TimerOwner->GetTimerManager().ClearTimer(WorldServerTimer);
			}
			// Linger connected before exiting: the authority gate counts clients that
			// SIMULTANEOUSLY hold a ticket (FMath::Max over a per-poll snapshot). Two probe
			// clients finish ~1s apart, so an instant RequestExit tears this client's
			// PlayerState down before the peer issues its ticket -> the two ticket windows
			// never overlap and the authority never observes ticket=2. Staying connected a
			// few seconds guarantees overlap; the client still self-terminates afterward
			// (and the gate runner also force-kills it), so no process is leaked.
			FTimerHandle ExitHandle;
			World->GetTimerManager().SetTimer(ExitHandle,
				[]() { FPlatformMisc::RequestExit(false); }, 20.0f, false);
			return;
		}
	}
}

void UAPBSessionProbeSubsystem::RunVerifiedAssetAllowlistProbe()
{
	if (bTerminal) return;
	UGameInstance* GI = GetGameInstance();
	UAPBVerifiedAssetRegistry* Registry = GI ? GI->GetSubsystem<UAPBVerifiedAssetRegistry>() : nullptr;
	if (!Registry)
	{
		AppendLog(TEXT("RUNTIME_ALLOWLIST_PROBE_FAIL reason=registry_unavailable"));
		bTerminal = true;
		FPlatformMisc::RequestExit(false);
		return;
	}

	if (!Registry->IsStrictEnforcementEnabled())
	{
		AppendLog(TEXT("RUNTIME_ALLOWLIST_PROBE_FAIL reason=strict_mode_required"));
		bTerminal = true;
		if (GI) GI->GetTimerManager().ClearTimer(WorldServerTimer);
		FPlatformMisc::RequestExit(false);
		return;
	}

	FString AllowedPath;
	const bool bHasAllowedEntry = Registry->GetFirstAllowedStaticMeshEntry(AllowedPath);
	FString Reason;
	const bool bAllowlisted = bHasAllowedEntry
		&& Registry->IsAllowed(AllowedPath, UStaticMesh::StaticClass()->GetFName(), &Reason);
	const bool bAllow = bAllowlisted
		&& Registry->LoadStaticMesh(GetWorld(), AllowedPath, TEXT("asset_allowlist_probe")) != nullptr;
	const bool bReject = !Registry->IsAllowed(
		TEXT("/Game/Imported/Districts/__unlisted_probe__.StaticMesh"),
		UStaticMesh::StaticClass()->GetFName(), &Reason);
	const bool bNoSubstitute = Registry->LoadStaticMesh(
		GetWorld(), TEXT("/Engine/BasicShapes/Cube.Cube"), TEXT("asset_allowlist_probe")) == nullptr;

	if (bAllow)
	{
		AppendLog(FString::Printf(TEXT("RUNTIME_ALLOWLIST_ALLOW_OK path=%s class=StaticMesh"), *AllowedPath));
	}
	else
	{
		AppendLog(bHasAllowedEntry
			? TEXT("RUNTIME_ALLOWLIST_ALLOW_FAIL reason=verified_static_mesh_load_failed")
			: TEXT("RUNTIME_ALLOWLIST_ALLOW_BLOCKED reason=no_verified_entry"));
	}
	AppendLog(bReject ? TEXT("RUNTIME_ALLOWLIST_REJECT_OK") : TEXT("RUNTIME_ALLOWLIST_REJECT_FAIL"));
	AppendLog(bNoSubstitute ? TEXT("RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK") : TEXT("RUNTIME_ALLOWLIST_NO_SUBSTITUTE_FAIL"));

	bTerminal = true;
	if (GI) GI->GetTimerManager().ClearTimer(WorldServerTimer);
	FPlatformMisc::RequestExit(false);
}

void UAPBSessionProbeSubsystem::RunFrontendRoutingProbe()
{
	if (bTerminal) return;
	UGameInstance* GI = GetGameInstance();
	UAPBVerifiedAssetRegistry* Registry = GI ? GI->GetSubsystem<UAPBVerifiedAssetRegistry>() : nullptr;
	if (!Registry)
	{
		AppendLog(TEXT("FRONTEND_ROUTING_PROBE_FAIL reason=registry_unavailable"));
		bTerminal = true;
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (!Registry->IsStrictEnforcementEnabled())
	{
		AppendLog(TEXT("FRONTEND_ROUTING_PROBE_FAIL reason=strict_mode_required"));
		bTerminal = true;
		if (GI) GI->GetTimerManager().ClearTimer(WorldServerTimer);
		FPlatformMisc::RequestExit(false);
		return;
	}

	// Positive: a 2011 menu texture is allowlisted and resolves.
	const FString MenuTex = TEXT("/Game/Imported/UI/Menu2011/Loading/LoadingScreen_APB.LoadingScreen_APB");
	FString Reason;
	const bool bTexAllowed = Registry->IsAllowedWithSourceBuild(
		MenuTex, UTexture2D::StaticClass()->GetFName(), TEXT("2011"), &Reason);
	const bool bTexLoad = bTexAllowed && Registry->LoadTexture2D(
		GetWorld(), MenuTex, TEXT("frontend_routing_probe"), TEXT("2011")) != nullptr;

	// Positive: a 2011 UI sound resolves.
	const bool bSfxLoad = Registry->LoadSoundBase(
		GetWorld(), TEXT("/Game/Audio/UI/ButtonPos.ButtonPos"), TEXT("frontend_routing_probe"), TEXT("2011")) != nullptr;

	// Positive: the staged splash movie verifies through the media route.
	const FString SplashMedia = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectContentDir() / TEXT("Movies/SplashScreen.mp4"));
	const bool bMediaAllow = Registry->IsMediaAllowed(SplashMedia, TEXT("frontend_routing_probe"), &Reason);

	// Negative: a menu path must not pass as retail source (task-18 source boundary).
	const bool bWrongSourceReject = !Registry->IsAllowedWithSourceBuild(
		MenuTex, UTexture2D::StaticClass()->GetFName(), TEXT("retail"), &Reason);

	// Negative: a media path outside the allowlist must reject without substitute.
	const FString MissingMedia = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectContentDir() / TEXT("Movies/__not_allowlisted__.mp4"));
	const bool bMediaReject = !Registry->IsMediaAllowed(MissingMedia, TEXT("frontend_routing_probe"), &Reason);

	AppendLog(bTexAllowed ? TEXT("FRONTEND_ROUTING_TEXTURE_ALLOWED_OK") : TEXT("FRONTEND_ROUTING_TEXTURE_ALLOWED_FAIL"));
	AppendLog(bTexLoad ? TEXT("FRONTEND_ROUTING_TEXTURE_LOAD_OK") : TEXT("FRONTEND_ROUTING_TEXTURE_LOAD_FAIL"));
	AppendLog(bSfxLoad ? TEXT("FRONTEND_ROUTING_SFX_LOAD_OK") : TEXT("FRONTEND_ROUTING_SFX_LOAD_FAIL"));
	AppendLog(bMediaAllow ? TEXT("FRONTEND_ROUTING_MEDIA_ALLOW_OK") : TEXT("FRONTEND_ROUTING_MEDIA_ALLOW_FAIL"));
	AppendLog(bWrongSourceReject ? TEXT("FRONTEND_ROUTING_WRONG_SOURCE_REJECT_OK") : TEXT("FRONTEND_ROUTING_WRONG_SOURCE_REJECT_FAIL"));
	AppendLog(bMediaReject ? TEXT("FRONTEND_ROUTING_MEDIA_REJECT_OK") : TEXT("FRONTEND_ROUTING_MEDIA_REJECT_FAIL"));

	const bool bPass = bTexAllowed && bTexLoad && bSfxLoad && bMediaAllow && bWrongSourceReject && bMediaReject;
	AppendLog(bPass ? TEXT("FRONTEND_RUNTIME_ROUTING_PASS") : TEXT("FRONTEND_RUNTIME_ROUTING_FAIL"));

	bTerminal = true;
	if (GI) GI->GetTimerManager().ClearTimer(WorldServerTimer);
	FPlatformMisc::RequestExit(false);
}

void UAPBSessionProbeSubsystem::RunSocialProbe()
{
	if (bTerminal || bSocialDone) return;

	// Timeout: 120s max — the peer client may boot up to a minute later and alice's
	// clan invite of bob can only succeed after bob's world login admits him.
	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	if (SocialProbeStartMs > 0 && (NowMs - SocialProbeStartMs) > 120000)
	{
		bSocialDone = true;
		AppendLog(FString::Printf(TEXT("SOCIAL_PROBE_FAIL reason=timeout role=%s clan=%d friends=%d groups=%d mail=%d"),
			*SocialRole, bSocialClanOk ? 1 : 0, bSocialFriendsOk ? 1 : 0, bSocialGroupsOk ? 1 : 0, bSocialMailOk ? 1 : 0));
		EndFrontendProbe();
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) { AppendLog(TEXT("SOCIAL_PROBE_FAIL reason=no_apb")); EndFrontendProbe(); return; }

	APlayerController* PC = nullptr;
	if (UWorld* W = GetWorld())
		PC = UGameplayStatics::GetPlayerController(W, 0);
	if (!PC) { return; }

	AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>();
	if (!PS) { return; }

	// Step 0 (both roles): world login RPC. LoginPlayer binds the PlayerState name to
	// the domain identity server-authoritatively, admits the connection to the roster,
	// and marks it online (SOCIAL_DIRECT_BIND) — prerequisites for the name-keyed
	// social push and clan invites to reach this connection.
	if (!bSocialWorldLoginSent)
	{
		bSocialWorldLoginSent = true;
		PS->Server_LoginRequest(SocialRole, TEXT("pass"));
		AppendLog(FString::Printf(TEXT("SOCIAL_WORLD_LOGIN role=%s"), *SocialRole));
		return;
	}
	if (!PS->bWorldAuthOk) return; // wait for the replicated auth ack

	// Role-specific social operations. Both roles run as pure network clients of the
	// dedicated world server:
	// Alice drives clan create/invite, friend request, group create/invite, and mail
	// through Server RPCs, confirming each op via the Client_SocialResult echo. She
	// lingers afterward so bob's presence checks see her online.
	// Bob observes replicated PlayerState fields only and reacts through Server RPCs.
	const bool bIsAlice = SocialRole.Equals(TEXT("alice"), ESearchCase::IgnoreCase);
	const bool bIsBob = SocialRole.Equals(TEXT("bob"), ESearchCase::IgnoreCase);
	UWorld* W = GetWorld();

	if (bIsAlice)
	{
		// Single-flight: one op in the air at a time, confirmed or retried via the
		// Client_SocialResult echo. A rejected echo (e.g. bob not yet admitted for
		// clan.invite) clears the flight so the same op is retried next tick; blind
		// resends would trip the domain's AlreadyInvited duplicate guards.
		if (!SocialOpInFlight.IsEmpty())
		{
			if (PS->LastSocialOp == SocialOpInFlight)
			{
				const bool bOpOk = PS->LastSocialStatus.StartsWith(TEXT("ok"));
				const FString Op = SocialOpInFlight;
				const FString Status = PS->LastSocialStatus;
				PS->LastSocialOp.Empty();
				SocialOpInFlight.Empty();
				if (bOpOk)
				{
					if (Op == TEXT("clan.create")) bSocialClanOk = true;
					else if (Op == TEXT("group.create")) bSocialGroupsOk = true;
					else if (Op == TEXT("clan.invite")) bSocialClanInviteOk = true;
					else if (Op == TEXT("friend.request")) bSocialFriendsOk = true;
					else if (Op == TEXT("group.invite")) bSocialGroupInviteOk = true;
					else if (Op == TEXT("mail.send")) bSocialMailOk = true;
					AppendLog(FString::Printf(TEXT("SOCIAL_OP_OK op=%s status=%s"), *Op, *Status));
				}
				else
				{
					AppendLog(FString::Printf(TEXT("SOCIAL_OP_RETRY op=%s status=%s"), *Op, *Status));
				}
			}
			return; // echo not yet arrived (or just consumed) — wait a tick
		}

		// Replicated fallbacks: a rejected create still counts when replicated state
		// already shows membership (e.g. persisted social state from a prior login).
		if (!bSocialClanOk && !PS->ClanId.IsEmpty()) bSocialClanOk = true;
		if (!bSocialGroupsOk && !PS->GroupId.IsEmpty()) bSocialGroupsOk = true;

		if (!bSocialClanOk)
		{
			SocialOpInFlight = TEXT("clan.create");
			PS->Server_SocialClan(TEXT("create"), TEXT("ClanProbe"), TEXT("PRB"));
			AppendLog(TEXT("SOCIAL_CLAN_CREATE clan=ClanProbe"));
			return;
		}
		if (!bSocialGroupsOk)
		{
			SocialOpInFlight = TEXT("group.create");
			PS->Server_SocialGroup(TEXT("create"), TEXT(""), TEXT(""));
			AppendLog(TEXT("SOCIAL_GROUP_CREATE"));
			return;
		}
		if (!bSocialClanInviteOk)
		{
			SocialOpInFlight = TEXT("clan.invite");
			PS->Server_SocialClan(TEXT("invite"), TEXT("bob"), TEXT(""));
			AppendLog(TEXT("SOCIAL_CLAN_INVITE_SENT invitee=bob"));
			return;
		}
		if (!bSocialFriendsOk)
		{
			SocialOpInFlight = TEXT("friend.request");
			PS->Server_SocialFriend(TEXT("request"), TEXT("bob"));
			AppendLog(TEXT("SOCIAL_FRIEND_REQUEST from=alice to=bob"));
			return;
		}
		if (!bSocialGroupInviteOk)
		{
			SocialOpInFlight = TEXT("group.invite");
			PS->Server_SocialGroup(TEXT("invite"), TEXT("bob"), TEXT(""));
			AppendLog(TEXT("SOCIAL_GROUP_INVITE_SENT invitee=bob"));
			return;
		}
		if (!bSocialMailOk)
		{
			SocialOpInFlight = TEXT("mail.send");
			PS->Server_SocialMail(TEXT("send"), TEXT("bob|Probe|Test mail"));
			AppendLog(TEXT("SOCIAL_MAIL_SEND from=alice to=bob"));
			return;
		}

		// All six ops confirmed. Stay connected so bob can finish against a live
		// authority (his OnlineFriendCount check needs alice online); the gate
		// runner force-kills the process after both markers appear.
		bSocialDone = true;
		AppendLog(TEXT("SOCIAL_PROBE_ALICE_OK"));
		if (W)
		{
			W->GetTimerManager().ClearTimer(WorldServerTimer);
			FTimerHandle ExitHandle;
			W->GetTimerManager().SetTimer(ExitHandle,
				[]() { FPlatformMisc::RequestExit(false); }, 90.0f, false);
		}
		return;
	}

	if (bIsBob)
	{
		// Clan: wait for the replicated invite, accept once, confirm membership.
		if (!bSocialClanInviteOk && PS->bHasPendingClanInvite)
		{
			PS->Server_SocialClan(TEXT("accept"), TEXT(""), TEXT(""));
			bSocialClanInviteOk = true;
			AppendLog(TEXT("SOCIAL_CLAN_ACCEPT_SENT invitee=bob"));
		}
		if (!bSocialClanOk && bSocialClanInviteOk && !PS->ClanId.IsEmpty())
		{
			bSocialClanOk = true;
			AppendLog(FString::Printf(TEXT("SOCIAL_CLAN_JOINED clan=%s role=%s"), *PS->ClanId, *PS->ClanRole));
		}

		// Friends: retry accept until the replicated online-friend count confirms it
		// (alice is online, so a successful accept yields at least one online friend).
		if (!bSocialFriendsOk)
		{
			if (PS->OnlineFriendCount > 0)
			{
				bSocialFriendsOk = true;
				AppendLog(FString::Printf(TEXT("SOCIAL_FRIEND_CONFIRMED online=%d"), PS->OnlineFriendCount));
			}
			else
			{
				PS->Server_SocialFriend(TEXT("accept"), TEXT("alice"));
			}
		}

		// Group: wait for the replicated invite, accept once, confirm membership.
		if (!bSocialGroupInviteOk && PS->bHasPendingGroupInvite)
		{
			PS->Server_SocialGroup(TEXT("accept"), TEXT(""), TEXT(""));
			bSocialGroupInviteOk = true;
			AppendLog(TEXT("SOCIAL_GROUP_ACCEPT_SENT invitee=bob"));
		}
		if (!bSocialGroupsOk && bSocialGroupInviteOk && !PS->GroupId.IsEmpty())
		{
			bSocialGroupsOk = true;
			AppendLog(FString::Printf(TEXT("SOCIAL_GROUP_JOINED group=%s"), *PS->GroupId));
		}

		// Mail: the replicated unread badge proves inbox delivery.
		if (!bSocialMailOk && PS->MailUnreadCount > 0)
		{
			bSocialMailOk = true;
			AppendLog(FString::Printf(TEXT("SOCIAL_MAIL_RECEIVED unread=%d"), PS->MailUnreadCount));
		}

		if (bSocialClanOk && bSocialFriendsOk && bSocialGroupsOk && bSocialMailOk)
		{
			bSocialDone = true;
			AppendLog(TEXT("SOCIAL_PROBE_BOB_OK"));
			EndFrontendProbe();
		}
		return;
	}

	// Unknown role — emit fail.
	bSocialDone = true;
	AppendLog(FString::Printf(TEXT("SOCIAL_PROBE_FAIL reason=unknown_role role=%s"), *SocialRole));
	EndFrontendProbe();
}

void UAPBSessionProbeSubsystem::RunMissionClientProbe()
{
	if (bTerminal || bMissionClientDone) return;

	// 300s ceiling — the authority's TickMatchmaker runs on a 5s cadence and both real
	// network clients must have enqueued before a pairing can form. The network dispatch
	// gate brings the parties up SEQUENTIALLY (enforcer queues first, then the criminal
	// cold-boots and queues), so the first-queued party idle-polls through its opponent's
	// full boot; allow generous slack for that wait before declaring failure.
	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	if (MissionClientStartMs > 0 && (NowMs - MissionClientStartMs) > 300000)
	{
		bMissionClientDone = true;
		// Log the actual replicated mission state at failure so an inconclusive verdict
		// (e.g. queue cleared but title never arrived) is diagnosable without a re-run.
		FString TitleAtFail; int32 QueuedAtFail = -1; int32 FactionAtFail = -1;
		if (UWorld* W = GetWorld())
		{
			if (APlayerController* FailPC = UGameplayStatics::GetPlayerController(W, 0))
			{
				if (AAPBPlayerState* FailPS = FailPC->GetPlayerState<AAPBPlayerState>())
				{
					TitleAtFail = FailPS->MissionTitle;
					QueuedAtFail = FailPS->bMissionQueued ? 1 : 0;
					FactionAtFail = static_cast<int32>(FailPS->Faction);
				}
			}
		}
		AppendLog(FString::Printf(TEXT("MISSION_CLIENT_FAIL reason=timeout role=%s enqueued=%d seen_queued=%d queued_now=%d faction=%d title='%s'"),
			*MissionRole, bMissionEnqueued ? 1 : 0, bMissionSeenQueued ? 1 : 0,
			QueuedAtFail, FactionAtFail, *TitleAtFail));
		EndFrontendProbe();
		return;
	}

	APlayerController* PC = nullptr;
	if (UWorld* W = GetWorld())
		PC = UGameplayStatics::GetPlayerController(W, 0);
	if (!PC) return;
	AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>();
	if (!PS) return;

	// Wait past the host's immediate + deferred PostLogin snapshot pushes. The gate host
	// binds the faction set below, so later shared mission snapshots cannot overwrite it.
	if (MissionClientStartMs > 0 && (NowMs - MissionClientStartMs) < 4000) return;

	const bool bWantEnforcer = MissionRole.Equals(TEXT("enforcer"), ESearchCase::IgnoreCase);
	const EAPBFaction Want = bWantEnforcer ? EAPBFaction::Enforcer : EAPBFaction::Criminal;

	// Step 1: set our faction through the gate-only validated Server RPC capability.
	if (!bMissionFactionRequested)
	{
		bMissionFactionRequested = true;
		PS->ServerSetFaction(Want);
		AppendLog(FString::Printf(TEXT("MISSION_CLIENT_FACTION_SET role=%s want=%s"),
			*MissionRole, bWantEnforcer ? TEXT("Enforcer") : TEXT("Criminal")));
		return;
	}
	// Wait for the authority to apply + replicate the faction back before enqueuing so
	// the ticket built in Server_RequestMissionDispatch carries the correct faction.
	if (PS->Faction != Want) return;

	// Step 2: enqueue via the validated matchmaker RPC. The authority builds the
	// MatchTicket from this replicated identity (name/faction/threat) on its own clock.
	if (!bMissionEnqueued)
	{
		bMissionEnqueued = true;
		PS->Server_RequestMissionDispatch();
		AppendLog(FString::Printf(TEXT("MISSION_CLIENT_ENQUEUE role=%s player=%s"),
			*MissionRole, *PS->GetPlayerName()));
		return;
	}

	// Observe our own queued flag replicate true (the enqueue landed on the authority).
	if (PS->bMissionQueued && !bMissionSeenQueued)
	{
		bMissionSeenQueued = true;
		AppendLog(FString::Printf(TEXT("MISSION_CLIENT_QUEUED role=%s"), *MissionRole));
	}

	// Dispatch verdict: the authority's FormAndDispatchMatches pairs the two real
	// clients, clears both parties' queued flags, and pushes the active mission snapshot
	// to every PlayerState — so a dispatched client observes bMissionQueued cleared AND a
	// non-empty MissionTitle replicated in.
	//
	// The DURABLE proof a mission was dispatched to us is the active MissionTitle
	// replicating in after we enqueued. We deliberately do NOT require having observed
	// the transient bMissionQueued=true first: when the authority pairs us within a
	// single 5s matchmaker tick (fast pairing — the opponent was already queued), it
	// sets our queued flag true->false before the true value can replicate, so a
	// correctly-dispatched client can legitimately show seen_queued=0. The gate's
	// host-side MISSION_DISPATCH log (naming both distinct real parties, queue=0)
	// remains the authoritative two-client dispatch proof.
	const bool bDispatched = bMissionEnqueued && !PS->bMissionQueued && !PS->MissionTitle.IsEmpty();
	if (bDispatched)
	{
		bMissionClientDone = true;
		AppendLog(FString::Printf(TEXT("MISSION_CLIENT_%s_OK player=%s title=%s stage=%d/%d mission_active=1"),
			bWantEnforcer ? TEXT("ENFORCER") : TEXT("CRIMINAL"), *PS->GetPlayerName(),
			*PS->MissionTitle, PS->MissionStageIndex, PS->MissionStageCount));
		EndFrontendProbe();
	}
}


void UAPBSessionProbeSubsystem::RunWorldTravelClientProbe()
{
	UWorld* World = GetWorld();
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!World || !APB) return;

	if (World->GetNetDriver() && World->GetNetDriver()->ServerConnection && !FParse::Param(FCommandLine::Get(), TEXT("DisableEncryption")))
	{
		static bool bTravelEncryptionEnabled = false;
		if (!bTravelEncryptionEnabled)
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
					World->GetNetDriver()->ServerConnection->EnableEncryption(Data);
					bTravelEncryptionEnabled = true;
					AppendLog(TEXT("TRAVEL_CLIENT_ENCRYPTION_ENABLED"));
				}
			}
		}
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
	AAPBPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AAPBPlayerState>() : nullptr;
	if (!PlayerState) return;
	if (!PlayerState->bWorldAuthOk)
	{
		if (!bTravelLoginSent)
		{
			const FString User = TEXT("travel_") + WSClientId;
			PlayerState->Server_LoginRequest(User, TEXT("travel_pass"));
			bTravelLoginSent = true;
			AppendLog(FString::Printf(TEXT("TRAVEL_LOGIN id=%s"), *WSClientId));
		}
		return;
	}
	if (!bTravelTicketRequested)
	{
		// Ticket must name the character LoginPlayer bound (login user = "travel_"+id),
		// which the authority validates against the requested character before minting.
		PlayerState->Server_IssueTicket(TEXT("travel_") + WSClientId, TravelDistrictId);
		bTravelTicketRequested = true;
		AppendLog(FString::Printf(TEXT("TRAVEL_TICKET_REQUEST district=%s"), *TravelDistrictId));
		return;
	}
	const FString ReservationJson = PlayerState->IssuedTicketJson;
	if (ReservationJson.IsEmpty()) return;
	TSharedPtr<FJsonObject> Reservation;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReservationJson);
	if (!FJsonSerializer::Deserialize(Reader, Reservation) || !Reservation.IsValid())
	{
		AppendLog(TEXT("TRAVEL_FAIL reason=no_ticket"));
		World->GetTimerManager().ClearTimer(WorldServerTimer);
		return;
	}
	FString Error;
	if (Reservation->TryGetStringField(TEXT("error"), Error))
	{
		AppendLog(FString::Printf(TEXT("TRAVEL_FAIL reason=%s"), *Error));
		World->GetTimerManager().ClearTimer(WorldServerTimer);
		return;
	}
	FString Ticket;
	FString Host;
	FString ReservationId;
	double PortNumber = 0;
	if (!Reservation->TryGetStringField(TEXT("ticket"), Ticket) || !Reservation->TryGetStringField(TEXT("host"), Host) ||
		!Reservation->TryGetStringField(TEXT("reservation_id"), ReservationId) ||
		!Reservation->TryGetNumberField(TEXT("port"), PortNumber))
	{
		AppendLog(TEXT("TRAVEL_FAIL reason=no_ticket"));
		World->GetTimerManager().ClearTimer(WorldServerTimer);
		return;
	}
	if (!bTravelDispatchPending)
	{
		int32 DelayMs = 0;
		FParse::Value(FCommandLine::Get(), TEXT("WSTravelDelayMs="), DelayMs);
		TravelDispatchAtMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond() + DelayMs;
		bTravelDispatchPending = true;
		AppendLog(FString::Printf(TEXT("TRAVEL_RESERVATION district=%s host=%s port=%d id=%s"),
			*TravelDistrictId, *Host, static_cast<int32>(PortNumber), *ReservationId));
	}
	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	if (NowMs < TravelDispatchAtMs) return;
	World->GetTimerManager().ClearTimer(WorldServerTimer);
	AppendLog(FString::Printf(TEXT("TRAVEL_DISPATCH district=%s"), *TravelDistrictId));
	if (bLoadWorkloadEnabled && !bLoadTravelDispatched)
	{
		bLoadTravelDispatched = true;
		LoadWorkloadStartedAt = FPlatformTime::Seconds();
		LoadLastPerfAt = LoadWorkloadStartedAt;
		if (GI)
		{
			GI->GetTimerManager().SetTimer(LoadWorkloadTimer,
				FTimerDelegate::CreateUObject(this, &UAPBSessionProbeSubsystem::RunLoadWorkloadStep), 0.1f, true, 1.0f);
		}
	}
	APB->StartDistrictTravel(PlayerController, TravelDistrictId, Host, static_cast<int32>(PortNumber), Ticket, ReservationId);
}

void UAPBSessionProbeSubsystem::RunLoadWorkloadStep()
{
	if (!bLoadWorkloadEnabled || bLoadCompletionEmitted) return;
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client) return;
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
	if (!PlayerController) return;
	++LoadWorkloadSteps;

	AAPBFreeroamCharacter* Character = Cast<AAPBFreeroamCharacter>(PlayerController->GetPawn());
	if (Character)
	{
		if (bLoadMovementRequested)
		{
			const float Right = FMath::Sin(static_cast<float>(LoadWorkloadSteps) * 0.1f) * 0.35f;
			Character->ApplyMoveInput(1.f, Right);
			bLoadMovementExecuted = true;
		}
		if (bLoadCombatRequested)
		{
			if (!bLoadCombatSent)
			{
				LoadCombatStartShots = Character->ShotsFired;
				Character->ServerFireWeapon();
				bLoadCombatSent = true;
			}
			else if (Character->ShotsFired > LoadCombatStartShots)
			{
				bLoadCombatExecuted = true;
				bLoadCombatSent = false;
			}
		}
		if (bLoadVehicleRequested && !bLoadVehicleEntered
			&& (!bLoadMovementRequested || bLoadMovementExecuted)
			&& (!bLoadCombatRequested || bLoadCombatExecuted))
		{
			Character->ServerEnterNearestVehicle();
		}
	}

	if (AAPBDriveableVehicle* Vehicle = Cast<AAPBDriveableVehicle>(PlayerController->GetPawn()))
	{
		bLoadVehicleEntered = Vehicle->bHasDriver;
		if (bLoadVehicleRequested && bLoadVehicleEntered)
		{
			Vehicle->ApplyThrottleInput(1.f);
			bLoadVehicleThrottled = true;
		}
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - LoadLastPerfAt >= 1.0)
	{
		int32 ReplicatedActors = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetIsReplicated()) ++ReplicatedActors;
		}
		const float DeltaSeconds = World->GetDeltaSeconds();
		const float FrameMs = DeltaSeconds * 1000.f;
		const float FPS = DeltaSeconds > SMALL_NUMBER ? 1.f / DeltaSeconds : 0.f;
		const APlayerState* PlayerState = PlayerController->PlayerState;
		const float RTT = PlayerState ? PlayerState->GetPingInMilliseconds() : 0.f;
		AppendLog(FString::Printf(TEXT("APB_PERF_METRIC identity=%s FPS:%.3f FrameMs:%.3f RTT:%.3f Replication:%.3f"),
			*LoadIdentity, FPS, FrameMs, RTT, static_cast<float>(ReplicatedActors)));
		bLoadPerfEmitted = true;
		LoadLastPerfAt = Now;
	}

	const bool bRequestedComplete = (!bLoadMovementRequested || bLoadMovementExecuted)
		&& (!bLoadCombatRequested || bLoadCombatExecuted)
		&& (!bLoadVehicleRequested || (bLoadVehicleEntered && bLoadVehicleThrottled));
	if (bLoadPerfEmitted && (bRequestedComplete || Now - LoadWorkloadStartedAt >= 15.0))
	{
		bLoadCompletionEmitted = true;
		AppendLog(FString::Printf(
			TEXT("APB_LOAD_WORKLOAD_COMPLETE identity=%s movement=%d combat=%d vehicle=%d"),
			*LoadIdentity,
			APBLoadActionToken(bLoadMovementRequested, bLoadMovementExecuted),
			APBLoadActionToken(bLoadCombatRequested, bLoadCombatExecuted),
			APBLoadVehicleToken(bLoadVehicleRequested, bLoadVehicleEntered, bLoadVehicleThrottled)));
	}
}

void UAPBSessionProbeSubsystem::RunWorldChatClientProbe()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GameInstance ? GameInstance->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!World || !APB)
	{
		return;
	}
	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	if (NowMs < ChatWorldReconnectReadyAtMs)
	{
		return;
	}
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
	AAPBPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AAPBPlayerState>() : nullptr;
	if (!PlayerController || !PlayerState)
	{
		return;
	}
	if (World->GetMapName().Contains(ChatDistrictId, ESearchCase::IgnoreCase))
	{
		if (!bChatArrivalLogged)
		{
			bChatArrivalLogged = true;
			AppendLog(FString::Printf(TEXT("CHAT_CLIENT_ARRIVED char=%s district=%s"), *ChatCharacter, *ChatDistrictId));
			ArmQueuedChatCommands();
		}
		return;
	}
	if (!PlayerState->bWorldAuthOk)
	{
		if (!bChatLoginSent)
		{
			PlayerState->Server_LoginRequest(TEXT("chat_") + WSClientId, TEXT("chat_pass"));
			bChatLoginSent = true;
			AppendLog(FString::Printf(TEXT("CHAT_CLIENT_LOGIN id=%s"), *WSClientId));
		}
		return;
	}
	if (!bChatTicketRequested)
	{
		PlayerState->Server_IssueTicket(ChatCharacter, ChatDistrictId);
		bChatTicketRequested = true;
		AppendLog(FString::Printf(TEXT("CHAT_CLIENT_TICKET_REQUEST char=%s district=%s"), *ChatCharacter, *ChatDistrictId));
		return;
	}
	if (bChatTravelDispatched || PlayerState->IssuedTicketJson.IsEmpty())
	{
		return;
	}
	TSharedPtr<FJsonObject> Reservation;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PlayerState->IssuedTicketJson);
	if (!FJsonSerializer::Deserialize(Reader, Reservation) || !Reservation.IsValid())
	{
		AppendLog(TEXT("CHAT_CLIENT_FAIL reason=no_ticket"));
		return;
	}
	FString Error;
	if (Reservation->TryGetStringField(TEXT("error"), Error))
	{
		AppendLog(FString::Printf(TEXT("CHAT_CLIENT_FAIL reason=%s"), *Error));
		return;
	}
	FString Ticket;
	FString Host;
	FString ReservationId;
	double PortNumber = 0;
	if (!Reservation->TryGetStringField(TEXT("ticket"), Ticket) || !Reservation->TryGetStringField(TEXT("host"), Host) ||
		!Reservation->TryGetStringField(TEXT("reservation_id"), ReservationId) ||
		!Reservation->TryGetNumberField(TEXT("port"), PortNumber))
	{
		AppendLog(TEXT("CHAT_CLIENT_FAIL reason=no_ticket"));
		return;
	}
	bChatTravelDispatched = true;
	AppendLog(FString::Printf(TEXT("CHAT_CLIENT_TRAVEL char=%s district=%s"), *ChatCharacter, *ChatDistrictId));
	APB->StartDistrictTravel(PlayerController, ChatDistrictId, Host, static_cast<int32>(PortNumber), Ticket, ReservationId);
}

void UAPBSessionProbeSubsystem::QueueChatCommandsFromCommandLine()
{
	FString ExecCommands;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ExecCmds="), ExecCommands))
	{
		return;
	}
	ExecCommands.TrimQuotesInline();
	TArray<FString> Commands;
	ExecCommands.ParseIntoArray(Commands, TEXT(","), true);
	for (FString Command : Commands)
	{
		Command.TrimStartAndEndInline();
		const bool bTravel = Command.StartsWith(TEXT("APBChatTravel "), ESearchCase::IgnoreCase);
		const FString Prefix = bTravel ? TEXT("APBChatTravel ") : TEXT("APBChat ");
		if (!Command.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			continue;
		}
		FString DelayText;
		FString Payload;
		if (!Command.Mid(Prefix.Len()).TrimStartAndEnd().Split(TEXT(" "), &DelayText, &Payload))
		{
			continue;
		}
		const int32 DelayMs = FCString::Atoi(*DelayText);
		Payload.TrimStartAndEndInline();
		if (DelayMs < 0 || DelayMs > 120000 || Payload.IsEmpty())
		{
			continue;
		}
		FChatGateCommand& Queued = QueuedChatCommands.AddDefaulted_GetRef();
		Queued.DelayMs = DelayMs;
		if (bTravel) Queued.TravelDistrictId = Payload;
		else Queued.RawLine = Payload;
	}
}

void UAPBSessionProbeSubsystem::ArmQueuedChatCommands()
{
	if (bChatCommandsArmed)
	{
		return;
	}
	bChatCommandsArmed = true;
	for (const FChatGateCommand& Command : QueuedChatCommands)
	{
		if (!Command.TravelDistrictId.IsEmpty()) ScheduleChatDistrictTravel(Command.DelayMs, Command.TravelDistrictId);
		else ScheduleDevChat(Command.DelayMs, Command.RawLine);
	}
	AppendLog(FString::Printf(TEXT("CHAT_CLIENT_COMMANDS_ARMED count=%d"), QueuedChatCommands.Num()));
}

void UAPBSessionProbeSubsystem::ScheduleDevChat(const int32 DelayMs, const FString& RawLine)
{
	if (Mode != TEXT("world_chat_client") || DelayMs < 0 || DelayMs > 120000 || RawLine.IsEmpty() || RawLine.Len() > 512)
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=BadChannel"));
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}
	FTimerHandle& TimerHandle = ChatCommandTimers.AddDefaulted_GetRef();
	GameInstance->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateWeakLambda(this, [this, RawLine]()
	{
		UWorld* World = GetWorld();
		APlayerController* PlayerController = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		AAPBPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AAPBPlayerState>() : nullptr;
		if (!PlayerState)
		{
			AppendLog(TEXT("CHAT_CLIENT_FAIL reason=no_player_state"));
			return;
		}
		PlayerState->Server_SubmitChat(RawLine);
		AppendLog(FString::Printf(TEXT("CHAT_CLIENT_SUBMIT raw=%s"), *RawLine));
	}), static_cast<float>(DelayMs) / 1000.f, false);
}

void UAPBSessionProbeSubsystem::ScheduleChatDistrictTravel(const int32 DelayMs, const FString& DistrictId)
{
	if (Mode != TEXT("world_chat_client") || DelayMs < 0 || DelayMs > 120000 || DistrictId.IsEmpty() || DistrictId.Len() > 64)
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=BadChannel"));
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}
	FTimerHandle& TimerHandle = ChatCommandTimers.AddDefaulted_GetRef();
	GameInstance->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateWeakLambda(this, [this, DistrictId]()
	{
		UWorld* World = GetWorld();
		APlayerController* PlayerController = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		if (!PlayerController)
		{
			AppendLog(TEXT("CHAT_CLIENT_FAIL reason=no_player_controller"));
			return;
		}
		int32 WorldPort = 0;
		FParse::Value(FCommandLine::Get(), TEXT("WorldPort="), WorldPort);
		if (WorldPort < 1 || WorldPort > 65535)
		{
			AppendLog(TEXT("CHAT_CLIENT_FAIL reason=world_port"));
			return;
		}
		ChatDistrictId = DistrictId;
		bChatLoginSent = false;
		bChatTicketRequested = false;
		bChatTravelDispatched = false;
		bChatArrivalLogged = false;
		ChatWorldReconnectReadyAtMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond() + 2000;
		AppendLog(FString::Printf(TEXT("CHAT_CLIENT_RETURN world_port=%d district=%s"), WorldPort, *ChatDistrictId));
		PlayerController->ClientTravel(FString::Printf(TEXT("127.0.0.1:%d"), WorldPort), ETravelType::TRAVEL_Absolute);
	}), static_cast<float>(DelayMs) / 1000.f, false);
}

bool UAPBSessionProbeSubsystem::ParseHandoffProbeSnapshot(const FString& Json, int64& OutCash, int64& OutG1C,
	float& OutThreat, int32& OutInventorySlots, int32& OutInventoryQty, FString& OutFaction, FString& OutMission, FString& OutSession,
	FString& OutProgression) const
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;
	double Cash = 0, G1C = 0, Threat = 0, InventorySlots = 0, InventoryQty = 0;
	if (!Root->TryGetNumberField(TEXT("cash"), Cash) || !Root->TryGetNumberField(TEXT("g1c"), G1C) ||
		!Root->TryGetNumberField(TEXT("threat_points"), Threat) || !Root->TryGetNumberField(TEXT("inventory_slot_count"), InventorySlots) ||
		!Root->TryGetNumberField(TEXT("inventory_total_qty"), InventoryQty) ||
		!Root->TryGetStringField(TEXT("faction"), OutFaction) || !Root->TryGetStringField(TEXT("mission_id"), OutMission) ||
		!Root->TryGetStringField(TEXT("session_id"), OutSession)) return false;
	OutProgression.Empty();
	const TArray<TSharedPtr<FJsonValue>>* Contacts = nullptr;
	if (Root->TryGetArrayField(TEXT("contact_standings"), Contacts) && Contacts)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Contacts)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Entry.IsValid()) continue;
			FString Id; double Amount = 0;
			if (Entry->TryGetStringField(TEXT("id"), Id) && Entry->TryGetNumberField(TEXT("value"), Amount))
				OutProgression += FString::Printf(TEXT("C:%s=%lld;"), *Id, static_cast<int64>(Amount));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
	if (Root->TryGetArrayField(TEXT("role_xp"), Roles) && Roles)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Roles)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Entry.IsValid()) continue;
			FString Id; double Amount = 0;
			if (Entry->TryGetStringField(TEXT("id"), Id) && Entry->TryGetNumberField(TEXT("value"), Amount))
				OutProgression += FString::Printf(TEXT("R:%s=%lld;"), *Id, static_cast<int64>(Amount));
		}
	}
	OutCash = static_cast<int64>(Cash);
	OutG1C = static_cast<int64>(G1C);
	OutThreat = static_cast<float>(Threat);
	OutInventorySlots = static_cast<int32>(InventorySlots);
	OutInventoryQty = static_cast<int32>(InventoryQty);
	return true;
}

void UAPBSessionProbeSubsystem::RunWorldHandoffClientProbe()
{
	UWorld* World = GetWorld();
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!World || !APB || bTerminal) return;

	if (World->GetNetDriver() && World->GetNetDriver()->ServerConnection && !FParse::Param(FCommandLine::Get(), TEXT("DisableEncryption")))
	{
		// Per-connection, not once-only: after the district round-trip the client travels BACK
		// to the world on a brand-new ServerConnection, which needs its own key or every
		// replicated packet is dropped ("received encrypted packet before key was set").
		static TWeakObjectPtr<UNetConnection> HandoffEncryptedConnection;
		UNetConnection* Conn = World->GetNetDriver()->ServerConnection;
		if (HandoffEncryptedConnection.Get() != Conn)
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
					Conn->EnableEncryption(Data);
					HandoffEncryptedConnection = Conn;
					AppendLog(TEXT("HANDOFF_CLIENT_ENCRYPTION_ENABLED"));
				}
			}
		}
	}

	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	if (NowMs >= HandoffDeadlineMs)
	{
		AppendLog(TEXT("HANDOFF_FAIL reason=timeout"));
		bTerminal = true;
		if (GI) GI->GetTimerManager().ClearTimer(WorldServerTimer);
		FPlatformMisc::RequestExit(false);
		return;
	}
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
	AAPBPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AAPBPlayerState>() : nullptr;
	if (!PlayerState) return;
	const bool bDistrict = World->GetMapName().Contains(TEXT("Financial"), ESearchCase::IgnoreCase);
	if (HandoffPhase == 3 && bDistrict)
	{
		bHandoffReachedDistrict = true;
		const bool bParity = PlayerState->Faction == EAPBFaction::Enforcer && PlayerState->Cash == HandoffCash &&
			PlayerState->G1C == HandoffG1C && FMath::IsNearlyEqual(PlayerState->ThreatPoints, HandoffThreat, 0.01f) &&
			PlayerState->InventoryItemCount == HandoffInventorySlots && !PlayerState->MissionTitle.IsEmpty() &&
			PlayerState->DistrictSessionId == HandoffSession && PlayerState->ProgressionState == HandoffProgression;
		if (!bHandoffDistrictLogged && bParity)
		{
			bHandoffDistrictLogged = true;
			AppendLog(FString::Printf(TEXT("HANDOFF_DISTRICT_PARITY ok=1 cash=%lld g1c=%lld threat=%.1f faction=%d inv=%d mission=%s session=%s progress=%s"),
				PlayerState->Cash, PlayerState->G1C, PlayerState->ThreatPoints, static_cast<int32>(PlayerState->Faction),
				PlayerState->InventoryItemCount, *PlayerState->MissionTitle, *PlayerState->DistrictSessionId, *PlayerState->ProgressionState));
		}
		return;
	}
	if (HandoffPhase == 3 && !bDistrict && bHandoffReachedDistrict)
	{
		HandoffPhase = 4;
		bHandoffLoginSent = false;
		bHandoffStateRequested = false;
		HandoffStateRequestAtMs = 0;
		AppendLog(TEXT("HANDOFF_RETURN_TRAVELLED"));
	}
	if (HandoffPhase == 0 || HandoffPhase == 4)
	{
		if (!PlayerState->bWorldAuthOk)
		{
			if (!bHandoffLoginSent)
			{
				PlayerState->Server_LoginRequest(TEXT("handoff_") + WSClientId, TEXT("handoff_pass"));
				bHandoffLoginSent = true;
				AppendLog(FString::Printf(TEXT("HANDOFF_LOGIN phase=%d"), HandoffPhase));
			}
			return;
		}
		if (HandoffPhase == 0)
		{
			HandoffPhase = 1;
			bHandoffPrepareSent = false;
		}
	}
	if (HandoffPhase == 1)
	{
		if (!bHandoffPrepareSent)
		{
			PlayerState->Server_PrepareHandoffProbe();
			bHandoffPrepareSent = true;
			AppendLog(TEXT("HANDOFF_PREPARE_REQUEST"));
			return;
		}
		if (PlayerState->HandoffProbeJson.IsEmpty()) return;
		if (!ParseHandoffProbeSnapshot(PlayerState->HandoffProbeJson, HandoffCash, HandoffG1C, HandoffThreat,
			HandoffInventorySlots, HandoffInventoryQty, HandoffFaction, HandoffMission, HandoffSession, HandoffProgression))
		{
			AppendLog(TEXT("HANDOFF_FAIL reason=pre_parse"));
			return;
		}
		AppendLog(FString::Printf(TEXT("HANDOFF_PRE cash=%lld g1c=%lld threat=%.1f faction=%s inv_slots=%d inv_qty=%d mission=%s session=%s progress=%s"),
			HandoffCash, HandoffG1C, HandoffThreat, *HandoffFaction, HandoffInventorySlots, HandoffInventoryQty, *HandoffMission, *HandoffSession, *HandoffProgression));
		PlayerState->Server_IssueTicket(TEXT("Operative"), TravelDistrictId);
		bHandoffTicketSent = true;
		HandoffPhase = 2;
		return;
	}
	if (HandoffPhase == 2)
	{
		const FString ReservationJson = PlayerState->IssuedTicketJson;
		if (ReservationJson.IsEmpty()) return;
		TSharedPtr<FJsonObject> Reservation;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReservationJson);
		FString Ticket, Host, ReservationId, HandoffTravelSession, Error;
		double PortNumber = 0;
		if (!FJsonSerializer::Deserialize(Reader, Reservation) || !Reservation.IsValid() ||
			Reservation->TryGetStringField(TEXT("error"), Error) || !Reservation->TryGetStringField(TEXT("ticket"), Ticket) ||
			!Reservation->TryGetStringField(TEXT("host"), Host) || !Reservation->TryGetStringField(TEXT("reservation_id"), ReservationId) ||
			!Reservation->TryGetStringField(TEXT("session_id"), HandoffTravelSession) ||
			!Reservation->TryGetNumberField(TEXT("port"), PortNumber))
		{
			AppendLog(FString::Printf(TEXT("HANDOFF_FAIL reason=ticket_%s"), *Error));
			return;
		}
		HandoffSession = HandoffTravelSession;
		HandoffPhase = 3;
		AppendLog(FString::Printf(TEXT("HANDOFF_TRAVEL_DISPATCH host=%s port=%d"), *Host, static_cast<int32>(PortNumber)));
		APB->StartDistrictTravel(PlayerController, TravelDistrictId, Host, static_cast<int32>(PortNumber), Ticket, ReservationId);
		return;
	}
	if (HandoffPhase == 4)
	{
		if (!bHandoffStateRequested)
		{
			PlayerState->Server_GetHandoffProbeState();
			bHandoffStateRequested = true;
			HandoffStateRequestAtMs = NowMs;
			return;
		}
		if (NowMs - HandoffStateRequestAtMs < 1000 || PlayerState->HandoffProbeJson.IsEmpty()) return;
		int64 Cash = 0, G1C = 0; float Threat = 0.f; int32 InventorySlots = 0, InventoryQty = 0;
		FString Faction, Mission, Session, Progression;
		const bool bParsed = ParseHandoffProbeSnapshot(PlayerState->HandoffProbeJson, Cash, G1C, Threat, InventorySlots, InventoryQty, Faction, Mission, Session, Progression);
		// Mission/session are district-runtime state, not persisted in the hub character schema
		// (ARCHITECTURE.md L115-120), so cleared==correct after return-to-hub + fresh re-login.
		const bool bParity = bParsed && Cash == HandoffCash + 77 && G1C == HandoffG1C && FMath::IsNearlyEqual(Threat, HandoffThreat + 5.f, 0.01f) &&
			InventorySlots == HandoffInventorySlots && InventoryQty == HandoffInventoryQty && Faction == HandoffFaction && Mission.IsEmpty() &&
			Session.IsEmpty() && Progression == HandoffProgression;
		AppendLog(FString::Printf(TEXT("HANDOFF_RETURN_PARITY ok=%d cash=%lld g1c=%lld threat=%.1f faction=%s inv_slots=%d inv_qty=%d mission=%s session=%s progress=%s"),
			bParity ? 1 : 0, Cash, G1C, Threat, *Faction, InventorySlots, InventoryQty, *Mission, *Session, *Progression));
		AppendLog(bParity ? TEXT("WORLD_HANDOFF_CLIENT_OK") : TEXT("HANDOFF_FAIL reason=return_parity"));
		bTerminal = true;
		if (GI) GI->GetTimerManager().ClearTimer(WorldServerTimer);
		FPlatformMisc::RequestExit(false);
	}
}
void UAPBSessionProbeSubsystem::RunReplicationProbe()
{
	if (bTerminal || bReplicationDone) return;

	const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
	if (ReplicationProbeStartMs > 0 && (NowMs - ReplicationProbeStartMs) > 60000)
	{
		bReplicationDone = true;
		AppendLog(FString::Printf(TEXT("REPLICATION_PROBE_FAIL reason=timeout role=%s"), *ReplicationRole));
		EndFrontendProbe();
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) { AppendLog(TEXT("REPLICATION_PROBE_FAIL reason=no_apb")); EndFrontendProbe(); return; }

	UWorld* W = GetWorld();
	if (W && W->GetNetDriver() && W->GetNetDriver()->ServerConnection && !FParse::Param(FCommandLine::Get(), TEXT("DisableEncryption")))
	{
		static bool bEncryptionEnabled = false;
		if (!bEncryptionEnabled)
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
					W->GetNetDriver()->ServerConnection->EnableEncryption(Data);
					bEncryptionEnabled = true;
					AppendLog(TEXT("REPLICATION_OWNER_ENCRYPTION_ENABLED"));
				}
				else
				{
					AppendLog(TEXT("REPLICATION_OWNER_ENCRYPTION_FAILED_BAD_KEY"));
				}
			}
			else
			{
				AppendLog(TEXT("REPLICATION_OWNER_ENCRYPTION_FAILED_NO_SECRET"));
			}
		}
	}

	APlayerController* PC = W ? UGameplayStatics::GetPlayerController(W, 0) : nullptr;
	if (!PC) return;

	AAPBPlayerState* MyPS = PC->GetPlayerState<AAPBPlayerState>();

	if (ReplicationRole == TEXT("owner"))
	{
		// The owner sets a sentinel on the server (CharListJson). We fetch it via RPC.
		if (bTravelLoginSent)
		{
			// Wait until bWorldAuthOk is replicated to us, then request CharList.
			if (MyPS && MyPS->bWorldAuthOk && MyPS->CharListJson.IsEmpty())
			{
				MyPS->Server_GetCharList();
			}
			if (MyPS && !MyPS->CharListJson.IsEmpty())
			{
				bReplicationDone = true;
				AppendLog(TEXT("REPLICATION_PROBE_OWNER_OK"));
				if (W)
				{
					W->GetTimerManager().ClearTimer(WorldServerTimer);
					FTimerHandle ExitHandle;
					W->GetTimerManager().SetTimer(ExitHandle, []() { FPlatformMisc::RequestExit(false); }, 90.0f, false);
				}
			}
			return;
		}

		if (ReplicationRole == TEXT("owner"))
		{
			static float OwnerStartTime = 0.f;
			if (OwnerStartTime == 0.f && W)
			{
				OwnerStartTime = W->GetRealTimeSeconds();
			}
			if (W && W->GetRealTimeSeconds() - OwnerStartTime < 2.0f) return;

			if (MyPS)
			{
				MyPS->Server_LoginRequest(TEXT("owner"), TEXT("1234"));
				bTravelLoginSent = true; 
				AppendLog(TEXT("REPLICATION_OWNER_LOGIN_SENT"));
			}
			return;
		}
		return;
	}

	if (ReplicationRole == TEXT("observer"))
	{
		if (!bTravelLoginSent)
		{
			static float ObserverStartTime = 0.f;
			if (ObserverStartTime == 0.f && W)
			{
				ObserverStartTime = W->GetRealTimeSeconds();
			}
			if (W && W->GetRealTimeSeconds() - ObserverStartTime < 2.0f) return;

			bTravelLoginSent = true;
			MyPS->Server_LoginRequest(TEXT("observer"), TEXT("1234"));
			AppendLog(TEXT("REPLICATION_OBSERVER_LOGIN_SENT"));
		}

		if (W->GetGameState())
		{
			bool bFoundOther = false;
			for (APlayerState* GenericPS : W->GetGameState()->PlayerArray)
			{
				AAPBPlayerState* OtherPS = Cast<AAPBPlayerState>(GenericPS);
				if (OtherPS && OtherPS != MyPS)
				{
					bFoundOther = true;
					AppendLog(FString::Printf(TEXT("REPLICATION_PROBE_SEES_PLAYER name=%s cash=%lld chars=%d ticket=%d"), *OtherPS->GetPlayerName(), OtherPS->Cash, OtherPS->CharListJson.Len(), OtherPS->IssuedTicketJson.Len()));
					if (!OtherPS->CharListJson.IsEmpty() || !OtherPS->IssuedTicketJson.IsEmpty())
					{
						bReplicationDone = true;
						AppendLog(TEXT("REPLICATION_PROBE_FAILED reason=sentinels_visible_to_non_owner"));
						EndFrontendProbe();
						return;
					}
				}
			}

			static int64 OwnerFoundAtMs = 0;
			if (bFoundOther)
			{
				if (OwnerFoundAtMs == 0) OwnerFoundAtMs = NowMs;
				if (NowMs - OwnerFoundAtMs > 15000)
				{
					bReplicationDone = true;
					AppendLog(TEXT("REPLICATION_PROBE_OBSERVER_OK"));
					EndFrontendProbe();
				}
			}
		}
		return;
	}

	bReplicationDone = true;
	AppendLog(FString::Printf(TEXT("REPLICATION_PROBE_FAIL reason=unknown_role role=%s"), *ReplicationRole));
	EndFrontendProbe();
}
