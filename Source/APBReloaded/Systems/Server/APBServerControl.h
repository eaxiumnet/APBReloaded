#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "APBServerControl.generated.h"

class AAPBWorldGameMode;
class APlayerController;

/** Role selector + auth-gated serving surface for the world-server process.
 *  Owned by AAPBWorldGameMode; created in BeginPlay on the authority. */
UCLASS()
class APBRELOADED_API UAPBServerControl : public UObject
{
	GENERATED_BODY()
public:
	void Init(AAPBWorldGameMode* InMode);

	bool IsWorldServerRole() const { return bWorldServerRole; }

	bool LoginRequest(APlayerController* PC,
	                  const FString& User, const FString& Pass,
	                  FString& OutError);

	FString GetCharListJson(APlayerController* PC) const;
	FString GetDistrictListJson(APlayerController* PC) const;

	/** Only callable when PC is already logged in. */
	FString IssueTicketJson(APlayerController* PC,
	                        const FString& CharName, const FString& DistrictId);

private:
	UPROPERTY()
	AAPBWorldGameMode* Mode = nullptr;

	bool bWorldServerRole = false;
};
