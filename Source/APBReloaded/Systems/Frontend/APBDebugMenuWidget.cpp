#include "APBDebugMenuWidget.h"
#include "APBFrontendWidget.h"
#include "APBFrontendPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

namespace APBDebugUI
{
	static const FLinearColor PanelBg(0.04f, 0.06f, 0.10f, 0.94f);
	static const FLinearColor TitleCol(0.35f, 0.85f, 1.f, 1.f);
	static const FLinearColor BodyCol(0.92f, 0.94f, 0.98f, 1.f);
	static const FLinearColor Muted(0.65f, 0.72f, 0.80f, 1.f);
	static const FLinearColor Accent(0.95f, 0.70f, 0.20f, 1.f);
	static const FLinearColor Good(0.35f, 0.90f, 0.45f, 1.f);
}

UTextBlock* UAPBDebugMenuWidget::MakeLbl(const FString& Name, const FString& Text, int32 Size, FLinearColor Color)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *Name);
	T->SetText(FText::FromString(Text));
	T->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", Size));
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetAutoWrapText(true);
	return T;
}

UButton* UAPBDebugMenuWidget::MakeBtn(const FString& Name, const FString& Label, float Width)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);
	FButtonStyle Style = B->GetStyle();
	Style.Normal.DrawAs = ESlateBrushDrawType::Box;
	Style.Normal.TintColor = FSlateColor(FLinearColor(0.10f, 0.22f, 0.38f, 1.f));
	Style.Hovered.DrawAs = ESlateBrushDrawType::Box;
	Style.Hovered.TintColor = FSlateColor(FLinearColor(0.16f, 0.38f, 0.62f, 1.f));
	Style.Pressed.DrawAs = ESlateBrushDrawType::Box;
	Style.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.16f, 0.28f, 1.f));
	B->SetStyle(Style);
	B->SetBackgroundColor(FLinearColor(0.12f, 0.28f, 0.48f, 1.f));

	UTextBlock* L = MakeLbl(Name + TEXT("_L"), Label, 12, APBDebugUI::BodyCol);
	B->AddChild(L);

	if (Width > 1.f)
	{
		// Width is applied by parent SizeBox when needed; button itself fills slot.
	}
	return B;
}

void UAPBDebugMenuWidget::AddRow(UWidget* W, float PadY)
{
	if (!BodyBox || !W) return;
	if (UVerticalBoxSlot* S = BodyBox->AddChildToVerticalBox(W))
	{
		S->SetPadding(FMargin(4.f, PadY));
		S->SetHorizontalAlignment(HAlign_Fill);
	}
}

TSharedRef<SWidget> UAPBDebugMenuWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UAPBDebugMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!RootCanvas)
	{
		BuildLayout();
	}
	// Start closed; F8 opens. Corner FPS can still show.
	SetMenuVisible(false);
	SetShowFpsOverlay(true);
	// Ensure default 60 FPS if nothing else set a cap yet
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		const float Cur = CVar->GetFloat();
		if (Cur <= 0.f)
		{
			ApplyFpsCap(60.f);
		}
		else
		{
			CurrentFpsCap = Cur;
		}
	}
	else
	{
		ApplyFpsCap(60.f);
	}
}

void UAPBDebugMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (InDeltaTime > KINDA_SMALL_NUMBER)
	{
		const float Inst = 1.f / InDeltaTime;
		SmoothedFps = (SmoothedFps <= 1.f) ? Inst : FMath::Lerp(SmoothedFps, Inst, 0.12f);
	}

	ReadoutAccum += InDeltaTime;
	if (ReadoutAccum >= 0.15f)
	{
		ReadoutAccum = 0.f;
		RefreshReadouts();
	}
}

void UAPBDebugMenuWidget::BindFrontend(UAPBFrontendWidget* InFrontend)
{
	FrontendRef = InFrontend;
}

UAPBFrontendWidget* UAPBDebugMenuWidget::ResolveFrontend() const
{
	if (FrontendRef)
	{
		return FrontendRef.Get();
	}
	if (AAPBFrontendPlayerController* PC = Cast<AAPBFrontendPlayerController>(GetOwningPlayer()))
	{
		return PC->FrontendWidget;
	}
	return nullptr;
}

void UAPBDebugMenuWidget::ToggleVisible()
{
	SetMenuVisible(!bMenuOpen);
}

