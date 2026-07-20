#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "APBFrontendTypes.h"
#include "APBFrontendWidget.generated.h"

class UTextBlock;
class UEditableTextBox;
class UButton;
class UVerticalBox;
class UHorizontalBox;
class UUniformGridPanel;
class UScrollBox;
class UCheckBox;
class UComboBoxString;
class UBorder;
class USizeBox;
class UCanvasPanel;
class UAudioComponent;
class UImage;
class UCanvasPanelSlot;
class AAPBCharacterCreatePreviewActor;
class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;
class USlider;
class USoundBase;

UCLASS()
class APBRELOADED_API UAPBFrontendWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="APB|UI")
	void SetStage(EAPBFrontendStage Stage);

	UFUNCTION(BlueprintCallable, Category="APB|UI")
	EAPBFrontendStage GetStage() const { return CurrentStage; }

	UFUNCTION(BlueprintCallable, Category="APB|UI")
	FString GetStageToken() const;

	UFUNCTION() void OnSplashContinue();
	UFUNCTION() void OnLoginClicked();
	UFUNCTION() void OnRegisterClicked();
	UFUNCTION() void OnCreateCharOpen();
	UFUNCTION() void OnSelectExistingChar();
	UFUNCTION() void OnCharCreateConfirm();
	UFUNCTION() void OnCharCreateBack();
	UFUNCTION() void OnEnterDistrict();
	UFUNCTION() void OnDistrictComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnBackToLogin();
	UFUNCTION() void OnClothingSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnEnforcerCheckChanged(bool bIsChecked);
	UFUNCTION() void OnBodyTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void OnPreviewRefreshClicked();
	UFUNCTION() void OnOpenSettings();
	UFUNCTION() void OnSettingsBack();
	UFUNCTION() void OnExitDesktop();
	UFUNCTION() void OnFactionCriminal();
	UFUNCTION() void OnFactionEnforcer();
	UFUNCTION() void OnMenuVolumeChanged(float Value);
	UFUNCTION() void OnVolPresetMute();
	UFUNCTION() void OnVolPresetLow();
	UFUNCTION() void OnVolPresetMed();
	UFUNCTION() void OnVolPresetHigh();
	UFUNCTION() void OnResolutionApply();
	UFUNCTION() void OnResModeWindowed();
	UFUNCTION() void OnResModeFullscreen();
	UFUNCTION() void OnResModeBorderless();
	UFUNCTION() void OnAspectFit();
	UFUNCTION() void OnAspectFill();
	UFUNCTION() void OnAspectStretch();
	UFUNCTION() void OnDistrictRow0();
	UFUNCTION() void OnDistrictRow1();
	UFUNCTION() void OnDistrictRow2();
	UFUNCTION() void OnDistrictRow3();
	UFUNCTION() void OnDistrictRow4();
	UFUNCTION() void OnDistrictRow5();
	UFUNCTION() void OnDistrictRow6();
	UFUNCTION() void OnDistrictRow7();
	UFUNCTION() void OnWardrobeTab1();
	UFUNCTION() void OnWardrobeTab2();
	UFUNCTION() void OnWardrobeTab3();
	UFUNCTION() void OnWardrobeTab4();
	UFUNCTION() void OnWardrobeTab5();
	UFUNCTION() void OnWardrobeTab6();
	UFUNCTION() void OnWardrobeTab7();
	UFUNCTION() void OnWardrobeTab8();
	UFUNCTION() void OnWardrobeTab9();
	UFUNCTION() void OnWardrobeTab10();
	UFUNCTION() void OnWardrobeTab11();
	UFUNCTION() void OnWardrobeTab12();
	UFUNCTION() void OnWardrobeTab13();
	UFUNCTION() void OnWardrobeTab14();
	UFUNCTION() void OnWardrobeTab15();
	UFUNCTION() void OnPaletteSwatch0();
	UFUNCTION() void OnPaletteSwatch1();
	UFUNCTION() void OnPaletteSwatch2();
	UFUNCTION() void OnPaletteSwatch3();
	UFUNCTION() void OnPaletteSwatch4();
	UFUNCTION() void OnPaletteSwatch5();
	UFUNCTION() void OnPaletteSwatch6();
	UFUNCTION() void OnPaletteSwatch7();
	UFUNCTION() void OnPaletteSwatch8();
	UFUNCTION() void OnPaletteSwatch9();
	UFUNCTION() void OnPaletteSwatch10();
	UFUNCTION() void OnPaletteSwatch11();
	UFUNCTION() void OnPaletteSwatch12();
	UFUNCTION() void OnPaletteSwatch13();
	UFUNCTION() void OnPaletteSwatch14();
	UFUNCTION() void OnPaletteSwatch15();
	UFUNCTION() void OnPaletteSwatch16();
	UFUNCTION() void OnPaletteSwatch17();
	UFUNCTION() void OnPaletteSwatch18();
	UFUNCTION() void OnPaletteSwatch19();
	UFUNCTION() void OnPaletteSwatch20();
	UFUNCTION() void OnPaletteSwatch21();
	UFUNCTION() void OnPaletteSwatch22();
	UFUNCTION() void OnPaletteSwatch23();
	UFUNCTION() void OnRandomizeAppearance();
	UFUNCTION() void OnAddSymbol();
	UFUNCTION() void OnAnyHover();
	UFUNCTION() void OnAccountLink();
	UFUNCTION() void OnReplayVideosLink();
	UFUNCTION() void OnRememberToggled(bool bIsChecked);

	void StartLoginMusic();
	void StopLoginMusic();

	/**
	 * Menu audio volume (0..1). Settings stage + domain lobby path use this.
	 * Video bed stays silent so one control owns "Menu" audio.
	 */
	UFUNCTION(BlueprintCallable, Category = "APB|UI|Settings")
	void SetMenuAudioVolume(float Volume01);

	UFUNCTION(BlueprintCallable, Category = "APB|UI|Settings")
	float GetMenuAudioVolume() const { return MenuAudioVolume; }

	/** Probe / tests: set credential fields before OnLoginClicked. */
	void SetLoginCredentials(const FString& User, const FString& Pass);

	// Login form (public for session probe login_ok / login_fail path)
	UPROPERTY() TObjectPtr<UEditableTextBox> UserBox = nullptr;
	UPROPERTY() TObjectPtr<UEditableTextBox> PassBox = nullptr;

