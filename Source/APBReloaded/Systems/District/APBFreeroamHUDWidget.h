#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "APBFreeroamHUDWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UBorder;
class UProgressBar;

UCLASS()
class APBRELOADED_API UAPBFreeroamHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	UFUNCTION()
	void OnMissionStateChanged();

	void BindOwningMissionState();
	void RefreshMissionDisplay();
	void RefreshCountdown();

	UPROPERTY() TObjectPtr<class AAPBPlayerState> BoundPlayerState = nullptr;

	UPROPERTY() TObjectPtr<UTextBlock> Line1 = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> Line2 = nullptr;
	UPROPERTY() TObjectPtr<UProgressBar> StageBar = nullptr;
	UPROPERTY() TObjectPtr<UProgressBar> OppBar = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> MissionLine = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> Hint = nullptr;
};