void UAPBDebugMenuWidget::SetMenuVisible(bool bVisible)
{
	bMenuOpen = bVisible;
	if (PanelBorder)
	{
		PanelBorder->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (BodyScroll)
	{
		BodyScroll->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (TitleText)
	{
		// Title lives inside panel — handled by panel visibility
	}
	UE_LOG(LogTemp, Log, TEXT("APBDebug menu_open=%d"), bMenuOpen ? 1 : 0);
}

void UAPBDebugMenuWidget::SetShowFpsOverlay(bool bShow)
{
	bShowFpsOverlay = bShow;
	if (FpsCornerText)
	{
		FpsCornerText->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UAPBDebugMenuWidget::ApplyFpsCap(float CapOrZero)
{
	CurrentFpsCap = FMath::Max(0.f, CapOrZero);
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		CVar->Set(CurrentFpsCap, ECVF_SetByCode);
	}
	if (GEngine)
	{
		// Also set engine path used by some builds
		GEngine->SetMaxFPS(CurrentFpsCap);
	}
	UE_LOG(LogTemp, Warning, TEXT("APBDebug t.MaxFPS=%.0f"), CurrentFpsCap);
	RefreshReadouts();
}

void UAPBDebugMenuWidget::ExecConsole(const FString& Cmd)
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ConsoleCommand(*Cmd);
	}
	else if (GEngine && GetWorld())
	{
		GEngine->Exec(GetWorld(), *Cmd);
	}
}

void UAPBDebugMenuWidget::JumpStage(EAPBFrontendStage Stage)
{
	if (UAPBFrontendWidget* UI = ResolveFrontend())
	{
		UI->SetStage(Stage);
		UE_LOG(LogTemp, Warning, TEXT("APBDebug JumpStage -> %s"), *UI->GetStageToken());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("APBDebug JumpStage failed: no FrontendWidget"));
	}
	RefreshReadouts();
}

void UAPBDebugMenuWidget::OnStageSplash() { JumpStage(EAPBFrontendStage::Splash); }
void UAPBDebugMenuWidget::OnStageLogin() { JumpStage(EAPBFrontendStage::Login); }
void UAPBDebugMenuWidget::OnStageCharSelect() { JumpStage(EAPBFrontendStage::CharacterSelect); }
void UAPBDebugMenuWidget::OnStageCharCreate() { JumpStage(EAPBFrontendStage::CharacterCreate); }
void UAPBDebugMenuWidget::OnStageDistrict() { JumpStage(EAPBFrontendStage::DistrictSelect); }
void UAPBDebugMenuWidget::OnStageLoading() { JumpStage(EAPBFrontendStage::Loading); }
void UAPBDebugMenuWidget::OnStageSettings() { JumpStage(EAPBFrontendStage::Settings); }

void UAPBDebugMenuWidget::OnFpsUncap() { ApplyFpsCap(0.f); }
void UAPBDebugMenuWidget::OnFps30() { ApplyFpsCap(30.f); }
void UAPBDebugMenuWidget::OnFps60() { ApplyFpsCap(60.f); }
void UAPBDebugMenuWidget::OnFps120() { ApplyFpsCap(120.f); }
void UAPBDebugMenuWidget::OnFps144() { ApplyFpsCap(144.f); }

void UAPBDebugMenuWidget::OnToggleFpsOverlay()
{
	SetShowFpsOverlay(!bShowFpsOverlay);
	RefreshReadouts();
}

void UAPBDebugMenuWidget::OnToggleStatFps()
{
	bStatFpsOn = !bStatFpsOn;
	ExecConsole(TEXT("stat fps"));
	RefreshReadouts();
}

void UAPBDebugMenuWidget::OnToggleStatUnit()
{
	bStatUnitOn = !bStatUnitOn;
	ExecConsole(TEXT("stat unit"));
	RefreshReadouts();
}

void UAPBDebugMenuWidget::OnToggleStatUnitGraph()
{
	bStatUnitGraphOn = !bStatUnitGraphOn;
	ExecConsole(TEXT("stat unitgraph"));
	RefreshReadouts();
}

void UAPBDebugMenuWidget::OnVolMute()
{
	if (UAPBFrontendWidget* UI = ResolveFrontend())
	{
		UI->SetMenuAudioVolume(0.f);
	}
	RefreshReadouts();
}

void UAPBDebugMenuWidget::OnVolHalf()
{
	if (UAPBFrontendWidget* UI = ResolveFrontend())
	{
		UI->SetMenuAudioVolume(0.5f);
	}
	RefreshReadouts();
}

void UAPBDebugMenuWidget::OnVolFull()
{
	if (UAPBFrontendWidget* UI = ResolveFrontend())
	{
		UI->SetMenuAudioVolume(1.f);
	}
	RefreshReadouts();
}

void UAPBDebugMenuWidget::OnHidePanel()
{
	SetMenuVisible(false);
}