protected:
	void BuildLayout();
	void RebuildStageBody();
	void LogStage(const FString& Extra = FString());
	void RefreshClothingCombos();
	void SelectWardrobeTab(int32 TabId);
	void RefreshWardrobeItems();
	void ApplyAppearanceFromEditor();
	void UpdateViewportScale();
	void ApplyPanelChrome(bool bShowForm, const FLinearColor& PanelColor);
	void SelectDistrictIndex(int32 Index);
	void RefreshFactionButtons();
	void RefreshVolumeLabel();
	void LoadMenu2011Assets();
	void ApplyDisplaySettings();
	void RefreshResolutionLabel();
	/** Design-space panel size for current stage (1920×1080 reference). */
	void GetDesignPanelSize(float& OutW, float& OutH) const;
	/** 2011 string table (Content/Data/ui_strings_2011.json) with hardcoded fallbacks. */
	void LoadUiStrings2011();
	FString S2011(const FString& SectionKey, const FString& Fallback) const;
	/** Staged 2011 UI sounds (/Game/Audio/UI) — spec §7 swappable mapping table. */
	void LoadUiSounds();
	void PlayUiSfx(FName SfxSlot);
	void ApplyTextureToImage(UImage* Img, UTexture2D* Tex, FLinearColor Tint = FLinearColor::White);
	void ApplyTextureToBorder(UBorder* Border, UTexture2D* Tex, FLinearColor Tint);
	UImage* MakeImage(const FString& Name, UTexture2D* Tex, float H = 0.f);
	UEditableTextBox* MakeTextField(const FString& Name, const FString& Hint, bool bPassword);
	void StartLoginBackgroundVideo();
	void StopLoginBackgroundVideo();
	void EnsureLoginMediaLoop();
	/** Open stage bed movie (login / char select / faction / generic). */
	void ApplyStageBackgroundVideo(EAPBFrontendStage Stage);
	void SyncHud();
	void EnsureCharacterPreview();
	void DestroyCharacterPreview();
	void RefreshCharacterPreviewFromUI();
	void BindPreviewImageToRT();
	UTextBlock* MakeLabel(const FString& Name, const FString& Text, int32 Size, FLinearColor Color);
	UButton* MakeButton(const FString& Name, const FString& Label);
	UButton* MakeAccentButton(const FString& Name, const FString& Label, FLinearColor NormalTint);
	void AddToScroll(UWidget* W, float PadY = 6.f);
	/** Login/select never scroll. Long stages get an inner ScrollBox only. */
	void BeginStageContent(bool bAllowScroll);
	UImage* AddLayerImage(const FName& Name, int32 ZOrder);
	FString ResolveLoginBgVideoPath() const;
	FString ResolveStageBgVideoPath(EAPBFrontendStage Stage) const;
	FString FirstExistingVideo(const TArray<FString>& Candidates) const;

	UPROPERTY() EAPBFrontendStage CurrentStage = EAPBFrontendStage::Splash;
	UPROPERTY() EAPBFrontendStage StageBeforeSettings = EAPBFrontendStage::Login;

	UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas = nullptr;
	UPROPERTY() TObjectPtr<UBorder> FullscreenBg = nullptr;
	/** Full-bleed video bed (Login_BG). */
	UPROPERTY() TObjectPtr<UImage> BgVideo = nullptr;
	/** Classic light graffiti plate under/with video (NewBackgroundImage). */
	UPROPERTY() TObjectPtr<UImage> BgArt = nullptr;
	/** Full-body package avatars (login/select framing). */
	UPROPERTY() TObjectPtr<UImage> AvatarLeft = nullptr;
	UPROPERTY() TObjectPtr<UImage> AvatarRight = nullptr;
	UPROPERTY() TObjectPtr<UImage> LogoImage = nullptr;
	UPROPERTY() TObjectPtr<USizeBox> LogoSizeBox = nullptr;
	UPROPERTY() TObjectPtr<UCanvasPanelSlot> LogoSlot = nullptr;
	UPROPERTY() TObjectPtr<UCanvasPanelSlot> PanelSlot = nullptr;
	UPROPERTY() TObjectPtr<UBorder> PanelAccentBar = nullptr;
	UPROPERTY() TObjectPtr<UComboBoxString> ResolutionCombo = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> DisplayModeLabel = nullptr;
	UPROPERTY() TObjectPtr<UImage> FactionCrimeIcon = nullptr;
	UPROPERTY() TObjectPtr<UImage> FactionEnfIcon = nullptr;
	UPROPERTY() TObjectPtr<UImage> CharSelectAvatar = nullptr;

	UPROPERTY() TObjectPtr<UTexture2D> TexLogo = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexGraffiti = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexFactionCrim = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexFactionCrimOff = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexFactionEnf = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexFactionEnfOff = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexAvatarMale = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexAvatarFemale = nullptr;
	bool bLobbyChromeLoaded = false;

	// ---- M4b: staged 2011 menu chrome (/Game/Imported/UI/Menu2011, spec §2) ----
	UPROPERTY() TObjectPtr<UTexture2D> TexWindowPanel = nullptr;   // MessageBox_BG (9-slice)
	UPROPERTY() TObjectPtr<UTexture2D> TexBtnOn = nullptr;         // Menu_Button_On
	UPROPERTY() TObjectPtr<UTexture2D> TexBtnOff = nullptr;        // Menu_Button_Off
	UPROPERTY() TObjectPtr<UTexture2D> TexBtnLight = nullptr;      // Menu_Button_Light (amber select)
	UPROPERTY() TObjectPtr<UTexture2D> TexTextEntry = nullptr;     // APB_BG_TextEntry (underline field)
	UPROPERTY() TObjectPtr<UTexture2D> TexCheckTrue = nullptr;     // Check_True
	UPROPERTY() TObjectPtr<UTexture2D> TexCheckFalse = nullptr;    // Check_False
	UPROPERTY() TObjectPtr<UTexture2D> TexBrandKey = nullptr;      // JKICON_login_header_key
	UPROPERTY() TObjectPtr<UTexture2D> TexFooter = nullptr;        // frontendFooter grunge strip
	UPROPERTY() TObjectPtr<UTexture2D> TexCloseBtn = nullptr;      // JKICON_close_default
	UPROPERTY() TObjectPtr<UTexture2D> TexRing = nullptr;          // BG_Button_Active_Ring (rating badge)
	UPROPERTY() TObjectPtr<UTexture2D> TexDistFinancial = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexDistSocial = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexDistWaterfront = nullptr;

	UPROPERTY() TObjectPtr<UBorder> FooterBar = nullptr;           // bottom grunge strip (Login only)
	UPROPERTY() TObjectPtr<UImage> SplashLogo = nullptr;           // LoadingScreen_APB (Splash only)
	UPROPERTY() TObjectPtr<UImage> TitleChip = nullptr;            // amber key glyph in title row
	UPROPERTY() TObjectPtr<UButton> TitleCloseBtn = nullptr;       // window close → exit to desktop
	UPROPERTY() TObjectPtr<UCheckBox> RememberCheck = nullptr;

	/** [Section].[Key] → text, from ui_strings_2011.json. */
	TMap<FString, FString> UiStrings2011;
	bool bUiStringsLoaded = false;
	/** Spec §7 sfx slots (UI_Hover/UI_Click/...) → staged sound. */
	UPROPERTY() TMap<FName, TObjectPtr<USoundBase>> UiSfx;
	bool bUiSfxLoaded = false;
	/** First-run state: tall Login window with TOS scroll + Accept (spec §3.2 State A). */
	bool bFirstRunTOS = false;

	UPROPERTY() TObjectPtr<USizeBox> PanelSizeBox = nullptr;
	UPROPERTY() TObjectPtr<UBorder> PanelBorder = nullptr;
	UPROPERTY() TObjectPtr<UScrollBox> BodyScroll = nullptr;
	/** Fixed dialog body root (never the scroll surface itself). */
	UPROPERTY() TObjectPtr<UVerticalBox> BodyBox = nullptr;
	/** Active content target: BodyBox (fixed) or inner box inside BodyScroll. */
	UPROPERTY() TObjectPtr<UVerticalBox> ContentBox = nullptr;
	bool bStageAllowsScroll = false;
	UPROPERTY() TObjectPtr<UTextBlock> TitleText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> SubtitleText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> StatusText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> HintText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> BrandBar = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> VolumeValueText = nullptr;
	UPROPERTY() TObjectPtr<USlider> MenuVolumeSlider = nullptr;

	UPROPERTY() TObjectPtr<UEditableTextBox> CharNameBox = nullptr;
	UPROPERTY() TObjectPtr<UCheckBox> EnforcerCheck = nullptr;
	UPROPERTY() TObjectPtr<UButton> FactionCriminalBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> FactionEnforcerBtn = nullptr;
	bool bCreateAsEnforcer = false;
	UPROPERTY() TObjectPtr<UComboBoxString> WardrobeItemCombo = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> SymbolCountLabel = nullptr;
	UPROPERTY() TObjectPtr<UUniformGridPanel> PaletteGrid = nullptr;
	UPROPERTY() TArray<FLinearColor> WardrobePaletteColors;
	UPROPERTY() TArray<FString> WardrobeItemIds;
	int32 ActiveWardrobeTab = 1;
	int32 SelectedColorIndex = 0;
	int32 AppearanceRandomSeed = 1;
	UPROPERTY() TObjectPtr<UEditableTextBox> BodyHeightBox = nullptr;
	UPROPERTY() TObjectPtr<UEditableTextBox> BodyBuildBox = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> PreviewSummary = nullptr;
	UPROPERTY() TObjectPtr<UImage> CharPreviewImage = nullptr;
	UPROPERTY() TObjectPtr<USizeBox> CharPreviewSizeBox = nullptr;

	UPROPERTY() TObjectPtr<UComboBoxString> DistrictCombo = nullptr;
	UPROPERTY() TArray<FString> DistrictIds;
	UPROPERTY() TArray<FString> DistrictMaps;
	UPROPERTY() TArray<FString> DistrictNames;
	UPROPERTY() FString SelectedDistrictId;
	UPROPERTY() FString SelectedDistrictMap;

	UPROPERTY() TObjectPtr<UAudioComponent> LoginMusicComp = nullptr;
	UPROPERTY() TObjectPtr<UMediaPlayer> LoginMediaPlayer = nullptr;
	UPROPERTY() TObjectPtr<UMediaTexture> LoginMediaTexture = nullptr;
	UPROPERTY() TObjectPtr<UFileMediaSource> LoginMediaSource = nullptr;
	bool bLoginVideoStarted = false;

	UPROPERTY() TObjectPtr<AAPBCharacterCreatePreviewActor> CharPreviewActor = nullptr;

	float SplashTimer = 0.f;
	bool bSplashAutoDone = false;
	bool bMusicStarted = false;
	bool bSuppressPreviewRefresh = false;
	float MenuAudioVolume = 0.55f;
	FString LastLoggedStage;
	FVector2D LastViewport = FVector2D::ZeroVector;

	/** UI scale modes for multi-aspect (4:3 / 16:9 / 16:10 / ultrawide). */
	enum class EAPBUiScaleMode : uint8 { Fit = 0, Fill, Stretch };
	EAPBUiScaleMode UiScaleMode = EAPBUiScaleMode::Fit;
	/** 0=windowed 1=fullscreen 2=borderless */
	int32 DisplayMode = 0;
	int32 PendingResX = 1920;
	int32 PendingResY = 1080;
};
