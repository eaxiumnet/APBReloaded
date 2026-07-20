#include "APBFrontendWidget.h"
#include "APBFrontendLayoutMath.h"
#include "APBGameInstanceSubsystem.h"
#include "APBFrontendHUD.h"
#include "APBCharacterCreatePreviewActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/BorderSlot.h"
#include "Components/ScrollBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/AudioComponent.h"
#include "Components/Image.h"
#include "Components/Slider.h"
#include "Blueprint/WidgetTree.h"
#include "HAL/PlatformMisc.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "HAL/PlatformFilemanager.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// 2011 RTW GameFlow palette (menu2011_spec §2.3): monochrome greys + single amber accent.
// #FFC254 is the Menu_Button_Light selection amber — replaces the old cyan identity.
static const FLinearColor APB_PAPER(0.f, 0.f, 0.f, 1.f);
static const FLinearColor APB_BG(0.f, 0.f, 0.f, 1.f);
static const FLinearColor APB_PANEL(0.31f, 0.31f, 0.31f, 0.96f);      // #4F4F4F MessageBox_BG
static const FLinearColor APB_PANEL_EDGE(1.f, 0.761f, 0.329f, 1.f);   // #FFC254 amber frame
static const FLinearColor APB_FIELD(0.08f, 0.09f, 0.10f, 1.f);
static const FLinearColor APB_FIELD_FOCUS(0.10f, 0.10f, 0.11f, 1.f);
static const FLinearColor APB_AMBER(1.f, 0.761f, 0.329f, 1.f);        // #FFC254
static const FLinearColor APB_AMBER_HI(1.f, 0.984f, 0.612f, 1.f);     // #FFFB9C
static const FLinearColor APB_WHITE(0.96f, 0.97f, 0.98f, 1.f);
static const FLinearColor APB_MUTED(0.62f, 0.66f, 0.70f, 1.f);
static const FLinearColor APB_BTN(0.16f, 0.17f, 0.19f, 1.f);
static const FLinearColor APB_BTN_HOVER(0.34f, 0.28f, 0.16f, 1.f);
static const FLinearColor APB_BTN_OK(0.42f, 0.30f, 0.10f, 1.f);       // amber-metal primary
static const FLinearColor APB_BTN_DANGER(0.28f, 0.14f, 0.14f, 1.f);
static const FLinearColor APB_CRIM(0.72f, 0.12f, 0.14f, 1.f);
static const FLinearColor APB_ENF(0.12f, 0.32f, 0.62f, 1.f);

static void APB_MakeBoxBrush(FSlateBrush& Brush, const FLinearColor& Tint)
{
	Brush = FSlateBrush();
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = FSlateColor(Tint);
	// Thin edge via margin (solid plate look)
	Brush.Margin = FMargin(0.f);
}

/** Flat image brush from a staged 2011 texture (shape lives in alpha; tint with care). */
static FSlateBrush APB_TexBrush(UTexture2D* Tex, const FLinearColor& Tint)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(Tex);
	Brush.TintColor = FSlateColor(Tint);
	return Brush;
}

/** 9-slice brush from a staged 2011 panel texture (MessageBox_BG margin ≈ 26/512). */
static FSlateBrush APB_PanelBrush(UTexture2D* Tex, const FLinearColor& Tint)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.SetResourceObject(Tex);
	Brush.Margin = FMargin(0.05f);
	Brush.TintColor = FSlateColor(Tint);
	return Brush;
}

UTextBlock* UAPBFrontendWidget::MakeLabel(const FString& Name, const FString& Text, int32 Size, FLinearColor Color)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *Name);
	T->SetText(FText::FromString(Text));
	// Regular (not Bold-everywhere) reads closer to 2011 UI fonts
	T->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetAutoWrapText(true);
	return T;
}

UButton* UAPBFrontendWidget::MakeButton(const FString& Name, const FString& Label)
{
	return MakeAccentButton(Name, Label, APB_BTN);
}

UButton* UAPBFrontendWidget::MakeAccentButton(const FString& Name, const FString& Label, FLinearColor NormalTint)
{
	// 2011 menu buttons: staged Menu_Button_Off/On/Light plates; amber box fallback while unstaged
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);
	FButtonStyle Style = B->GetStyle();
	const bool bHasPlates = (TexBtnOff != nullptr);
	if (bHasPlates)
	{
		Style.Normal = APB_TexBrush(TexBtnOff, FLinearColor::White);
		Style.Hovered = APB_TexBrush(TexBtnOn ? TexBtnOn : TexBtnOff, FLinearColor::White);
		Style.Pressed = APB_TexBrush(TexBtnLight ? TexBtnLight : TexBtnOff, FLinearColor::White);
		Style.Disabled = APB_TexBrush(TexBtnOff, FLinearColor::White.CopyWithNewOpacity(0.5f));
	}
	else
	{
		const FLinearColor Hover = FLinearColor(
			FMath::Min(1.f, NormalTint.R + 0.12f),
			FMath::Min(1.f, NormalTint.G + 0.22f),
			FMath::Min(1.f, NormalTint.B + 0.28f),
			1.f);
		const FLinearColor Press = NormalTint * FLinearColor(0.55f, 0.55f, 0.55f, 1.f);
		APB_MakeBoxBrush(Style.Normal, NormalTint);
		APB_MakeBoxBrush(Style.Hovered, Hover);
		APB_MakeBoxBrush(Style.Pressed, Press);
		APB_MakeBoxBrush(Style.Disabled, NormalTint * 0.5f);
	}
	Style.NormalPadding = FMargin(14.f, 7.f);
	Style.PressedPadding = FMargin(14.f, 7.f);
	B->SetStyle(Style);
	B->SetBackgroundColor(NormalTint);
	B->OnHovered.AddDynamic(this, &UAPBFrontendWidget::OnAnyHover);
	UTextBlock* L = MakeLabel(Name + TEXT("_L"), Label.ToUpper(), 12, bHasPlates ? APB_AMBER : APB_WHITE);
	L->SetJustification(ETextJustify::Center);
	B->AddChild(L);
	return B;
}

UEditableTextBox* UAPBFrontendWidget::MakeTextField(const FString& Name, const FString& Hint, bool bPassword)
{
	// 2011 login field: staged APB_BG_TextEntry underline plate; dark inset fallback
	UEditableTextBox* Box = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), *Name);
	Box->SetHintText(FText::FromString(Hint));
	Box->SetIsPassword(bPassword);
	Box->SetForegroundColor(APB_WHITE);
	FEditableTextBoxStyle Style = Box->WidgetStyle;
	if (TexTextEntry)
	{
		Style.BackgroundImageNormal = APB_TexBrush(TexTextEntry, FLinearColor::White);
		Style.BackgroundImageHovered = APB_TexBrush(TexTextEntry, APB_AMBER);
		Style.BackgroundImageFocused = APB_TexBrush(TexTextEntry, APB_AMBER_HI);
		Style.BackgroundImageReadOnly = APB_TexBrush(TexTextEntry, FLinearColor::White.CopyWithNewOpacity(0.6f));
	}
	else
	{
		APB_MakeBoxBrush(Style.BackgroundImageNormal, APB_FIELD);
		APB_MakeBoxBrush(Style.BackgroundImageHovered, APB_FIELD + FLinearColor(0.03f, 0.03f, 0.04f, 0.f));
		APB_MakeBoxBrush(Style.BackgroundImageFocused, APB_FIELD_FOCUS);
		APB_MakeBoxBrush(Style.BackgroundImageReadOnly, APB_FIELD * 0.8f);
	}
	Style.Padding = FMargin(10.f, 8.f);
	Style.ForegroundColor = FSlateColor(APB_WHITE);
	Box->WidgetStyle = Style;
	return Box;
}

/** Load a staged 2011 menu texture from /Game/Imported/UI/Menu2011/<Sub>/<Name>. */
static UTexture2D* APB_LoadMenu2011Tex(const TCHAR* Sub, const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("/Game/Imported/UI/Menu2011/%s/%s.%s"), Sub, Name, Name);
	UTexture2D* T = LoadObject<UTexture2D>(nullptr, *Path);
	if (!T) UE_LOG(LogTemp, Warning, TEXT("APBFrontend TEX miss %s"), *Path);
	return T;
}

void UAPBFrontendWidget::LoadMenu2011Assets()
{
	if (bLobbyChromeLoaded) return;
	bLobbyChromeLoaded = true;

	// Branding / splash
	TexLogo = APB_LoadMenu2011Tex(TEXT("Loading"), TEXT("LoadingScreen_APB"));
	// Full-bleed graffiti backdrop
	TexGraffiti = APB_LoadMenu2011Tex(TEXT("Login"), TEXT("NewBackgroundImage"));
	// Faction icons (kept for character create/select plates)
	TexFactionCrim = APB_LoadMenu2011Tex(TEXT("CharSelect"), TEXT("CriminalFactionicon"));
	TexFactionCrimOff = APB_LoadMenu2011Tex(TEXT("CharSelect"), TEXT("CriminalFactionicon_Unselected"));
	TexFactionEnf = APB_LoadMenu2011Tex(TEXT("CharSelect"), TEXT("EnforcerFactionicon"));
	TexFactionEnfOff = APB_LoadMenu2011Tex(TEXT("CharSelect"), TEXT("EnforcerFactionicon_Unselected"));
	// Window chrome + controls
	TexWindowPanel = APB_LoadMenu2011Tex(TEXT("Chrome"), TEXT("MessageBox_BG"));
	TexBtnOn = APB_LoadMenu2011Tex(TEXT("Chrome"), TEXT("Menu_Button_On"));
	TexBtnOff = APB_LoadMenu2011Tex(TEXT("Chrome"), TEXT("Menu_Button_Off"));
	TexBtnLight = APB_LoadMenu2011Tex(TEXT("Chrome"), TEXT("Menu_Button_Light"));
	TexTextEntry = APB_LoadMenu2011Tex(TEXT("Login"), TEXT("APB_BG_TextEntry"));
	TexCheckTrue = APB_LoadMenu2011Tex(TEXT("Login"), TEXT("Check_True"));
	TexCheckFalse = APB_LoadMenu2011Tex(TEXT("Login"), TEXT("Check_False"));
	TexBrandKey = APB_LoadMenu2011Tex(TEXT("Login"), TEXT("JKICON_login_header_key"));
	TexFooter = APB_LoadMenu2011Tex(TEXT("Login"), TEXT("frontendFooter"));
	TexCloseBtn = APB_LoadMenu2011Tex(TEXT("Login"), TEXT("JKICON_close_default"));
	TexRing = APB_LoadMenu2011Tex(TEXT("Chrome"), TEXT("BG_Button_Active_Ring"));
	// District splash photos
	TexDistFinancial = APB_LoadMenu2011Tex(TEXT("DistrictSelect"), TEXT("FinancialDistrict_MainPhoto256x195"));
	TexDistSocial = APB_LoadMenu2011Tex(TEXT("DistrictSelect"), TEXT("SocialDistrict_MainPhoto256x195"));
	TexDistWaterfront = APB_LoadMenu2011Tex(TEXT("DistrictSelect"), TEXT("WaterfrontDistrict_MainPhoto256x195"));

	// No full-screen male/female character overlays
	TexAvatarMale = nullptr;
	TexAvatarFemale = nullptr;
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend ART2011 logo=%d graffiti=%d panel=%d btn=%d/%d/%d entry=%d footer=%d ring=%d facC=%d facE=%d"),
		TexLogo ? 1 : 0, TexGraffiti ? 1 : 0, TexWindowPanel ? 1 : 0,
		TexBtnOn ? 1 : 0, TexBtnOff ? 1 : 0, TexBtnLight ? 1 : 0,
		TexTextEntry ? 1 : 0, TexFooter ? 1 : 0, TexRing ? 1 : 0,
		TexFactionCrim ? 1 : 0, TexFactionEnf ? 1 : 0);
}

void UAPBFrontendWidget::LoadUiStrings2011()
{
	if (bUiStringsLoaded) return;
	bUiStringsLoaded = true;

	const FString JsonPath = FPaths::ProjectContentDir() / TEXT("Data/ui_strings_2011.json");
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *JsonPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend STR2011 missing %s"), *JsonPath);
		return;
	}
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend STR2011 parse failed %s"), *JsonPath);
		return;
	}
	for (const auto& Sec : Root->Values)
	{
		if (Sec.Key == TEXT("_meta")) continue;
		const TSharedPtr<FJsonObject>* SecObj = nullptr;
		if (!Sec.Value.IsValid() || !Sec.Value->TryGetObject(SecObj) || !SecObj || !SecObj->IsValid()) continue;
		for (const auto& KV : (*SecObj)->Values)
		{
			FString Val;
			if (KV.Value.IsValid() && KV.Value->TryGetString(Val))
			{
				UiStrings2011.Add(FString(Sec.Key) + TEXT(".") + FString(KV.Key), Val);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("APBFrontend STR2011 loaded %d strings"), UiStrings2011.Num());
}

FString UAPBFrontendWidget::S2011(const FString& SectionKey, const FString& Fallback) const
{
	if (const FString* Found = UiStrings2011.Find(SectionKey))
	{
		return *Found;
	}
	return Fallback;
}

void UAPBFrontendWidget::LoadUiSounds()
{
	if (bUiSfxLoaded) return;
	bUiSfxLoaded = true;

	// Spec §7: 2011 menu sfx slots → staged /Game/Audio/UI assets
	struct FSfxRow { FName Slot; const TCHAR* Asset; };
	const FSfxRow Rows[] = {
		{ TEXT("UI_Hover"),       TEXT("TabSound_10") },
		{ TEXT("UI_Click"),       TEXT("ButtonPos") },
		{ TEXT("UI_Back"),        TEXT("Button2") },
		{ TEXT("UI_Error"),       TEXT("Error") },
		{ TEXT("UI_Popup"),       TEXT("PopUp") },
		{ TEXT("UI_SceneOpen"),   TEXT("Positive3") },
		{ TEXT("UI_ListSelect"),  TEXT("Spark") },
		{ TEXT("UI_CharConfirm"), TEXT("Positive") },
		{ TEXT("UI_SliderTick"),  TEXT("Button4_616844292") },
		{ TEXT("UI_LoadingPing"), TEXT("CSABeep2") },
	};
	for (const FSfxRow& Row : Rows)
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/UI/%s.%s"), Row.Asset, Row.Asset);
		if (USoundBase* SB = LoadObject<USoundBase>(nullptr, *Path))
		{
			UiSfx.Add(Row.Slot, SB);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("APBFrontend SFX miss %s"), *Path);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("APBFrontend SFX loaded %d/%d slots"), UiSfx.Num(), (int32)UE_ARRAY_COUNT(Rows));
}