void UAPBDebugMenuWidget::OnCloseMenu()
{
	SetMenuVisible(false);
}

void UAPBDebugMenuWidget::RefreshReadouts()
{
	const UAPBFrontendWidget* UI = ResolveFrontend();
	const FString Stage = UI ? UI->GetStageToken() : TEXT("(no frontend)");
	const float MenuVol = UI ? UI->GetMenuAudioVolume() : -1.f;

	FVector2D VP(0.f, 0.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(VP);
	}

	const FString CapStr = (CurrentFpsCap <= 0.f) ? TEXT("uncapped") : FString::Printf(TEXT("%.0f"), CurrentFpsCap);

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("Stage: %s\nFPS: %.0f  |  Cap: %s\nViewport: %.0fx%.0f\nMenuVol: %.2f  |  OverlayFPS: %s\nstat fps:%s  unit:%s  unitgraph:%s"),
			*Stage,
			SmoothedFps,
			*CapStr,
			VP.X, VP.Y,
			MenuVol,
			bShowFpsOverlay ? TEXT("ON") : TEXT("off"),
			bStatFpsOn ? TEXT("ON") : TEXT("off"),
			bStatUnitOn ? TEXT("ON") : TEXT("off"),
			bStatUnitGraphOn ? TEXT("ON") : TEXT("off"))));
	}

	if (FpsCornerText && bShowFpsOverlay)
	{
		FLinearColor C = APBDebugUI::Good;
		if (SmoothedFps < 30.f) C = FLinearColor(1.f, 0.25f, 0.2f, 1.f);
		else if (SmoothedFps < 55.f) C = APBDebugUI::Accent;
		FpsCornerText->SetColorAndOpacity(FSlateColor(C));
		FpsCornerText->SetText(FText::FromString(FString::Printf(
			TEXT("%.0f FPS | %s | F8 debug"),
			SmoothedFps, *Stage)));
	}
}

