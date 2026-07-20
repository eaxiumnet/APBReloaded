#include "APBPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"
#include "APBWorldGameMode.h"
#include "APBServerControl.h"

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
	DOREPLIFETIME(AAPBPlayerState, InventoryItemCount);
	DOREPLIFETIME(AAPBPlayerState, DistrictSessionId);
	DOREPLIFETIME(AAPBPlayerState, bWorldAuthOk);
	DOREPLIFETIME(AAPBPlayerState, CharListJson);
	DOREPLIFETIME(AAPBPlayerState, DistrictListJson);
	DOREPLIFETIME(AAPBPlayerState, IssuedTicketJson);
}

void AAPBPlayerState::ApplyFactionAuthority(EAPBFaction NewFaction)
{
	if (!HasAuthority()) return;
	Faction = NewFaction;
}

void AAPBPlayerState::ApplyDomainSnapshot(float Threat, int64 InCash, int64 InG1C, int32 InvCount,
	const FString& Mission, int32 StageIdx, int32 StageCount, const FString& SessionId)
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
	AppendPeerObserve(FString::Printf(TEXT("CLIENT_OBS mission=%s stage=%d/%d session=%s player=%s"),
		*MissionTitle, MissionStageIndex, MissionStageCount, *DistrictSessionId, *GetPlayerName()));
}