void UAPBFrontendWidget::PlayUiSfx(FName SfxSlot)
{
	if (!bUiSfxLoaded) LoadUiSounds();
	if (TObjectPtr<USoundBase>* Found = UiSfx.Find(SfxSlot))
	{
		if (USoundBase* SB = *Found)
		{
			UGameplayStatics::PlaySound2D(this, SB, FMath::Max(MenuAudioVolume, 0.01f));
		}
	}
}

void UAPBFrontendWidget::OnAnyHover()
{
	PlayUiSfx(TEXT("UI_Hover"));
}

void UAPBFrontendWidget::OnAccountLink()
{
	PlayUiSfx(TEXT("UI_Click"));
	LogStage(TEXT("account link (external account page — M4c)"));
}

void UAPBFrontendWidget::OnReplayVideosLink()
{
	PlayUiSfx(TEXT("UI_Click"));
	LogStage(TEXT("replay videos (intro movie gallery — M4c)"));
}

void UAPBFrontendWidget::OnRememberToggled(bool bIsChecked)
{
	PlayUiSfx(TEXT("UI_Click"));
	UE_LOG(LogTemp, Log, TEXT("APBFrontend remember-userid %d"), bIsChecked ? 1 : 0);
}

void UAPBFrontendWidget::ApplyTextureToImage(UImage* Img, UTexture2D* Tex, FLinearColor Tint)
{
	if (!Img) return;
	if (!Tex)
	{
		Img->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Img->SetBrushFromTexture(Tex, true);
	Img->SetColorAndOpacity(Tint);
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UAPBFrontendWidget::ApplyTextureToBorder(UBorder* Border, UTexture2D* Tex, FLinearColor Tint)
{
	if (!Border) return;
	if (!Tex)
	{
		Border->SetBrushColor(Tint);
		return;
	}
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(Tex);
	Brush.TintColor = FSlateColor(Tint);
	Border->SetBrush(Brush);
	Border->SetBrushColor(Tint);
}

UImage* UAPBFrontendWidget::MakeImage(const FString& Name, UTexture2D* Tex, float H)
{
	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *Name);
	ApplyTextureToImage(Img, Tex);
	if (H > 1.f)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(Name + TEXT("_SZ")));
		Box->SetHeightOverride(H);
		Box->AddChild(Img);
		// Caller may want SizeBox; return Image for texture updates — attach SizeBox via separate path when needed.
	}
	return Img;
}

void UAPBFrontendWidget::ApplyPanelChrome(bool bShowForm, const FLinearColor& PanelColor)
{
	if (PanelSizeBox)
	{
		PanelSizeBox->SetVisibility(bShowForm ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	}
	if (PanelBorder)
	{
		const FLinearColor FallbackColor = PanelColor.A > 0.01f ? PanelColor : APB_PANEL;
		FSlateBrush Brush = TexWindowPanel
			? APB_PanelBrush(TexWindowPanel, FLinearColor::White)
			: FSlateBrush();
		if (!TexWindowPanel)
		{
			APB_MakeBoxBrush(Brush, FallbackColor);
		}
		PanelBorder->SetBrush(Brush);
		PanelBorder->SetBrushColor(TexWindowPanel ? FLinearColor::White : FallbackColor);
	}
	if (PanelAccentBar)
	{
		PanelAccentBar->SetVisibility(bShowForm ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (FooterBar)
	{
		FooterBar->SetVisibility(CurrentStage == EAPBFrontendStage::Login
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (SplashLogo)
	{
		SplashLogo->SetVisibility(CurrentStage == EAPBFrontendStage::Splash
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	// Wordmark fixed top-center; no character avatar overlays
	if (LogoImage)
	{
		LogoImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (AvatarLeft) AvatarLeft->SetVisibility(ESlateVisibility::Collapsed);
	if (AvatarRight) AvatarRight->SetVisibility(ESlateVisibility::Collapsed);
}

void UAPBFrontendWidget::RefreshVolumeLabel()
{
	if (VolumeValueText)
	{
		VolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("Menu volume: %d%%"), FMath::RoundToInt(MenuAudioVolume * 100.f))));
	}
	if (MenuVolumeSlider && FMath::Abs(MenuVolumeSlider->GetValue() - MenuAudioVolume) > 0.01f)
	{
		MenuVolumeSlider->SetValue(MenuAudioVolume);
	}
}

void UAPBFrontendWidget::RefreshFactionButtons()
{
	// Solid faction cards + optional package icons (icons are images, not button skins)
	if (FactionCriminalBtn)
	{
		const FLinearColor C = bCreateAsEnforcer ? APB_BTN : APB_CRIM;
		FactionCriminalBtn->SetBackgroundColor(C);
		FButtonStyle S = FactionCriminalBtn->GetStyle();
		S.Normal.TintColor = FSlateColor(C);
		S.Hovered.TintColor = FSlateColor(C + FLinearColor(0.12f, 0.08f, 0.08f, 0.f));
		S.Pressed.TintColor = FSlateColor(C * 0.7f);
		FactionCriminalBtn->SetStyle(S);
	}
	if (FactionEnforcerBtn)
	{
		const FLinearColor C = bCreateAsEnforcer ? APB_ENF : APB_BTN;
		FactionEnforcerBtn->SetBackgroundColor(C);
		FButtonStyle S = FactionEnforcerBtn->GetStyle();
		S.Normal.TintColor = FSlateColor(C);
		S.Hovered.TintColor = FSlateColor(C + FLinearColor(0.08f, 0.10f, 0.14f, 0.f));
		S.Pressed.TintColor = FSlateColor(C * 0.7f);
		FactionEnforcerBtn->SetStyle(S);
	}
	if (FactionCrimeIcon)
	{
		ApplyTextureToImage(FactionCrimeIcon, bCreateAsEnforcer ? (TexFactionCrimOff ? TexFactionCrimOff.Get() : TexFactionCrim.Get()) : TexFactionCrim.Get());
	}
	if (FactionEnfIcon)
	{
		ApplyTextureToImage(FactionEnfIcon, bCreateAsEnforcer ? TexFactionEnf.Get() : (TexFactionEnfOff ? TexFactionEnfOff.Get() : TexFactionEnf.Get()));
	}
	if (EnforcerCheck)
	{
		EnforcerCheck->SetIsChecked(bCreateAsEnforcer);
	}
}

void UAPBFrontendWidget::SelectDistrictIndex(int32 Index)
{
	if (!DistrictIds.IsValidIndex(Index)) return;
	SelectedDistrictId = DistrictIds[Index];
	SelectedDistrictMap = DistrictMaps.IsValidIndex(Index) ? DistrictMaps[Index] : TEXT("Lvl_APB_Financial_Freeroam");
	if (DistrictCombo && DistrictCombo->GetOptionCount() > Index)
	{
		DistrictCombo->SetSelectedIndex(Index);
	}
	const FString Name = DistrictNames.IsValidIndex(Index) ? DistrictNames[Index] : SelectedDistrictId;
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("Selected server: %s  [%s]  — ONLINE"), *Name, *SelectedDistrictId)));
	}
	LogStage(FString::Printf(TEXT("district_select idx=%d id=%s"), Index, *SelectedDistrictId));
}

void UAPBFrontendWidget::AddToScroll(UWidget* W, float PadY)
{
	// Name is historical — login never scrolls; ContentBox is a fixed VBox there.
	UVerticalBox* Target = ContentBox ? ContentBox.Get() : BodyBox.Get();
	if (!Target || !W) return;
	if (UVerticalBoxSlot* S = Target->AddChildToVerticalBox(W))
	{
		S->SetPadding(FMargin(8.f, PadY));
		S->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UAPBFrontendWidget::BeginStageContent(bool bAllowScroll)
{
	bStageAllowsScroll = bAllowScroll;
	if (!BodyBox || !WidgetTree) return;
	BodyBox->ClearChildren();
	BodyScroll = nullptr;
	ContentBox = nullptr;

	if (!bAllowScroll)
	{
		// Fixed retro dialog body — NO ScrollBox (login/select/loading)
		ContentBox = BodyBox;
		return;
	}

	// Long stages only (char create / settings / district list)
	BodyScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BodyScroll"));
	BodyScroll->SetOrientation(Orient_Vertical);
	BodyScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	BodyScroll->SetAnimateWheelScrolling(true);
	if (UVerticalBoxSlot* S = BodyBox->AddChildToVerticalBox(BodyScroll))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ScrollContentBox"));
	BodyScroll->AddChild(ContentBox);
}

TSharedRef<SWidget> UAPBFrontendWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UAPBFrontendWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!RootCanvas) BuildLayout();
	SetVisibility(ESlateVisibility::Visible);
	SetIsEnabled(true);
	SetIsFocusable(true);
	// Mandatory classic path: short splash then LOGIN (not character-first)
	SetStage(EAPBFrontendStage::Splash);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			APB->InitCatalogFromProjectData();
		}
	}
	StartLoginMusic();
	StartLoginBackgroundVideo();
	UpdateViewportScale();
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend NativeConstruct classic_ui root=%d"), RootCanvas != nullptr ? 1 : 0);
}

void UAPBFrontendWidget::NativeDestruct()
{
	StopLoginBackgroundVideo();
	DestroyCharacterPreview();
	StopLoginMusic();
	Super::NativeDestruct();
}

void UAPBFrontendWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateViewportScale();
	EnsureLoginMediaLoop();
	if (CurrentStage == EAPBFrontendStage::Splash && !bSplashAutoDone)
	{
		SplashTimer += InDeltaTime;
		if (SplashTimer >= 1.5f)
		{
			bSplashAutoDone = true;
			OnSplashContinue();
		}
	}
}

UImage* UAPBFrontendWidget::AddLayerImage(const FName& Name, int32 ZOrder)
{
	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	Img->SetColorAndOpacity(FLinearColor::White);
	if (UCanvasPanelSlot* CS = RootCanvas->AddChildToCanvas(Img))
	{
		CS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CS->SetOffsets(FMargin(0.f));
		CS->SetZOrder(ZOrder);
	}
	return Img;
}

FString UAPBFrontendWidget::FirstExistingVideo(const TArray<FString>& Candidates) const
{
	for (const FString& P : Candidates)
	{
		if (FPaths::FileExists(P))
		{
			return FPaths::ConvertRelativePathToFull(P);
		}
	}
	return FString();
}

FString UAPBFrontendWidget::ResolveLoginBgVideoPath() const
{
	return ResolveStageBgVideoPath(EAPBFrontendStage::Login);
}

FString UAPBFrontendWidget::ResolveStageBgVideoPath(EAPBFrontendStage Stage) const
{
	const FString ContentRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	const FString Movies = ContentRoot / TEXT("Movies/Login");
	const FString AI = ContentRoot / TEXT("Extracted/2011/LoginAnimatedBackground_ai_upscale");

	TArray<FString> Candidates;
	auto AddLadder = [&](const TCHAR* CompatMp4, const TCHAR* Webm4kMovies, const TCHAR* Webm4kAI, const TCHAR* Webm5kAI)
	{
		// H.264 first (max PC), then 4k webm in Movies, then AI extract folder
		if (CompatMp4) Candidates.Add(Movies / CompatMp4);
		if (Webm4kMovies) Candidates.Add(Movies / Webm4kMovies);
		if (Webm4kAI) Candidates.Add(AI / Webm4kAI);
		if (Webm5kAI) Candidates.Add(AI / Webm5kAI);
	};

	switch (Stage)
	{
	case EAPBFrontendStage::CharacterSelect:
		AddLadder(
			TEXT("Character_Select_BG_AI_compat.mp4"),
			TEXT("Character_Select_BG_AI_4k.webm"),
			TEXT("01_Character_Select_BG_AI_x4_to_3840x2400.webm"),
			TEXT("01_Character_Select_BG_AI_x4_5120x3200.webm"));
		break;
	case EAPBFrontendStage::CharacterCreate:
		// Faction beds — Criminal default; Enforcer when faction toggle is on
		if (bCreateAsEnforcer)
		{
			AddLadder(
				TEXT("Faction_Enforcer_BG_AI_compat.mp4"),
				TEXT("Faction_Enforcer_BG_AI_4k.webm"),
				TEXT("03_Faction_Select_Enforcer_BG_AI_x4_to_3840x2400.webm"),
				TEXT("03_Faction_Select_Enforcer_BG_AI_x4_5120x3200.webm"));
		}
		else
		{
			AddLadder(
				TEXT("Faction_Criminal_BG_AI_compat.mp4"),
				TEXT("Faction_Criminal_BG_AI_4k.webm"),
				TEXT("02_Faction_Select_Criminal_BG_AI_x4_to_3840x2400.webm"),
				TEXT("02_Faction_Select_Criminal_BG_AI_x4_5120x3200.webm"));
		}
		break;
	case EAPBFrontendStage::DistrictSelect:
	case EAPBFrontendStage::Settings:
	case EAPBFrontendStage::Loading:
		AddLadder(
			TEXT("Generic_BG_AI_compat.mp4"),
			TEXT("Generic_BG_AI_4k.webm"),
			TEXT("04_Generic_BG_AI_x4_to_3840x2400.webm"),
			TEXT("04_Generic_BG_AI_x4_5120x3200.webm"));
		break;
	case EAPBFrontendStage::Splash:
	case EAPBFrontendStage::Login:
	default:
		Candidates.Add(Movies / TEXT("Login_BG_AI_compat.mp4"));
		Candidates.Add(Movies / TEXT("Login_BG_AI_hd.mp4"));
		Candidates.Add(Movies / TEXT("Login_BG_AI_4k.mp4"));
		Candidates.Add(Movies / TEXT("Login_BG_AI_full.webm"));
		Candidates.Add(Movies / TEXT("Login_BG_AI_4k.webm"));
		Candidates.Add(Movies / TEXT("05_Login_BG_AI_x4_5120x3200.webm"));
		Candidates.Add(AI / TEXT("05_Login_BG_AI_x4_to_3840x2400.webm"));
		Candidates.Add(AI / TEXT("05_Login_BG_AI_x4_5120x3200.webm"));
		break;
	}
	return FirstExistingVideo(Candidates);
}

void UAPBFrontendWidget::StartLoginBackgroundVideo()
{
	ApplyStageBackgroundVideo(EAPBFrontendStage::Login);
}

