#include "APBPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"
#include "APBWorldGameMode.h"
#include "APBServerControl.h"
#include "APBDistrictGameMode.h"
#include "APBGameInstanceSubsystem.h"
#include "APBRelayProtocol.h"
#include "Server/APBSecretProvider.h"
#include <string>

void AAPBPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAPBPlayerState, Faction);
	DOREPLIFETIME(AAPBPlayerState, ThreatPoints);
	DOREPLIFETIME(AAPBPlayerState, Cash);
	DOREPLIFETIME(AAPBPlayerState, G1C);
	DOREPLIFETIME(AAPBPlayerState, MissionTitle);
	DOREPLIFETIME(AAPBPlayerState, MissionStageIndex);
	DOREPLIFETIME(AAPBPlayerState, MissionStageCount);
	DOREPLIFETIME(AAPBPlayerState, MissionStageProgress);
	DOREPLIFETIME(AAPBPlayerState, MissionOppStageProgress);
	DOREPLIFETIME(AAPBPlayerState, bMissionOppositionContesting);
	DOREPLIFETIME(AAPBPlayerState, bMissionOppositionWon);
	DOREPLIFETIME(AAPBPlayerState, bMissionTimedOut);
	DOREPLIFETIME(AAPBPlayerState, MissionStageTimeLimitSec);
	DOREPLIFETIME(AAPBPlayerState, MissionStageDeadlineServerSec);
	DOREPLIFETIME(AAPBPlayerState, InventoryItemCount);
	DOREPLIFETIME(AAPBPlayerState, ProgressionState);
	DOREPLIFETIME(AAPBPlayerState, DistrictSessionId);
	DOREPLIFETIME(AAPBPlayerState, bWorldAuthOk);
	DOREPLIFETIME(AAPBPlayerState, CharListJson);
	DOREPLIFETIME(AAPBPlayerState, DistrictListJson);
	DOREPLIFETIME(AAPBPlayerState, IssuedTicketJson);
	DOREPLIFETIME(AAPBPlayerState, HandoffProbeJson);
	DOREPLIFETIME(AAPBPlayerState, ClanId);
	DOREPLIFETIME(AAPBPlayerState, ClanRole);
	DOREPLIFETIME(AAPBPlayerState, GroupId);
	DOREPLIFETIME(AAPBPlayerState, OnlineFriendCount);
	DOREPLIFETIME(AAPBPlayerState, bHasPendingClanInvite);
	DOREPLIFETIME(AAPBPlayerState, bHasPendingGroupInvite);
	DOREPLIFETIME(AAPBPlayerState, bGroupAllReady);
}

void AAPBPlayerState::ApplyFactionAuthority(EAPBFaction NewFaction)
{
	if (!HasAuthority()) return;
	Faction = NewFaction;
}

void AAPBPlayerState::ApplyDomainSnapshot(float Threat, int64 InCash, int64 InG1C, int32 InvCount,
	const FString& Mission, int32 StageIdx, int32 StageCount, const FString& SessionId,
	const FString& InProgressionState, const FAPBMissionSnapshotUE& MissionSnap)
{
	if (!HasAuthority()) return;
	ThreatPoints = Threat;
	Cash = InCash;
	G1C = InG1C;
	InventoryItemCount = InvCount;
	MissionTitle = Mission;
	MissionStageIndex = StageIdx;
	MissionStageCount = StageCount;
	DistrictSessionId = SessionId;
	ProgressionState = InProgressionState;
	MissionStageProgress = MissionSnap.MissionStageProgress;
	MissionOppStageProgress = MissionSnap.MissionOppStageProgress;
	bMissionOppositionContesting = MissionSnap.bMissionOppositionContesting;
	bMissionOppositionWon = MissionSnap.bMissionOppositionWon;
	bMissionTimedOut = MissionSnap.bMissionTimedOut;
	MissionStageTimeLimitSec = MissionSnap.MissionStageTimeLimitSec;
	MissionStageDeadlineServerSec = MissionSnap.MissionStageDeadlineServerSec;
	OnRep_Mission();
	ForceNetUpdate();
}

void AAPBPlayerState::ServerSetFaction_Implementation(EAPBFaction NewFaction)
{
	ApplyFactionAuthority(NewFaction);
}

