#include "APBSocialWidget.h"
#include "APBPlayerState.h"
#include "APBGameInstanceSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/SizeBox.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UAPBSocialWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Build the full widget tree in C++ (mirrors APBFreeroamHUDWidget approach).
	auto MakeText = [&](const FString& Text) -> UTextBlock*
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Text));
		return T;
	};

	auto MakeButton = [&](const FString& Label, TObjectPtr<UButton>& OutBtn) -> UButton*
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		OutBtn = B;
		UTextBlock* LabelText = MakeText(Label);
		LabelText->Slot = B->AddChild(LabelText);
		return B;
	};

	auto MakeInput = [&](TObjectPtr<UEditableTextBox>& OutInput) -> UEditableTextBox*
	{
		UEditableTextBox* E = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		OutInput = E;
		return E;
	};

	// Root: a dark border panel
	UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RootBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 0.92f));
	RootBorder->SetPadding(FMargin(8.f));
	WidgetTree->RootWidget = RootBorder;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	RootBorder->SetContent(Canvas);

	// Title bar with tab buttons + close
	UHorizontalBox* TabBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Canvas->AddChild(TabBar);
	MakeButton(TEXT("Clan"), TabClanBtn);
	MakeButton(TEXT("Friends"), TabFriendsBtn);
	MakeButton(TEXT("Group"), TabGroupBtn);
	MakeButton(TEXT("Mail"), TabMailBtn);
	MakeButton(TEXT("X"), CloseBtn);
	TabBar->AddChild(TabClanBtn);
	TabBar->AddChild(TabFriendsBtn);
	TabBar->AddChild(TabGroupBtn);
	TabBar->AddChild(TabMailBtn);
	TabBar->AddChild(CloseBtn);

	// Tab switcher with 4 panels
	TabSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass());
	Canvas->AddChild(TabSwitcher);

	// ── Clan Panel ──
	{
		ClanPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TabSwitcher->AddChild(ClanPanel);
		ClanInfoText = MakeText(TEXT("Not in a clan"));
		ClanPanel->AddChild(ClanInfoText);
		UHorizontalBox* ClanRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		ClanPanel->AddChild(ClanRow);
		MakeInput(ClanNameInput);
		ClanNameInput->SetHintText(FText::FromString(TEXT("Clan Name")));
		MakeButton(TEXT("Create Clan"), ClanCreateBtn);
		ClanRow->AddChild(ClanNameInput);
		ClanRow->AddChild(ClanCreateBtn);
	}

	// ── Friends Panel ──
	{
		FriendsPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TabSwitcher->AddChild(FriendsPanel);
		FriendsListText = MakeText(TEXT("No friends"));
		FriendsPanel->AddChild(FriendsListText);
		UHorizontalBox* FriendRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		FriendsPanel->AddChild(FriendRow);
		MakeInput(FriendNameInput);
		FriendNameInput->SetHintText(FText::FromString(TEXT("Player Name")));
		MakeButton(TEXT("Add Friend"), FriendRequestBtn);
		FriendRow->AddChild(FriendNameInput);
		FriendRow->AddChild(FriendRequestBtn);
	}

	// ── Group Panel ──
	{
		GroupPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TabSwitcher->AddChild(GroupPanel);
		GroupInfoText = MakeText(TEXT("Not in a group"));
		GroupPanel->AddChild(GroupInfoText);
		MakeButton(TEXT("Create Group"), GroupCreateBtn);
		GroupPanel->AddChild(GroupCreateBtn);
	}

	// ── Mail Panel ──
	{
		MailPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TabSwitcher->AddChild(MailPanel);
		MailInboxText = MakeText(TEXT("No mail"));
		MailPanel->AddChild(MailInboxText);
		MakeInput(MailToInput);
		MailToInput->SetHintText(FText::FromString(TEXT("To")));
		MakeInput(MailSubjectInput);
		MailSubjectInput->SetHintText(FText::FromString(TEXT("Subject")));
		MakeInput(MailBodyInput);
		MailBodyInput->SetHintText(FText::FromString(TEXT("Body")));
		MakeButton(TEXT("Send Mail"), MailSendBtn);
		MailPanel->AddChild(MailToInput);
		MailPanel->AddChild(MailSubjectInput);
		MailPanel->AddChild(MailBodyInput);
		MailPanel->AddChild(MailSendBtn);
	}

	// Bind button events
	if (TabClanBtn) TabClanBtn->OnClicked.AddDynamic(this, &UAPBSocialWidget::OnTabClan);
	if (TabFriendsBtn) TabFriendsBtn->OnClicked.AddDynamic(this, &UAPBSocialWidget::OnTabFriends);
	if (TabGroupBtn) TabGroupBtn->OnClicked.AddDynamic(this, &UAPBSocialWidget::OnTabGroup);
	if (TabMailBtn) TabMailBtn->OnClicked.AddDynamic(this, &UAPBSocialWidget::OnTabMail);
	if (ClanCreateBtn) ClanCreateBtn->OnClicked.AddDynamic(this, &UAPBSocialWidget::OnClanCreate);
	if (FriendRequestBtn) FriendRequestBtn->OnClicked.AddDynamic(this, &UAPBSocialWidget::OnFriendRequest);
	if (GroupCreateBtn) GroupCreateBtn->OnClicked.AddDynamic(this, &UAPBSocialWidget::OnGroupCreate);
	if (MailSendBtn) MailSendBtn->OnClicked.AddDynamic(this, &UAPBSocialWidget::OnMailSend);
	if (CloseBtn) CloseBtn->OnClicked.AddDynamic(this, &UAPBSocialWidget::OnClose);

	// Default to Clan tab
	SwitchTab(ESocialTab::Clan);

	// Start collapsed
	SetVisibility(ESlateVisibility::Collapsed);

	RefreshAll();
}

void UAPBSocialWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccum += InDeltaTime;
	if (RefreshAccum >= 1.0f)
	{
		RefreshAccum = 0.f;
		RefreshAll();
	}
}

AAPBPlayerState* UAPBSocialWidget::GetOwnerPlayerState() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		return PC->GetPlayerState<AAPBPlayerState>();
	}
	return nullptr;
}

void UAPBSocialWidget::ToggleVisibility()
{
	if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
	{
		SetVisibility(ESlateVisibility::Visible);
		RefreshAll();
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAPBSocialWidget::SwitchTab(ESocialTab Tab)
{
	if (!TabSwitcher) return;
	TabSwitcher->SetActiveWidgetIndex(static_cast<int32>(Tab));
}

void UAPBSocialWidget::OnTabClan() { SwitchTab(ESocialTab::Clan); }
void UAPBSocialWidget::OnTabFriends() { SwitchTab(ESocialTab::Friends); }
void UAPBSocialWidget::OnTabGroup() { SwitchTab(ESocialTab::Group); }
void UAPBSocialWidget::OnTabMail() { SwitchTab(ESocialTab::Mail); }
void UAPBSocialWidget::OnClose() { SetVisibility(ESlateVisibility::Collapsed); }

void UAPBSocialWidget::RefreshAll()
{
	RefreshClan();
	RefreshFriends();
	RefreshGroup();
	RefreshMail();
}

void UAPBSocialWidget::RefreshClan()
{
	if (!ClanInfoText) return;
	AAPBPlayerState* PS = GetOwnerPlayerState();
	if (!PS) { ClanInfoText->SetText(FText::FromString(TEXT("No player state"))); return; }
	if (PS->ClanId.IsEmpty())
	{
		ClanInfoText->SetText(FText::FromString(TEXT("Not in a clan")));
	}
	else
	{
		ClanInfoText->SetText(FText::FromString(FString::Printf(TEXT("Clan: %s | Role: %s | Pending Invite: %d"),
			*PS->ClanId, *PS->ClanRole, PS->bHasPendingClanInvite ? 1 : 0)));
	}
}

void UAPBSocialWidget::RefreshFriends()
{
	if (!FriendsListText) return;
	AAPBPlayerState* PS = GetOwnerPlayerState();
	if (!PS) return;
	FriendsListText->SetText(FText::FromString(FString::Printf(TEXT("Online Friends: %d"),
		PS->OnlineFriendCount)));
}

void UAPBSocialWidget::RefreshGroup()
{
	if (!GroupInfoText) return;
	AAPBPlayerState* PS = GetOwnerPlayerState();
	if (!PS) return;
	if (PS->GroupId.IsEmpty())
	{
		GroupInfoText->SetText(FText::FromString(TEXT("Not in a group")));
	}
	else
	{
		GroupInfoText->SetText(FText::FromString(FString::Printf(TEXT("Group: %s | All Ready: %d | Pending Invite: %d"),
			*PS->GroupId, PS->bGroupAllReady ? 1 : 0, PS->bHasPendingGroupInvite ? 1 : 0)));
	}
}

void UAPBSocialWidget::RefreshMail()
{
	if (!MailInboxText) return;
	AAPBPlayerState* PS = GetOwnerPlayerState();
	if (!PS) return;
	MailInboxText->SetText(FText::FromString(TEXT("Mail: check inbox via social menu")));
}

void UAPBSocialWidget::OnClanCreate()
{
	AAPBPlayerState* PS = GetOwnerPlayerState();
	if (!PS || !ClanNameInput) return;
	const FString Name = ClanNameInput->GetText().ToString();
	if (Name.IsEmpty()) return;
	PS->Server_SocialClan(TEXT("create"), Name, TEXT(""));
}

void UAPBSocialWidget::OnFriendRequest()
{
	AAPBPlayerState* PS = GetOwnerPlayerState();
	if (!PS || !FriendNameInput) return;
	const FString Target = FriendNameInput->GetText().ToString();
	if (Target.IsEmpty()) return;
	PS->Server_SocialFriend(TEXT("request"), Target);
}

void UAPBSocialWidget::OnGroupCreate()
{
	AAPBPlayerState* PS = GetOwnerPlayerState();
	if (!PS) return;
	PS->Server_SocialGroup(TEXT("create"), TEXT(""), TEXT(""));
}

void UAPBSocialWidget::OnMailSend()
{
	AAPBPlayerState* PS = GetOwnerPlayerState();
	if (!PS || !MailToInput || !MailSubjectInput || !MailBodyInput) return;
	const FString To = MailToInput->GetText().ToString();
	const FString Subject = MailSubjectInput->GetText().ToString();
	const FString Body = MailBodyInput->GetText().ToString();
	if (To.IsEmpty() || Subject.IsEmpty()) return;
	// Encode To|Subject|Body for the relay op payload (Server_SocialMail takes a single payload string).
	PS->Server_SocialMail(TEXT("send"), To + TEXT("|") + Subject + TEXT("|") + Body);
}