void UAPBFrontendWidget::ApplyStageBackgroundVideo(EAPBFrontendStage Stage)
{
	const FString VideoPath = ResolveStageBgVideoPath(Stage);
	if (VideoPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend StageBG missing stage=%s"), *GetStageToken());
		return;
	}

	if (!LoginMediaPlayer)
	{
		LoginMediaPlayer = NewObject<UMediaPlayer>(this, TEXT("LoginMediaPlayer"));
		LoginMediaPlayer->PlayOnOpen = true;
		LoginMediaPlayer->SetLooping(true);
	}
	if (!LoginMediaTexture)
	{
		LoginMediaTexture = NewObject<UMediaTexture>(this, TEXT("LoginMediaTexture"));
		LoginMediaTexture->SetMediaPlayer(LoginMediaPlayer);
		LoginMediaTexture->UpdateResource();
	}
	if (!LoginMediaSource)
	{
		LoginMediaSource = NewObject<UFileMediaSource>(this, TEXT("LoginMediaSource"));
	}

	// Stage AI movies are the bed — hide still graffiti plate (was reading as junk overlay)
	if (BgArt)
	{
		BgArt->SetVisibility(ESlateVisibility::Collapsed);
	}

	LoginMediaSource->SetFilePath(VideoPath);
	LoginMediaPlayer->SetLooping(true);

	const bool bOk = LoginMediaPlayer->OpenSource(LoginMediaSource);
	const TCHAR* StageName =
		(Stage == EAPBFrontendStage::CharacterSelect) ? TEXT("CharacterSelect")
		: (Stage == EAPBFrontendStage::CharacterCreate) ? (bCreateAsEnforcer ? TEXT("FactionEnforcer") : TEXT("FactionCriminal"))
		: (Stage == EAPBFrontendStage::DistrictSelect) ? TEXT("DistrictSelect")
		: (Stage == EAPBFrontendStage::Login) ? TEXT("Login")
		: TEXT("Other");
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend StageBG OpenSource ok=%d stage=%s path=%s"),
		bOk ? 1 : 0, StageName, *VideoPath);
	if (!bOk && !LoginMediaPlayer->OpenFile(VideoPath))
	{
		UE_LOG(LogTemp, Error, TEXT("APBFrontend StageBG OpenFile failed: %s"), *VideoPath);
		return;
	}

	if (BgVideo)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Brush.SetResourceObject(LoginMediaTexture);
		Brush.ImageSize = FVector2D(5120.f, 3200.f);
		BgVideo->SetBrush(Brush);
		BgVideo->SetColorAndOpacity(FLinearColor::White);
		BgVideo->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	LoginMediaPlayer->Play();
	bLoginVideoStarted = true;
}

void UAPBFrontendWidget::StopLoginBackgroundVideo()
{
	if (LoginMediaPlayer)
	{
		LoginMediaPlayer->Close();
	}
	bLoginVideoStarted = false;
	if (BgVideo)
	{
		BgVideo->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAPBFrontendWidget::EnsureLoginMediaLoop()
{
	if (!bLoginVideoStarted || !LoginMediaPlayer)
	{
		return;
	}
	// Keep looping even if platform plugin ignores SetLooping
	if (LoginMediaPlayer->IsClosed())
	{
		return;
	}
	if (!LoginMediaPlayer->IsPlaying() && !LoginMediaPlayer->IsPreparing() && !LoginMediaPlayer->IsBuffering())
	{
		const float Dur = LoginMediaPlayer->GetDuration().GetTotalSeconds();
		const float T = LoginMediaPlayer->GetTime().GetTotalSeconds();
		if (Dur > 0.1f && T >= Dur - 0.15f)
		{
			LoginMediaPlayer->Seek(FTimespan::Zero());
		}
		LoginMediaPlayer->Play();
	}
}

void UAPBFrontendWidget::UpdateViewportScale()
{
	if (!PanelSizeBox) return;
	FVector2D VP(1920.f, 1080.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(VP);
	}
	if (VP.Equals(LastViewport, 1.f)) return;
	LastViewport = VP;

	// Multi-aspect scale via shipped pure math (APBFrontendLayoutMath.h)
	const FString Token = GetStageToken();
	const apb_layout::ScaleMode Mode =
		(UiScaleMode == EAPBUiScaleMode::Fill) ? apb_layout::ScaleMode::Fill
		: (UiScaleMode == EAPBUiScaleMode::Stretch) ? apb_layout::ScaleMode::Stretch
		: apb_layout::ScaleMode::Fit;

	float UseW = 0.f, UseH = 0.f;
	apb_layout::ScaledPanelSize(TCHAR_TO_UTF8(*Token), VP.X, VP.Y, Mode, UseW, UseH);
	float ScaleX = 1.f, ScaleY = 1.f, Uni = 1.f;
	apb_layout::ComputeUiScale(VP.X, VP.Y, Mode, ScaleX, ScaleY, Uni);

	// Fixed stages (login/select): exact height — no scroll, no stretchy empty region
	const bool bFixed = !bStageAllowsScroll;
	PanelSizeBox->SetWidthOverride(UseW);
	PanelSizeBox->SetMaxDesiredHeight(UseH);
	if (bFixed)
	{
		PanelSizeBox->SetMinDesiredHeight(UseH);
	}
	else
	{
		PanelSizeBox->SetMinDesiredHeight(FMath::Min(UseH * 0.65f, UseH));
	}
	if (PanelSlot)
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}
	// Logo scales with dialog width (same group as plate — never odd vs bed)
	if (LogoSizeBox)
	{
		float LogoW = 0.f, LogoH = 0.f;
		apb_layout::LogoSizeFromPanelWidth(UseW, LogoW, LogoH);
		LogoSizeBox->SetWidthOverride(LogoW);
		LogoSizeBox->SetHeightOverride(LogoH);
	}
	if (LogoImage && TexLogo)
	{
		// Keep native logo brush aspect 512×128 while size box scales
		FSlateBrush Br = LogoImage->GetBrush();
		Br.ImageSize = FVector2D(512.f, 128.f);
		Br.DrawAs = ESlateBrushDrawType::Image;
		LogoImage->SetBrush(Br);
	}
	// Login never shows a scrollbar
	if (BodyScroll)
	{
		BodyScroll->SetScrollBarVisibility(
			bStageAllowsScroll ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		BodyScroll->SetConsumeMouseWheel(
			bStageAllowsScroll ? EConsumeMouseWheel::WhenScrollingPossible : EConsumeMouseWheel::Never);
	}
	if (TitleText)
	{
		const int32 TitleSz = FMath::Clamp(FMath::RoundToInt(16.f * Uni), 13, 22);
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", TitleSz));
	}
	// Video always full-bleed cover (independent of UI Fit)
	if (BgVideo)
	{
		if (UCanvasPanelSlot* VS = Cast<UCanvasPanelSlot>(BgVideo->Slot))
		{
			VS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			VS->SetOffsets(FMargin(0.f));
			VS->SetZOrder(1);
		}
		BgVideo->SetColorAndOpacity(FLinearColor::White);
	}
}

void UAPBFrontendWidget::GetDesignPanelSize(float& OutW, float& OutH) const
{
	apb_layout::DesignPanelSize(TCHAR_TO_UTF8(*GetStageToken()), OutW, OutH);
}

// Shared PCM for looping re-queue (procedural waves don't own a full decoded buffer the way assets do).
static TArray<uint8> GLoginThemePcm;
static uint32 GLoginThemeSampleRate = 0;
static uint16 GLoginThemeNumChannels = 0;

// Load 16-bit PCM WAV as USoundWaveProcedural (safe for runtime; plain USoundWave+RawPCM asserts on audio thread).
static USoundWaveProcedural* APB_LoadPcmWavProcedural(UObject* Outer, const FString& AbsPath, bool bLooping)
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *AbsPath) || FileData.Num() < 44)
	{
		return nullptr;
	}
	if (!(FileData[0] == 'R' && FileData[1] == 'I' && FileData[2] == 'F' && FileData[3] == 'F'))
	{
		return nullptr;
	}

	uint16 AudioFormat = 0;
	uint16 NumChannels = 0;
	uint32 SampleRate = 0;
	uint16 BitsPerSample = 0;
	const uint8* Pcm = nullptr;
	int32 PcmSize = 0;

	int32 Off = 12;
	while (Off + 8 <= FileData.Num())
	{
		const uint8* Tag = FileData.GetData() + Off;
		const uint32 ChunkSize = *reinterpret_cast<const uint32*>(FileData.GetData() + Off + 4);
		const int32 Payload = Off + 8;
		if (Payload + static_cast<int32>(ChunkSize) > FileData.Num())
		{
			break;
		}
		if (Tag[0] == 'f' && Tag[1] == 'm' && Tag[2] == 't' && Tag[3] == ' ')
		{
			if (ChunkSize >= 16)
			{
				FMemory::Memcpy(&AudioFormat, FileData.GetData() + Payload, sizeof(uint16));
				FMemory::Memcpy(&NumChannels, FileData.GetData() + Payload + 2, sizeof(uint16));
				FMemory::Memcpy(&SampleRate, FileData.GetData() + Payload + 4, sizeof(uint32));
				FMemory::Memcpy(&BitsPerSample, FileData.GetData() + Payload + 14, sizeof(uint16));
			}
		}
		else if (Tag[0] == 'd' && Tag[1] == 'a' && Tag[2] == 't' && Tag[3] == 'a')
		{
			Pcm = FileData.GetData() + Payload;
			PcmSize = static_cast<int32>(ChunkSize);
			break;
		}
		Off = Payload + static_cast<int32>(ChunkSize);
		if (Off & 1)
		{
			++Off;
		}
	}

	// PCM (1) only — our vgmstream decode writes 16-bit integer WAV.
	if (AudioFormat != 1 || NumChannels == 0 || SampleRate == 0 || BitsPerSample != 16 || !Pcm || PcmSize <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend WAV unsupported fmt=%u ch=%u sr=%u bits=%u size=%d path=%s"),
			AudioFormat, NumChannels, SampleRate, BitsPerSample, PcmSize, *AbsPath);
		return nullptr;
	}

	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(Outer, NAME_None, RF_Transient);
	if (!Wave)
	{
		return nullptr;
	}

	GLoginThemePcm.Reset(PcmSize);
	GLoginThemePcm.Append(Pcm, PcmSize);
	GLoginThemeSampleRate = SampleRate;
	GLoginThemeNumChannels = NumChannels;

	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = NumChannels;
	Wave->SampleByteSize = sizeof(int16);
	Wave->Duration = static_cast<float>(PcmSize) / static_cast<float>(SampleRate * NumChannels * sizeof(int16));
	Wave->SoundGroup = SOUNDGROUP_Music;
	Wave->bLooping = bLooping;
	// Avoid Inherited loading-behavior path that asserts off the game thread.
	Wave->LoadingBehavior = ESoundWaveLoadingBehavior::ForceInline;

	// Prime buffer, then re-queue on underrun so the ~2 min theme loops.
	Wave->QueueAudio(GLoginThemePcm.GetData(), GLoginThemePcm.Num());
	if (bLooping)
	{
		Wave->OnSoundWaveProceduralUnderflow.BindLambda(
			[](USoundWaveProcedural* InWave, int32 /*SamplesNeeded*/)
			{
				if (InWave && GLoginThemePcm.Num() > 0)
				{
					InWave->QueueAudio(GLoginThemePcm.GetData(), GLoginThemePcm.Num());
				}
			});
	}
	return Wave;
}

void UAPBFrontendWidget::StartLoginMusic()
{
	if (bMusicStarted) return;
	UWorld* World = GetWorld();
	if (!World) return;

	// 2011 RTW classic theme: StreamedSFX 841514482_APBTheme1 (user-selected nostalgia track).
	// Fallback: ThemePreMaster / packaged sound assets.
	USoundBase* Theme = nullptr;
	FString ThemeTag = TEXT("841514482_APBTheme1");

	const FString ContentRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	const FString WavCandidates[] = {
		ContentRoot / TEXT("Audio/841514482_APBTheme1.wav"),
		ContentRoot / TEXT("Audio/LoginTheme_APBTheme1.wav"),
		ContentRoot / TEXT("Extracted/Audio/2011/StreamedSFX/wav/841514482_APBTheme1.wav"),
		FPaths::ProjectContentDir() / TEXT("Audio/841514482_APBTheme1.wav"),
		// legacy fallbacks
		ContentRoot / TEXT("Audio/LoginTheme_APB_ThemePreMaster.wav"),
		FPaths::ProjectContentDir() / TEXT("Audio/LoginTheme_APB_ThemePreMaster.wav"),
	};
	for (const FString& WavPath : WavCandidates)
	{
		if (!FPaths::FileExists(WavPath)) continue;
		Theme = APB_LoadPcmWavProcedural(this, WavPath, /*bLooping=*/true);
		if (Theme)
		{
			ThemeTag = FPaths::GetCleanFilename(WavPath);
			UE_LOG(LogTemp, Warning, TEXT("APBFrontend MUSIC loaded procedural WAV %s (%d bytes pcm)"),
				*WavPath, GLoginThemePcm.Num());
			break;
		}
	}
	if (!Theme)
	{
		Theme = LoadObject<USoundBase>(nullptr,
			TEXT("/Game/Audio/LoginTheme_APBTheme1.LoginTheme_APBTheme1"));
		if (Theme) ThemeTag = TEXT("LoginTheme_APBTheme1");
	}
	if (!Theme)
	{
		Theme = LoadObject<USoundBase>(nullptr,
			TEXT("/Game/Audio/LoginTheme_APB_ThemePreMaster.LoginTheme_APB_ThemePreMaster"));
		if (Theme) ThemeTag = TEXT("LoginTheme_APB_ThemePreMaster");
	}
	if (!Theme)
	{
		Theme = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/LoginTheme.LoginTheme"));
		if (Theme) ThemeTag = TEXT("LoginTheme");
	}
	if (!Theme)
	{
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend MUSIC missing 841514482_APBTheme1.wav (2011 StreamedSFX)"));
		LogStage(TEXT("music_missing"));
		return;
	}
	if (!LoginMusicComp)
	{
		LoginMusicComp = NewObject<UAudioComponent>(this, TEXT("LoginMusic"));
		LoginMusicComp->bAutoActivate = false;
		LoginMusicComp->bIsUISound = true;
		LoginMusicComp->bAllowSpatialization = false;
		LoginMusicComp->RegisterComponentWithWorld(World);
	}
	LoginMusicComp->SetSound(Theme);
	// Menu volume: single knob for future settings UI (video bed is silent).
	LoginMusicComp->SetVolumeMultiplier(MenuAudioVolume);
	LoginMusicComp->bAutoDestroy = false;
	LoginMusicComp->Play();
	bMusicStarted = true;
	LogStage(FString::Printf(TEXT("music_play=%s"), *ThemeTag));
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend MUSIC_PLAY %s"), *ThemeTag);
}

void UAPBFrontendWidget::StopLoginMusic()
{
	if (LoginMusicComp && LoginMusicComp->IsPlaying())
	{
		LoginMusicComp->Stop();
		LogStage(TEXT("music_stop"));
	}
	bMusicStarted = false;
}