bool AAPBPlayerState::ServerSetFaction_Validate(EAPBFaction NewFaction)
{
	return NewFaction == EAPBFaction::Enforcer || NewFaction == EAPBFaction::Criminal;
}

void AAPBPlayerState::OnRep_Faction() {}

static AAPBWorldGameMode* GetAuthGameMode(APlayerState* PS)
{
	if (!PS) return nullptr;
	APlayerController* PC = Cast<APlayerController>(PS->GetOwner());
	if (!PC) return nullptr;
	return PC->GetWorld()
		? Cast<AAPBWorldGameMode>(PC->GetWorld()->GetAuthGameMode())
		: nullptr;
}

static APlayerController* GetOwnerPC(APlayerState* PS)
{
	return PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;
}

void AAPBPlayerState::Server_LoginRequest_Implementation(const FString& User, const FString& Pass)
{
	AAPBWorldGameMode* GM = GetAuthGameMode(this);
	if (!GM || !GM->ServerControl) return;
	FString Err;
	const bool Ok = GM->ServerControl->LoginRequest(GetOwnerPC(this), User, Pass, Err);
	bWorldAuthOk = Ok;
	ForceNetUpdate();
}

bool AAPBPlayerState::Server_LoginRequest_Validate(const FString& User, const FString& Pass)
{
	return User.Len() > 0 && User.Len() <= 64 && Pass.Len() > 0 && Pass.Len() <= 128;
}

void AAPBPlayerState::Server_GetCharList_Implementation()
{
	AAPBWorldGameMode* GM = GetAuthGameMode(this);
	if (!GM || !GM->ServerControl) return;
	CharListJson = GM->ServerControl->GetCharListJson(GetOwnerPC(this));
	ForceNetUpdate();
}

bool AAPBPlayerState::Server_GetCharList_Validate()
{
	return true;
}

void AAPBPlayerState::Server_GetDistrictList_Implementation()
{
	AAPBWorldGameMode* GM = GetAuthGameMode(this);
	if (!GM || !GM->ServerControl) return;
	DistrictListJson = GM->ServerControl->GetDistrictListJson(GetOwnerPC(this));
	ForceNetUpdate();
}

bool AAPBPlayerState::Server_GetDistrictList_Validate()
{
	return true;
}

void AAPBPlayerState::Server_IssueTicket_Implementation(const FString& CharName, const FString& DistrictId)
{
	AAPBWorldGameMode* GM = GetAuthGameMode(this);
	if (!GM || !GM->ServerControl || !bWorldAuthOk) return;
	IssuedTicketJson = GM->ServerControl->IssueTicketJson(GetOwnerPC(this), CharName, DistrictId);
	ForceNetUpdate();
}

void AAPBPlayerState::Server_ReleaseTravelReservation_Implementation(const FString& ReservationId)
{
	AAPBWorldGameMode* GM = GetAuthGameMode(this);
	if (GM && GM->ServerControl)
	{
		GM->ReleaseTravelReservation(GetOwnerPC(this), ReservationId);
	}
}

bool AAPBPlayerState::Server_ReleaseTravelReservation_Validate(const FString& ReservationId)
{
	return ReservationId.Len() > 0 && ReservationId.Len() <= 128;
}

void AAPBPlayerState::Server_PrepareHandoffProbe_Implementation()
{
	if (AAPBWorldGameMode* GM = GetAuthGameMode(this))
	{
		HandoffProbeJson = GM->PrepareHandoffProbeJson(GetOwnerPC(this));
		ForceNetUpdate();
	}
}

bool AAPBPlayerState::Server_PrepareHandoffProbe_Validate()
{
	return true;
}

void AAPBPlayerState::Server_GetHandoffProbeState_Implementation()
{
	if (AAPBWorldGameMode* GM = GetAuthGameMode(this))
	{
		HandoffProbeJson = GM->GetHandoffProbeJson(GetOwnerPC(this));
		ForceNetUpdate();
	}
}

bool AAPBPlayerState::Server_GetHandoffProbeState_Validate()
{
	return true;
}

void AAPBPlayerState::Server_SubmitChat_Implementation(const FString& RawLine)
{
	APlayerController* OwnerController = GetOwnerPC(this);
	AAPBDistrictGameMode* DistrictGameMode = OwnerController && OwnerController->GetWorld()
		? Cast<AAPBDistrictGameMode>(OwnerController->GetWorld()->GetAuthGameMode())
		: nullptr;
	if (DistrictGameMode) DistrictGameMode->SubmitChat(OwnerController, RawLine);
}

