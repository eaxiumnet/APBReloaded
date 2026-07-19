#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "APBFrontendPlayerController.generated.h"

class UAPBFrontendWidget;
class UAPBDebugMenuWidget;

UCLASS()
class APBRELOADED_API AAPBFrontendPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UFUNCTION()
	void ShowFrontendUI();

	UFUNCTION()
	void ToggleDebugMenu();

	UPROPERTY(BlueprintReadOnly, Category="APB|UI")
	TObjectPtr<UAPBFrontendWidget> FrontendWidget = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="APB|Debug")
	TObjectPtr<UAPBDebugMenuWidget> DebugMenuWidget = nullptr;
};
