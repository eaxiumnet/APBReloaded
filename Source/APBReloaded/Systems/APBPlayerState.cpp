#include "APBPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
