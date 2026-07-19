#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "APBFreeroamHUD.generated.h"

class UAPBFreeroamHUDWidget;

UCLASS()
class APBRELOADED_API AAPBFreeroamHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;

	UPROPERTY()
	TObjectPtr<UAPBFreeroamHUDWidget> HudWidget = nullptr;
};
