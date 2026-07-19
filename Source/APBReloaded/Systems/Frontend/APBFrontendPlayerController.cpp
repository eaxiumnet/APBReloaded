#include "APBFrontendPlayerController.h"
#include "APBFrontendWidget.h"
#include "APBDebugMenuWidget.h"
#include "APBFrontendHUD.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"

void AAPBFrontendPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &AAPBFrontendPlayerController::ShowFrontendUI));
		FTimerHandle Retry;
		World->GetTimerManager().SetTimer(Retry, FTimerDelegate::CreateUObject(this, &AAPBFrontendPlayerController::ShowFrontendUI), 0.25f, false);
	}
	else
	{
		ShowFrontendUI();
	}
}

void AAPBFrontendPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent)
	{
		return;
	}

	// F8 = open/close debug menu (works in GameAndUI)
	InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &AAPBFrontendPlayerController::ToggleDebugMenu);
	// Backtick as alternate (common for debug)
	InputComponent->BindKey(EKeys::Tilde, IE_Pressed, this, &AAPBFrontendPlayerController::ToggleDebugMenu);
}

void AAPBFrontendPlayerController::ToggleDebugMenu()
{
	if (!DebugMenuWidget)
	{
		DebugMenuWidget = CreateWidget<UAPBDebugMenuWidget>(this, UAPBDebugMenuWidget::StaticClass());
		if (DebugMenuWidget)
		{
			DebugMenuWidget->SetIsFocusable(true);
			DebugMenuWidget->AddToViewport(5000);
			DebugMenuWidget->BindFrontend(FrontendWidget);
		}
	}
	if (DebugMenuWidget)
	{
		if (FrontendWidget)
		{
			DebugMenuWidget->BindFrontend(FrontendWidget);
		}
		DebugMenuWidget->ToggleVisible();
		bShowMouseCursor = true;
		UE_LOG(LogTemp, Warning, TEXT("APBDebug toggle open=%d"), DebugMenuWidget->IsMenuVisible() ? 1 : 0);
	}
}

void AAPBFrontendPlayerController::ShowFrontendUI()
{
	if (FrontendWidget && FrontendWidget->IsInViewport())
	{
		bShowMouseCursor = true;
		// Ensure debug HUD host exists even on retry
		if (!DebugMenuWidget)
		{
			DebugMenuWidget = CreateWidget<UAPBDebugMenuWidget>(this, UAPBDebugMenuWidget::StaticClass());
			if (DebugMenuWidget)
			{
				DebugMenuWidget->SetIsFocusable(true);
				DebugMenuWidget->AddToViewport(5000);
				DebugMenuWidget->BindFrontend(FrontendWidget);
				// Corner FPS visible; panel starts closed
			}
		}
		return;
	}

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	if (!FrontendWidget)
	{
		FrontendWidget = CreateWidget<UAPBFrontendWidget>(this, UAPBFrontendWidget::StaticClass());
	}
	if (FrontendWidget)
	{
		FrontendWidget->SetIsFocusable(true);
		FrontendWidget->SetVisibility(ESlateVisibility::Visible);
		FrontendWidget->SetIsEnabled(true);
		if (!FrontendWidget->IsInViewport())
		{
			FrontendWidget->AddToViewport(1000);
		}

		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetWidgetToFocus(FrontendWidget->TakeWidget());
		SetInputMode(Mode);
	}
	else
	{
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}

	if (!DebugMenuWidget)
	{
		DebugMenuWidget = CreateWidget<UAPBDebugMenuWidget>(this, UAPBDebugMenuWidget::StaticClass());
		if (DebugMenuWidget)
		{
			DebugMenuWidget->SetIsFocusable(true);
			DebugMenuWidget->AddToViewport(5000);
			DebugMenuWidget->BindFrontend(FrontendWidget);
		}
	}
	else if (FrontendWidget)
	{
		DebugMenuWidget->BindFrontend(FrontendWidget);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9000, 12.f, FColor::Cyan,
			TEXT("APB: F8 = Debug menu (stages / FPS). Corner FPS is on by default."));
	}
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend ShowFrontendUI widget=%d debug=%d"),
		FrontendWidget != nullptr ? 1 : 0,
		DebugMenuWidget != nullptr ? 1 : 0);

	if (AAPBFrontendHUD* H = Cast<AAPBFrontendHUD>(GetHUD()))
	{
		H->SetHudStage(TEXT("Splash"), TEXT("F8 debug | login video + form"));
	}
}
