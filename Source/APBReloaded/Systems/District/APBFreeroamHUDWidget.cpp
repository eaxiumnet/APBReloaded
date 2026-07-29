#include "APBFreeroamHUDWidget.h"
#include "APBGameInstanceSubsystem.h"
#include "APBPlayerState.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"

void UAPBFreeroamHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HudRoot"));
	Root->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
	WidgetTree->RootWidget = Root;
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HudV"));
	Root->AddChild(V);
	auto Mk = [&](const FName& N) {
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), N);
		FSlateFontInfo F = T->GetFont(); F.Size = 14; T->SetFont(F);
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		if (UVerticalBoxSlot* S = V->AddChildToVerticalBox(T)) S->SetPadding(FMargin(12, 4));
		return T;
	};
	auto MkBar = [&](const FName& N, const FLinearColor& Fill) {
		UProgressBar* B = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), N);
		B->SetFillColorAndOpacity(Fill);
		B->SetPercent(0.f);
		if (UVerticalBoxSlot* S = V->AddChildToVerticalBox(B)) S->SetPadding(FMargin(12, 2));
		return B;
	};
	Line1 = Mk(TEXT("L1"));
	Line2 = Mk(TEXT("L2"));
	MissionLine = Mk(TEXT("MissionLine"));
	StageBar = MkBar(TEXT("StageBar"), FLinearColor(0.10f, 0.55f, 1.0f, 1.f));
	OppBar = MkBar(TEXT("OppBar"), FLinearColor(1.0f, 0.30f, 0.15f, 1.f));
	Hint = Mk(TEXT("Hint"));
	Hint->SetText(FText::FromString(TEXT("WASD | Mouse | LMB Fire | E Vehicle | F Interact (mailbox/ammo/resupply)")));

	BindOwningMissionState();
	RefreshMissionDisplay();
}

void UAPBFreeroamHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BoundPlayerState)
	{
		BindOwningMissionState();
		RefreshMissionDisplay();
	}
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB || !Line1) return;
	const FAPBDomainSnapshotUE S = APB->CaptureDomainSnapshot();
	Line1->SetText(FText::FromString(FString::Printf(
		TEXT("%s | %s | Cash %lld | G1C %lld"),
		*S.CharacterName, S.bEnforcer ? TEXT("Enforcer") : TEXT("Criminal"), S.Cash, S.G1C)));
	Line2->SetText(FText::FromString(FString::Printf(
		TEXT("Threat %.0f (bots %d) | Inv %d | District %s"),
		S.ThreatPoints, S.ThreatBots, S.InventorySlotCount, *S.DistrictId)));

	// Countdown must use GetServerWorldTimeSeconds() - same epoch the ticker armed the deadline against.
	if (StageBar && BoundPlayerState && BoundPlayerState->MissionStageCount > 0)
	{
		RefreshCountdown();
	}
}

void UAPBFreeroamHUDWidget::BindOwningMissionState()
{
	AAPBPlayerState* PS = GetOwningPlayerState<AAPBPlayerState>();
	if (!PS || PS == BoundPlayerState) return;
	if (BoundPlayerState)
	{
		BoundPlayerState->OnMissionUpdated.RemoveDynamic(this, &UAPBFreeroamHUDWidget::OnMissionStateChanged);
	}
	BoundPlayerState = PS;
	BoundPlayerState->OnMissionUpdated.AddDynamic(this, &UAPBFreeroamHUDWidget::OnMissionStateChanged);
}

void UAPBFreeroamHUDWidget::OnMissionStateChanged()
{
	RefreshMissionDisplay();
}

void UAPBFreeroamHUDWidget::RefreshMissionDisplay()
{
	AAPBPlayerState* PS = BoundPlayerState;
	const bool bActive = PS && PS->MissionStageCount > 0;

	if (StageBar) StageBar->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (OppBar) OppBar->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (!MissionLine) return;
	if (!bActive)
	{
		MissionLine->SetText(FText::FromString(TEXT("Mission: none")));
		return;
	}

	if (StageBar) StageBar->SetPercent(FMath::Clamp(PS->MissionStageProgress, 0.f, 1.f));
	if (OppBar) OppBar->SetPercent(FMath::Clamp(PS->MissionOppStageProgress, 0.f, 1.f));

	const TCHAR* Status =
		PS->bMissionTimedOut ? TEXT("TIMED OUT") :
		PS->bMissionOppositionWon ? TEXT("OPP WON") :
		PS->bMissionOppositionContesting ? TEXT("CONTESTED") : TEXT("active");

	MissionLine->SetText(FText::FromString(FString::Printf(
		TEXT("Mission: %s  stage %d/%d  [%s]"),
		*PS->MissionTitle, PS->MissionStageIndex + 1, PS->MissionStageCount, Status)));

	RefreshCountdown();
}

void UAPBFreeroamHUDWidget::RefreshCountdown()
{
	AAPBPlayerState* PS = BoundPlayerState;
	if (!PS || !MissionLine) return;
	if (PS->MissionStageDeadlineServerSec <= 0.f) return;

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS) return;
	const float Now = static_cast<float>(GS->GetServerWorldTimeSeconds());
	const float Remaining = FMath::Max(0.f, PS->MissionStageDeadlineServerSec - Now);

	const TCHAR* Status =
		PS->bMissionTimedOut ? TEXT("TIMED OUT") :
		PS->bMissionOppositionWon ? TEXT("OPP WON") :
		PS->bMissionOppositionContesting ? TEXT("CONTESTED") : TEXT("active");

	MissionLine->SetText(FText::FromString(FString::Printf(
		TEXT("Mission: %s  stage %d/%d  [%s]  %02d:%02d"),
		*PS->MissionTitle, PS->MissionStageIndex + 1, PS->MissionStageCount, Status,
		FMath::FloorToInt(Remaining) / 60, FMath::FloorToInt(Remaining) % 60)));
}