void UAPBFrontendWidget::SetMenuAudioVolume(float Volume01)
{
	MenuAudioVolume = FMath::Clamp(Volume01, 0.f, 1.f);
	if (LoginMusicComp)
	{
		LoginMusicComp->SetVolumeMultiplier(MenuAudioVolume);
	}
	// Video bed stays silent — future Menu slider only drives UI music/SFX.
	UE_LOG(LogTemp, Log, TEXT("APBFrontend MenuAudioVolume=%.2f"), MenuAudioVolume);
}

FString UAPBFrontendWidget::GetStageToken() const
{
	switch (CurrentStage)
	{
	case EAPBFrontendStage::Splash: return TEXT("Splash");
	case EAPBFrontendStage::Login: return bFirstRunTOS ? TEXT("LoginTOS") : TEXT("Login");
	case EAPBFrontendStage::CharacterSelect: return TEXT("CharacterSelect");
	case EAPBFrontendStage::CharacterCreate: return TEXT("CharacterCreate");
	case EAPBFrontendStage::DistrictSelect: return TEXT("DistrictSelect");
	case EAPBFrontendStage::Settings: return TEXT("Settings");
	case EAPBFrontendStage::Loading: return TEXT("Loading");
	case EAPBFrontendStage::InDistrict: return TEXT("InDistrict");
	default: return TEXT("Unknown");
	}
}

void UAPBFrontendWidget::SyncHud()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AAPBFrontendHUD* H = Cast<AAPBFrontendHUD>(PC->GetHUD()))
		{
			const FString Status = StatusText ? StatusText->GetText().ToString() : FString();
			H->SetHudStage(GetStageToken(), Status);
		}
	}
}

void UAPBFrontendWidget::LogStage(const FString& Extra)
{
	const FString Token = GetStageToken();
	const FString Line = FString::Printf(TEXT("UI_STAGE=%s %s"), *Token, *Extra);
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend %s"), *Line);
	if (GEngine) GEngine->AddOnScreenDebugMessage(9002, 6.f, FColor::Green, Line);
	SyncHud();
	FString Scratch = FPlatformMisc::GetEnvironmentVariable(TEXT("APB_SCRATCH"));
	if (Scratch.IsEmpty())
	{
		Scratch = TEXT("C:/Users/Support/AppData/Local/Temp/grok-goal-8fe59cc1a4c5/implementer");
	}
	Scratch.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (!Scratch.EndsWith(TEXT("/"))) Scratch += TEXT("/");
	IFileManager::Get().MakeDirectory(*Scratch, true);
	FFileHelper::SaveStringToFile(Line + TEXT("\n"), *(Scratch + TEXT("frontend_ui_stages.log")),
		FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
}

void UAPBFrontendWidget::BuildLayout()
{
	if (!WidgetTree) return;

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	LoadMenu2011Assets();

	// Classic composition (from Login_Scene_Preview / live Login_BG):
	// z0 paper | z1 graffiti still | z2 video bed | z5 L/R avatars | z15 logo | z20 center black card
	FullscreenBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FullscreenBg"));
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(APB_PAPER);
		FullscreenBg->SetBrush(Brush);
		FullscreenBg->SetBrushColor(APB_PAPER);
	}
	if (UCanvasPanelSlot* CS = RootCanvas->AddChildToCanvas(FullscreenBg))
	{
		CS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CS->SetOffsets(FMargin(0.f));
		CS->SetZOrder(0);
	}

	// Stage AI video full-bleed only (no character art overlays)
	BgArt = nullptr;
	AvatarLeft = nullptr;
	AvatarRight = nullptr;

	BgVideo = AddLayerImage(TEXT("BgLoginVideo"), 1);
	if (BgVideo) BgVideo->SetColorAndOpacity(FLinearColor::White);

	FooterBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FooterBar"));
	FooterBar->SetPadding(FMargin(24.f, 20.f, 24.f, 12.f));
	{
		FSlateBrush Brush = TexFooter ? APB_TexBrush(TexFooter, FLinearColor::White) : FSlateBrush();
		if (!TexFooter) APB_MakeBoxBrush(Brush, FLinearColor(0.f, 0.f, 0.f, 0.88f));
		FooterBar->SetBrush(Brush);
		FooterBar->SetBrushColor(TexFooter ? FLinearColor::White : FLinearColor(0.f, 0.f, 0.f, 0.88f));
	}
	UHorizontalBox* FooterLinks = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FooterLinks"));
	auto AddFooterLink = [&](UButton* Link)
	{
		if (UHorizontalBoxSlot* Slot = FooterLinks->AddChildToHorizontalBox(Link))
		{
			Slot->SetPadding(FMargin(6.f, 0.f));
			Slot->SetHorizontalAlignment(HAlign_Right);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
	};
	UButton* FooterExit = MakeButton(TEXT("FooterExit"), S2011(TEXT("APBLoginScreen.ExitToDesktop"), TEXT("EXIT TO DESKTOP")));
	FooterExit->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnExitDesktop);
	AddFooterLink(FooterExit);
	UButton* FooterAccount = MakeButton(TEXT("FooterAccount"), S2011(TEXT("APBLoginScreen.AccountManagement"), TEXT("ACCOUNT")));
	FooterAccount->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnAccountLink);
	AddFooterLink(FooterAccount);
	UButton* FooterReplay = MakeButton(TEXT("FooterReplay"), S2011(TEXT("APBLoginScreen.ReplayVideos"), TEXT("REPLAY VIDEOS")));
	FooterReplay->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnReplayVideosLink);
	AddFooterLink(FooterReplay);
	if (UBorderSlot* FooterSlot = Cast<UBorderSlot>(FooterBar->AddChild(FooterLinks)))
	{
		FooterSlot->SetHorizontalAlignment(HAlign_Right);
		FooterSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UCanvasPanelSlot* CS = RootCanvas->AddChildToCanvas(FooterBar))
	{
		CS->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		CS->SetAlignment(FVector2D(0.f, 1.f));
		CS->SetOffsets(FMargin(0.f, -80.f, 0.f, 80.f));
		CS->SetZOrder(15);
	}
	FooterBar->SetVisibility(ESlateVisibility::Collapsed);

	SplashLogo = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SplashLogo"));
	SplashLogo->SetBrush(APB_TexBrush(TexLogo, FLinearColor::White));
	if (UCanvasPanelSlot* CS = RootCanvas->AddChildToCanvas(SplashLogo))
	{
		CS->SetAnchors(FAnchors(0.5f, 0.5f));
		CS->SetAlignment(FVector2D(0.5f, 0.5f));
		CS->SetOffsets(FMargin(0.f, 0.f, 640.f, 160.f));
		CS->SetZOrder(16);
	}
	SplashLogo->SetVisibility(ESlateVisibility::Collapsed);

	PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSizeBox"));
	PanelSizeBox->SetWidthOverride(380.f);
	PanelSizeBox->SetMaxDesiredHeight(400.f);
	PanelSlot = RootCanvas->AddChildToCanvas(PanelSizeBox);
	if (PanelSlot)
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetZOrder(20);
	}

	UBorder* OuterFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterFrame"));
	{
		FSlateBrush Brush;
		APB_MakeBoxBrush(Brush, APB_PANEL_EDGE);
		OuterFrame->SetBrush(Brush);
		OuterFrame->SetBrushColor(APB_PANEL_EDGE);
		OuterFrame->SetPadding(FMargin(1.f));
	}
	PanelSizeBox->AddChild(OuterFrame);

	UVerticalBox* Shell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelShell"));
	OuterFrame->AddChild(Shell);

	PanelAccentBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelAccentBar"));
	{
		FSlateBrush Brush;
		APB_MakeBoxBrush(Brush, APB_PANEL);
		PanelAccentBar->SetBrush(Brush);
		PanelAccentBar->SetBrushColor(APB_PANEL);
		PanelAccentBar->SetPadding(FMargin(16.f, 8.f));
	}
	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TitleRow"));
	TitleChip = MakeImage(TEXT("TitleChip"), TexBrandKey);
	if (TitleChip) TitleChip->SetColorAndOpacity(APB_AMBER);
	USizeBox* TitleChipSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TitleChipSize"));
	TitleChipSize->SetWidthOverride(28.f);
	TitleChipSize->SetHeightOverride(28.f);
	TitleChipSize->AddChild(TitleChip);
	if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(TitleChipSize))
	{
		HS->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		HS->SetVerticalAlignment(VAlign_Center);
	}
	TitleText = MakeLabel(TEXT("Title"), TEXT("LOGIN"), 20, APB_WHITE);
	TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
	TitleText->SetJustification(ETextJustify::Left);
	if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(TitleText))
	{
		HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HS->SetVerticalAlignment(VAlign_Center);
	}
	TitleCloseBtn = MakeButton(TEXT("TitleCloseBtn"), TEXT("X"));
	if (TexCloseBtn)
	{
		FButtonStyle Style = TitleCloseBtn->GetStyle();
		Style.Normal = APB_TexBrush(TexCloseBtn, FLinearColor::White);
		Style.Hovered = APB_TexBrush(TexCloseBtn, APB_AMBER_HI);
		Style.Pressed = APB_TexBrush(TexCloseBtn, APB_AMBER);
		TitleCloseBtn->SetStyle(Style);
	}
	TitleCloseBtn->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnExitDesktop);
	USizeBox* TitleCloseSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TitleCloseSize"));
	TitleCloseSize->SetWidthOverride(28.f);
	TitleCloseSize->SetHeightOverride(28.f);
	TitleCloseSize->AddChild(TitleCloseBtn);
	if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(TitleCloseSize))
	{
		HS->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		HS->SetVerticalAlignment(VAlign_Center);
	}
	PanelAccentBar->AddChild(TitleRow);
	USizeBox* AccentSz = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AccentSz"));
	AccentSz->SetHeightOverride(48.f);
	AccentSz->AddChild(PanelAccentBar);
	if (UVerticalBoxSlot* S = Shell->AddChildToVerticalBox(AccentSz))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
	}

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetPadding(FMargin(18.f, 12.f, 18.f, 14.f));
	{
		FSlateBrush Brush = TexWindowPanel
			? APB_PanelBrush(TexWindowPanel, FLinearColor::White)
			: FSlateBrush();
		if (!TexWindowPanel) APB_MakeBoxBrush(Brush, APB_PANEL);
		PanelBorder->SetBrush(Brush);
		PanelBorder->SetBrushColor(TexWindowPanel ? FLinearColor::White : APB_PANEL);
	}
	if (UVerticalBoxSlot* S = Shell->AddChildToVerticalBox(PanelBorder))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* OuterV = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OuterV"));
	if (UBorderSlot* BS = Cast<UBorderSlot>(PanelBorder->AddChild(OuterV)))
	{
		BS->SetHorizontalAlignment(HAlign_Fill);
		BS->SetVerticalAlignment(VAlign_Fill);
	}
	else PanelBorder->AddChild(OuterV);

	// Login_APB_Logo.png inside plate (black field blends with panel)
	LogoSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LogoSizeBox"));
	LogoSizeBox->SetHeightOverride(72.f);
	LogoSizeBox->SetWidthOverride(300.f);
	LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LogoAPBReloaded"));
	ApplyTextureToImage(LogoImage, TexLogo);
	if (LogoImage && TexLogo)
	{
		FSlateBrush Br;
		Br.SetResourceObject(TexLogo);
		Br.DrawAs = ESlateBrushDrawType::Image;
		Br.ImageSize = FVector2D(512.f, 128.f);
		Br.Tiling = ESlateBrushTileType::NoTile;
		LogoImage->SetBrush(Br);
		LogoImage->SetColorAndOpacity(FLinearColor::White);
		LogoImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	LogoSizeBox->AddChild(LogoImage);
	if (UVerticalBoxSlot* S = OuterV->AddChildToVerticalBox(LogoSizeBox))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 2.f, 0.f, 8.f));
	}

	BrandBar = MakeLabel(TEXT("BrandBar"), TEXT(""), 1, APB_MUTED);
	BrandBar->SetVisibility(ESlateVisibility::Collapsed);
	SubtitleText = MakeLabel(TEXT("Sub"), TEXT(""), 10, APB_MUTED);
	SubtitleText->SetJustification(ETextJustify::Center);
	StatusText = MakeLabel(TEXT("Status"), TEXT(""), 11, APB_AMBER);
	StatusText->SetJustification(ETextJustify::Center);
	HintText = MakeLabel(TEXT("Hint"), TEXT(""), 10, APB_MUTED);
	HintText->SetVisibility(ESlateVisibility::Collapsed);

	auto AddHeader = [&](UWidget* W, float Top)
	{
		if (UVerticalBoxSlot* S = OuterV->AddChildToVerticalBox(W))
		{
			S->SetPadding(FMargin(2.f, Top, 2.f, 1.f));
			S->SetHorizontalAlignment(HAlign_Fill);
		}
	};
	AddHeader(SubtitleText, 2.f);
	AddHeader(StatusText, 4.f);

	// Fixed body — login never scrolls
	BodyBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BodyBox"));
	ContentBox = BodyBox;
	BodyScroll = nullptr;
	if (UVerticalBoxSlot* S = OuterV->AddChildToVerticalBox(BodyBox))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetPadding(FMargin(2.f, 4.f, 2.f, 0.f));
	}

	UE_LOG(LogTemp, Warning, TEXT("APBFrontend BuildLayout GameFlow plate logo=%d"), TexLogo ? 1 : 0);
}

