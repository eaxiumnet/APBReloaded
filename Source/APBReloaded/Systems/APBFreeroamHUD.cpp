#include "APBFreeroamHUD.h"
#include "APBFreeroamHUDWidget.h"
#include "Blueprint/UserWidget.h"

void AAPBFreeroamHUD::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* PC = GetOwningPlayerController())
	{
		HudWidget = CreateWidget<UAPBFreeroamHUDWidget>(PC, UAPBFreeroamHUDWidget::StaticClass());
		if (HudWidget)
		{
			HudWidget->AddToViewport(10);
		}
		PC->bShowMouseCursor = false;
		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
	}
}