bool AAPBPlayerState::Server_SubmitChat_Validate(const FString& RawLine)
{
	return !RawLine.IsEmpty() && RawLine.Len() <= 512;
}

void AAPBPlayerState::Client_ReceiveChat_Implementation(const FString& Channel, const FString& Sender,
	const FString& Recipient, const FString& Body)
{
	UE_LOG(LogTemp, Log, TEXT("CHAT_RECEIVED channel=%s from=%s to=%s body=%s"),
		*Channel, *Sender, *Recipient, *Body);
}

// ── M14 social RPCs (T13) ───────────────────────────────────────────────────
// Resolve the authenticated character name for this PlayerState. On a district with
// ticketed players, uses the verified ticket; otherwise falls back to GetPlayerName.
static FString AuthCharacterFor(APlayerState* PS)
{
	if (!PS) return FString();
	if (APlayerController* PC = Cast<APlayerController>(PS->GetOwner()))
	{
		if (AAPBDistrictGameMode* DGM = Cast<AAPBDistrictGameMode>(PC->GetWorld() ? PC->GetWorld()->GetAuthGameMode() : nullptr))
		{
			return DGM->ResolveDistrictPlayerName(PC);
		}
	}
	return PS->GetPlayerName();
}

// Helper: dispatch a social op through the UGI bridge and return a status string.
static FString DispatchSocialOpDirect(UAPBGameInstanceSubsystem* APB, const FString& Character,
	const FString& SocialOp, const FString& Arg1, const FString& Arg2);

// Dispatch a social operation on the world authority. On the world GameMode, calls
// the UGI bridge directly. On a district, forwards a SocialRequest relay message
// to the world. The result is pushed back to the PlayerState replicated fields by
// the world-side relay handler.
static void DispatchSocialOp(APlayerState* PS, const FString& Character,
	const FString& SocialOp, const FString& Arg1, const FString& Arg2)
{
	if (!PS) return;
	APlayerController* PC = Cast<APlayerController>(PS->GetOwner());
	if (!PC || !PC->GetWorld()) return;
	UWorld* World = PC->GetWorld();
	UAPBGameInstanceSubsystem* APB = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;

	// On the world authority (or standalone), the bridge dispatches directly.
	if (World->GetAuthGameMode<AAPBWorldGameMode>() || World->GetNetMode() == NM_Standalone)
	{
		const FString Status = DispatchSocialOpDirect(APB, Character, SocialOp, Arg1, Arg2);
		UE_LOG(LogTemp, Log, TEXT("SOCIAL_RPC_OP character=%s op=%s status=%s"),
			*Character, *SocialOp, *Status);
		return;
	}

	// On a district server, forward as a SocialRequest relay message to the world.
	if (AAPBDistrictGameMode* DGM = Cast<AAPBDistrictGameMode>(World->GetAuthGameMode()))
	{
		UAPBServerControl* Relay = DGM->GetRelayControl();
		if (Relay)
		{
			const FString& Secret = FAPBSecretProvider::RelaySecret();
			if (!Secret.IsEmpty())
			{
				const int64 NowMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL + FDateTime::UtcNow().GetMillisecond();
				const std::string OpId = "soc-" + std::to_string(Relay->GetDistrictNumericId()) +
					"-" + TCHAR_TO_UTF8(*SocialOp) + "-" + std::to_string(NowMs);
				const std::string Body = std::string("{\"arg1\":\"") +
					TCHAR_TO_UTF8(*Arg1) + "\",\"arg2\":\"" + TCHAR_TO_UTF8(*Arg2) + "\"}";
				const apb::RelayMessage Msg = apb::RelayCodec::MakeSocialRequest(
					"", TCHAR_TO_UTF8(*Character), TCHAR_TO_UTF8(*SocialOp), OpId, Body,
					Relay->GetDistrictNumericId(), OpId, NowMs,
					TCHAR_TO_UTF8(*Secret));
				Relay->SendRelayToWorld(Msg);
				UE_LOG(LogTemp, Log, TEXT("SOCIAL_RELAY_FORWARD character=%s op=%s op_id=%s"),
					*Character, *SocialOp, UTF8_TO_TCHAR(OpId.c_str()));
			}
		}
		return;
	}

	// Fallback: standalone bridge
	DispatchSocialOpDirect(APB, Character, SocialOp, Arg1, Arg2);
}