void UAPBFrontendWidget::SetStage(EAPBFrontendStage Stage)
{
	if (CurrentStage == EAPBFrontendStage::CharacterCreate && Stage != EAPBFrontendStage::CharacterCreate)
	{
		DestroyCharacterPreview();
	}
	CurrentStage = Stage;
	RebuildStageBody();
	// Stage-specific AI beds: Login / CharacterSelect / Faction(Criminal|Enforcer) / Generic
	ApplyStageBackgroundVideo(Stage);
	LogStage();
	UpdateViewportScale();
	if (Stage == EAPBFrontendStage::CharacterCreate)
	{
		EnsureCharacterPreview();
		RefreshCharacterPreviewFromUI();
	}
	if (LogoImage)
	{
		LogoImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (LogoSizeBox)
	{
		// Logo only on main lobby stages (above dialog stack)
		const bool bLogo =
			Stage == EAPBFrontendStage::Login ||
			Stage == EAPBFrontendStage::CharacterSelect ||
			Stage == EAPBFrontendStage::DistrictSelect ||
			Stage == EAPBFrontendStage::Settings ||
			Stage == EAPBFrontendStage::CharacterCreate;
		LogoSizeBox->SetVisibility(bLogo ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UAPBFrontendWidget::RebuildStageBody()
{
	if (!BodyBox || !WidgetTree) return;
	UserBox = PassBox = CharNameBox = nullptr;
	RememberCheck = nullptr;
	EnforcerCheck = nullptr;
	FactionCriminalBtn = FactionEnforcerBtn = nullptr;
	SlotHead = SlotTorso = SlotLegs = SlotFeet = SlotHands = SlotAccessory = SlotFace = nullptr;
	DistrictCombo = nullptr;
	BodyHeightBox = BodyBuildBox = nullptr;
	PreviewSummary = nullptr;
	CharPreviewImage = nullptr;
	CharPreviewSizeBox = nullptr;
	MenuVolumeSlider = nullptr;
	VolumeValueText = nullptr;
	ResolutionCombo = nullptr;
	DisplayModeLabel = nullptr;

	const FLinearColor PanelCol = APB_PANEL;

	switch (CurrentStage)
	{
	case EAPBFrontendStage::Splash:
	{
		BeginStageContent(false);
		if (SplashLogo) SplashLogo->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (FooterBar) FooterBar->SetVisibility(ESlateVisibility::Collapsed);
		if (TitleText) TitleText->SetVisibility(ESlateVisibility::Collapsed);
		if (SubtitleText) SubtitleText->SetVisibility(ESlateVisibility::Collapsed);
		if (StatusText) StatusText->SetVisibility(ESlateVisibility::Collapsed);
		if (HintText) HintText->SetVisibility(ESlateVisibility::Collapsed);
		ApplyPanelChrome(false, FLinearColor(0.f, 0.f, 0.f, 0.f));
		break;
	}
	case EAPBFrontendStage::Login:
	{
		BeginStageContent(false);
		ApplyPanelChrome(true, PanelCol);
		if (SplashLogo) SplashLogo->SetVisibility(ESlateVisibility::Collapsed);
		if (FooterBar) FooterBar->SetVisibility(ESlateVisibility::Visible);
		if (LogoSizeBox) LogoSizeBox->SetVisibility(ESlateVisibility::Visible);
		if (TitleText)
		{
			TitleText->SetVisibility(ESlateVisibility::Visible);
			TitleText->SetText(FText::FromString(bFirstRunTOS
				? S2011(TEXT("APBLoginScreen.TermsOfService"), TEXT("TERMS OF SERVICE"))
				: S2011(TEXT("APBLoginScreen.Login"), TEXT("LOGIN"))));
			TitleText->SetColorAndOpacity(FSlateColor(APB_WHITE));
			TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
			TitleText->SetJustification(ETextJustify::Left);
		}
		// Keep subtitle empty on login (original plate is clean; status shows errors)
		if (SubtitleText)
		{
			SubtitleText->SetVisibility(ESlateVisibility::Collapsed);
			SubtitleText->SetText(FText::GetEmpty());
		}
		if (StatusText)
		{
			StatusText->SetVisibility(ESlateVisibility::Visible);
			StatusText->SetText(FText::GetEmpty());
			StatusText->SetJustification(ETextJustify::Center);
		}
		if (HintText) HintText->SetVisibility(ESlateVisibility::Collapsed);

		AddToScroll(MakeLabel(TEXT("lu"), S2011(TEXT("APBLoginScreen.EmailAddress"), TEXT("Email Address")), 13, APB_MUTED), 4.f);
		UserBox = MakeTextField(TEXT("LoginUserBox"), TEXT(""), false);
		UserBox->SetText(FText::GetEmpty());
		AddToScroll(UserBox, 2.f);

		AddToScroll(MakeLabel(TEXT("lp"), S2011(TEXT("APBLoginScreen.Password"), TEXT("Password")), 13, APB_MUTED), 8.f);
		PassBox = MakeTextField(TEXT("LoginPassBox"), TEXT(""), true);
		PassBox->SetText(FText::GetEmpty());
		AddToScroll(PassBox, 2.f);

		if (bFirstRunTOS)
		{
			UScrollBox* TosScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TosScroll"));
			TosScroll->SetAlwaysShowScrollbar(true);
			UTextBlock* TosText = MakeLabel(TEXT("TosText"),
				TEXT("1. LICENSE\n\nThis software is provided for personal use in accordance with the APB Reloaded terms of service. Access to the service is subject to account eligibility and these terms.\n\n")
				TEXT("2. RESTRICTIONS\n\nYou may not interfere with the service, attempt unauthorized access, exploit defects, or use software that provides an unfair advantage.\n\n")
				TEXT("3. CONDUCT\n\nPlayers are responsible for account security and for conduct associated with their account. Service access may be suspended when these terms are violated.\n\n")
				TEXT("4. ACCEPTANCE\n\nSelecting ACCEPT confirms that you have read and agree to these terms."),
				15, APB_WHITE);
			TosText->SetAutoWrapText(true);
			TosScroll->AddChild(TosText);
			USizeBox* TosSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TosSize"));
			TosSize->SetHeightOverride(500.f);
			TosSize->AddChild(TosScroll);
			AddToScroll(TosSize, 12.f);
		}

		UHorizontalBox* RememberRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RememberRow"));
		RememberCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("RememberCheck"));
		if (TexCheckTrue && TexCheckFalse)
		{
			FCheckBoxStyle Style = RememberCheck->GetWidgetStyle();
			Style.UncheckedImage = APB_TexBrush(TexCheckFalse, FLinearColor::White);
			Style.UncheckedHoveredImage = APB_TexBrush(TexCheckFalse, APB_AMBER_HI);
			Style.UncheckedPressedImage = APB_TexBrush(TexCheckFalse, APB_AMBER);
			Style.CheckedImage = APB_TexBrush(TexCheckTrue, FLinearColor::White);
			Style.CheckedHoveredImage = APB_TexBrush(TexCheckTrue, APB_AMBER_HI);
			Style.CheckedPressedImage = APB_TexBrush(TexCheckTrue, APB_AMBER);
			RememberCheck->SetWidgetStyle(Style);
		}
		RememberCheck->OnCheckStateChanged.AddDynamic(this, &UAPBFrontendWidget::OnRememberToggled);
		UTextBlock* RememberLabel = MakeLabel(TEXT("RememberLabel"), S2011(TEXT("APBLoginScreen.RememberUserID"), TEXT("Remember Me")), 13, APB_WHITE);
		if (UHorizontalBoxSlot* HS = RememberRow->AddChildToHorizontalBox(RememberCheck))
		{
			HS->SetPadding(FMargin(0.f, 4.f, 8.f, 0.f));
			HS->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* HS = RememberRow->AddChildToHorizontalBox(RememberLabel))
		{
			HS->SetVerticalAlignment(VAlign_Center);
		}
		AddToScroll(RememberRow, 8.f);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LoginRow"));
		UButton* LoginB = MakeAccentButton(TEXT("LoginBtn"), bFirstRunTOS
			? S2011(TEXT("APBLoginScreen.Accept"), TEXT("Accept"))
			: S2011(TEXT("APBLoginScreen.Login"), TEXT("Login")), APB_BTN_OK);
		LoginB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnLoginClicked);
		if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(LoginB))
		{
			HS->SetPadding(FMargin(0.f, 12.f));
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HS->SetHorizontalAlignment(HAlign_Center);
		}
		AddToScroll(Row, 4.f);

		UHorizontalBox* Links = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LinkRow"));
		UButton* NewB = MakeAccentButton(TEXT("NewBtn"), S2011(TEXT("APBLoginScreen.NewAccount"), TEXT("New Account")), APB_BTN);
		NewB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnRegisterClicked);
		UButton* SetB = MakeAccentButton(TEXT("SettingsBtn"), S2011(TEXT("APBLoginScreen.Settings"), TEXT("Settings")), APB_BTN);
		SetB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnOpenSettings);
		if (UHorizontalBoxSlot* HS = Links->AddChildToHorizontalBox(NewB))
		{
			HS->SetPadding(FMargin(0.f, 8.f, 4.f, 0.f));
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (UHorizontalBoxSlot* HS = Links->AddChildToHorizontalBox(SetB))
		{
			HS->SetPadding(FMargin(4.f, 8.f, 0.f, 0.f));
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		AddToScroll(Links, 2.f);
		break;
	}
		case EAPBFrontendStage::CharacterSelect:
		{
			BeginStageContent(false);
			ApplyPanelChrome(true, PanelCol);
			if (TitleText)
			{
				TitleText->SetVisibility(ESlateVisibility::Visible);
				TitleText->SetText(FText::FromString(S2011(TEXT("CharacterSelectScreen.CharacterSelect"), TEXT("CHARACTER SELECT"))));
				TitleText->SetColorAndOpacity(FSlateColor(APB_WHITE));
				TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 28));
				TitleText->SetJustification(ETextJustify::Left);
			}
			if (SubtitleText)
			{
				SubtitleText->SetVisibility(ESlateVisibility::Collapsed);
				SubtitleText->SetText(FText::GetEmpty());
			}
			if (StatusText) StatusText->SetVisibility(ESlateVisibility::Visible);
			if (HintText) HintText->SetVisibility(ESlateVisibility::Collapsed);

			FString CharacterName = S2011(TEXT("CharacterSelectScreen.EmptyCharacter"), TEXT("Empty"));
			FString FactionName = TEXT("-");
			int32 ThreatRating = 0;
			bool bHas = false;
			bool bEnforcer = false;
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
				{
					const auto Snap = APB->CaptureDomainSnapshot();
					bHas = Snap.bHasCharacter;
					if (bHas)
					{
						CharacterName = Snap.CharacterName;
						bEnforcer = Snap.bEnforcer;
						FactionName = bEnforcer ? TEXT("ENFORCER") : TEXT("CRIMINAL");
						ThreatRating = FMath::Max(0, FMath::RoundToInt(Snap.ThreatPoints));
						if (StatusText)
						{
							StatusText->SetText(FText::FromString(FString::Printf(TEXT("Ready - %s"), *Snap.CharacterName)));
						}
					}
					else if (StatusText)
					{
						StatusText->SetText(FText::FromString(TEXT("No character on this account")));
					}
				}
			}
			if (TexBrandKey.Get())
			{
				UImage* BrandChip = MakeImage(TEXT("CharacterBrandChip"), TexBrandKey.Get());
				USizeBox* BrandChipSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CharacterBrandChipSize"));
				BrandChipSize->SetWidthOverride(32.f);
				BrandChipSize->SetHeightOverride(32.f);
				BrandChipSize->AddChild(BrandChip);
				AddToScroll(BrandChipSize, 2.f);
			}

			UBorder* LeftPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CharacterLeftPanel"));
			{
				FSlateBrush Brush;
				if (TexWindowPanel.Get()) Brush = APB_PanelBrush(TexWindowPanel.Get(), FLinearColor::White);
				else APB_MakeBoxBrush(Brush, APB_PANEL);
				LeftPanel->SetBrush(Brush);
				LeftPanel->SetBrushColor(TexWindowPanel.Get() ? FLinearColor::White : APB_PANEL);
				LeftPanel->SetPadding(FMargin(18.f, 14.f));
			}
			UVerticalBox* LeftV = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CharacterLeftPanelV"));
			LeftPanel->AddChild(LeftV);
			UTextBlock* CharactersHeader = MakeLabel(TEXT("CharactersHeader"), TEXT("CHARACTERS"), 15, APB_WHITE);
			CharactersHeader->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
			if (UVerticalBoxSlot* VS = LeftV->AddChildToVerticalBox(CharactersHeader)) VS->SetPadding(FMargin(2.f, 4.f, 2.f, 8.f));

			UButton* CreateB = MakeAccentButton(TEXT("CreateOpen"), S2011(TEXT("CharacterSelectScreen.CreateCharacter"), TEXT("CREATE CHARACTER")), APB_AMBER);
			CreateB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCreateCharOpen);
			if (UVerticalBoxSlot* VS = LeftV->AddChildToVerticalBox(CreateB)) VS->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

			UButton* CharacterRow = MakeAccentButton(TEXT("CharacterRow"), CharacterName, bHas ? APB_AMBER : APB_BTN);
			if (bHas && TexBtnLight.Get())
			{
				FButtonStyle Style = CharacterRow->GetStyle();
				Style.Normal = APB_TexBrush(TexBtnLight.Get(), FLinearColor::White);
				Style.Hovered = APB_TexBrush(TexBtnLight.Get(), APB_AMBER_HI);
				Style.Pressed = APB_TexBrush(TexBtnLight.Get(), APB_AMBER);
				CharacterRow->SetStyle(Style);
			}
			CharacterRow->SetIsEnabled(bHas);
			if (!bHas)
			{
				if (UTextBlock* EmptyLabel = Cast<UTextBlock>(CharacterRow->GetContent())) EmptyLabel->SetColorAndOpacity(FSlateColor(APB_MUTED));
			}
			if (UVerticalBoxSlot* VS = LeftV->AddChildToVerticalBox(CharacterRow)) VS->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

			if (bHas)
			{
				if (UTexture2D* FactionTex = bEnforcer ? TexFactionEnf.Get() : TexFactionCrim.Get())
				{
					UImage* FI = MakeImage(TEXT("CharacterFactionIcon"), FactionTex);
					USizeBox* FISz = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CS_FacIcoSz"));
					FISz->SetHeightOverride(32.f);
					FISz->SetWidthOverride(32.f);
					FISz->AddChild(FI);
					if (UVerticalBoxSlot* VS = LeftV->AddChildToVerticalBox(FISz)) VS->SetHorizontalAlignment(HAlign_Right);
				}
			}
			AddToScroll(LeftPanel, 8.f);

			UBorder* NamePlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CharacterNamePlate"));
			{
				FSlateBrush Brush;
				if (TexBtnOn.Get()) Brush = APB_TexBrush(TexBtnOn.Get(), FLinearColor::White);
				else APB_MakeBoxBrush(Brush, APB_BTN);
				NamePlate->SetBrush(Brush);
				NamePlate->SetPadding(FMargin(16.f, 10.f));
			}
			UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CharacterNameRow"));
			NamePlate->AddChild(NameRow);
			UVerticalBox* NameTextV = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CharacterNameTextV"));
			UTextBlock* NameText = MakeLabel(TEXT("CharacterName"), CharacterName.ToUpper(), 20, bHas ? APB_WHITE : APB_MUTED);
			NameText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
			NameTextV->AddChildToVerticalBox(NameText);
			NameTextV->AddChildToVerticalBox(MakeLabel(TEXT("CharacterFaction"), FString::Printf(TEXT("%s: %s"), *S2011(TEXT("CharacterSelectScreen.Faction"), TEXT("FACTION")), *FactionName), 13, APB_MUTED));
			NameTextV->AddChildToVerticalBox(MakeLabel(TEXT("CharacterThreat"), FString::Printf(TEXT("%s / %s"), *S2011(TEXT("CharacterSelectScreen.Rating"), TEXT("RATING")), *S2011(TEXT("CharacterSelectScreen.Threat"), TEXT("THREAT"))), 13, APB_MUTED));
			if (UHorizontalBoxSlot* HS = NameRow->AddChildToHorizontalBox(NameTextV)) HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			if (TexRing.Get())
			{
				UBorder* Badge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ThreatBadge"));
				Badge->SetBrush(APB_TexBrush(TexRing.Get(), FLinearColor::White));
				Badge->SetPadding(FMargin(10.f));
				UTextBlock* ThreatText = MakeLabel(TEXT("ThreatValue"), FString::FromInt(ThreatRating), 20, APB_AMBER_HI);
				ThreatText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
				ThreatText->SetJustification(ETextJustify::Center);
				Badge->AddChild(ThreatText);
				if (UHorizontalBoxSlot* HS = NameRow->AddChildToHorizontalBox(Badge))
				{
					HS->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
					HS->SetHorizontalAlignment(HAlign_Right);
					HS->SetVerticalAlignment(VAlign_Center);
				}
			}
			AddToScroll(NamePlate, 8.f);

			UButton* PlayB = MakeAccentButton(TEXT("UseExisting"), S2011(TEXT("CharacterSelectScreen.Play"), TEXT("PLAY")), APB_AMBER);
			PlayB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnSelectExistingChar);
			AddToScroll(PlayB, 8.f);
			UHorizontalBox* Bot = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CSBot"));
			UButton* Back = MakeButton(TEXT("BackLogin"), S2011(TEXT("CharacterSelectScreen.Logout"), TEXT("LOGOUT")));
			Back->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnBackToLogin);
			UButton* SetB = MakeButton(TEXT("CSSettings"), S2011(TEXT("APBLoginScreen.Settings"), TEXT("SETTINGS")));
			SetB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnOpenSettings);
			if (UHorizontalBoxSlot* HS = Bot->AddChildToHorizontalBox(Back)) { HS->SetPadding(FMargin(2.f)); HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
			if (UHorizontalBoxSlot* HS = Bot->AddChildToHorizontalBox(SetB)) { HS->SetPadding(FMargin(2.f)); HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
			AddToScroll(Bot, 12.f);
			LogStage(TEXT("char_select_ui_built"));
			break;
		}
	case EAPBFrontendStage::CharacterCreate:
	{
		BeginStageContent(true); // long form may scroll
		ApplyPanelChrome(true, PanelCol);
		if (TitleText) TitleText->SetText(FText::FromString(TEXT("CREATE CHARACTER")));
		if (SubtitleText) SubtitleText->SetText(FText::FromString(TEXT("NAME · FACTION · APPEARANCE")));
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Select Criminal or Enforcer")));
		if (HintText) HintText->SetText(FText::FromString(TEXT("Confirm returns to character select")));

		// 3D studio only — no male/female package art overlays
		CharPreviewSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CharPreviewSizeBox"));
		CharPreviewSizeBox->SetHeightOverride(280.f);
		CharPreviewSizeBox->SetWidthOverride(220.f);
		CharPreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CharPreviewImage"));
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.TintColor = FSlateColor(FLinearColor(0.02f, 0.02f, 0.03f, 1.f));
			CharPreviewImage->SetBrush(Brush);
		}
		UBorder* PrevFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PrevFrame"));
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
			PrevFrame->SetBrush(Brush);
			PrevFrame->SetPadding(FMargin(4.f));
		}
		PrevFrame->AddChild(CharPreviewImage);
		CharPreviewSizeBox->AddChild(PrevFrame);
		AddToScroll(CharPreviewSizeBox, 4.f);
		AddToScroll(MakeLabel(TEXT("Prev3DLabel"), TEXT("3D PREVIEW"), 11, APB_AMBER), 2.f);

		AddToScroll(MakeLabel(TEXT("cn"), TEXT("OPERATIVE NAME"), 12, APB_AMBER), 6.f);
		CharNameBox = MakeTextField(TEXT("CharCreateNameBox"), TEXT("Display name"), false);
		CharNameBox->SetText(FText::FromString(TEXT("Operative")));
		AddToScroll(CharNameBox, 2.f);

		AddToScroll(MakeLabel(TEXT("facTitle"), TEXT("FACTION"), 12, APB_AMBER), 10.f);
		// Icon row (stripped Criminal/Enforcer faction icons)
		UHorizontalBox* IcoRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacIcoRow"));
		FactionCrimeIcon = MakeImage(TEXT("FacCrimeIco"), TexFactionCrim);
		FactionEnfIcon = MakeImage(TEXT("FacEnfIco"), TexFactionEnfOff ? TexFactionEnfOff : TexFactionEnf);
		USizeBox* CIcoSz = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CIcoSz"));
		CIcoSz->SetHeightOverride(72.f);
		CIcoSz->SetWidthOverride(72.f);
		CIcoSz->AddChild(FactionCrimeIcon);
		USizeBox* EIcoSz = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EIcoSz"));
		EIcoSz->SetHeightOverride(72.f);
		EIcoSz->SetWidthOverride(72.f);
		EIcoSz->AddChild(FactionEnfIcon);
		if (UHorizontalBoxSlot* HS = IcoRow->AddChildToHorizontalBox(CIcoSz)) { HS->SetPadding(FMargin(8.f)); HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); HS->SetHorizontalAlignment(HAlign_Center); }
		if (UHorizontalBoxSlot* HS = IcoRow->AddChildToHorizontalBox(EIcoSz)) { HS->SetPadding(FMargin(8.f)); HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); HS->SetHorizontalAlignment(HAlign_Center); }
		AddToScroll(IcoRow, 4.f);

		UHorizontalBox* FacRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacRow"));
		FactionCriminalBtn = MakeAccentButton(TEXT("FacCrim"), TEXT("  CRIMINAL  "), FLinearColor(0.55f, 0.12f, 0.12f, 1.f));
		FactionEnforcerBtn = MakeAccentButton(TEXT("FacEnf"), TEXT("  ENFORCER  "), FLinearColor(0.12f, 0.14f, 0.18f, 1.f));
		FactionCriminalBtn->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnFactionCriminal);
		FactionEnforcerBtn->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnFactionEnforcer);
		if (UHorizontalBoxSlot* HS = FacRow->AddChildToHorizontalBox(FactionCriminalBtn))
		{
			HS->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (UHorizontalBoxSlot* HS = FacRow->AddChildToHorizontalBox(FactionEnforcerBtn))
		{
			HS->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		AddToScroll(FacRow, 4.f);
		RefreshFactionButtons();
		// Hidden checkbox kept for preview/domain path compatibility
		EnforcerCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("EnfCheck"));
		EnforcerCheck->SetVisibility(ESlateVisibility::Collapsed);
		EnforcerCheck->OnCheckStateChanged.AddDynamic(this, &UAPBFrontendWidget::OnEnforcerCheckChanged);
		AddToScroll(EnforcerCheck, 0.f);
		bCreateAsEnforcer = false;
		RefreshFactionButtons();

		AddToScroll(MakeLabel(TEXT("bh"), TEXT("Body height (0.8–1.2)"), 11, APB_MUTED), 8.f);
		BodyHeightBox = MakeTextField(TEXT("EditBodyHeight"), TEXT("1.05"), false);
		BodyHeightBox->SetText(FText::FromString(TEXT("1.05")));
		BodyHeightBox->OnTextCommitted.AddDynamic(this, &UAPBFrontendWidget::OnBodyTextCommitted);
		AddToScroll(BodyHeightBox, 2.f);
		AddToScroll(MakeLabel(TEXT("bb"), TEXT("Body build (0.8–1.2)"), 11, APB_MUTED), 6.f);
		BodyBuildBox = MakeTextField(TEXT("EditBodyBuild"), TEXT("0.95"), false);
		BodyBuildBox->SetText(FText::FromString(TEXT("0.95")));
		BodyBuildBox->OnTextCommitted.AddDynamic(this, &UAPBFrontendWidget::OnBodyTextCommitted);
		AddToScroll(BodyBuildBox, 2.f);

		auto AddSlot = [&](const TCHAR* Label, TObjectPtr<UComboBoxString>& Out, const TCHAR* Name)
		{
			AddToScroll(MakeLabel(FString(Name) + TEXT("L"), Label, 11, APB_AMBER), 4.f);
			Out = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), Name);
			Out->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnClothingSelectionChanged);
			AddToScroll(Out, 2.f);
		};
		AddSlot(TEXT("Head"), SlotHead, TEXT("SlotHead"));
		AddSlot(TEXT("Torso"), SlotTorso, TEXT("SlotTorso"));
		AddSlot(TEXT("Legs"), SlotLegs, TEXT("SlotLegs"));
		AddSlot(TEXT("Feet"), SlotFeet, TEXT("SlotFeet"));
		AddSlot(TEXT("Hands"), SlotHands, TEXT("SlotHands"));
		AddSlot(TEXT("Accessory"), SlotAccessory, TEXT("SlotAcc"));
		AddSlot(TEXT("Face"), SlotFace, TEXT("SlotFace"));
		RefreshClothingCombos();

		PreviewSummary = MakeLabel(TEXT("PrevSummary"), TEXT("3D studio ready"), 11, APB_MUTED);
		AddToScroll(PreviewSummary, 6.f);

		UButton* Confirm = MakeAccentButton(TEXT("ConfirmChar"), TEXT("  CONFIRM CHARACTER  "), FLinearColor(0.12f, 0.42f, 0.22f, 1.f));
		Confirm->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCharCreateConfirm);
		AddToScroll(Confirm, 10.f);
		UButton* Back = MakeButton(TEXT("CreateBack"), TEXT("  BACK  "));
		Back->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCharCreateBack);
		AddToScroll(Back, 6.f);
		LogStage(TEXT("char_create_ui_faction_buttons"));
		break;
	}
		case EAPBFrontendStage::DistrictSelect:
		{
			BeginStageContent(true); // district list can scroll
			ApplyPanelChrome(true, PanelCol);
			if (TitleText)
			{
				TitleText->SetVisibility(ESlateVisibility::Visible);
				TitleText->SetText(FText::FromString(S2011(TEXT("DistrictSelect_Action.DistrictSelect"), TEXT("SELECT DISTRICT"))));
				TitleText->SetColorAndOpacity(FSlateColor(APB_WHITE));
				TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 28));
				TitleText->SetJustification(ETextJustify::Left);
			}
			if (SubtitleText)
			{
				SubtitleText->SetVisibility(ESlateVisibility::Visible);
				SubtitleText->SetText(FText::FromString(S2011(TEXT("DistrictSelect_Action.Title"), TEXT("ACTION DISTRICTS"))));
				SubtitleText->SetColorAndOpacity(FSlateColor(APB_AMBER));
				SubtitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
			}
			if (HintText) HintText->SetText(FText::FromString(TEXT("Select a district, then enter San Paro")));

		DistrictIds.Reset();
		DistrictMaps.Reset();
		DistrictNames.Reset();
		DistrictCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("DistCombo"));
		DistrictCombo->SetVisibility(ESlateVisibility::Collapsed);

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
			{
				for (const FString& Row : APB->GetDistrictList())
				{
					TArray<FString> Parts;
					Row.ParseIntoArray(Parts, TEXT("|"), true);
					if (Parts.Num() < 2) continue;
					DistrictIds.Add(Parts[0]);
					DistrictNames.Add(Parts[1]);
					DistrictMaps.Add(Parts.Num() > 2 ? Parts[2] : TEXT("Lvl_APB_Financial_Freeroam"));
					DistrictCombo->AddOption(FString::Printf(TEXT("%s - %s"), *Parts[1], *Parts[0]));
					}
				}
			}

			const int32 N = FMath::Min(DistrictIds.Num(), 8);
			for (int32 i = 0; i < N; ++i)
			{
				UBorder* DistrictRow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("DistrictPanel_%d"), i));
				{
					FSlateBrush Brush;
					if (TexWindowPanel.Get()) Brush = APB_PanelBrush(TexWindowPanel.Get(), FLinearColor::White);
					else APB_MakeBoxBrush(Brush, APB_PANEL);
					DistrictRow->SetBrush(Brush);
					DistrictRow->SetBrushColor(TexWindowPanel.Get() ? FLinearColor::White : APB_PANEL);
					DistrictRow->SetPadding(FMargin(16.f, 12.f));
				}
				UHorizontalBox* DistrictRowH = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("DistrictRowH_%d"), i));
				DistrictRow->AddChild(DistrictRowH);

				UTexture2D* DistrictPhoto = nullptr;
				if (DistrictNames[i].Contains(TEXT("Financial"), ESearchCase::IgnoreCase)) DistrictPhoto = TexDistFinancial.Get();
				else if (DistrictNames[i].Contains(TEXT("Social"), ESearchCase::IgnoreCase)) DistrictPhoto = TexDistSocial.Get();
				else if (DistrictNames[i].Contains(TEXT("Waterfront"), ESearchCase::IgnoreCase)) DistrictPhoto = TexDistWaterfront.Get();
				if (DistrictPhoto)
				{
					UImage* Photo = MakeImage(FString::Printf(TEXT("DistrictPhoto_%d"), i), DistrictPhoto);
					USizeBox* PhotoSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("DistrictPhotoSize_%d"), i));
					PhotoSize->SetWidthOverride(256.f);
					PhotoSize->SetHeightOverride(195.f);
					PhotoSize->AddChild(Photo);
					if (UHorizontalBoxSlot* HS = DistrictRowH->AddChildToHorizontalBox(PhotoSize))
					{
						HS->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
						HS->SetVerticalAlignment(VAlign_Center);
					}
				}

				UVerticalBox* DistrictInfo = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("DistrictInfo_%d"), i));
				UTextBlock* DistrictName = MakeLabel(FString::Printf(TEXT("DistrictName_%d"), i), DistrictNames[i].ToUpper(), 20, APB_WHITE);
				DistrictName->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
				if (UVerticalBoxSlot* VS = DistrictInfo->AddChildToVerticalBox(DistrictName)) VS->SetPadding(FMargin(0.f, 12.f, 0.f, 4.f));
				DistrictInfo->AddChildToVerticalBox(MakeLabel(FString::Printf(TEXT("DistrictId_%d"), i), FString::Printf(TEXT("%s - ONLINE"), *DistrictIds[i]), 13, APB_MUTED));

				UButton* RowB = MakeAccentButton(FString::Printf(TEXT("DistRow_%d"), i), S2011(TEXT("DistrictSelect_Action.JoinDistrict"), TEXT("ENTER DISTRICT")), APB_AMBER);
				switch (i)
				{
				case 0: RowB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnDistrictRow0); break;
				case 1: RowB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnDistrictRow1); break;
				case 2: RowB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnDistrictRow2); break;
				case 3: RowB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnDistrictRow3); break;
				case 4: RowB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnDistrictRow4); break;
				case 5: RowB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnDistrictRow5); break;
				case 6: RowB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnDistrictRow6); break;
				case 7: RowB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnDistrictRow7); break;
				default: break;
				}
				RowB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnEnterDistrict);
				if (UVerticalBoxSlot* VS = DistrictInfo->AddChildToVerticalBox(RowB))
				{
					VS->SetPadding(FMargin(0.f, 24.f, 0.f, 8.f));
					VS->SetHorizontalAlignment(HAlign_Right);
				}
				if (UHorizontalBoxSlot* HS = DistrictRowH->AddChildToHorizontalBox(DistrictInfo))
				{
					HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					HS->SetVerticalAlignment(VAlign_Fill);
				}
				AddToScroll(DistrictRow, 10.f);
			}
			if (DistrictIds.Num() == 0)
			{
				AddToScroll(MakeLabel(TEXT("noDist"), TEXT("No districts loaded - check Content/Data/districts.json"), 12, APB_AMBER), 8.f);
				if (StatusText) StatusText->SetText(FText::FromString(TEXT("District list empty")));
			}
			else
			{
				SelectDistrictIndex(0);
			}
			DistrictCombo->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnDistrictComboChanged);
			AddToScroll(DistrictCombo, 0.f);

			UButton* Enter = MakeAccentButton(TEXT("EnterDist"), S2011(TEXT("DistrictSelect_Action.JoinDistrict"), TEXT("ENTER DISTRICT")), APB_AMBER);
			Enter->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnEnterDistrict);
			AddToScroll(Enter, 12.f);
			UHorizontalBox* DistrictFooter = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DistrictFooter"));
			UButton* BackCS = MakeButton(TEXT("DistBack"), TEXT("BACK"));
			BackCS->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCharCreateBack);
			UButton* SetB = MakeButton(TEXT("DistSettings"), S2011(TEXT("APBLoginScreen.Settings"), TEXT("SETTINGS")));
			SetB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnOpenSettings);
			if (UHorizontalBoxSlot* HS = DistrictFooter->AddChildToHorizontalBox(BackCS)) { HS->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f)); HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
			if (UHorizontalBoxSlot* HS = DistrictFooter->AddChildToHorizontalBox(SetB)) { HS->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f)); HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
			AddToScroll(DistrictFooter, 6.f);
			LogStage(TEXT("district_select_ui_built"));
			break;
		}
	case EAPBFrontendStage::Settings:
	{
		BeginStageContent(true);
		ApplyPanelChrome(true, PanelCol);
		if (LogoSizeBox) LogoSizeBox->SetVisibility(ESlateVisibility::Visible);
		if (TitleText) TitleText->SetText(FText::FromString(TEXT("SETTINGS")));
		if (SubtitleText) SubtitleText->SetText(FText::FromString(TEXT("DISPLAY · UI SCALE · MENU AUDIO")));
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Applies resolution + aspect scaling")));
		if (HintText) HintText->SetText(FText::FromString(TEXT("Fit works on 4:3 / 16:9 / 16:10")));

		// --- Resolution ---
		AddToScroll(MakeLabel(TEXT("resL"), TEXT("RESOLUTION"), 12, APB_AMBER), 4.f);
		ResolutionCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ResCombo"));
		{
			const TCHAR* Presets[] = {
				TEXT("1280 x 720  (16:9)"),
				TEXT("1280 x 960  (4:3)"),
				TEXT("1600 x 900  (16:9)"),
				TEXT("1680 x 1050 (16:10)"),
				TEXT("1920 x 1080 (16:9)"),
				TEXT("1920 x 1200 (16:10)"),
				TEXT("2560 x 1440 (16:9)"),
				TEXT("2560 x 1600 (16:10)"),
				TEXT("3840 x 2160 (16:9)"),
			};
			for (const TCHAR* P : Presets) ResolutionCombo->AddOption(P);
			ResolutionCombo->SetSelectedIndex(4); // 1080p default
			PendingResX = 1920; PendingResY = 1080;
		}
		AddToScroll(ResolutionCombo, 4.f);

		AddToScroll(MakeLabel(TEXT("modeL"), TEXT("DISPLAY MODE"), 12, APB_AMBER), 10.f);
		UHorizontalBox* ModeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ModeRow"));
		UButton* WinB = MakeAccentButton(TEXT("ModeWin"), TEXT(" Windowed "), APB_BTN);
		UButton* FsB = MakeAccentButton(TEXT("ModeFs"), TEXT(" Fullscreen "), APB_BTN);
		UButton* BlB = MakeAccentButton(TEXT("ModeBl"), TEXT(" Borderless "), APB_BTN);
		WinB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnResModeWindowed);
		FsB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnResModeFullscreen);
		BlB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnResModeBorderless);
		for (UButton* B : { WinB, FsB, BlB })
		{
			if (UHorizontalBoxSlot* HS = ModeRow->AddChildToHorizontalBox(B))
			{
				HS->SetPadding(FMargin(2.f));
				HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				}
		}
		AddToScroll(ModeRow, 4.f);

		AddToScroll(MakeLabel(TEXT("aspL"), TEXT("UI ASPECT MODE"), 12, APB_AMBER), 10.f);
		UHorizontalBox* AspRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AspRow"));
		UButton* FitB = MakeAccentButton(TEXT("AspFit"), TEXT(" Fit "), APB_BTN_OK);
		UButton* FillB = MakeAccentButton(TEXT("AspFill"), TEXT(" Fill "), APB_BTN);
		UButton* StrB = MakeAccentButton(TEXT("AspStr"), TEXT(" Stretch "), APB_BTN);
		FitB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnAspectFit);
		FillB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnAspectFill);
		StrB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnAspectStretch);
		for (UButton* B : { FitB, FillB, StrB })
		{
			if (UHorizontalBoxSlot* HS = AspRow->AddChildToHorizontalBox(B))
			{
				HS->SetPadding(FMargin(2.f));
				HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				}
				}
		AddToScroll(AspRow, 4.f);
		DisplayModeLabel = MakeLabel(TEXT("DispLbl"), TEXT(""), 11, APB_MUTED);
		RefreshResolutionLabel();
		AddToScroll(DisplayModeLabel, 6.f);

		UButton* ApplyRes = MakeAccentButton(TEXT("ApplyRes"), TEXT("  APPLY RESOLUTION  "), APB_BTN_OK);
		ApplyRes->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnResolutionApply);
		AddToScroll(ApplyRes, 8.f);

		// --- Menu audio ---
		AddToScroll(MakeLabel(TEXT("audL"), TEXT("MENU AUDIO"), 12, APB_AMBER), 12.f);
		VolumeValueText = MakeLabel(TEXT("VolVal"), TEXT(""), 14, APB_WHITE);
		RefreshVolumeLabel();
		AddToScroll(VolumeValueText, 4.f);
		MenuVolumeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("MenuVolSlider"));
		MenuVolumeSlider->SetMinValue(0.f);
		MenuVolumeSlider->SetMaxValue(1.f);
		MenuVolumeSlider->SetValue(MenuAudioVolume);
		MenuVolumeSlider->SetStepSize(0.01f);
		MenuVolumeSlider->OnValueChanged.AddDynamic(this, &UAPBFrontendWidget::OnMenuVolumeChanged);
		AddToScroll(MenuVolumeSlider, 4.f);

		UButton* Back = MakeAccentButton(TEXT("SettingsBack"), TEXT("  BACK  "), APB_BTN);
		Back->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnSettingsBack);
		AddToScroll(Back, 12.f);
		LogStage(FString::Printf(TEXT("settings_ui vol=%.2f scale=%d"), MenuAudioVolume, (int32)UiScaleMode));
		break;
		}
	case EAPBFrontendStage::Loading:
	{
		BeginStageContent(false);
		ApplyPanelChrome(true, PanelCol);
		if (TitleText) TitleText->SetText(FText::FromString(TEXT("ENTERING DISTRICT")));
		if (SubtitleText) SubtitleText->SetText(FText::FromString(SelectedDistrictId));
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Streaming San Paro…")));
		AddToScroll(MakeLabel(TEXT("load"), TEXT("Please wait"), 16, APB_AMBER), 8.f);
		break;
	}
	default: break;
	}
}

