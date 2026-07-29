#include "APBFreeroamHUD.h"
#include "APBFreeroamHUDWidget.h"
#include "APBSocialWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"

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
		// M14: create social widget, add above HUD, start collapsed.
		SocialWidget = CreateWidget<UAPBSocialWidget>(PC, UAPBSocialWidget::StaticClass());
		if (SocialWidget)
		{
			SocialWidget->AddToViewport(11);
			SocialWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		// Bind 'O' key (retail APB social menu key) to toggle the social panel.
		// Bind 'O' key (retail APB social menu key) to toggle the social panel.
		PC->InputComponent->BindKey(EKeys::O, IE_Pressed, this, &AAPBFreeroamHUD::OnToggleSocial);
		PC->bShowMouseCursor = false;
		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
	}
}

void AAPBFreeroamHUD::OnToggleSocial()
{
	if (!SocialWidget) return;
	SocialWidget->ToggleVisibility();
	const bool bVisible = SocialWidget->GetVisibility() != ESlateVisibility::Collapsed;
	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->bShowMouseCursor = bVisible;
		if (bVisible)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(SocialWidget->TakeWidget());
			PC->SetInputMode(InputMode);
		}
		else
		{
			FInputModeGameOnly GameMode;
			PC->SetInputMode(GameMode);
		}
	}
}
