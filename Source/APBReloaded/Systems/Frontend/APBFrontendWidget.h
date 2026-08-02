#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/EngineBaseTypes.h"
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
class UScaleBox;
class UAudioComponent;
class UImage;
class UCanvasPanelSlot;
class AAPBCharacterCreatePreviewActor;
class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;
class UMediaSoundComponent;
class USlider;
class USoundBase;
struct FKeyEvent;

UCLASS()
class APBRELOADED_API UAPBFrontendWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	/** Any key dismisses the fullscreen replay overlay (ReplayVideos > Play Intro Movie). */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

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
	UFUNCTION() void OnDeleteCharClicked();
	UFUNCTION() void OnSelectExistingChar();
	UFUNCTION() void OnCharCreateConfirm();
	UFUNCTION() void OnCharCreateBack();
	UFUNCTION() void OnEnterDistrict();
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	UFUNCTION() void OnDistrictComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnBackToLogin();
	UFUNCTION() void OnClothingSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnEnforcerCheckChanged(bool bIsChecked);
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
	// Retail character editor (basic + advanced modes)
	UFUNCTION() void OnEditorModeBasic();
	UFUNCTION() void OnEditorModeAdvanced();
	UFUNCTION() void OnCharHeightChanged(float Value);
	UFUNCTION() void OnCharBulkChanged(float Value);
	UFUNCTION() void OnBuildPresetSkinny();
	UFUNCTION() void OnBuildPresetAverage();
	UFUNCTION() void OnBuildPresetBulky();
	UFUNCTION() void OnBuildPresetMuscular();
	UFUNCTION() void OnGenderComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnSkinToneComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnFacePresetComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnHairStyleComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnHairColorComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnEyeColorComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnAgeGroupComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnMakeupChannel1();
	UFUNCTION() void OnMakeupChannel2();
	UFUNCTION() void OnMakeupChannel3();
	UFUNCTION() void OnMakeupChannel4();
	UFUNCTION() void OnMakeupComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnAddScarClicked();
	UFUNCTION() void OnRemoveScarClicked();
	UFUNCTION() void OnCamFullBody();
	UFUNCTION() void OnCamFace();
	UFUNCTION() void OnCamDolly();
	UFUNCTION() void OnAnyHover();
	UFUNCTION() void OnAccountLink();
	UFUNCTION() void OnReplayVideosLink();
	UFUNCTION() void OnReplayIntroMovie();
	UFUNCTION() void OnReplayMovieChosen();
	UFUNCTION() void OnReplayStopClicked();
	UFUNCTION() void OnReplayMovieEnded();
	UFUNCTION() void OnCreditsLink();
	UFUNCTION() void OnRegistrationSubmit();
	UFUNCTION() void OnRegistrationBack();
	UFUNCTION() void OnRememberToggled(bool bIsChecked);
	UFUNCTION() void OnLoginFieldsChanged(const FText& Text);

	void StartLoginMusic();
	void StopLoginMusic();

	/**
	 * Menu audio volume (0..1). Settings stage + domain lobby path use this.
	 * Video bed audio follows the same knob as the theme music.
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
	UPROPERTY() TObjectPtr<UButton> LoginBtn = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> LoginLabel = nullptr;
	UPROPERTY() TObjectPtr<UEditableTextBox> RegisterEmailBox = nullptr;
	UPROPERTY() TObjectPtr<UEditableTextBox> RegisterPasswordBox = nullptr;
	UPROPERTY() TObjectPtr<UEditableTextBox> RegisterConfirmBox = nullptr;
	UPROPERTY() TObjectPtr<UCheckBox> RegisterTosCheck = nullptr;
	UPROPERTY() TObjectPtr<UCheckBox> RegisterPrivacyCheck = nullptr;
	UPROPERTY() TObjectPtr<UCheckBox> RegisterCaptchaCheck = nullptr;

protected:
	void BuildLayout();
	void RebuildStageBody();
	/** Size the design canvas + drive the ScaleBox stretch mode for the active stage. */
	void SetDesignCanvasSize(float DesignW, float DesignH);
	/** Add a child to DesignCanvas at an absolute design-space rect (top-left anchored). */
	UCanvasPanelSlot* PlaceRect(UWidget* Child, float X, float Y, float W, float H, int32 ZOrder = 0);
	void ClearDesignCanvas();
	void BuildLoginDesign();
	void BuildRegistrationBody();
	void BuildCharacterSelectDesign();
	void LogStage(const FString& Extra = FString());
	void CompleteWorldTravelFailure(const FString& Reason, bool bEmitMarker = true);
	void PollWorldTravelReservation();
	void RefreshClothingCombos();
	void SelectWardrobeTab(int32 TabId);
	void RefreshWardrobeItems();
	bool ApplyAppearanceFromEditor();
	void RefreshCharacterEditorFromUI();
	bool ApplyCharacterEditorToDomain();
	void RefreshEditorCombos();
	void RefreshAdvancedPanel();
	void UpdateViewportScale();
	void ApplyPanelChrome(bool bShowForm, const FLinearColor& PanelColor);
	void SelectDistrictIndex(int32 Index);
	void RefreshFactionButtons();
	void RefreshVolumeLabel();
	void LoadMenu2011Assets();
	UTexture2D* LoadMenu2011Tex(const TCHAR* Sub, const TCHAR* Name) const;
	void ApplyDisplaySettings();
	void RefreshResolutionLabel();
	/** Design-space panel size for current stage (1920×1080 reference). */
	void GetDesignPanelSize(float& OutW, float& OutH) const;
	/** 2011 string table (Content/Data/ui_strings_2011.json) with hardcoded fallbacks. */
	void LoadUiStrings2011();
	FString S2011(const FString& SectionKey, const FString& Fallback) const;
	/** Retail character-creation string table (Content/Data/ui_strings_retail.json). */
	void LoadUiStringsRetail();
	FString SRetail(const FString& SectionKey, const FString& Fallback) const;
	void SetCharacterCreateStatus(const FString& Text);
	void SetCharacterCreateUnavailable(const FString& Text);
	/** Staged 2011 UI sounds (/Game/Audio/UI) — spec §7 swappable mapping table. */
	void LoadUiSounds();
	void PlayUiSfx(FName SfxSlot);
	void ApplyTextureToImage(UImage* Img, UTexture2D* Tex, FLinearColor Tint = FLinearColor::White);
	void ApplyTextureToBorder(UBorder* Border, UTexture2D* Tex, FLinearColor Tint);
	UImage* MakeImage(const FString& Name, UTexture2D* Tex, float H = 0.f);
	UEditableTextBox* MakeTextField(const FString& Name, const FString& Hint, bool bPassword, bool bLight = false);
	void StartStartupMovie();
	void StartReplayMovie(const FString& MoviePath = FString());
	void StopReplayMovie();
	void CollectReplayMovies(TArray<FString>& OutPaths) const;
	FString FriendlyMovieName(const FString& Path) const;
	void EnsureStartupMovieReady();
	void StartLoginBackgroundVideo();
	void StopLoginBackgroundVideo();
	void EnsureLoginMediaLoop();
	/** Open stage bed movie (login / char select / faction / generic). */
	void ApplyStageBackgroundVideo(EAPBFrontendStage Stage);
	void SyncHud();
	void EnsureCharacterPreview();
	void DestroyCharacterPreview();
	void RefreshCharacterPreviewFromUI();
	/** CharacterSelect viewer: apply the saved character's faction mesh, body, and equipped clothing to the studio actor. */
	void RefreshCharacterPreviewFromSaved();
	void BindPreviewImageToRT();
	UTextBlock* MakeLabel(const FString& Name, const FString& Text, int32 Size, FLinearColor Color);
	UButton* MakeButton(const FString& Name, const FString& Label);
	UButton* MakeAccentButton(const FString& Name, const FString& Label, FLinearColor NormalTint);
	/** Login/CharSelect-only styling; kept separate so shared MakeButton/MakeAccentButton (CharacterCreate/District/Settings) stay byte-identical. */
	UButton* MakeFlatButton(const FString& Name, const FString& Label, bool bPrimary = false, int32 FontSize = 11);
	UButton* MakeLinkButton(const FString& Name, const FString& Label);
	void AddToScroll(UWidget* W, float PadY = 6.f);
	/** Login/select never scroll. Long stages get an inner ScrollBox only. */
	void BeginStageContent(bool bAllowScroll);
	UImage* AddLayerImage(const FName& Name, int32 ZOrder);
	FString ResolveLoginBgVideoPath() const;
	FString ResolveStageBgVideoPath(EAPBFrontendStage Stage) const;
	FString FirstExistingVideo(const TArray<FString>& Candidates) const;
	/** Task-18 media gate: registry-verify a resolved movie/WAV path before open. */
	bool VerifyMediaFile(FString& InOutPath, const TCHAR* Context);

	UPROPERTY() EAPBFrontendStage CurrentStage = EAPBFrontendStage::Splash;
	UPROPERTY() EAPBFrontendStage StageBeforeSettings = EAPBFrontendStage::Login;

	UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas = nullptr;
	/** ScaleToFit wrapper reproducing UE3 UIScene uniform scale (design canvas -> viewport). */
	UPROPERTY() TObjectPtr<UScaleBox> DesignScale = nullptr;
	UPROPERTY() TObjectPtr<USizeBox> DesignSizeBox = nullptr;
	/** Fixed design-space canvas (Login 1152x720 / Lobby 800x600) holding 1:1 widget rects. */
	UPROPERTY() TObjectPtr<UCanvasPanel> DesignCanvas = nullptr;
	UPROPERTY() TObjectPtr<UBorder> FullscreenBg = nullptr;
	/** Full-bleed video bed (Login_BG). */
	UPROPERTY() TObjectPtr<UImage> BgVideo = nullptr;
	/** Deprecated placeholder layer retained as a null compatibility slot; never rendered in live frontend stages. */
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
	/** Authored APB Reborn splash image (ApbReborn2.png, staged under Menu2011/Loading). */
	UPROPERTY() TObjectPtr<UTexture2D> TexSplash = nullptr;
	/** Deprecated NewBackgroundImage slot; live Login uses the authored movie only. */
	UPROPERTY() TObjectPtr<UTexture2D> TexGraffiti = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexFactionCrim = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexFactionCrimOff = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexFactionEnf = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexFactionEnfOff = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexCharacterSelectIcon = nullptr;
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
	UPROPERTY() TObjectPtr<UTexture2D> TexDropShadow = nullptr;    // APB_DropShadow (panel drop shadow)
	UPROPERTY() TObjectPtr<UTexture2D> TexGenericContent = nullptr;// APB_BG_GenericContent_01 (inner content plate)
	UPROPERTY() TObjectPtr<UTexture2D> TexTitleAccent = nullptr;   // Window_Title_Accent_01 (header/title bar)
	UPROPERTY() TObjectPtr<UTexture2D> TexWindowBG = nullptr;      // APB_Window_BG (list/mesh window plate)
	UPROPERTY() TObjectPtr<UTexture2D> TexListCell = nullptr;       // APB_List_Cell_NoBG_20
	UPROPERTY() TObjectPtr<UTexture2D> TexListCellActive = nullptr; // APB_List_Cell_NoBG_20_Active
	UPROPERTY() TObjectPtr<UTexture2D> TexListCellPressed = nullptr;// APB_List_Cell_NoBG_20_Pressed
	UPROPERTY() TObjectPtr<UTexture2D> TexSmallListItem = nullptr;  // APB_SmallListItem_Generic_01
	UPROPERTY() TObjectPtr<UTexture2D> TexDistFinancial = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexDistSocial = nullptr;
	UPROPERTY() TObjectPtr<UTexture2D> TexDistWaterfront = nullptr;

	UPROPERTY() TObjectPtr<UBorder> FooterBar = nullptr;           // bottom grunge strip (Login only)
	UPROPERTY() TObjectPtr<UImage> SplashLogo = nullptr;           // LoadingScreen_APB (Splash only)
	UPROPERTY() TObjectPtr<UImage> SplashBg = nullptr;             // authored ApbReborn2 splash (Splash only)
	UPROPERTY() TObjectPtr<UImage> TitleChip = nullptr;            // amber key glyph in title row
	UPROPERTY() TObjectPtr<UButton> TitleCloseBtn = nullptr;       // window close → exit to desktop
	UPROPERTY() TObjectPtr<UCheckBox> RememberCheck = nullptr;
	UPROPERTY() TObjectPtr<UBorder> CapsLockPanel = nullptr;        // CapsLockWarningPanel (Login only)
	UPROPERTY() TObjectPtr<UTextBlock> CapsLockWarning = nullptr;   // CapsLockWarningText (Login only)

	/** [Section].[Key] → text, from ui_strings_2011.json. */
	TMap<FString, FString> UiStrings2011;
	bool bUiStringsLoaded = false;
	/** [Section].[Key] → text, from ui_strings_retail.json. */
	TMap<FString, FString> UiStringsRetail;
	bool bUiStringsRetailLoaded = false;
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
	// Retail character editor state
	UPROPERTY() TObjectPtr<USlider> CharHeightSlider = nullptr;
	UPROPERTY() TObjectPtr<USlider> CharBulkSlider = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> CharHeightLabel = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> CharBulkLabel = nullptr;
	UPROPERTY() TObjectPtr<UComboBoxString> GenderCombo = nullptr;
	UPROPERTY() TObjectPtr<UComboBoxString> SkinToneCombo = nullptr;
	UPROPERTY() TObjectPtr<UComboBoxString> FacePresetCombo = nullptr;
	UPROPERTY() TObjectPtr<UComboBoxString> HairStyleCombo = nullptr;
	UPROPERTY() TObjectPtr<UComboBoxString> HairColorCombo = nullptr;
	UPROPERTY() TObjectPtr<UComboBoxString> EyeColorCombo = nullptr;
	UPROPERTY() TObjectPtr<UComboBoxString> AgeGroupCombo = nullptr;
	UPROPERTY() TObjectPtr<UComboBoxString> MakeupCombo = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> MakeupChannelLabel = nullptr;
	UPROPERTY() TObjectPtr<UButton> ModeBasicBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> ModeAdvancedBtn = nullptr;
	UPROPERTY() TObjectPtr<UBorder> AdvancedPanel = nullptr;
	UPROPERTY() TObjectPtr<UButton> ScarAddBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> ScarRemoveBtn = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> ScarCountLabel = nullptr;
	bool bEditorAdvancedMode = false;
	int32 ActiveMakeupChannel = 0; // 0 lipstick 1 eyeshadow 2 eyeliner 3 blusher
	int32 EditorScarCount = 0;
	UPROPERTY() TObjectPtr<UUniformGridPanel> PaletteGrid = nullptr;
	UPROPERTY() TArray<FLinearColor> WardrobePaletteColors;
	UPROPERTY() TArray<FString> WardrobeItemIds;
	int32 ActiveWardrobeTab = 1;
	int32 SelectedColorIndex = 0;
	int32 AppearanceRandomSeed = 1;
	UPROPERTY() TObjectPtr<UTextBlock> PreviewSummary = nullptr;
	UPROPERTY() TObjectPtr<UImage> CharPreviewImage = nullptr;
	UPROPERTY() TObjectPtr<USizeBox> CharPreviewSizeBox = nullptr;
	/** CharacterSelect viewer chrome (delete confirm + drag hint). */
	UPROPERTY() TObjectPtr<UButton> CharSelectDeleteBtn = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> CharSelectDeleteLabel = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> ViewerHint = nullptr;
	bool bDeleteArmed = false;
	double DeleteArmedTime = 0.0;
	bool bPreviewDragging = false;
	FVector2D LastDragCursor = FVector2D::ZeroVector;

	UPROPERTY() TObjectPtr<UComboBoxString> DistrictCombo = nullptr;
	UPROPERTY() TArray<FString> DistrictIds;
	UPROPERTY() TArray<FString> DistrictMaps;
	UPROPERTY() TArray<FString> DistrictNames;
	UPROPERTY() TArray<FString> DistrictAbbrevs;
	UPROPERTY() FString SelectedDistrictId;
	UPROPERTY() FString SelectedDistrictMap;

	UPROPERTY() TObjectPtr<UAudioComponent> LoginMusicComp = nullptr;
	UPROPERTY() TObjectPtr<UMediaPlayer> LoginMediaPlayer = nullptr;
	UPROPERTY() TObjectPtr<UMediaTexture> LoginMediaTexture = nullptr;
	UPROPERTY() TObjectPtr<UFileMediaSource> LoginMediaSource = nullptr;
	UPROPERTY() TObjectPtr<UMediaSoundComponent> LoginMediaSoundComp = nullptr;
	UPROPERTY() TObjectPtr<UMediaPlayer> StartupMediaPlayer = nullptr;
	UPROPERTY() TObjectPtr<UMediaTexture> StartupMediaTexture = nullptr;
	/** Fullscreen top-layer replay overlay (above panel z20, under debug z60). */
	UPROPERTY() TObjectPtr<UBorder> ReplayOverlay = nullptr;
	UPROPERTY() TObjectPtr<UImage> ReplayImage = nullptr;
	UPROPERTY() TObjectPtr<UButton> ReplayStopBtn = nullptr;
	UPROPERTY() TObjectPtr<UButton> ReplayClickCatcher = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> ReplayHint = nullptr;
	UPROPERTY() TArray<TObjectPtr<UButton>> ReplayMovieButtons;
	UPROPERTY() TArray<FString> ReplayMoviePaths;
	UPROPERTY() TObjectPtr<UMediaPlayer> ReplayMediaPlayer = nullptr;
	UPROPERTY() TObjectPtr<UMediaTexture> ReplayMediaTexture = nullptr;
	UPROPERTY() TObjectPtr<UMediaSoundComponent> ReplayMediaSoundComp = nullptr;
	bool bReplayActive = false;
	double ReplayStartedTime = 0.0;
	/** Absolute path of the bed movie currently open; equal-path calls skip re-open churn. */
	FString ActiveStageVideoPath;
	bool bLoginVideoStarted = false;
	double LoginReopenTime = 0.0;
	bool bStartupMovieStarted = false;
	bool bStartupReady = false;
	float StartupReadinessTimer = 0.f;
	TArray<FString> StartupMoviePaths;
	FString StartupMoviePath;

	UPROPERTY() TObjectPtr<AAPBCharacterCreatePreviewActor> CharPreviewActor = nullptr;

	float SplashTimer = 0.f;
	bool bSplashAutoDone = false;
	bool bMusicStarted = false;
	bool bSuppressPreviewRefresh = false;
	float MenuAudioVolume = 0.55f;
	FString LastLoggedStage;
	FVector2D LastViewport = FVector2D::ZeroVector;
	FString LastScaleToken;

	/** UI scale modes for multi-aspect (4:3 / 16:9 / 16:10 / ultrawide). */
	enum class EAPBUiScaleMode : uint8 { Fit = 0, Fill, Stretch };
	EAPBUiScaleMode UiScaleMode = EAPBUiScaleMode::Fit;
	/** 0=windowed 1=fullscreen 2=borderless */
	int32 DisplayMode = 0;
	int32 PendingResX = 1920;
	int32 PendingResY = 1080;

	FTimerHandle WorldAuthPollTimer;
	float WorldAuthTimeout = 0.f;
	FString PendingTravelPreviousTicket;
	FString PendingTravelReservationId;
	bool bWorldTravelPending = false;
};
