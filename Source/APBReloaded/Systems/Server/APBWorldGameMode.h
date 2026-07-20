#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "APBWorldService.h"
#include "APBWorldGameMode.generated.h"

class UAPBServerControl;

struct FAPBPlayerService {
	TUniquePtr<apb::WorldService> Service;
};

UCLASS()
class APBRELOADED_API AAPBWorldGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AAPBWorldGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UPROPERTY()
	UAPBServerControl* ServerControl = nullptr;

	bool LoginPlayer(APlayerController* PC,
	                 const FString& User, const FString& Pass,
	                 FString& OutError);
	FString GetCharListJson(APlayerController* PC) const;
	FString GetDistrictListJson(APlayerController* PC) const;
	FString IssueTicketJson(APlayerController* PC,
	                        const FString& CharName, const FString& DistrictId);

private:
	TMap<FString, TUniquePtr<FAPBPlayerService>> PlayerServices;
	FString PersistDir;
	FString DataDir;
	FAPBPlayerService* ServiceFor(APlayerController* PC) const;
	static FString PCKey(APlayerController* PC);
};