void UAPBFrontendWidget::OnDistrictComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!DistrictCombo) return;
	const int32 Idx = DistrictCombo->GetSelectedIndex();
	if (DistrictIds.IsValidIndex(Idx))
	{
		SelectedDistrictId = DistrictIds[Idx];
		SelectedDistrictMap = DistrictMaps.IsValidIndex(Idx) ? DistrictMaps[Idx] : TEXT("Lvl_APB_Financial_Freeroam");
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("Selected: %s"), *SelectedDistrictId)));
		}
		LogStage(FString::Printf(TEXT("selected=%s"), *SelectedDistrictId));
	}
}

void UAPBFrontendWidget::RefreshClothingCombos()
{
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	bSuppressPreviewRefresh = true;
	auto Fill = [&](UComboBoxString* Box, const FString& ClothSlot)
	{
		if (!Box) return;
		Box->ClearOptions();
		for (const FAPBClothingChoice& C : APB->GetClothingForSlot(ClothSlot, 24))
		{
			Box->AddOption(FString::Printf(TEXT("%s | %s"), *C.Id, *C.Name));
		}
		if (Box->GetOptionCount() > 0) Box->SetSelectedIndex(0);
	};
	Fill(SlotHead, TEXT("head"));
	Fill(SlotTorso, TEXT("torso"));
	Fill(SlotLegs, TEXT("legs"));
	Fill(SlotFeet, TEXT("feet"));
	Fill(SlotHands, TEXT("hands"));
	Fill(SlotAccessory, TEXT("accessory"));
	Fill(SlotFace, TEXT("face"));
	bSuppressPreviewRefresh = false;
}

