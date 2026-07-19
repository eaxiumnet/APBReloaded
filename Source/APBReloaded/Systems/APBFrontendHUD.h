#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "APBFrontendTypes.h"
#include "APBFrontendHUD.generated.h"

/** Always-on canvas text so a black map never looks "dead" without UMG. */
UCLASS()
class APBRELOADED_API AAPBFrontendHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;

	UPROPERTY(BlueprintReadWrite, Category="APB|UI")
	FString StageLine = TEXT("Splash");

	UPROPERTY(BlueprintReadWrite, Category="APB|UI")
	FString StatusLine = TEXT("Loading...");

	UPROPERTY(BlueprintReadWrite, Category="APB|UI")
	FString HelpLine = TEXT("Click CONTINUE or wait 2s  |  Mouse to click UI");

	void SetHudStage(const FString& Stage, const FString& Status);
};
