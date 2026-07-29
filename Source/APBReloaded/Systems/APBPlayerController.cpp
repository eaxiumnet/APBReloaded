#include "APBPlayerController.h"
#include "APBSessionProbeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/Parse.h"

void AAPBPlayerController::APBChat(const int32 DelayMs, const FString& RawLine)
{
	FString Probe;
	if (!FParse::Value(FCommandLine::Get(), TEXT("APBProbe="), Probe) ||
		!Probe.Equals(TEXT("world_chat_client"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=DevCommandDisabled"));
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UAPBSessionProbeSubsystem* ProbeSubsystem = GameInstance->GetSubsystem<UAPBSessionProbeSubsystem>())
		{
			ProbeSubsystem->ScheduleDevChat(DelayMs, RawLine);
		}
	}
}

void AAPBPlayerController::APBChatTravel(const int32 DelayMs, const FString& DistrictId)
{
	FString Probe;
	if (!FParse::Value(FCommandLine::Get(), TEXT("APBProbe="), Probe) ||
		!Probe.Equals(TEXT("world_chat_client"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Warning, TEXT("CHAT_DENIED reason=DevCommandDisabled"));
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UAPBSessionProbeSubsystem* ProbeSubsystem = GameInstance->GetSubsystem<UAPBSessionProbeSubsystem>())
		{
			ProbeSubsystem->ScheduleChatDistrictTravel(DelayMs, DistrictId);
		}
	}
}