void UAPBDebugMenuWidget::BuildLayout()
{
	if (!WidgetTree) return;

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DebugRoot"));
	WidgetTree->RootWidget = RootCanvas;

	// Corner FPS (always available)
	FpsCornerText = MakeLbl(TEXT("FpsCorner"), TEXT("-- FPS"), 14, APBDebugUI::Good);
	FpsCornerText->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* CS = RootCanvas->AddChildToCanvas(FpsCornerText))
	{
		CS->SetAnchors(FAnchors(0.f, 0.f));
		CS->SetAlignment(FVector2D(0.f, 0.f));
		CS->SetOffsets(FMargin(12.f, 10.f, 0.f, 0.f));
		CS->SetAutoSize(true);
		CS->SetZOrder(50);
	}

	// Main panel (top-right)
	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DebugPanelSize"));
	PanelSize->SetWidthOverride(420.f);
	PanelSize->SetMaxDesiredHeight(640.f);
	if (UCanvasPanelSlot* CS = RootCanvas->AddChildToCanvas(PanelSize))
	{
		CS->SetAnchors(FAnchors(1.f, 0.f));
		CS->SetAlignment(FVector2D(1.f, 0.f));
		CS->SetOffsets(FMargin(0.f, 36.f, 12.f, 0.f));
		CS->SetAutoSize(true);
		CS->SetZOrder(100);
	}

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DebugPanelBorder"));
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(APBDebugUI::PanelBg);
		PanelBorder->SetBrush(Brush);
		PanelBorder->SetBrushColor(APBDebugUI::PanelBg);
		PanelBorder->SetPadding(FMargin(10.f, 8.f));
	}
	PanelSize->AddChild(PanelBorder);

	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DebugOuter"));
	PanelBorder->AddChild(Outer);

	auto AddOuter = [&](UWidget* W, float Top)
	{
		if (UVerticalBoxSlot* S = Outer->AddChildToVerticalBox(W))
		{
			S->SetPadding(FMargin(2.f, Top, 2.f, 2.f));
			S->SetHorizontalAlignment(HAlign_Fill);
		}
	};

	TitleText = MakeLbl(TEXT("DebugTitle"), TEXT("APB DEBUG  [F8]"), 16, APBDebugUI::TitleCol);
	AddOuter(TitleText, 0.f);
	HintText = MakeLbl(TEXT("DebugHint"), TEXT("Stages / FPS / stats — does not ship to players"), 10, APBDebugUI::Muted);
	AddOuter(HintText, 0.f);
	StatusText = MakeLbl(TEXT("DebugStatus"), TEXT("..."), 11, APBDebugUI::BodyCol);
	AddOuter(StatusText, 4.f);

	BodyScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DebugScroll"));
	BodyScroll->SetOrientation(Orient_Vertical);
	if (UVerticalBoxSlot* S = Outer->AddChildToVerticalBox(BodyScroll))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetPadding(FMargin(0.f, 4.f));
	}
	BodyBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DebugBody"));
	BodyScroll->AddChild(BodyBox);

	auto AddSection = [&](const FString& Title)
	{
		AddRow(MakeLbl(TEXT("Sec_") + Title, Title, 12, APBDebugUI::Accent), 8.f);
	};

	auto AddHRow = [&](TArray<UButton*> Btns)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (UButton* B : Btns)
		{
			if (!B) continue;
			if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(B))
			{
				HS->SetPadding(FMargin(2.f));
				HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		AddRow(Row, 2.f);
	};

	// --- Stages ---
	AddSection(TEXT("FRONTEND STAGES"));
	{
		UButton* B0 = MakeBtn(TEXT("StSplash"), TEXT("Splash"));
		UButton* B1 = MakeBtn(TEXT("StLogin"), TEXT("Login"));
		UButton* B2 = MakeBtn(TEXT("StCS"), TEXT("Char Select"));
		B0->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnStageSplash);
		B1->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnStageLogin);
		B2->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnStageCharSelect);
		AddHRow({ B0, B1, B2 });
	}
	{
		UButton* B0 = MakeBtn(TEXT("StCC"), TEXT("Char Create"));
		UButton* B1 = MakeBtn(TEXT("StDist"), TEXT("District"));
		UButton* B2 = MakeBtn(TEXT("StLoad"), TEXT("Loading"));
		B0->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnStageCharCreate);
		B1->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnStageDistrict);
		B2->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnStageLoading);
		AddHRow({ B0, B1, B2 });
	}
	{
		UButton* B0 = MakeBtn(TEXT("StSet"), TEXT("Settings"));
		B0->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnStageSettings);
		AddHRow({ B0 });
	}

	// --- FPS cap ---
	AddSection(TEXT("FPS LIMIT (t.MaxFPS)"));
	{
		UButton* B0 = MakeBtn(TEXT("FUncap"), TEXT("Uncap"));
		UButton* B1 = MakeBtn(TEXT("F30"), TEXT("30"));
		UButton* B2 = MakeBtn(TEXT("F60"), TEXT("60"));
		B0->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnFpsUncap);
		B1->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnFps30);
		B2->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnFps60);
		AddHRow({ B0, B1, B2 });
	}
	{
		UButton* B0 = MakeBtn(TEXT("F120"), TEXT("120"));
		UButton* B1 = MakeBtn(TEXT("F144"), TEXT("144"));
		B0->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnFps120);
		B1->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnFps144);
		AddHRow({ B0, B1 });
	}

	// --- Stats / overlay ---
	AddSection(TEXT("DISPLAY / STATS"));
	{
		UButton* B0 = MakeBtn(TEXT("TogFpsOv"), TEXT("Toggle FPS corner"));
		UButton* B1 = MakeBtn(TEXT("TogStatFps"), TEXT("stat fps"));
		B0->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnToggleFpsOverlay);
		B1->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnToggleStatFps);
		AddHRow({ B0, B1 });
	}
	{
		UButton* B0 = MakeBtn(TEXT("TogUnit"), TEXT("stat unit"));
		UButton* B1 = MakeBtn(TEXT("TogUG"), TEXT("stat unitgraph"));
		B0->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnToggleStatUnit);
		B1->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnToggleStatUnitGraph);
		AddHRow({ B0, B1 });
	}

	// --- Menu audio (preview future settings slider) ---
	AddSection(TEXT("MENU AUDIO (preview slider)"));
	{
		UButton* B0 = MakeBtn(TEXT("V0"), TEXT("Mute"));
		UButton* B1 = MakeBtn(TEXT("V50"), TEXT("50%"));
		UButton* B2 = MakeBtn(TEXT("V100"), TEXT("100%"));
		B0->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnVolMute);
		B1->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnVolHalf);
		B2->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnVolFull);
		AddHRow({ B0, B1, B2 });
	}

	AddSection(TEXT("MENU"));
	{
		UButton* CloseB = MakeBtn(TEXT("CloseDbg"), TEXT("  Close (F8)  "));
		CloseB->OnClicked.AddDynamic(this, &UAPBDebugMenuWidget::OnCloseMenu);
		AddRow(CloseB, 4.f);
	}

	// Sync FPS cap from console if already set
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		CurrentFpsCap = CVar->GetFloat();
	}

	RefreshReadouts();
	UE_LOG(LogTemp, Warning, TEXT("APBDebug BuildLayout ok"));
}