static FString DispatchSocialOpDirect(UAPBGameInstanceSubsystem* APB, const FString& Character,
	const FString& SocialOp, const FString& Arg1, const FString& Arg2)
{
	if (!APB) return TEXT("no_authority");
	const FString Op = SocialOp.ToLower();

	// Clan ops
	if (Op == TEXT("clan.create"))
	{
		const bool bOk = APB->SocialClanCreate(Arg1, Arg1, Arg2, Character, false);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("clan.invite"))
	{
		const bool bOk = APB->SocialClanInvite(Character, Arg1);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("clan.accept"))
	{
		const bool bOk = APB->SocialClanAcceptInvite(Character);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("clan.decline"))
	{
		const bool bOk = APB->SocialClanDeclineInvite(Character);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("clan.leave"))
	{
		const bool bOk = APB->SocialClanLeave(Character);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("clan.kick"))
	{
		const bool bOk = APB->SocialClanKick(Character, Arg1);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("clan.setmotd"))
	{
		const bool bOk = APB->SocialClanSetMotd(Character, Arg1);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("clan.disband"))
	{
		const bool bOk = APB->SocialClanDisband(Character);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}

	// Friend ops
	if (Op == TEXT("friend.request"))
	{
		const bool bOk = APB->SocialFriendRequest(Character, Arg1);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("friend.accept"))
	{
		const bool bOk = APB->SocialFriendAccept(Character, Arg1);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("friend.decline"))
	{
		const bool bOk = APB->SocialFriendDecline(Character, Arg1);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("friend.remove"))
	{
		const bool bOk = APB->SocialFriendRemove(Character, Arg1);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("friend.ignore"))
	{
		const bool bOk = APB->SocialFriendIgnore(Character, Arg1);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("friend.unignore"))
	{
		const bool bOk = APB->SocialFriendUnignore(Character, Arg1);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}

	// Group ops
	if (Op == TEXT("group.create"))
	{
		FString OutId;
		const bool bOk = APB->SocialGroupCreate(Character, OutId);
		return bOk ? FString::Printf(TEXT("ok:%s"), *OutId) : TEXT("domain_rejected");
	}
	if (Op == TEXT("group.invite"))
	{
		const bool bOk = APB->SocialGroupInvite(Character, Arg1);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("group.accept"))
	{
		const bool bOk = APB->SocialGroupAccept(Character);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("group.leave"))
	{
		const bool bOk = APB->SocialGroupLeave(Character);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("group.setready"))
	{
		const bool bOk = APB->SocialGroupSetReady(Character, Arg1 == TEXT("1") || Arg1 == TEXT("true"));
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("group.disband"))
	{
		const bool bOk = APB->SocialGroupDisband(Character);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}

	// Mail ops
	if (Op == TEXT("mail.send"))
	{
		// Arg1 = recipient, Arg2 = subject|body (pipe-separated)
		FString Subject, BodyText;
		if (Arg2.Contains(TEXT("|")))
		{
			Subject = Arg2.Left(Arg2.Find(TEXT("|")));
			BodyText = Arg2.RightChop(Arg2.Find(TEXT("|") ) + 1);
		}
		else
		{
			Subject = Arg2;
			BodyText = TEXT("");
		}
		const bool bOk = APB->SocialMailSend(Character, Arg1, Subject, BodyText, 0);
		return bOk ? TEXT("ok") : TEXT("domain_rejected");
	}
	if (Op == TEXT("mail.markread"))
	{
		const EAPBMailResult R = APB->SocialMailMarkRead(Character, Arg1);
		return R == EAPBMailResult::Ok ? TEXT("ok") : TEXT("domain_rejected");
	}

	return TEXT("unknown_op");
}

void AAPBPlayerState::Server_SocialClan_Implementation(const FString& Op, const FString& Arg1, const FString& Arg2)
{
	const FString Character = AuthCharacterFor(this);
	if (Character.IsEmpty()) return;
	DispatchSocialOp(this, Character, TEXT("clan.") + Op, Arg1, Arg2);
}

bool AAPBPlayerState::Server_SocialClan_Validate(const FString& Op, const FString& Arg1, const FString& Arg2)
{
	return Op.Len() > 0 && Op.Len() <= 32 && Arg1.Len() <= 64 && Arg2.Len() <= 256;
}