void UAPBFrontendWidget::OnSplashContinue() { SetStage(EAPBFrontendStage::Login); }
void UAPBFrontendWidget::OnBackToLogin() { SetStage(EAPBFrontendStage::Login); }
void UAPBFrontendWidget::OnCharCreateBack() { SetStage(EAPBFrontendStage::CharacterSelect); }

void UAPBFrontendWidget::SetLoginCredentials(const FString& User, const FString& Pass)
{
	if (UserBox) UserBox->SetText(FText::FromString(User));
	if (PassBox) PassBox->SetText(FText::FromString(Pass));
}

void UAPBFrontendWidget::OnLoginClicked()
{
	PlayUiSfx(TEXT("UI_Click"));
	// Classic gate: credentials only — no auto-register (use CREATE NEW ACCOUNT).
	const FString User = UserBox ? UserBox->GetText().ToString().TrimStartAndEnd() : FString();
	const FString Pass = PassBox ? PassBox->GetText().ToString() : FString();
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	if (User.IsEmpty() || Pass.IsEmpty())
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Enter email and password")));
		LogStage(TEXT("login_fail_empty"));
		return;
	}
	if (!APB->Login(User, Pass))
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Login failed — wrong password or unknown account. Use CREATE NEW ACCOUNT.")));
		LogStage(TEXT("login_fail"));
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend login_fail user=%s"), *User);
		return;
	}
	bFirstRunTOS = false;
	APB->EnterWorld(TEXT("W1"));
	LogStage(TEXT("login_ok"));
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend login_ok user=%s"), *User);
	SetStage(EAPBFrontendStage::CharacterSelect);
}

void UAPBFrontendWidget::OnRegisterClicked()
{
	const FString User = UserBox ? UserBox->GetText().ToString().TrimStartAndEnd() : FString();
	const FString Pass = PassBox ? PassBox->GetText().ToString() : FString();
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	if (User.IsEmpty() || Pass.IsEmpty())
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Enter email and password to register")));
		LogStage(TEXT("register_fail_empty"));
		return;
	}
	if (APB->RegisterAccount(User, Pass) && APB->Login(User, Pass))
	{
		APB->EnterWorld(TEXT("W1"));
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Account created")));
		LogStage(TEXT("register_ok"));
		SetStage(EAPBFrontendStage::CharacterSelect);
	}
	else
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Register failed — account may already exist; try LOGIN")));
		LogStage(TEXT("register_fail"));
	}
}

void UAPBFrontendWidget::OnCreateCharOpen() { SetStage(EAPBFrontendStage::CharacterCreate); }

void UAPBFrontendWidget::OnSelectExistingChar()
{
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	if (!APB->CaptureDomainSnapshot().bHasCharacter)
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("No character — create one first")));
		LogStage(TEXT("char_select_empty"));
		return;
	}
	LogStage(TEXT("char_select_ok"));
	SetStage(EAPBFrontendStage::DistrictSelect);
}

void UAPBFrontendWidget::OnOpenSettings()
{
	StageBeforeSettings = CurrentStage;
	if (StageBeforeSettings == EAPBFrontendStage::Settings)
	{
		StageBeforeSettings = EAPBFrontendStage::Login;
	}
	SetStage(EAPBFrontendStage::Settings);
}

void UAPBFrontendWidget::OnSettingsBack()
{
	SetStage(StageBeforeSettings);
}

void UAPBFrontendWidget::OnExitDesktop()
{
	LogStage(TEXT("exit_desktop"));
	FGenericPlatformMisc::RequestExit(false);
}

void UAPBFrontendWidget::OnFactionCriminal()
{
	bCreateAsEnforcer = false;
	RefreshFactionButtons();
	RefreshCharacterPreviewFromUI();
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("Faction: CRIMINAL")));
	// Switch to Criminal AI faction bed
	if (CurrentStage == EAPBFrontendStage::CharacterCreate)
	{
		ApplyStageBackgroundVideo(EAPBFrontendStage::CharacterCreate);
	}
}

void UAPBFrontendWidget::OnFactionEnforcer()
{
	bCreateAsEnforcer = true;
	RefreshFactionButtons();
	RefreshCharacterPreviewFromUI();
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("Faction: ENFORCER")));
	if (CurrentStage == EAPBFrontendStage::CharacterCreate)
	{
		ApplyStageBackgroundVideo(EAPBFrontendStage::CharacterCreate);
	}
}

void UAPBFrontendWidget::OnMenuVolumeChanged(float Value)
{
	SetMenuAudioVolume(Value);
	RefreshVolumeLabel();
	LogStage(FString::Printf(TEXT("menu_vol=%.2f"), MenuAudioVolume));
}

void UAPBFrontendWidget::OnVolPresetMute() { SetMenuAudioVolume(0.f); RefreshVolumeLabel(); if (MenuVolumeSlider) MenuVolumeSlider->SetValue(0.f); }
void UAPBFrontendWidget::OnVolPresetLow() { SetMenuAudioVolume(0.25f); RefreshVolumeLabel(); if (MenuVolumeSlider) MenuVolumeSlider->SetValue(0.25f); }
void UAPBFrontendWidget::OnVolPresetMed() { SetMenuAudioVolume(0.55f); RefreshVolumeLabel(); if (MenuVolumeSlider) MenuVolumeSlider->SetValue(0.55f); }
void UAPBFrontendWidget::OnVolPresetHigh() { SetMenuAudioVolume(1.f); RefreshVolumeLabel(); if (MenuVolumeSlider) MenuVolumeSlider->SetValue(1.f); }

