#include "APBFreeroamHUDWidget.h"
#include "APBGameInstanceSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"

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
	Line1 = Mk(TEXT("L1"));
	Line2 = Mk(TEXT("L2"));
	Line3 = Mk(TEXT("L3"));
	Hint = Mk(TEXT("Hint"));
	Hint->SetText(FText::FromString(TEXT("WASD | Mouse | LMB Fire | E Vehicle | F Interact (mailbox/ammo/resupply)")));
}

void UAPBFreeroamHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
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
	Line3->SetText(FText::FromString(FString::Printf(
		TEXT("Mission: %s  stage %d/%d  [%s]"),
		*S.MissionTitle, S.MissionStageIndex, S.MissionStageCount, *S.MissionStatus)));
}