void AAPBPlayerState::Server_SocialFriend_Implementation(const FString& Op, const FString& Target)
{
	const FString Character = AuthCharacterFor(this);
	if (Character.IsEmpty()) return;
	DispatchSocialOp(this, Character, TEXT("friend.") + Op, Target, FString());
}

bool AAPBPlayerState::Server_SocialFriend_Validate(const FString& Op, const FString& Target)
{
	return Op.Len() > 0 && Op.Len() <= 32 && Target.Len() <= 64;
}

void AAPBPlayerState::Server_SocialGroup_Implementation(const FString& Op, const FString& Arg1, const FString& Arg2)
{
	const FString Character = AuthCharacterFor(this);
	if (Character.IsEmpty()) return;
	DispatchSocialOp(this, Character, TEXT("group.") + Op, Arg1, Arg2);
}

bool AAPBPlayerState::Server_SocialGroup_Validate(const FString& Op, const FString& Arg1, const FString& Arg2)
{
	return Op.Len() > 0 && Op.Len() <= 32 && Arg1.Len() <= 64 && Arg2.Len() <= 256;
}

void AAPBPlayerState::Server_SocialMail_Implementation(const FString& Op, const FString& Arg1)
{
	const FString Character = AuthCharacterFor(this);
	if (Character.IsEmpty()) return;
	DispatchSocialOp(this, Character, TEXT("mail.") + Op, Arg1, FString());
}

bool AAPBPlayerState::Server_SocialMail_Validate(const FString& Op, const FString& Arg1)
{
	return Op.Len() > 0 && Op.Len() <= 32 && Arg1.Len() <= 256;
}

void AAPBPlayerState::Client_SocialResult_Implementation(const FString& Op, const FString& Status, const FString& Body)
{
	UE_LOG(LogTemp, Log, TEXT("SOCIAL_RESULT op=%s status=%s body=%s"), *Op, *Status, *Body);
}


bool AAPBPlayerState::Server_IssueTicket_Validate(const FString& CharName, const FString& DistrictId)
{
	return CharName.Len() > 0 && CharName.Len() <= 64
		&& DistrictId.Len() > 0 && DistrictId.Len() <= 64;
}

void AAPBPlayerState::OnRep_WorldAuth()
{
	UE_LOG(LogTemp, Warning, TEXT("APBPlayerState OnRep_WorldAuth authOk=%d charList=%s districtList=%s ticket=%s"),
		bWorldAuthOk ? 1 : 0, *CharListJson, *DistrictListJson, *IssuedTicketJson);
}

void AAPBPlayerState::OnRep_Social()
{
	UE_LOG(LogTemp, Log, TEXT("APBPlayerState OnRep_Social clan=%s role=%s group=%s onlineFriends=%d clanInvite=%d groupInvite=%d allReady=%d"),
		*ClanId, *ClanRole, *GroupId, OnlineFriendCount, bHasPendingClanInvite ? 1 : 0,
		bHasPendingGroupInvite ? 1 : 0, bGroupAllReady ? 1 : 0);
}

static void AppendPeerObserve(const FString& Line)
{
	const FString Path = TEXT("C:/Users/Support/AppData/Local/Temp/grok-goal-9ca60165ac93/implementer/mp_client_observe.log");
	FFileHelper::SaveStringToFile(Line + TEXT("\n"), *Path, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
}

void AAPBPlayerState::OnRep_Economy()
{
	AppendPeerObserve(FString::Printf(TEXT("CLIENT_OBS economy threat=%.1f cash=%lld g1c=%lld inv=%d player=%s"),
		ThreatPoints, Cash, G1C, InventoryItemCount, *GetPlayerName()));
}

void AAPBPlayerState::OnRep_Mission()
{
	AppendPeerObserve(FString::Printf(TEXT("CLIENT_OBS mission=%s stage=%d/%d timed_out=%d opp_won=%d contesting=%d opp=%.2f session=%s player=%s"),
		*MissionTitle, MissionStageIndex, MissionStageCount, bMissionTimedOut ? 1 : 0, bMissionOppositionWon ? 1 : 0,
		bMissionOppositionContesting ? 1 : 0, MissionOppStageProgress, *DistrictSessionId, *GetPlayerName()));
	OnMissionUpdated.Broadcast();
}