void UAPBFrontendWidget::RefreshResolutionLabel()
{
	const TCHAR* Mode =
		(DisplayMode == 1) ? TEXT("Fullscreen") :
		(DisplayMode == 2) ? TEXT("Borderless") : TEXT("Windowed");
	const TCHAR* Asp =
		(UiScaleMode == EAPBUiScaleMode::Fill) ? TEXT("Fill") :
		(UiScaleMode == EAPBUiScaleMode::Stretch) ? TEXT("Stretch") : TEXT("Fit");
	if (DisplayModeLabel)
	{
		DisplayModeLabel->SetText(FText::FromString(FString::Printf(
			TEXT("Pending: %dx%d  |  %s  |  UI %s  (FPS cap 60)"),
			PendingResX, PendingResY, Mode, Asp)));
	}
}

void UAPBFrontendWidget::OnResModeWindowed() { DisplayMode = 0; RefreshResolutionLabel(); }
void UAPBFrontendWidget::OnResModeFullscreen() { DisplayMode = 1; RefreshResolutionLabel(); }
void UAPBFrontendWidget::OnResModeBorderless() { DisplayMode = 2; RefreshResolutionLabel(); }
void UAPBFrontendWidget::OnAspectFit() { UiScaleMode = EAPBUiScaleMode::Fit; LastViewport = FVector2D::ZeroVector; UpdateViewportScale(); RefreshResolutionLabel(); }
void UAPBFrontendWidget::OnAspectFill() { UiScaleMode = EAPBUiScaleMode::Fill; LastViewport = FVector2D::ZeroVector; UpdateViewportScale(); RefreshResolutionLabel(); }
void UAPBFrontendWidget::OnAspectStretch() { UiScaleMode = EAPBUiScaleMode::Stretch; LastViewport = FVector2D::ZeroVector; UpdateViewportScale(); RefreshResolutionLabel(); }

void UAPBFrontendWidget::OnResolutionApply()
{
	// Parse combo "1920 x 1080  (16:9)"
	if (ResolutionCombo)
	{
		const FString Opt = ResolutionCombo->GetSelectedOption();
		int32 X = 0, Y = 0;
		// find "N x M"
		FString Left, Right;
		if (Opt.Split(TEXT(" x "), &Left, &Right))
		{
			X = FCString::Atoi(*Left.TrimStartAndEnd());
			// Right starts with height then spaces
			Y = FCString::Atoi(*Right.TrimStartAndEnd());
		}
		if (X >= 640 && Y >= 480)
		{
			PendingResX = X;
			PendingResY = Y;
		}
	}
	ApplyDisplaySettings();
	RefreshResolutionLabel();
	LastViewport = FVector2D::ZeroVector;
	UpdateViewportScale();
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("Applied %dx%d"), PendingResX, PendingResY)));
	}
	LogStage(FString::Printf(TEXT("res_apply %dx%d mode=%d asp=%d"), PendingResX, PendingResY, DisplayMode, (int32)UiScaleMode));
}

void UAPBFrontendWidget::ApplyDisplaySettings()
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		// Fallback console
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->ConsoleCommand(*FString::Printf(TEXT("r.SetRes %dx%dw"), PendingResX, PendingResY));
		}
		return;
	}
	Settings->SetScreenResolution(FIntPoint(PendingResX, PendingResY));
	switch (DisplayMode)
	{
	case 1:
		Settings->SetFullscreenMode(EWindowMode::Fullscreen);
		break;
	case 2:
		Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
		break;
	default:
		Settings->SetFullscreenMode(EWindowMode::Windowed);
		break;
	}
	Settings->SetFrameRateLimit(60.f);
	Settings->ApplySettings(false);
	Settings->SaveSettings();
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend ApplyDisplay %dx%d mode=%d"), PendingResX, PendingResY, DisplayMode);
}

void UAPBFrontendWidget::OnDistrictRow0() { SelectDistrictIndex(0); }
void UAPBFrontendWidget::OnDistrictRow1() { SelectDistrictIndex(1); }
void UAPBFrontendWidget::OnDistrictRow2() { SelectDistrictIndex(2); }
void UAPBFrontendWidget::OnDistrictRow3() { SelectDistrictIndex(3); }
void UAPBFrontendWidget::OnDistrictRow4() { SelectDistrictIndex(4); }
void UAPBFrontendWidget::OnDistrictRow5() { SelectDistrictIndex(5); }
void UAPBFrontendWidget::OnDistrictRow6() { SelectDistrictIndex(6); }
void UAPBFrontendWidget::OnDistrictRow7() { SelectDistrictIndex(7); }

static FString ComboId(UComboBoxString* Box)
{
	if (!Box) return FString();
	const FString Opt = Box->GetSelectedOption();
	int32 Pipe = INDEX_NONE;
	if (Opt.FindChar(TEXT('|'), Pipe)) return Opt.Left(Pipe).TrimStartAndEnd();
	return Opt;
}

void UAPBFrontendWidget::EnsureCharacterPreview()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (CharPreviewActor && IsValid(CharPreviewActor))
	{
		BindPreviewImageToRT();
		return;
	}
	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Sp.ObjectFlags |= RF_Transient;
	CharPreviewActor = World->SpawnActor<AAPBCharacterCreatePreviewActor>(
		AAPBCharacterCreatePreviewActor::StaticClass(),
		FVector(0.f, 0.f, -50000.f), FRotator::ZeroRotator, Sp);
	if (CharPreviewActor)
	{
		BindPreviewImageToRT();
		LogStage(FString::Printf(TEXT("preview_spawn mesh=%s"), *CharPreviewActor->GetLastMeshPath()));
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend PREVIEW_SPAWN ok mesh=%s"), *CharPreviewActor->GetLastMeshPath());
	}
	else
	{
		LogStage(TEXT("preview_spawn_fail"));
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend PREVIEW_SPAWN fail"));
	}
}

void UAPBFrontendWidget::DestroyCharacterPreview()
{
	if (CharPreviewActor && IsValid(CharPreviewActor))
	{
		CharPreviewActor->Destroy();
	}
	CharPreviewActor = nullptr;
}

void UAPBFrontendWidget::BindPreviewImageToRT()
{
	if (!CharPreviewImage || !CharPreviewActor) return;
	if (UTextureRenderTarget2D* RT = CharPreviewActor->GetRenderTarget())
	{
		CharPreviewImage->SetBrushFromTexture(nullptr);
		// Bind RT as dynamic brush
		FSlateBrush Brush;
		Brush.SetResourceObject(RT);
		Brush.ImageSize = FVector2D(512.f, 640.f);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		CharPreviewImage->SetBrush(Brush);
		CharPreviewActor->CaptureNow();
	}
}

void UAPBFrontendWidget::RefreshCharacterPreviewFromUI()
{
	EnsureCharacterPreview();
	if (!CharPreviewActor) return;

	const bool bEnf = bCreateAsEnforcer || (EnforcerCheck && EnforcerCheck->IsChecked());
	const FString MeshPath = CharPreviewActor->ApplyBaseMesh(bEnf);

	float Height = 1.05f, Bulk = 0.95f;
	if (BodyHeightBox) Height = FCString::Atof(*BodyHeightBox->GetText().ToString());
	if (BodyBuildBox) Bulk = FCString::Atof(*BodyBuildBox->GetText().ToString());
	CharPreviewActor->ApplyBodyProfile(Height, Bulk);

	struct Pair { const TCHAR* Slot; UComboBoxString* Box; };
	const Pair Pairs[] = {
		{ TEXT("head"), SlotHead }, { TEXT("torso"), SlotTorso }, { TEXT("legs"), SlotLegs },
		{ TEXT("feet"), SlotFeet }, { TEXT("hands"), SlotHands }, { TEXT("accessory"), SlotAccessory },
		{ TEXT("face"), SlotFace },
	};
	int32 Bound = 0;
	for (const Pair& P : Pairs)
	{
		const FString Id = ComboId(P.Box);
		if (!Id.IsEmpty() && CharPreviewActor->ApplyClothingSlotVisual(P.Slot, Id))
		{
			++Bound;
		}
	}
	BindPreviewImageToRT();
	const FString Line = FString::Printf(
		TEXT("PREVIEW_OK mesh=%s slots_bound=%d height=%.2f build=%.2f enf=%d"),
		*MeshPath, Bound, Height, Bulk, bEnf ? 1 : 0);
	if (PreviewSummary) PreviewSummary->SetText(FText::FromString(Line));
	LogStage(Line);
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend %s"), *Line);
}

void UAPBFrontendWidget::OnClothingSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bSuppressPreviewRefresh) return;
	if (CurrentStage != EAPBFrontendStage::CharacterCreate) return;
	// Domain equip for the changed selection set
	if (UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr)
	{
		struct Pair { const TCHAR* Slot; UComboBoxString* Box; };
		const Pair Pairs[] = {
			{ TEXT("head"), SlotHead }, { TEXT("torso"), SlotTorso }, { TEXT("legs"), SlotLegs },
			{ TEXT("feet"), SlotFeet }, { TEXT("hands"), SlotHands }, { TEXT("accessory"), SlotAccessory },
			{ TEXT("face"), SlotFace },
		};
		for (const Pair& P : Pairs)
		{
			const FString Id = ComboId(P.Box);
			if (!Id.IsEmpty()) APB->EquipClothingItem(P.Slot, Id);
		}
	}
	RefreshCharacterPreviewFromUI();
}

void UAPBFrontendWidget::OnEnforcerCheckChanged(bool bIsChecked)
{
	if (CurrentStage != EAPBFrontendStage::CharacterCreate) return;
	bCreateAsEnforcer = bIsChecked;
	RefreshFactionButtons();
	RefreshCharacterPreviewFromUI();
}

void UAPBFrontendWidget::OnBodyTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CurrentStage != EAPBFrontendStage::CharacterCreate) return;
	if (UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr)
	{
		float Height = 1.f, Bulk = 1.f;
		if (BodyHeightBox) Height = FCString::Atof(*BodyHeightBox->GetText().ToString());
		if (BodyBuildBox) Bulk = FCString::Atof(*BodyBuildBox->GetText().ToString());
		APB->ApplyBodyProfile(FMath::Clamp(Height, 0.8f, 1.2f), FMath::Clamp(Bulk, 0.8f, 1.2f), 1, 1);
	}
	RefreshCharacterPreviewFromUI();
}

void UAPBFrontendWidget::OnPreviewRefreshClicked()
{
	RefreshCharacterPreviewFromUI();
}

void UAPBFrontendWidget::ApplyAppearanceFromEditor()
{
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	float Height = 1.0f, Bulk = 1.0f;
	if (BodyHeightBox) Height = FCString::Atof(*BodyHeightBox->GetText().ToString());
	if (BodyBuildBox) Bulk = FCString::Atof(*BodyBuildBox->GetText().ToString());
	Height = FMath::Clamp(Height, 0.8f, 1.2f);
	Bulk = FMath::Clamp(Bulk, 0.8f, 1.2f);
	APB->ApplyBodyProfile(Height, Bulk, 1, 1);
	LogStage(FString::Printf(TEXT("body height=%.3f bulk=%.3f"), Height, Bulk));
	struct Pair { const TCHAR* Slot; UComboBoxString* Box; };
	const Pair Pairs[] = {
		{ TEXT("head"), SlotHead }, { TEXT("torso"), SlotTorso }, { TEXT("legs"), SlotLegs },
		{ TEXT("feet"), SlotFeet }, { TEXT("hands"), SlotHands }, { TEXT("accessory"), SlotAccessory },
		{ TEXT("face"), SlotFace },
	};
	FString Summary = FString::Printf(TEXT("body H=%.2f B=%.2f;"), Height, Bulk);
	for (const Pair& P : Pairs)
	{
		const FString Id = ComboId(P.Box);
		if (!Id.IsEmpty())
		{
			APB->EquipClothingItem(P.Slot, Id);
			Summary += FString::Printf(TEXT("%s=%s;"), P.Slot, *Id);
		}
	}
	if (PreviewSummary) PreviewSummary->SetText(FText::FromString(TEXT("Equipped: ") + Summary));
	LogStage(TEXT("appearance=") + Summary);
	RefreshCharacterPreviewFromUI();
}

void UAPBFrontendWidget::OnCharCreateConfirm()
{
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	const FString Name = CharNameBox ? CharNameBox->GetText().ToString().TrimStartAndEnd() : TEXT("Operative");
	if (Name.IsEmpty())
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Enter a character name")));
		return;
	}
	const bool bEnf = bCreateAsEnforcer;
	if (!APB->CreateCharacter(Name, bEnf))
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("CreateCharacter failed")));
		LogStage(TEXT("char_create_fail"));
		return;
	}
	ApplyAppearanceFromEditor();
	APB->PushDomainSnapshotToAllPlayerStates();
	float H = 0.f, B = 0.f;
	APB->GetBodyProfile(H, B);
	LogStage(FString::Printf(TEXT("char_create name=%s enf=%d bodyH=%.3f bodyB=%.3f"), *Name, bEnf ? 1 : 0, H, B));
	// Classic flow: return to select so the new character is visible
	SetStage(EAPBFrontendStage::CharacterSelect);
}

void UAPBFrontendWidget::OnEnterDistrict()
{
	if (SelectedDistrictId.IsEmpty())
	{
		SelectedDistrictId = TEXT("Financial");
		SelectedDistrictMap = TEXT("Lvl_APB_Financial_Freeroam");
	}
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	if (!APB->CaptureDomainSnapshot().bHasCharacter)
	{
		APB->CreateCharacter(TEXT("Operative"), false);
	}
	if (!APB->JoinDistrict(SelectedDistrictId))
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("JoinDistrict failed")));
		LogStage(TEXT("join_fail"));
		return;
	}
	APB->PushDomainSnapshotToAllPlayerStates();
	StopLoginMusic();
	SetStage(EAPBFrontendStage::Loading);
	LogStage(FString::Printf(TEXT("travel=%s map=%s"), *SelectedDistrictId, *SelectedDistrictMap));
	FString MapName = SelectedDistrictMap;
	if (MapName.IsEmpty()) MapName = TEXT("Lvl_APB_Financial_Freeroam");
	const FString Opts = TEXT("listen?game=/Script/APBReloaded.APBFreeroamGameMode");
	UGameplayStatics::OpenLevel(this, FName(*MapName), true, Opts);
}
