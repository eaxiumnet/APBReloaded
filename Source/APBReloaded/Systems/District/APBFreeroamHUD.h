#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "APBFreeroamHUD.generated.h"

class UAPBFreeroamHUDWidget;
class UAPBSocialWidget;

UCLASS()
class APBRELOADED_API AAPBFreeroamHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;

	/** M14: Toggle social panel (bound to 'O' key via InputComponent). */
	UFUNCTION() void OnToggleSocial();

	UPROPERTY()
	TObjectPtr<UAPBFreeroamHUDWidget> HudWidget = nullptr;

	/** M14 social panel — toggled with the 'O' key (retail APB social menu key). */
	UPROPERTY()
	TObjectPtr<UAPBSocialWidget> SocialWidget = nullptr;
};
