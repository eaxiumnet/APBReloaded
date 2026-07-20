#include "APBServerControl.h"
#include "APBWorldGameMode.h"
#include "Misc/Parse.h"

void UAPBServerControl::Init(AAPBWorldGameMode* InMode)
{
	Mode = InMode;
	bWorldServerRole = FParse::Param(FCommandLine::Get(), TEXT("WorldServer"));
	if (bWorldServerRole)
	{
		UE_LOG(LogTemp, Warning, TEXT("APBServerControl role=WorldServer"));
	}
}

bool UAPBServerControl::LoginRequest(APlayerController* PC,
                                     const FString& User, const FString& Pass,
                                     FString& OutError)
{
	if (!Mode) { OutError = TEXT("no_mode"); return false; }
	return Mode->LoginPlayer(PC, User, Pass, OutError);
}

FString UAPBServerControl::GetCharListJson(APlayerController* PC) const
{
	if (!Mode) return TEXT("[]");
	return Mode->GetCharListJson(PC);
}

FString UAPBServerControl::GetDistrictListJson(APlayerController* PC) const
{
	if (!Mode) return TEXT("[]");
	return Mode->GetDistrictListJson(PC);
}

FString UAPBServerControl::IssueTicketJson(APlayerController* PC,
                                           const FString& CharName,
                                           const FString& DistrictId)
{
	if (!Mode) return TEXT("");
	return Mode->IssueTicketJson(PC, CharName, DistrictId);
}
