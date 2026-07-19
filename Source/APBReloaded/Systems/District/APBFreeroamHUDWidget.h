#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "APBFreeroamHUDWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UBorder;

UCLASS()
class APBRELOADED_API UAPBFreeroamHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	UPROPERTY() TObjectPtr<UTextBlock> Line1 = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> Line2 = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> Line3 = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> Hint = nullptr;
};
