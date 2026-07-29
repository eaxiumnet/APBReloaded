#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "APBSocialWidget.generated.h"

class UTextBlock;
class UButton;
class UVerticalBox;
class UWidgetSwitcher;
class UEditableTextBox;
class UScrollBox;
class AAPBPlayerState;

UENUM(BlueprintType)
enum class ESocialTab : uint8
{
	Clan UMETA(DisplayName="Clan"),
	Friends UMETA(DisplayName="Friends"),
	Group UMETA(DisplayName="Group"),
	Mail UMETA(DisplayName="Mail")
};

/** M14 social panel — 4-tab UMG widget built in C++ (no Blueprint asset).
 *  Reads from replicated PlayerState fields + UGI bridge queries.
 *  Action buttons dispatch through Server_Social* RPCs. */
UCLASS()
class APBRELOADED_API UAPBSocialWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Toggle visibility (bound to a key by the HUD). */
	void ToggleVisibility();

protected:
	// Tab switcher + content panels built in NativeConstruct via WidgetTree.
	UPROPERTY() TObjectPtr<UWidgetSwitcher> TabSwitcher = nullptr;
	UPROPERTY() TObjectPtr<UVerticalBox> ClanPanel = nullptr;
	UPROPERTY() TObjectPtr<UVerticalBox> FriendsPanel = nullptr;
	UPROPERTY() TObjectPtr<UVerticalBox> GroupPanel = nullptr;
	UPROPERTY() TObjectPtr<UVerticalBox> MailPanel = nullptr;

	UPROPERTY() TObjectPtr<UTextBlock> ClanInfoText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> FriendsListText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> GroupInfoText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> MailInboxText = nullptr;

	UPROPERTY() TObjectPtr<UEditableTextBox> ClanNameInput = nullptr;
	UPROPERTY() TObjectPtr<UEditableTextBox> FriendNameInput = nullptr;
	UPROPERTY() TObjectPtr<UEditableTextBox> MailToInput = nullptr;
	UPROPERTY() TObjectPtr<UEditableTextBox> MailSubjectInput = nullptr;
	UPROPERTY() TObjectPtr<UEditableTextBox> MailBodyInput = nullptr;

	UPROPERTY() TObjectPtr<UButton> TabClanBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> TabFriendsBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> TabGroupBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> TabMailBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> ClanCreateBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> FriendRequestBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> GroupCreateBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> MailSendBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> CloseBtn = nullptr;

	UFUNCTION() void OnTabClan();
	UFUNCTION() void OnTabFriends();
	UFUNCTION() void OnTabGroup();
	UFUNCTION() void OnTabMail();
	UFUNCTION() void OnClanCreate();
	UFUNCTION() void OnFriendRequest();
	UFUNCTION() void OnGroupCreate();
	UFUNCTION() void OnMailSend();
	UFUNCTION() void OnClose();

	void RefreshAll();
	void RefreshClan();
	void RefreshFriends();
	void RefreshGroup();
	void RefreshMail();

	AAPBPlayerState* GetOwnerPlayerState() const;
	void SwitchTab(ESocialTab Tab);

	float RefreshAccum = 0.f;
};
