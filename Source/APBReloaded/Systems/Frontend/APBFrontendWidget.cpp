#include "APBFrontendWidget.h"
#include "APBFrontendLayoutMath.h"
#include "APBGameInstanceSubsystem.h"
#include "APBFrontendHUD.h"
#include "APBCharacterCreatePreviewActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ButtonSlot.h"
#include "Components/SizeBox.h"
#include "Components/BorderSlot.h"
#include "Components/ScrollBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "APBFrontendSceneData.h"
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
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "HAL/PlatformFilemanager.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundBase.h"
#include "APBVerifiedAssetRegistry.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "MediaSoundComponent.h"
#include "HAL/FileManager.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "APBPlayerState.h"

// 2011 RTW GameFlow palette (menu2011_spec §2.3): monochrome greys + single amber accent.
// #FFC254 is the Menu_Button_Light selection amber — replaces the old cyan identity.
static const FLinearColor APB_PAPER(0.f, 0.f, 0.f, 1.f);
static const FLinearColor APB_BG(0.f, 0.f, 0.f, 1.f);
static const FLinearColor APB_PANEL = FLinearColor::FromSRGBColor(FColor(0x4F, 0x4F, 0x4F, 245));      // #4F4F4F MessageBox_BG
static const FLinearColor APB_PANEL_EDGE = FLinearColor::FromSRGBColor(FColor(0xFF, 0xC2, 0x54, 255)); // #FFC254 amber frame
static const FLinearColor APB_FIELD(0.08f, 0.09f, 0.10f, 1.f);
static const FLinearColor APB_FIELD_FOCUS(0.10f, 0.10f, 0.11f, 1.f);
static const FLinearColor APB_AMBER = FLinearColor::FromSRGBColor(FColor(0xFF, 0xC2, 0x54, 255));    // #FFC254
static const FLinearColor APB_AMBER_HI = FLinearColor::FromSRGBColor(FColor(0xFF, 0xFB, 0x9C, 255)); // #FFFB9C
static const FLinearColor APB_WHITE(0.96f, 0.97f, 0.98f, 1.f);
static const FLinearColor APB_MUTED(0.62f, 0.66f, 0.70f, 1.f);
static const FLinearColor APB_BTN(0.f, 0.f, 0.f, 1.f);
static const FLinearColor APB_BTN_HOVER(0.34f, 0.28f, 0.16f, 1.f);
static const FLinearColor APB_BTN_OK(0.42f, 0.30f, 0.10f, 1.f);       // amber-metal primary
static const FLinearColor APB_BTN_DANGER(0.28f, 0.14f, 0.14f, 1.f);
static const FLinearColor APB_CRIM(0.72f, 0.12f, 0.14f, 1.f);
static const FLinearColor APB_ENF(0.12f, 0.32f, 0.62f, 1.f);
static const FLinearColor APB_INK = FLinearColor::FromSRGBColor(FColor(0xE5, 0xE0, 0xD5, 235));
static const FLinearColor APB_INK_SEC = FLinearColor::FromSRGBColor(FColor(0xCF, 0xCA, 0xBE, 176));
static const FLinearColor APB_INK_DARK = FLinearColor::FromSRGBColor(FColor(0x1A, 0x1A, 0x18, 255));
static const FLinearColor APB_FLAT(0.043f, 0.047f, 0.051f, 0.82f);
static const FLinearColor APB_FLAT_HOVER(0.12f, 0.125f, 0.13f, 0.90f);
static const FLinearColor APB_FLAT_PRIMARY(0.16f, 0.13f, 0.06f, 0.86f);
static const FLinearColor APB_PANEL_DARK(0.039f, 0.047f, 0.043f, 0.72f);
static const FLinearColor APB_WELL_DARK(0.012f, 0.016f, 0.016f, 0.78f);
static const FLinearColor APB_FIELD_LIGHT = FLinearColor::FromSRGBColor(FColor(0xCF, 0xCF, 0xCB, 255));
static const FLinearColor APB_HAIR_AMBER = FLinearColor::FromSRGBColor(FColor(0xFF, 0xC2, 0x54, 97));

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

static FSlateBrush APB_TiledTexBrush(UTexture2D* Tex, const FLinearColor& Tint)
{
	FSlateBrush Brush = APB_TexBrush(Tex, Tint);
	Brush.Tiling = ESlateBrushTileType::Both;
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
	// Flat black/charcoal rectangles (2011 Menu_Button_* plates read as noise here).
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);
	FButtonStyle Style = B->GetStyle();
	const float MaxC = FMath::Max3(NormalTint.R, NormalTint.G, NormalTint.B);
	const FLinearColor Hover = MaxC < 0.25f
		? FLinearColor(0.10f, 0.10f, 0.10f, 1.f)
		: FLinearColor(
			FMath::Min(1.f, NormalTint.R + 0.12f),
			FMath::Min(1.f, NormalTint.G + 0.22f),
			FMath::Min(1.f, NormalTint.B + 0.28f),
			1.f);
	const FLinearColor Press = NormalTint * FLinearColor(0.55f, 0.55f, 0.55f, 1.f);
	APB_MakeBoxBrush(Style.Normal, NormalTint);
	APB_MakeBoxBrush(Style.Hovered, Hover);
	APB_MakeBoxBrush(Style.Pressed, Press);
	APB_MakeBoxBrush(Style.Disabled, NormalTint * 0.5f);
	Style.NormalPadding = FMargin(14.f, 7.f);
	Style.PressedPadding = FMargin(14.f, 7.f);
	B->SetStyle(Style);
	B->SetBackgroundColor(NormalTint);
	B->OnHovered.AddDynamic(this, &UAPBFrontendWidget::OnAnyHover);
	UTextBlock* L = MakeLabel(Name + TEXT("_L"), Label.ToUpper(), 12, APB_WHITE);
	L->SetJustification(ETextJustify::Center);
	B->AddChild(L);
	return B;
}

UButton* UAPBFrontendWidget::MakeFlatButton(const FString& Name, const FString& Label, bool bPrimary, int32 FontSize)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);
	FButtonStyle Style = B->GetStyle();
	// Flat black rectangles (2011 Menu_Button_* plates read as noise here).
	const FLinearColor Base = bPrimary ? APB_FLAT_PRIMARY : FLinearColor(0.f, 0.f, 0.f, 1.f);
	APB_MakeBoxBrush(Style.Normal, Base);
	APB_MakeBoxBrush(Style.Hovered, bPrimary ? FLinearColor(0.26f, 0.20f, 0.08f, 0.94f) : FLinearColor(0.05f, 0.05f, 0.05f, 1.f));
	APB_MakeBoxBrush(Style.Pressed, Base * FLinearColor(0.7f, 0.7f, 0.7f, 1.f));
	APB_MakeBoxBrush(Style.Disabled, Base * FLinearColor(0.5f, 0.5f, 0.5f, 0.6f));
	// Small fonts sit in short rects (footer 2x2 is 18px pre-scale); shrink vpad so glyphs don't clip.
	const float VPad = FontSize <= 9 ? 1.f : 3.f;
	Style.NormalPadding = FMargin(9.f, VPad);
	Style.PressedPadding = FMargin(9.f, VPad);
	B->SetStyle(Style);
	B->OnHovered.AddDynamic(this, &UAPBFrontendWidget::OnAnyHover);
	const bool bLoginButton = Name.StartsWith(TEXT("Login"));
	UTextBlock* L = MakeLabel(Name + TEXT("_L"), Label, FontSize,
		bLoginButton ? APB_WHITE : (bPrimary ? APB_AMBER : APB_WHITE));
	L->SetJustification(ETextJustify::Center);
	L->SetAutoWrapText(false);
	B->AddChild(L);
	// Center the glyph in the button's full height: the default UButtonSlot
	// padding (4,2) on top of the style padding pushes text off the middle.
	if (UButtonSlot* BSL = Cast<UButtonSlot>(L->Slot))
	{
		BSL->SetHorizontalAlignment(HAlign_Center);
		BSL->SetVerticalAlignment(VAlign_Center);
		BSL->SetPadding(FMargin(0.f));
	}
	return B;
}

UButton* UAPBFrontendWidget::MakeLinkButton(const FString& Name, const FString& Label)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);
	FButtonStyle Style = B->GetStyle();
	APB_MakeBoxBrush(Style.Normal, FLinearColor(0.f, 0.f, 0.f, 0.01f));
	APB_MakeBoxBrush(Style.Hovered, FLinearColor(1.f, 1.f, 1.f, 0.06f));
	APB_MakeBoxBrush(Style.Pressed, FLinearColor(0.f, 0.f, 0.f, 0.12f));
	APB_MakeBoxBrush(Style.Disabled, FLinearColor(0.f, 0.f, 0.f, 0.01f));
	Style.NormalPadding = FMargin(4.f, 0.f);
	Style.PressedPadding = FMargin(4.f, 0.f);
	B->SetStyle(Style);
	B->OnHovered.AddDynamic(this, &UAPBFrontendWidget::OnAnyHover);
	UTextBlock* L = MakeLabel(Name + TEXT("_L"), Label, 9, APB_INK_SEC);
	L->SetJustification(ETextJustify::Center);
	L->SetAutoWrapText(false);
	B->AddChild(L);
	if (UButtonSlot* BSL = Cast<UButtonSlot>(L->Slot))
	{
		BSL->SetHorizontalAlignment(HAlign_Center);
		BSL->SetVerticalAlignment(VAlign_Center);
		BSL->SetPadding(FMargin(0.f));
	}
	return B;
}

UEditableTextBox* UAPBFrontendWidget::MakeTextField(const FString& Name, const FString& Hint, bool bPassword, bool bLight)
{
	UEditableTextBox* Box = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), *Name);
	Box->SetHintText(FText::FromString(Hint));
	Box->SetIsPassword(bPassword);
	const FLinearColor Fg = bLight ? APB_INK_DARK : APB_WHITE;
	Box->SetForegroundColor(Fg);
	FEditableTextBoxStyle Style = Box->WidgetStyle;
	if (bLight)
	{
		APB_MakeBoxBrush(Style.BackgroundImageNormal, APB_FIELD_LIGHT);
		APB_MakeBoxBrush(Style.BackgroundImageHovered, APB_FIELD_LIGHT * FLinearColor(1.02f, 1.02f, 1.02f, 1.f));
		APB_MakeBoxBrush(Style.BackgroundImageFocused, FLinearColor::White);
		APB_MakeBoxBrush(Style.BackgroundImageReadOnly, APB_FIELD_LIGHT * FLinearColor(0.85f, 0.85f, 0.85f, 1.f));
	}
	else
	{
		// Straight gray fields — no APB_BG_TextEntry 9-slice texture.
		APB_MakeBoxBrush(Style.BackgroundImageNormal, APB_FIELD);
		APB_MakeBoxBrush(Style.BackgroundImageHovered, APB_FIELD + FLinearColor(0.03f, 0.03f, 0.04f, 0.f));
		APB_MakeBoxBrush(Style.BackgroundImageFocused, APB_FIELD_FOCUS);
		APB_MakeBoxBrush(Style.BackgroundImageReadOnly, APB_FIELD * 0.8f);
	}
	// LoginRects edit boxes are 19px tall in design space; font 10 + zero vertical
	// padding is required to keep glyphs inside the box instead of overflowing.
	Style.Padding = FMargin(8.f, 0.f);
	Style.ForegroundColor = FSlateColor(Fg);
	Style.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
	Box->WidgetStyle = Style;
	Box->SetJustification(ETextJustify::Left);
	return Box;
}

/** Load a staged 2011 menu texture from /Game/Imported/UI/Menu2011/<Sub>/<Name>. */
UTexture2D* UAPBFrontendWidget::LoadMenu2011Tex(const TCHAR* Sub, const TCHAR* Name) const
{
	const FString Path = FString::Printf(TEXT("/Game/Imported/UI/Menu2011/%s/%s.%s"), Sub, Name, Name);
	UGameInstance* GI = GetGameInstance();
	UAPBVerifiedAssetRegistry* Registry = GI ? GI->GetSubsystem<UAPBVerifiedAssetRegistry>() : nullptr;
	// Menu stages are 2011-sourced: the registry rejects a retail-sourced match.
	UTexture2D* T = Registry ? Registry->LoadTexture2D(GetWorld(), Path, TEXT("frontend_menu_2011_texture"), TEXT("2011")) : nullptr;
	if (!T) UE_LOG(LogTemp, Warning, TEXT("APBFrontend TEX blocked %s"), *Path);
	return T;
}

void UAPBFrontendWidget::LoadMenu2011Assets()
{
	if (bLobbyChromeLoaded) return;
	bLobbyChromeLoaded = true;

	// Branding / splash
	TexLogo = LoadMenu2011Tex(TEXT("Loading"), TEXT("LoadingScreen_APB"));
	TexSplash = LoadMenu2011Tex(TEXT("Loading"), TEXT("ApbReborn2"));
	// Live Login background is the authored movie; NewBackgroundImage is not a runtime asset.
	TexGraffiti = nullptr;
	// Faction icons (kept for character create/select plates)
	TexFactionCrim = LoadMenu2011Tex(TEXT("CharSelect"), TEXT("CriminalFactionicon"));
	TexFactionCrimOff = LoadMenu2011Tex(TEXT("CharSelect"), TEXT("CriminalFactionicon_Unselected"));
	TexFactionEnf = LoadMenu2011Tex(TEXT("CharSelect"), TEXT("EnforcerFactionicon"));
	TexFactionEnfOff = LoadMenu2011Tex(TEXT("CharSelect"), TEXT("EnforcerFactionicon_Unselected"));
	TexCharacterSelectIcon = LoadMenu2011Tex(TEXT("CharSelect"), TEXT("CharacterSelectIcon"));
	// Window chrome + controls
	TexWindowPanel = LoadMenu2011Tex(TEXT("Chrome"), TEXT("MessageBox_BG"));
	TexBtnOn = LoadMenu2011Tex(TEXT("Chrome"), TEXT("Menu_Button_On"));
	TexBtnOff = LoadMenu2011Tex(TEXT("Chrome"), TEXT("Menu_Button_Off"));
	TexBtnLight = LoadMenu2011Tex(TEXT("Chrome"), TEXT("Menu_Button_Light"));
	TexTextEntry = LoadMenu2011Tex(TEXT("Login"), TEXT("APB_BG_TextEntry"));
	TexCheckTrue = LoadMenu2011Tex(TEXT("Login"), TEXT("Check_True"));
	TexCheckFalse = LoadMenu2011Tex(TEXT("Login"), TEXT("Check_False"));
	TexBrandKey = LoadMenu2011Tex(TEXT("Login"), TEXT("JKICON_login_header_key"));
	TexFooter = LoadMenu2011Tex(TEXT("Login"), TEXT("frontendFooter"));
	TexCloseBtn = LoadMenu2011Tex(TEXT("Login"), TEXT("JKICON_close_default"));
	TexRing = LoadMenu2011Tex(TEXT("Chrome"), TEXT("BG_Button_Active_Ring"));
	// Design-canvas panel plates (2011 Login/Lobby scene backgrounds)
	TexDropShadow = LoadMenu2011Tex(TEXT("Chrome"), TEXT("APB_DropShadow"));
	TexGenericContent = LoadMenu2011Tex(TEXT("Chrome"), TEXT("APB_BG_GenericContent_01"));
	TexTitleAccent = LoadMenu2011Tex(TEXT("Chrome"), TEXT("Window_Title_Accent_01"));
	TexWindowBG = LoadMenu2011Tex(TEXT("Chrome"), TEXT("APB_Window_BG"));
	TexListCell = LoadMenu2011Tex(TEXT("Chrome"), TEXT("APB_List_Cell_NoBG_20"));
	TexListCellActive = LoadMenu2011Tex(TEXT("Chrome"), TEXT("APB_List_Cell_NoBG_20_Active"));
	TexListCellPressed = LoadMenu2011Tex(TEXT("Chrome"), TEXT("APB_List_Cell_NoBG_20_Pressed"));
	TexSmallListItem = LoadMenu2011Tex(TEXT("Chrome"), TEXT("APB_SmallListItem_Generic_01"));
	// District splash photos
	TexDistFinancial = LoadMenu2011Tex(TEXT("DistrictSelect"), TEXT("FinancialDistrict_MainPhoto256x195"));
	TexDistSocial = LoadMenu2011Tex(TEXT("DistrictSelect"), TEXT("SocialDistrict_MainPhoto256x195"));
	TexDistWaterfront = LoadMenu2011Tex(TEXT("DistrictSelect"), TEXT("WaterfrontDistrict_MainPhoto256x195"));

	// No full-screen male/female character overlays
	TexAvatarMale = nullptr;
	TexAvatarFemale = nullptr;
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend ART2011 logo=%d movie_only=1 panel=%d btn=%d/%d/%d entry=%d footer=%d ring=%d facC=%d facE=%d | plates shadow=%d content=%d title=%d winbg=%d"),
		TexLogo ? 1 : 0, TexWindowPanel ? 1 : 0,
		TexBtnOn ? 1 : 0, TexBtnOff ? 1 : 0, TexBtnLight ? 1 : 0,
		TexTextEntry ? 1 : 0, TexFooter ? 1 : 0, TexRing ? 1 : 0,
		TexFactionCrim ? 1 : 0, TexFactionEnf ? 1 : 0,
		TexDropShadow ? 1 : 0, TexGenericContent ? 1 : 0, TexTitleAccent ? 1 : 0, TexWindowBG ? 1 : 0);
	UE_LOG(LogTemp, Log, TEXT("APBFrontend CHARSELECT_ART icon=%d list_cell=%d/%d/%d small=%d menu_buttons=source-backed"),
		TexCharacterSelectIcon ? 1 : 0, TexListCell ? 1 : 0, TexListCellActive ? 1 : 0,
		TexListCellPressed ? 1 : 0, TexSmallListItem ? 1 : 0);
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

void UAPBFrontendWidget::LoadUiStringsRetail()
{
	if (bUiStringsRetailLoaded) return;
	bUiStringsRetailLoaded = true;

	const FString JsonPath = FPaths::ProjectContentDir() / TEXT("Data/ui_strings_retail.json");
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *JsonPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend STRRETAIL missing %s"), *JsonPath);
		return;
	}
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend STRRETAIL parse failed %s"), *JsonPath);
		return;
	}
	for (const auto& Section : Root->Values)
	{
		if (Section.Key == TEXT("_meta")) continue;
		const TSharedPtr<FJsonObject>* SectionObject = nullptr;
		if (!Section.Value.IsValid() || !Section.Value->TryGetObject(SectionObject) || !SectionObject || !SectionObject->IsValid()) continue;
		for (const auto& Entry : (*SectionObject)->Values)
		{
			FString Value;
			if (Entry.Value.IsValid() && Entry.Value->TryGetString(Value))
			{
				UiStringsRetail.Add(FString(Section.Key) + TEXT(".") + FString(Entry.Key), Value);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("APBFrontend STRRETAIL loaded %d strings"), UiStringsRetail.Num());
}

FString UAPBFrontendWidget::SRetail(const FString& SectionKey, const FString& Fallback) const
{
	if (const FString* Found = UiStringsRetail.Find(SectionKey))
	{
		return *Found;
	}
	return Fallback;
}

void UAPBFrontendWidget::SetCharacterCreateStatus(const FString& Text)
{
	if (StatusText) StatusText->SetText(FText::FromString(Text));
}

void UAPBFrontendWidget::SetCharacterCreateUnavailable(const FString& Text)
{
	SetCharacterCreateStatus(TEXT("UNAVAILABLE: ") + Text);
	if (PreviewSummary) PreviewSummary->SetText(FText::FromString(TEXT("RETAIL SOURCE BLOCKED: ") + Text));
	LogStage(TEXT("CHAR_CREATE_BLOCKED ") + Text);
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
		// Button2 (94ms mono, ~-1dB clipped) was jarring/loud as a back click;
		// reuse the standard entry click so Back sounds the same as any menu action.
		{ TEXT("UI_Back"),        TEXT("ButtonPos") },
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
		UGameInstance* GI = GetGameInstance();
		UAPBVerifiedAssetRegistry* Registry = GI ? GI->GetSubsystem<UAPBVerifiedAssetRegistry>() : nullptr;
		if (Registry)
		{
			// 2011 RTW UI sounds are the only sanctioned source for menu sfx.
			if (USoundBase* SB = Registry->LoadSoundBase(GetWorld(), Path, TEXT("frontend_ui_sound"), TEXT("2011")))
			{
				UiSfx.Add(Row.Slot, SB);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("APBFrontend SFX blocked %s"), *Path);
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
}

void UAPBFrontendWidget::OnAccountLink()
{
	PlayUiSfx(TEXT("UI_Click"));
	SetStage(EAPBFrontendStage::Register);
}

void UAPBFrontendWidget::OnReplayVideosLink()
{
	PlayUiSfx(TEXT("UI_Click"));
	SetStage(EAPBFrontendStage::ReplayVideos);
}

void UAPBFrontendWidget::OnReplayIntroMovie()
{
	PlayUiSfx(TEXT("UI_Click"));
	StartReplayMovie();
}

void UAPBFrontendWidget::OnCreditsLink()
{
	PlayUiSfx(TEXT("UI_Click"));
	SetStage(EAPBFrontendStage::Credits);
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
		PanelSizeBox->SetVisibility(bShowForm ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
	const bool bSplashStage = CurrentStage == EAPBFrontendStage::Splash;
	if (SplashBg)
	{
		SplashBg->SetVisibility(bSplashStage && TexSplash
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (SplashLogo)
	{
		SplashLogo->SetVisibility(bSplashStage && !TexSplash
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (LogoImage)
	{
		LogoImage->SetVisibility(ESlateVisibility::Collapsed);
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
	BodyScroll->SetClipping(EWidgetClipping::ClipToBounds);
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
	SetVisibility(ESlateVisibility::Collapsed);
	SetIsEnabled(true);
	SetIsFocusable(true);
	// Mandatory classic path: short splash then LOGIN (not character-first).
	SetStage(EAPBFrontendStage::Splash);
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Log, TEXT("APBFrontend STATIC_SPLASH_VISIBLE"));
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			APB->InitCatalogFromProjectData();
		}
	}
	StartStartupMovie();
	UpdateViewportScale();
	if (GEngine)
	{
		GEngine->OnTravelFailure().AddUObject(this, &UAPBFrontendWidget::HandleTravelFailure);
	}
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend NativeConstruct classic_ui root=%d"), RootCanvas != nullptr ? 1 : 0);

	// QA-only capture hook: -APBHoldStage=<Login|CharacterSelect|CharacterCreate|DistrictSelect|Settings|Loading>
	// Suppresses splash auto-advance, seeds a character (so CharacterSelect renders populated),
	// jumps straight to the named stage and holds. Never fires for players (no flag => no-op).
	FString HoldStage;
	if (FParse::Value(FCommandLine::Get(), TEXT("APBHoldStage="), HoldStage) && !HoldStage.IsEmpty())
	{
		bSplashAutoDone = true;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
			{
				APB->RegisterAccount(TEXT("player1"), TEXT("password"));
				APB->Login(TEXT("player1"), TEXT("password"));
				APB->EnterWorld(TEXT("W1"));
				if (!APB->CaptureDomainSnapshot().bHasCharacter)
				{
					APB->CreateCharacter(TEXT("FrontendOp"), false);
				}
			}
		}
		EAPBFrontendStage Target = EAPBFrontendStage::Login;
		if (HoldStage.Equals(TEXT("CharacterSelect"), ESearchCase::IgnoreCase)) Target = EAPBFrontendStage::CharacterSelect;
		else if (HoldStage.Equals(TEXT("CharacterCreate"), ESearchCase::IgnoreCase)) Target = EAPBFrontendStage::CharacterCreate;
		else if (HoldStage.Equals(TEXT("DistrictSelect"), ESearchCase::IgnoreCase)) Target = EAPBFrontendStage::DistrictSelect;
		else if (HoldStage.Equals(TEXT("Settings"), ESearchCase::IgnoreCase)) Target = EAPBFrontendStage::Settings;
		else if (HoldStage.Equals(TEXT("Loading"), ESearchCase::IgnoreCase)) Target = EAPBFrontendStage::Loading;
		else if (HoldStage.Equals(TEXT("Splash"), ESearchCase::IgnoreCase)) Target = EAPBFrontendStage::Splash;
		SetStage(Target);
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend HOLD_STAGE=%s"), *GetStageToken());
	}
}

void UAPBFrontendWidget::NativeDestruct()
{
	if (GEngine)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	StopLoginBackgroundVideo();
	if (StartupMediaPlayer)
	{
		StartupMediaPlayer->Close();
	}
	DestroyCharacterPreview();
	StopLoginMusic();
	Super::NativeDestruct();
}

void UAPBFrontendWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateViewportScale();
	EnsureLoginMediaLoop();
	// Replay overlay: auto-stop when the movie reaches its end (in case the
	// OnEndReached delegate is not raised by the platform player).
	if (bReplayActive && ReplayMediaPlayer && FPlatformTime::Seconds() - ReplayStartedTime > 1.5)
	{
		const float Dur = ReplayMediaPlayer->GetDuration().GetTotalSeconds();
		const float Pos = ReplayMediaPlayer->GetTime().GetTotalSeconds();
		if (Dur > 0.1f && Pos >= Dur - 0.2f)
		{
			StopReplayMovie();
		}
	}
	// Watchdog: if the movie never begins playing (open ok but no frames,
	// codec hiccup, stuck buffer), bail out of the fullscreen overlay so the
	// user is never trapped in a black screen (the F8-only exit bug).
	// Only fires when the player is idle (not preparing/buffering) so a slow
	// 5k AI webm decode that is still working is never killed.
	if (bReplayActive && FPlatformTime::Seconds() - ReplayStartedTime > 8.0
		&& (!ReplayMediaPlayer || (!ReplayMediaPlayer->IsPlaying()
			&& !ReplayMediaPlayer->IsPreparing() && !ReplayMediaPlayer->IsBuffering()
			&& ReplayMediaPlayer->GetTime() == FTimespan::Zero())))
	{
		StopReplayMovie();
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Movie failed to start; returning to menu.")));
		LogStage(TEXT("replay_watchdog_stop"));
	}
	// 2011 scene CapsLock warning (CapsLockWarningText) — shown only on Login with CapsLock on
	if (CapsLockWarning && CurrentStage == EAPBFrontendStage::Login)
	{
		const bool bCaps = FSlateApplication::Get().GetModifierKeys().AreCapsLocked();
		const ESlateVisibility Vis = bCaps ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		CapsLockWarning->SetVisibility(Vis);
		if (CapsLockPanel) CapsLockPanel->SetVisibility(Vis);
	}
	// CharacterSelect studio viewer: LMB drag rotates the turntable; release resumes auto-spin.
	if ((CurrentStage == EAPBFrontendStage::CharacterSelect || CurrentStage == EAPBFrontendStage::CharacterCreate)
		&& CharPreviewActor && CharPreviewImage
		&& CharPreviewImage->GetVisibility() != ESlateVisibility::Collapsed)
	{
		const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
		const bool bLeftDown = FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
		const bool bOverViewer = CharPreviewImage->GetCachedGeometry().IsUnderLocation(CursorPos);
		if (bLeftDown && bOverViewer)
		{
			if (!bPreviewDragging)
			{
				bPreviewDragging = true;
				CharPreviewActor->SetAutoSpin(false);
			}
			else
			{
				const float ScaleX = CharPreviewImage->GetCachedGeometry().GetLocalSize().X > 1.f
					? apb_scene::LobbyDesignW / CharPreviewImage->GetCachedGeometry().GetLocalSize().X : 1.f;
				CharPreviewActor->AddYaw((CursorPos.X - LastDragCursor.X) * ScaleX * 0.45f);
			}
			LastDragCursor = CursorPos;
		}
		else if (bPreviewDragging)
		{
			bPreviewDragging = false;
			CharPreviewActor->SetAutoSpin(true);
		}
	}
	// Delete confirm auto-disarms after 4s so a stray second click can't nuke a character.
	if (bDeleteArmed && FPlatformTime::Seconds() - DeleteArmedTime > 4.0)
	{
		bDeleteArmed = false;
		if (CharSelectDeleteLabel)
		{
			CharSelectDeleteLabel->SetText(FText::FromString(S2011(TEXT("CharacterSelectScreen.DeleteCharacter"), TEXT("DELETE CHARACTER"))));
			CharSelectDeleteLabel->SetColorAndOpacity(APB_WHITE);
		}
		if (CharSelectDeleteBtn) CharSelectDeleteBtn->SetIsEnabled(true);
	}
	if (bStartupMovieStarted && !bStartupReady)
	{
		StartupReadinessTimer += InDeltaTime;
	}
	EnsureStartupMovieReady();
	if (!bStartupReady) return;
	if (CurrentStage == EAPBFrontendStage::Splash && !bSplashAutoDone)
	{
		SplashTimer += InDeltaTime;
		const float Duration = StartupMediaPlayer ? StartupMediaPlayer->GetDuration().GetTotalSeconds() : 0.f;
		const float Position = StartupMediaPlayer ? StartupMediaPlayer->GetTime().GetTotalSeconds() : 0.f;
		if ((Duration > 0.1f && Position >= Duration - 0.05f) || SplashTimer >= 8.f)
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

bool UAPBFrontendWidget::VerifyMediaFile(FString& InOutPath, const TCHAR* Context)
{
	if (InOutPath.IsEmpty())
	{
		return false;
	}
	UGameInstance* GI = GetGameInstance();
	UAPBVerifiedAssetRegistry* Registry = GI ? GI->GetSubsystem<UAPBVerifiedAssetRegistry>() : nullptr;
	if (!Registry)
	{
		UE_LOG(LogTemp, Error, TEXT("APBFrontend %s BLOCKED path=%s reason=registry_unavailable"), Context, *InOutPath);
		InOutPath.Reset();
		return false;
	}
	FString Reason;
	if (!Registry->IsMediaAllowed(InOutPath, Context, &Reason))
	{
		UE_LOG(LogTemp, Error, TEXT("APBFrontend %s BLOCKED path=%s reason=%s"), Context, *InOutPath, *Reason);
		InOutPath.Reset();
		return false;
	}
	return true;
}

void UAPBFrontendWidget::CollectReplayMovies(TArray<FString>& OutPaths) const
{
	const FString ContentRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	const FString MoviesDir = ContentRoot / TEXT("Movies");
	TArray<FString> Found;
	IFileManager::Get().FindFilesRecursive(Found, *MoviesDir, TEXT("*.mp4"), true, false);
	TArray<FString> Webm;
	IFileManager::Get().FindFilesRecursive(Webm, *MoviesDir, TEXT("*.webm"), true, false);
	Found.Append(Webm);
	Found.Sort();
	OutPaths.Reset();
	for (const FString& P : Found)
	{
		// Prefer the H.264 companion when both mp4 and webm exist for the same base.
		const FString Base = FPaths::GetBaseFilename(P);
		const FString Dir = FPaths::GetPath(P);
		if (P.EndsWith(TEXT(".webm")) && FPaths::FileExists(Dir / Base + TEXT(".mp4")))
		{
			continue;
		}
		OutPaths.Add(P);
	}
}

FString UAPBFrontendWidget::FriendlyMovieName(const FString& Path) const
{
	FString Base = FPaths::GetBaseFilename(Path);
	FString Dir = FPaths::GetPath(Path);
	FString Sub = FPaths::GetCleanFilename(Dir);
	if (Sub != TEXT("Movies"))
	{
		Base = Sub + TEXT(" / ") + Base;
	}
	Base.ReplaceInline(TEXT("_"), TEXT(" "));
	return Base.ToUpper();
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

void UAPBFrontendWidget::StartStartupMovie()
{
	// Authored splash image replaces the startup movie entirely (no audio, no wasted decode).
	if (TexSplash)
	{
		bStartupReady = true;
		SetVisibility(ESlateVisibility::Visible);
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend STARTUP_MOVIE_SKIPPED; authored splash image only"));
		return;
	}
	const FString ContentRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	StartupMoviePaths = {
		ContentRoot / TEXT("Movies/SplashScreen.mp4"),
		ContentRoot / TEXT("Movies/IntroTitles.mp4")
	};
	StartupMoviePath = FirstExistingVideo(StartupMoviePaths);
	if (!VerifyMediaFile(StartupMoviePath, TEXT("STARTUP_MOVIE")))
	{
		bStartupReady = true;
		SetVisibility(ESlateVisibility::Visible);
		UE_LOG(LogTemp, Error, TEXT("APBFrontend STARTUP_MOVIE_DENIED; using static splash"));
		return;
	}
	if (!StartupMediaPlayer)
	{
		StartupMediaPlayer = NewObject<UMediaPlayer>(this, TEXT("StartupMediaPlayer"));
		StartupMediaPlayer->PlayOnOpen = false;
		StartupMediaPlayer->SetLooping(false);
	}
	if (!StartupMediaTexture)
	{
		StartupMediaTexture = NewObject<UMediaTexture>(this, TEXT("StartupMediaTexture"));
		StartupMediaTexture->SetMediaPlayer(StartupMediaPlayer);
		StartupMediaTexture->UpdateResource();
	}
	const bool bOpened = StartupMediaPlayer->OpenFile(StartupMoviePath); // verified above
	if (!bOpened)
	{
		bStartupReady = true;
		SetVisibility(ESlateVisibility::Visible);
		UE_LOG(LogTemp, Error, TEXT("APBFrontend STARTUP_MOVIE_OPEN_FAILED path=%s; using static splash"), *StartupMoviePath);
		return;
	}
	if (BgVideo)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Brush.SetResourceObject(StartupMediaTexture);
		Brush.ImageSize = FVector2D(1920.f, 1080.f);
		BgVideo->SetBrush(Brush);
		BgVideo->SetVisibility(ESlateVisibility::Collapsed);
	}
	StartupMediaPlayer->Play();
	bStartupMovieStarted = true;
	bStartupReady = false;
	StartupReadinessTimer = 0.f;
	UE_LOG(LogTemp, Log, TEXT("APBFrontend STARTUP_MOVIE_OPENED path=%s"), *StartupMoviePath);
}

void UAPBFrontendWidget::EnsureStartupMovieReady()
{
	if (!bStartupMovieStarted || bStartupReady || !StartupMediaPlayer || !StartupMediaTexture) return;
	const bool bMediaReady =
		StartupMediaPlayer->IsReady() &&
		StartupMediaPlayer->IsPlaying() &&
		StartupMediaPlayer->GetDuration() > FTimespan::Zero() &&
		StartupMediaPlayer->GetTime() > FTimespan::Zero();
	if (!bMediaReady && StartupReadinessTimer < 2.f) return;
	bStartupReady = true;
	SplashTimer = 0.f;
	if (CurrentStage != EAPBFrontendStage::Splash)
	{
		// Stage advanced past splash (e.g. -APBHoldStage jump): the stage bed
		// owns BgVideo now, so never collapse/replace it with the splash movie.
		StartupMediaPlayer->Close();
		bStartupMovieStarted = false;
	}
	else if (BgVideo)
	{
		if (bMediaReady)
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.Tiling = ESlateBrushTileType::NoTile;
			Brush.SetResourceObject(StartupMediaTexture);
			Brush.ImageSize = FVector2D(1920.f, 1080.f);
			BgVideo->SetBrush(Brush);
			BgVideo->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			BgVideo->SetVisibility(ESlateVisibility::Collapsed);
			StartupMediaPlayer->Close();
			bStartupMovieStarted = false;
		}
	}
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Log, TEXT("APBFrontend STARTUP_MOVIE_%s path=%s"), bMediaReady ? TEXT("READY") : TEXT("FALLBACK_STATIC"), *StartupMoviePath);
}

void UAPBFrontendWidget::StartLoginBackgroundVideo()
{
	ApplyStageBackgroundVideo(EAPBFrontendStage::Login);
}

void UAPBFrontendWidget::ApplyStageBackgroundVideo(EAPBFrontendStage Stage)
{
	FString VideoPath = ResolveStageBgVideoPath(Stage);
	if (VideoPath.IsEmpty())
	{
		if (BgVideo) BgVideo->SetVisibility(ESlateVisibility::Collapsed);
		bLoginVideoStarted = false;
		ActiveStageVideoPath.Empty();
		UE_LOG(LogTemp, Warning, TEXT("APBFrontend StageBG missing stage=%s"), *GetStageToken());
		return;
	}
	// Same bed already open (Register/Credits/ReplayVideos all resolve to the
	// Login bed) — keep the player untouched so opening those stages never
	// restarts the video (no "Playback resumed" churn).
	if (bLoginVideoStarted && LoginMediaPlayer && !ActiveStageVideoPath.IsEmpty()
		&& VideoPath == ActiveStageVideoPath && LoginMediaPlayer->IsPlaying())
	{
		UE_LOG(LogTemp, Log, TEXT("APBFrontend StageBG same-path skip stage=%s"), *GetStageToken());
		return;
	}
	// Task-18 gate: the resolved stage bed movie must be registry-verified. On
	// denial the bed stays hidden (no alternate asset is substituted).
	if (!VerifyMediaFile(VideoPath, TEXT("STAGE_BG_VIDEO")))
	{
		if (BgVideo) BgVideo->SetVisibility(ESlateVisibility::Collapsed);
		bLoginVideoStarted = false;
		ActiveStageVideoPath.Empty();
		UE_LOG(LogTemp, Error, TEXT("APBFrontend StageBG denied stage=%s"), *GetStageToken());
		return;
	}

	ActiveStageVideoPath = VideoPath;

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
	if (!LoginMediaSoundComp)
	{
		if (UWorld* SoundWorld = GetWorld())
		{
			LoginMediaSoundComp = NewObject<UMediaSoundComponent>(this, TEXT("LoginMediaSound"));
			LoginMediaSoundComp->bIsUISound = true;
			LoginMediaSoundComp->bAutoActivate = false;
			LoginMediaSoundComp->SetMediaPlayer(LoginMediaPlayer);
			LoginMediaSoundComp->RegisterComponentWithWorld(SoundWorld);
			LoginMediaSoundComp->SetVolumeMultiplier(MenuAudioVolume);
			LoginMediaSoundComp->Start();
		}
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
		bLoginVideoStarted = false;
		ActiveStageVideoPath.Empty();
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
	ActiveStageVideoPath.Empty();
	if (LoginMediaPlayer)
	{
		LoginMediaPlayer->Close();
	}
	if (LoginMediaSoundComp)
	{
		LoginMediaSoundComp->Stop();
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
		// Transient close (seek/loop hiccup) — reopen the stage bed so the
		// background never dies permanently. Cooldown prevents reopen spam.
		const double Now = FPlatformTime::Seconds();
		if (Now - LoginReopenTime > 2.0)
		{
			LoginReopenTime = Now;
			ApplyStageBackgroundVideo(CurrentStage);
		}
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
	if (DesignScale && DesignScale->GetVisibility() != ESlateVisibility::Collapsed)
	{
		if (CurrentStage == EAPBFrontendStage::CharacterSelect)
			SetDesignCanvasSize(apb_scene::LobbyDesignW, apb_scene::LobbyDesignH);
		else if (CurrentStage == EAPBFrontendStage::Login)
			SetDesignCanvasSize(apb_scene::LoginDesignW, apb_scene::LoginDesignH);
	}
	if (!PanelSizeBox) return;
	FVector2D VP(1920.f, 1080.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(VP);
	}
	const float PopupMaxHeight = apb_layout::SafePopupMaxHeight(VP.Y);
	if (WardrobeItemCombo) WardrobeItemCombo->SetMaxListHeight(PopupMaxHeight);
	if (DistrictCombo) DistrictCombo->SetMaxListHeight(PopupMaxHeight);
	if (ResolutionCombo) ResolutionCombo->SetMaxListHeight(PopupMaxHeight);
	// Multi-aspect scale via shipped pure math (APBFrontendLayoutMath.h)
	const FString StageToken = GetStageToken();
	float PresentationScale = 1.f;
	if (CurrentStage == EAPBFrontendStage::Login)
	{
		const float SafeW = VP.X * 0.96f;
		const float SafeH = VP.Y * 0.96f;
		const float SafeScale = FMath::Min(SafeW / apb_scene::LoginDesignW, SafeH / apb_scene::LoginDesignH);
		PresentationScale = FMath::Clamp(SafeScale, 0.01f, 1.12f);
	}
	const FString Token = FString::Printf(TEXT("%s|scale_mode=%d|safe_fraction=0.96|presentation=%.3f"), *StageToken, static_cast<int32>(UiScaleMode), PresentationScale);
	// Panel size is stage- and scale-mode-dependent; recompute whenever either changes.
	if (VP.Equals(LastViewport, 1.f) && Token == LastScaleToken) return;
	LastViewport = VP;
	LastScaleToken = Token;
	if (DesignScale)
	{
		DesignScale->SetStretch(CurrentStage == EAPBFrontendStage::Login
			? EStretch::UserSpecified
			: EStretch::ScaleToFit);
		DesignScale->SetStretchDirection(CurrentStage == EAPBFrontendStage::Login
			? EStretchDirection::Both
			: EStretchDirection::DownOnly);
		DesignScale->SetUserSpecifiedScale(PresentationScale);
		if (UCanvasPanelSlot* DS = Cast<UCanvasPanelSlot>(DesignScale->Slot))
		{
			const float InsetX = VP.X * 0.02f;
			const float InsetY = VP.Y * 0.02f;
			DS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			DS->SetOffsets(FMargin(InsetX, InsetY, InsetX, InsetY));
		}
		DesignScale->SetClipping(EWidgetClipping::ClipToBounds);
		if (DesignSizeBox) DesignSizeBox->SetClipping(EWidgetClipping::ClipToBounds);
		if (DesignCanvas) DesignCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	}
	const apb_layout::ScaleMode Mode =
		(UiScaleMode == EAPBUiScaleMode::Fill) ? apb_layout::ScaleMode::Fill
		: (UiScaleMode == EAPBUiScaleMode::Stretch) ? apb_layout::ScaleMode::Stretch
		: apb_layout::ScaleMode::Fit;

	float DesignW = 440.f, DesignH = 400.f;
	apb_layout::DesignPanelSize(TCHAR_TO_UTF8(*StageToken), DesignW, DesignH);
	float UseW = 0.f, UseH = 0.f;
	apb_layout::SafeScaledPanelSize(TCHAR_TO_UTF8(*StageToken), VP.X, VP.Y, Mode, 0.96f, UseW, UseH);
	float ScaleX = 1.f, ScaleY = 1.f, Uni = 1.f;
	apb_layout::ComputeSceneUiScale(VP.X, VP.Y, DesignW, DesignH, Mode, ScaleX, ScaleY, Uni);

	// Fixed stages (login/select): exact height — no scroll, no stretchy empty region
	const bool bFixed = !bStageAllowsScroll;
	PanelSizeBox->SetWidthOverride(UseW);
	PanelSizeBox->SetHeightOverride(UseH);
	PanelSizeBox->SetMinDesiredHeight(UseH);
	PanelSizeBox->SetMaxDesiredHeight(UseH);
	PanelSizeBox->SetClipping(EWidgetClipping::ClipToBounds);
	if (PanelBorder) PanelBorder->SetClipping(EWidgetClipping::ClipToBounds);
	if (BodyScroll)
	{
		BodyScroll->SetClipping(EWidgetClipping::ClipToBounds);
		BodyScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	}
	if (bFixed)
	{
		PanelSizeBox->SetMinDesiredHeight(UseH);
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
		// Keep the source LoadingScreen_APB 256x128 aspect while the widget scales
		FSlateBrush Br = LogoImage->GetBrush();
		Br.ImageSize = FVector2D(256.f, 128.f);
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
	UGameInstance* GI = GetGameInstance();
	UAPBVerifiedAssetRegistry* Registry = GI ? GI->GetSubsystem<UAPBVerifiedAssetRegistry>() : nullptr;

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
		// Task-18 gate: raw WAV paths use the verified registry like packaged files.
		FString VerifiedWav = WavPath;
		if (!VerifyMediaFile(VerifiedWav, TEXT("LOGIN_THEME_WAV"))) continue;
		Theme = APB_LoadPcmWavProcedural(this, VerifiedWav, /*bLooping=*/true);
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
		Theme = Registry ? Registry->LoadSoundBase(World,
			TEXT("/Game/Audio/LoginTheme_APBTheme1.LoginTheme_APBTheme1"), TEXT("frontend_login_theme")) : nullptr;
		if (Theme) ThemeTag = TEXT("LoginTheme_APBTheme1");
	}
	if (!Theme)
	{
		Theme = Registry ? Registry->LoadSoundBase(World,
			TEXT("/Game/Audio/LoginTheme_APB_ThemePreMaster.LoginTheme_APB_ThemePreMaster"), TEXT("frontend_login_theme")) : nullptr;
		if (Theme) ThemeTag = TEXT("LoginTheme_APB_ThemePreMaster");
	}
	if (!Theme)
	{
		Theme = Registry ? Registry->LoadSoundBase(World,
			TEXT("/Game/Audio/LoginTheme.LoginTheme"), TEXT("frontend_login_theme")) : nullptr;
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
	if (LoginMediaSoundComp)
	{
		LoginMediaSoundComp->SetVolumeMultiplier(MenuAudioVolume);
	}
	UE_LOG(LogTemp, Log, TEXT("APBFrontend MenuAudioVolume=%.2f"), MenuAudioVolume);
}

FString UAPBFrontendWidget::GetStageToken() const
{
	switch (CurrentStage)
	{
	case EAPBFrontendStage::Splash: return TEXT("Splash");
	case EAPBFrontendStage::Login: return bFirstRunTOS ? TEXT("LoginTOS") : TEXT("Login");
	case EAPBFrontendStage::Register: return TEXT("Register");
	case EAPBFrontendStage::Credits: return TEXT("Credits");
	case EAPBFrontendStage::ReplayVideos: return TEXT("ReplayVideos");
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
	if (GEngine && FParse::Param(FCommandLine::Get(), TEXT("APBDebug")))
	{
		GEngine->AddOnScreenDebugMessage(9002, 6.f, FColor::Green, Line);
	}
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
	SetVisibility(ESlateVisibility::Collapsed);
	WidgetTree->RootWidget = RootCanvas;

	LoadMenu2011Assets();
	LoadUiStrings2011();
	LoadUiStringsRetail();

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

	BgArt = nullptr;
	AvatarLeft = nullptr;
	AvatarRight = nullptr;

	BgVideo = AddLayerImage(TEXT("BgLoginVideo"), 1);
	if (BgVideo)
	{
		BgVideo->SetColorAndOpacity(FLinearColor::White);
		BgVideo->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Fullscreen top-layer replay overlay (ReplayVideos > Play Intro Movie).
	// z40 sits above the design canvas (z18) and panel (z20), so the intro
	// movie covers the whole menu; click, STOP button, or any key dismisses it.
	ReplayOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ReplayOverlay"));
	{
		FSlateBrush Brush;
		APB_MakeBoxBrush(Brush, FLinearColor(0.f, 0.f, 0.f, 1.f));
		ReplayOverlay->SetBrush(Brush);
		ReplayOverlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 1.f));
		ReplayOverlay->SetPadding(FMargin(0.f));
	}
	UCanvasPanel* ReplayPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ReplayPanel"));
	ReplayOverlay->AddChild(ReplayPanel);
	ReplayImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ReplayImage"));
	ReplayImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	ReplayImage->SetColorAndOpacity(FLinearColor::White);
	if (UCanvasPanelSlot* IS = ReplayPanel->AddChildToCanvas(ReplayImage))
	{
		IS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		IS->SetOffsets(FMargin(0.f));
		IS->SetZOrder(0);
	}
	// Invisible full-screen catcher: click anywhere dismisses the overlay.
	ReplayClickCatcher = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ReplayClickCatcher"));
	{
		FButtonStyle CatchStyle = ReplayClickCatcher->GetStyle();
		APB_MakeBoxBrush(CatchStyle.Normal, FLinearColor(0.f, 0.f, 0.f, 0.01f));
		APB_MakeBoxBrush(CatchStyle.Hovered, FLinearColor(0.f, 0.f, 0.f, 0.01f));
		APB_MakeBoxBrush(CatchStyle.Pressed, FLinearColor(0.f, 0.f, 0.f, 0.01f));
		CatchStyle.NormalPadding = FMargin(0.f);
		CatchStyle.PressedPadding = FMargin(0.f);
		ReplayClickCatcher->SetStyle(CatchStyle);
		ReplayClickCatcher->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnReplayStopClicked);
	}
	if (UCanvasPanelSlot* CS2 = ReplayPanel->AddChildToCanvas(ReplayClickCatcher))
	{
		CS2->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CS2->SetOffsets(FMargin(0.f));
		CS2->SetZOrder(1);
	}
	// Bottom hint + STOP button so the user always has a visible way out.
	ReplayHint = MakeLabel(TEXT("ReplayHint"), TEXT("PRESS ESC / CLICK TO STOP"), 14, APB_WHITE);
	ReplayHint->SetJustification(ETextJustify::Center);
	ReplayHint->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* HS = ReplayPanel->AddChildToCanvas(ReplayHint))
	{
		HS->SetAnchors(FAnchors(0.f, 1.f));
		HS->SetAlignment(FVector2D(0.5f, 1.f));
		HS->SetPosition(FVector2D(0.f, -40.f));
		HS->SetAutoSize(true);
		HS->SetZOrder(2);
	}
	ReplayStopBtn = MakeAccentButton(TEXT("ReplayStopBtn"), TEXT("STOP"), APB_BTN_DANGER);
	ReplayStopBtn->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnReplayStopClicked);
	if (UCanvasPanelSlot* SS = ReplayPanel->AddChildToCanvas(ReplayStopBtn))
	{
		SS->SetAnchors(FAnchors(1.f, 1.f));
		SS->SetAlignment(FVector2D(1.f, 1.f));
		SS->SetPosition(FVector2D(-20.f, -20.f));
		SS->SetSize(FVector2D(160.f, 36.f));
		SS->SetZOrder(3);
	}
	if (UCanvasPanelSlot* CS = RootCanvas->AddChildToCanvas(ReplayOverlay))
	{
		CS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CS->SetOffsets(FMargin(0.f));
		CS->SetZOrder(40);
	}
	ReplayOverlay->SetVisibility(ESlateVisibility::Collapsed);

	FooterBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FooterBar"));
	FooterBar->SetPadding(FMargin(24.f, 20.f, 24.f, 12.f));
	{
		FSlateBrush Brush;
		APB_MakeBoxBrush(Brush, FLinearColor(0.f, 0.f, 0.f, 0.88f));
		FooterBar->SetBrush(Brush);
		FooterBar->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.88f));
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

	SplashBg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SplashBg"));
	if (TexSplash)
	{
		FSlateBrush SplashBrush = APB_TexBrush(TexSplash, FLinearColor::White);
		// APB_TexBrush leaves ImageSize zero; supply the real pixel size so the
		// ScaleToFit wrapper has a valid child aspect to letterbox against.
		SplashBrush.ImageSize = FVector2D(TexSplash->GetSizeX(), TexSplash->GetSizeY());
		SplashBg->SetBrush(SplashBrush);
	}
	// Preserve the 2:1 authored splash aspect; letterbox instead of stretching.
	UScaleBox* SplashScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("SplashScale"));
	SplashScale->SetStretch(EStretch::ScaleToFit);
	SplashScale->SetStretchDirection(EStretchDirection::Both);
	SplashScale->AddChild(SplashBg);
	if (UCanvasPanelSlot* CS = RootCanvas->AddChildToCanvas(SplashScale))
	{
		CS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CS->SetOffsets(FMargin(0.f));
		CS->SetZOrder(2);
	}
	SplashBg->SetVisibility(ESlateVisibility::Collapsed);

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

	DesignScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("DesignScale"));
	DesignScale->SetStretch(EStretch::ScaleToFit);
	DesignScale->SetStretchDirection(EStretchDirection::DownOnly);
	DesignSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DesignSizeBox"));
	DesignSizeBox->SetWidthOverride(apb_scene::LoginDesignW);
	DesignSizeBox->SetHeightOverride(apb_scene::LoginDesignH);
	DesignCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DesignCanvas"));
	DesignCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	if (UScaleBoxSlot* SBS = Cast<UScaleBoxSlot>(DesignSizeBox->AddChild(DesignCanvas)))
	{
		SBS->SetHorizontalAlignment(HAlign_Fill);
		SBS->SetVerticalAlignment(VAlign_Fill);
	}
	DesignScale->AddChild(DesignSizeBox);
	if (UCanvasPanelSlot* CS = RootCanvas->AddChildToCanvas(DesignScale))
	{
		CS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CS->SetOffsets(FMargin(0.f));
		CS->SetZOrder(18);
	}
	DesignScale->SetVisibility(ESlateVisibility::Collapsed);

	PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSizeBox"));
	PanelSizeBox->SetWidthOverride(380.f);
	PanelSizeBox->SetMinDesiredHeight(0.f);
	PanelSizeBox->SetClipping(EWidgetClipping::ClipToBounds);
	PanelSizeBox->SetMaxDesiredHeight(0.f);
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
	PanelBorder->SetClipping(EWidgetClipping::ClipToBounds);
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

void UAPBFrontendWidget::SetDesignCanvasSize(float DesignW, float DesignH)
{
	if (DesignSizeBox)
	{
		DesignSizeBox->SetWidthOverride(DesignW);
		DesignSizeBox->SetHeightOverride(DesignH);
	}
	if (DesignScale)
	{
		DesignScale->SetStretch(CurrentStage == EAPBFrontendStage::Login
			? EStretch::UserSpecified
			: EStretch::ScaleToFit);
		DesignScale->SetStretchDirection(CurrentStage == EAPBFrontendStage::Login
			? EStretchDirection::Both
			: EStretchDirection::DownOnly);
		DesignScale->SetClipping(EWidgetClipping::ClipToBounds);
	}
}

UCanvasPanelSlot* UAPBFrontendWidget::PlaceRect(UWidget* Child, float X, float Y, float W, float H, int32 ZOrder)
{
	if (!DesignCanvas || !Child) return nullptr;
	UCanvasPanelSlot* CS = DesignCanvas->AddChildToCanvas(Child);
	if (CS)
	{
		CS->SetAnchors(FAnchors(0.f, 0.f));
		CS->SetAlignment(FVector2D(0.f, 0.f));
		CS->SetPosition(FVector2D(X, Y));
		CS->SetSize(FVector2D(W, H));
		CS->SetZOrder(ZOrder);
	}
	return CS;
}

void UAPBFrontendWidget::ClearDesignCanvas()
{
	if (DesignCanvas)
	{
		DesignCanvas->ClearChildren();
	}
}

void UAPBFrontendWidget::BuildLoginDesign()
{
	SetDesignCanvasSize(apb_scene::LoginDesignW, apb_scene::LoginDesignH);
	ClearDesignCanvas();
	UserBox = PassBox = nullptr;
	RememberCheck = nullptr;

	using namespace apb_scene;	auto Place = [&](const char* N, UWidget* W, int32 Z)
	{
		if (const FRectDef* D = FindRect(LoginRects, LoginRectCount, N))
			PlaceRect(W, D->X, D->Y, D->W, D->H, Z);
	};
	// Text placed in a fixed rect draws top-aligned; wrap in an invisible UOverlay
	// whose slot centers the label on both axes (true middle-middle).
	// SOverlay honors slot alignment against the forced canvas size, unlike
	// a VerticalBox child slot (Auto-sized, so VAlign_Center is a no-op).
	auto PlaceVC = [&](float X, float Y, float W, float H, UTextBlock* T, int32 Z,
		EHorizontalAlignment HAlign = HAlign_Fill)
	{
		if (!T) return;
		UOverlay* OV = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		OV->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* OS = OV->AddChildToOverlay(T))
		{
			OS->SetVerticalAlignment(VAlign_Center);
			// HAlign_Center auto-sizes the label and centers it in its rect, so
			// field labels are not flush against the panel's left edge.
			OS->SetHorizontalAlignment(HAlign);
		}
		PlaceRect(OV, X, Y, W, H, Z);
	};
	auto TexPanel = [&](const FName& Name, UTexture2D* Tex, bool bNineSlice, const FLinearColor& Fallback, const FLinearColor& Tint = FLinearColor::White)
	{
		UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		if (Tex)
		{
			// Pre-colored plates (MessageBox_BG, GenericContent, DropShadow, Footer) render with White.
			// White-on-alpha art (Window_Title_Accent) must be re-tinted per context (spec §2.1).
			// Brush carries the tint; border color stays White so the tint applies exactly once.
			B->SetBrush(bNineSlice ? APB_PanelBrush(Tex, Tint) : APB_TexBrush(Tex, Tint));
			B->SetBrushColor(FLinearColor::White);
		}
		else
		{
			FSlateBrush Brush; APB_MakeBoxBrush(Brush, Fallback);
			// Brush.TintColor already holds Fallback; keep UBorder color White so
			// the tint is applied once (loaded path uses White x White = neutral).
			B->SetBrush(Brush); B->SetBrushColor(FLinearColor::White);
		}
		return B;
	};

	// 2011 scene plates (LoginRects): solid flat wells. The small source textures
	// (GenericContent 64x64, DropShadow 64x64, TitleAccent 128x16) smear when stretched
	// to these rects, so render solid plates tinted to the 2011 dark panel palette.
	// Align the drop plate with the panel body so the background lines up with
	// the menu items instead of wrapping them in a larger box.
	const FRectDef* ShadowD = FindRect(LoginRects, LoginRectCount, "UIPanel_EULA_MainPanel");
	PlaceRect(TexPanel(TEXT("LoginShadow"), nullptr, false, FLinearColor(0.f, 0.f, 0.f, 0.62f)),
		ShadowD ? ShadowD->X : 300.f, ShadowD ? ShadowD->Y : 256.f,
		ShadowD ? ShadowD->W : 552.f, ShadowD ? ShadowD->H : 207.f, 0);
	// Panel body: MessageBox_BG (512x512) is the one properly-sized panel texture (spec §2.1,
	// 9-slice margin ~0.05). Falls back to solid #4F4F4F when the texture is missing.
	Place("UIPanel_EULA_MainPanel", TexPanel(TEXT("LoginPanelBody"), TexWindowPanel, true, FLinearColor::FromSRGBColor(FColor(0x4F, 0x4F, 0x4F, 245))), 1);
	Place("UIImage_main_under", TexPanel(TEXT("LoginMainUnder"), nullptr, false, FLinearColor(0.f, 0.f, 0.f, 0.5f)), 2);
	// Header band (LOGIN + instructions): solid black, a touch taller for proper UX spacing.
	if (const FRectDef* Hdr = FindRect(LoginRects, LoginRectCount, "UIImage_header"))
	{
		PlaceRect(TexPanel(TEXT("LoginHeader"), nullptr, false, FLinearColor(0.f, 0.f, 0.f, 1.f)),
			Hdr->X, Hdr->Y, Hdr->W, Hdr->H + 8.f, 2);
	}
	// 2011 scene row plates (LoginRects): 75% black lines under each field band
	Place("UIImage_email_under", TexPanel(TEXT("LoginEmailUnder"), nullptr, false, FLinearColor(0.f, 0.f, 0.f, 0.75f)), 3);
	Place("UIImage_password_under", TexPanel(TEXT("LoginPassUnder"), nullptr, false, FLinearColor(0.f, 0.f, 0.f, 0.75f)), 3);


	UScaleBox* LoginLogoBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("LoginWordmarkBox"));
	LoginLogoBox->SetStretch(EStretch::ScaleToFit);
	LoginLogoBox->SetStretchDirection(EStretchDirection::DownOnly);
	UImage* LoginLogo = MakeImage(TEXT("LoginWordmark"), TexLogo);
	if (LoginLogo)
	{
		LoginLogo->SetColorAndOpacity(FLinearColor::Black);
		LoginLogo->SetVisibility(ESlateVisibility::HitTestInvisible);
		FSlateBrush LogoBrush = LoginLogo->GetBrush();
		LogoBrush.ImageSize = FVector2D(256.f, 128.f);
		LogoBrush.DrawAs = ESlateBrushDrawType::Image;
		LoginLogo->SetBrush(LogoBrush);
		LoginLogoBox->AddChild(LoginLogo);
	}
	Place("UIImage_APBlogo", LoginLogoBox, 5);

	UImage* Key = MakeImage(TEXT("LoginKey"), TexBrandKey);
	if (Key) Key->SetColorAndOpacity(APB_WHITE);
	Place("UIImage_Key_Icon", Key, 6);
	UTextBlock* Title = MakeLabel(TEXT("LoginTitle"), S2011(TEXT("APBLoginScreen.Login"), TEXT("LOGIN")).ToUpper(), 16, APB_WHITE);
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
	Title->SetJustification(ETextJustify::Left);
	Title->SetAutoWrapText(false);
	// Vertically center the title+instructions block in the taller header band.
	// 2011 rects: title (355,270,23) + instructions (356,293,19) = 42px block;
	// band is 272..320 (48px with the +8 UX bump), so shift down by 5px.
	PlaceVC(355.f, 275.f, 481.f, 23.f, Title, 6);
	UTextBlock* Instructions = MakeLabel(TEXT("LoginInstr"),
		S2011(TEXT("APBLoginScreen.Instructions"), TEXT("Please enter your account details to continue.")).ToUpper(), 9, APB_WHITE);
	Instructions->SetJustification(ETextJustify::Left);
	Instructions->SetAutoWrapText(false);
	PlaceVC(356.f, 298.f, 475.f, 19.f, Instructions, 6);

	UTextBlock* UserLabel = MakeLabel(TEXT("LoginUserLabel"),
		S2011(TEXT("APBLoginScreen.EmailAddress"), TEXT("Email Address")), 12, APB_INK);
	UserLabel->SetJustification(ETextJustify::Center);
	UserLabel->SetAutoWrapText(false);
	if (const FRectDef* UL = FindRect(LoginRects, LoginRectCount, "Label_UserID"))
	{
		PlaceVC(UL->X, UL->Y, UL->W, UL->H, UserLabel, 6, HAlign_Center);
	}
	UserBox = MakeTextField(TEXT("LoginUserBox"), TEXT(""), false, false);
	UserBox->OnTextChanged.AddDynamic(this, &UAPBFrontendWidget::OnLoginFieldsChanged);
	Place("EditBox_UserID", UserBox, 7);

	UTextBlock* PassLabel = MakeLabel(TEXT("LoginPassLabel"),
		S2011(TEXT("APBLoginScreen.Password"), TEXT("Password")), 12, APB_INK);
	PassLabel->SetJustification(ETextJustify::Center);
	PassLabel->SetAutoWrapText(false);
	if (const FRectDef* PL = FindRect(LoginRects, LoginRectCount, "Label_Password"))
	{
		PlaceVC(PL->X, PL->Y, PL->W, PL->H, PassLabel, 6, HAlign_Center);
	}
	PassBox = MakeTextField(TEXT("LoginPassBox"), TEXT(""), true, false);
	PassBox->OnTextChanged.AddDynamic(this, &UAPBFrontendWidget::OnLoginFieldsChanged);
	Place("EditBox_Password", PassBox, 7);

	RememberCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("LoginRememberCheck"));
	if (TexCheckTrue && TexCheckFalse)
	{
		FCheckBoxStyle Style = RememberCheck->GetWidgetStyle();
		// Source Check_* textures are 256x256; pin draw size so the box renders
		// at the 16x16 design rect instead of the native texture size.
		const FVector2D CheckSize(14.f, 14.f);
		Style.UncheckedImage = APB_TexBrush(TexCheckFalse, FLinearColor::White);
		Style.UncheckedImage.ImageSize = CheckSize;
		Style.UncheckedHoveredImage = Style.UncheckedImage;
		Style.CheckedImage = APB_TexBrush(TexCheckTrue, FLinearColor::White);
		Style.CheckedImage.ImageSize = CheckSize;
		Style.CheckedHoveredImage = Style.CheckedImage;
		// Small gap between the 14x14 glyph and the label content.
		Style.Padding = FMargin(4.f, 0.f, 0.f, 0.f);
		RememberCheck->SetWidgetStyle(Style);
	}
	RememberCheck->OnCheckStateChanged.AddDynamic(this, &UAPBFrontendWidget::OnRememberToggled);
	UTextBlock* RememberLabel = MakeLabel(TEXT("LoginRememberLabel"),
		S2011(TEXT("APBLoginScreen.RememberUserID"), TEXT("Remember Me")), 12, APB_INK);
	RememberLabel->SetJustification(ETextJustify::Left);
	RememberLabel->SetAutoWrapText(false);
	// The label is the checkbox's content: clicking the text toggles the box,
	// and box + text share one centerline (union of Check_RememberData and
	// UILabel_RememberMeLabel: 438..586 x 356..372).
	RememberCheck->AddChild(RememberLabel);
	PlaceRect(RememberCheck, 438.f, 356.f, 148.f, 16.f, 7);


	UButton* ExitB = MakeFlatButton(TEXT("LoginExitBtn"), S2011(TEXT("APBLoginScreen.ExitToDesktop"), TEXT("Exit to Desktop")), false);
	ExitB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnExitDesktop);
	Place("UILabelButton_Exit", ExitB, 7);
	UButton* LoginB = MakeFlatButton(TEXT("LoginBtn"), S2011(TEXT("APBLoginScreen.Login"), TEXT("Login")), false);
	LoginB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnLoginClicked);
	LoginBtn = LoginB;
	LoginBtn->SetIsEnabled(false);
	Place("Button_Login", LoginB, 7);

	// Solid action colors: dark red Exit (bright red hover) and blue Login.
	// White glyphs with a soft drop shadow read on both hues (color-theory
	// contrast on red/blue plates).
	FButtonStyle ExitStyle = ExitB->GetStyle();
	APB_MakeBoxBrush(ExitStyle.Normal, FLinearColor(0.45f, 0.04f, 0.04f, 1.f));
	APB_MakeBoxBrush(ExitStyle.Hovered, FLinearColor(0.85f, 0.08f, 0.08f, 1.f));
	APB_MakeBoxBrush(ExitStyle.Pressed, FLinearColor(0.30f, 0.02f, 0.02f, 1.f));
	APB_MakeBoxBrush(ExitStyle.Disabled, FLinearColor(0.13f, 0.13f, 0.13f, 0.9f));
	ExitB->SetStyle(ExitStyle);
	FButtonStyle LoginStyle = LoginB->GetStyle();
	APB_MakeBoxBrush(LoginStyle.Normal, FLinearColor(0.05f, 0.30f, 0.55f, 1.f));
	APB_MakeBoxBrush(LoginStyle.Hovered, FLinearColor(0.08f, 0.45f, 0.75f, 1.f));
	APB_MakeBoxBrush(LoginStyle.Pressed, FLinearColor(0.03f, 0.20f, 0.38f, 1.f));
	APB_MakeBoxBrush(LoginStyle.Disabled, FLinearColor(0.13f, 0.13f, 0.13f, 0.9f));
	LoginB->SetStyle(LoginStyle);
	// White text on the red/blue plates needs a soft drop shadow for contrast.
	if (UTextBlock* ExitL = ExitB ? Cast<UTextBlock>(ExitB->GetChildAt(0)) : nullptr)
	{
		ExitL->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		ExitL->SetShadowOffset(FVector2D(0.f, 1.f));
	}
	if (UTextBlock* LoginL = LoginB ? Cast<UTextBlock>(LoginB->GetChildAt(0)) : nullptr)
	{
		LoginLabel = LoginL;
		LoginL->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		LoginL->SetShadowOffset(FVector2D(0.f, 1.f));
	}
	// Bottom nav 2x2 grid: taller (30px) black flat buttons with readable 11px
	// white text. Rects below keep the 258px column alignment of the 2011 scene.
	UButton* AccB = MakeFlatButton(TEXT("LoginAccBtn"), S2011(TEXT("APBLoginScreen.AccountManagement"), TEXT("Account")), false, 11);
	AccB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnAccountLink);
	PlaceRect(AccB, 316.f, 480.f, 258.f, 30.f, 7);
	UButton* ReplayB = MakeFlatButton(TEXT("LoginReplayBtn"), S2011(TEXT("APBLoginScreen.ReplayVideos"), TEXT("Replay Videos")), false, 11);
	ReplayB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnReplayVideosLink);
	PlaceRect(ReplayB, 578.f, 480.f, 258.f, 30.f, 7);
	UButton* NewAccB = MakeFlatButton(TEXT("LoginNewAccBtn"), S2011(TEXT("APBLoginScreen.NewAccount"), TEXT("New Account")), false, 11);
	NewAccB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnAccountLink);
	PlaceRect(NewAccB, 316.f, 515.f, 258.f, 30.f, 7);
	UButton* CreditsB = MakeFlatButton(TEXT("LoginCreditsBtn"), S2011(TEXT("APBLoginScreen.Credits"), TEXT("Credits")), false, 11);
	CreditsB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCreditsLink);
	PlaceRect(CreditsB, 578.f, 515.f, 258.f, 30.f, 7);
	// Centered OPTIONS row under the 2x2 grid — opens the video/audio settings
	// dialog (same Settings stage as CharacterSelect/DistrictSelect).
	UButton* OptionsB = MakeFlatButton(TEXT("LoginOptionsBtn"), S2011(TEXT("APBLoginScreen.Options"), TEXT("Options")), false, 11);
	OptionsB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnOpenSettings);
	PlaceRect(OptionsB, 447.f, 552.f, 258.f, 30.f, 7);

	// 2011 scene CapsLock warning (LoginRects): panel + text, shown only while CapsLock is on
	CapsLockPanel = TexPanel(TEXT("LoginCapsPanel"), nullptr, false, FLinearColor(0.02f, 0.02f, 0.02f, 0.72f));
	Place("CapsLockWarningPanel", CapsLockPanel, 8);
	CapsLockWarning = MakeLabel(TEXT("LoginCapsWarning"),
		S2011(TEXT("APBLoginScreen.CapsLockWarning"), TEXT("Caps Lock is on")), 9, APB_AMBER);
	CapsLockWarning->SetJustification(ETextJustify::Center);
	CapsLockWarning->SetAutoWrapText(false);
	CapsLockWarning->SetVisibility(ESlateVisibility::Collapsed);
	CapsLockPanel->SetVisibility(ESlateVisibility::Collapsed);
	Place("CapsLockWarningText", CapsLockWarning, 9);
}

void UAPBFrontendWidget::BuildRegistrationBody()
{
	if (TitleText) TitleText->SetText(FText::FromString(TEXT("CREATE ACCOUNT")));
	if (SubtitleText) SubtitleText->SetText(FText::FromString(TEXT("REGISTER FOR APB RELOADED")));
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("All fields and agreements are required.")));
	if (HintText) HintText->SetText(FText::FromString(TEXT("Use the email address you want to use for your APB account.")));

	AddToScroll(MakeLabel(TEXT("RegisterIntro"), TEXT("CREATE A NEW ACCOUNT"), 13, APB_AMBER), 8.f);
	AddToScroll(MakeLabel(TEXT("RegisterEmailLabel"), TEXT("EMAIL"), 11, APB_INK), 2.f);
	RegisterEmailBox = MakeTextField(TEXT("RegisterEmailBox"), TEXT("name@example.com"), false, false);
	RegisterEmailBox->SetJustification(ETextJustify::Left);
	AddToScroll(RegisterEmailBox, 2.f);
	AddToScroll(MakeLabel(TEXT("RegisterPasswordLabel"), TEXT("PASSWORD"), 11, APB_INK), 6.f);
	RegisterPasswordBox = MakeTextField(TEXT("RegisterPasswordBox"), TEXT("Password"), true, false);
	RegisterPasswordBox->SetJustification(ETextJustify::Left);
	AddToScroll(RegisterPasswordBox, 2.f);
	AddToScroll(MakeLabel(TEXT("RegisterConfirmLabel"), TEXT("CONFIRM PASSWORD"), 11, APB_INK), 6.f);
	RegisterConfirmBox = MakeTextField(TEXT("RegisterConfirmBox"), TEXT("Confirm password"), true, false);
	RegisterConfirmBox->SetJustification(ETextJustify::Left);
	AddToScroll(RegisterConfirmBox, 2.f);

	auto AddRequiredCheck = [&](const FString& Name, const FString& Text, TObjectPtr<UCheckBox>& OutCheck)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *(Name + TEXT("Row")));
		OutCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), *Name);
		if (UHorizontalBoxSlot* CheckSlot = Row->AddChildToHorizontalBox(OutCheck))
		{
			CheckSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			CheckSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* CheckLabel = MakeLabel(Name + TEXT("Label"), Text, 10, APB_INK);
		CheckLabel->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(CheckLabel))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		AddToScroll(Row, 3.f);
	};
	AddRequiredCheck(TEXT("RegisterTos"), TEXT("I have read and agree to the Terms of Service."), RegisterTosCheck);
	AddRequiredCheck(TEXT("RegisterPrivacy"), TEXT("I have read and agree to the Privacy Policy."), RegisterPrivacyCheck);
	AddRequiredCheck(TEXT("RegisterCaptcha"), TEXT("I am not a robot. (reCAPTCHA placeholder)"), RegisterCaptchaCheck);
	AddToScroll(MakeLabel(TEXT("RegisterCaptchaNote"), TEXT("reCAPTCHA will be connected before account service launch."), 9, APB_MUTED), 2.f);

	UButton* Submit = MakeAccentButton(TEXT("RegisterSubmit"), TEXT("CREATE ACCOUNT"), APB_BTN_OK);
	Submit->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnRegistrationSubmit);
	AddToScroll(Submit, 14.f);
	UButton* Back = MakeButton(TEXT("RegisterBack"), TEXT("BACK TO LOGIN"));
	Back->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnRegistrationBack);
	AddToScroll(Back, 6.f);
}

void UAPBFrontendWidget::BuildCharacterSelectDesign()
{
	SetDesignCanvasSize(apb_scene::LobbyDesignW, apb_scene::LobbyDesignH);
	ClearDesignCanvas();

	using namespace apb_scene;
	auto Place = [&](const char* N, UWidget* W, int32 Z)
	{
		if (const FRectDef* D = FindRect(LobbyRects, LobbyRectCount, N))
			PlaceRect(W, D->X, D->Y, D->W, D->H, Z);
	};
	auto TexPanel = [&](const FName& Name, UTexture2D* Tex, bool bNineSlice, const FLinearColor& Tint = FLinearColor::White)
	{
		UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		if (Tex)
		{
			// Pre-colored plates (MessageBox_BG, GenericContent, DropShadow, Footer) render with White.
			// White-on-alpha art (Window_Title_Accent) must be re-tinted per context (spec §2.1).
			// Brush carries the tint; border color stays White so the tint applies exactly once.
			B->SetBrush(bNineSlice ? APB_PanelBrush(Tex, Tint) : APB_TexBrush(Tex, Tint));
			B->SetBrushColor(FLinearColor::White);
		}
		else
		{
			B->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogTemp, Error, TEXT("APBFrontend CHARSELECT_ART_MISSING panel=%s"), *Name.ToString());
		}
		return B;
	};

	FString CharacterName = S2011(TEXT("CharacterSelectScreen.EmptyCharacter"), TEXT("Empty"));
	FString FactionName = TEXT("-");
	int32 ThreatRating = 0;
	bool bHas = false, bEnforcer = false;
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
			}
		}
	}

	Place("UIImage_headerBG", TexPanel(TEXT("CSHeaderBG"), TexTitleAccent, false, APB_AMBER), 1);
	UImage* LobbyIcon = MakeImage(TEXT("CSLobbyIcon"), TexCharacterSelectIcon.Get());
	if (LobbyIcon) LobbyIcon->SetColorAndOpacity(APB_AMBER);
	Place("UIImage_Lobby_Icon", LobbyIcon, 3);
	UTextBlock* LobbyTitle = MakeLabel(TEXT("CSTitle"),
		S2011(TEXT("CharacterSelectScreen.CharacterSelect"), TEXT("CHARACTER SELECT")).ToUpper(), 15, APB_AMBER);
	LobbyTitle->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
	Place("UILabel_Lobby_TITLE", LobbyTitle, 3);

	Place("UIPanel_CharacterList", TexPanel(TEXT("CSCharListBG"), TexWindowPanel, true), 1);
	if (TexDropShadow)
	{
		PlaceRect(TexPanel(TEXT("CSAccountShadow"), TexDropShadow, false), -6.f, 50.f, 351.f, 120.f, 1);
	}
	PlaceRect(TexPanel(TEXT("CSAccountBlock"), TexGenericContent, true), 10.f, 50.f, 319.f, 105.f, 1);
	Place("UIPanel_Mesh", TexPanel(TEXT("CSMeshBG"), TexWindowBG, true), 0);
	Place("UIPanel_C_Content", TexPanel(TEXT("CSNamePlateBG"), TexWindowPanel, true), 1);

	UTextBlock* AccountTag = MakeLabel(TEXT("CSAccountTag"),
		S2011(TEXT("APBLoginScreen.AccountManagement"), TEXT("ACCOUNT")).ToUpper(), 10, APB_AMBER);
	AccountTag->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	Place("UILabel_RealTag", AccountTag, 3);
	FString AccountIdentity = UserBox ? UserBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (AccountIdentity.IsEmpty()) AccountIdentity = TEXT("ACCOUNT");
	UTextBlock* AccountValue = MakeLabel(TEXT("CSAccountValue"), AccountIdentity, 11, APB_INK);
	AccountValue->SetAutoWrapText(false);
	Place("UILabel_Email", AccountValue, 3);
	FString PlaytimeText = S2011(TEXT("CharacterSelectScreen.PlayTimeLeft"), TEXT("Gametime Remaining: <TotalHours>h"));
	PlaytimeText.ReplaceInline(TEXT("<TotalHours>"), TEXT("—"));
	UTextBlock* Playtime = MakeLabel(TEXT("CSPlaytime"), PlaytimeText, 10, APB_INK_SEC);
	Playtime->SetAutoWrapText(false);
	Place("UILabel_GametimeInfo", Playtime, 3);
	Place("UIImage_Characterheader", TexPanel(TEXT("CSCharHeaderBar"), TexTitleAccent, false, APB_AMBER), 2);
	UTextBlock* CharHdr = MakeLabel(TEXT("CSCharHeader"),
		S2011(TEXT("CharacterSelectScreen.Characters"), TEXT("CHARACTERS")), 13, APB_INK);
	CharHdr->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
	Place("UILabel_CharacterHeader", CharHdr, 3);
	UTextBlock* CharCount = MakeLabel(TEXT("CSCharCount"), bHas ? TEXT("1/1") : TEXT("0/1"), 12, APB_INK_SEC);
	CharCount->SetJustification(ETextJustify::Right);
	Place("UILabel_CharacterCount", CharCount, 3);

	auto MakeLobbyButton = [&](const FString& Name, const FString& Label, UTexture2D* NormalTex, UTexture2D* HoverTex, bool bEnabled = true)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);
		FButtonStyle Style = Button->GetStyle();
		const bool bLightButton = NormalTex == TexBtnLight.Get();
		const FLinearColor TextColor = bLightButton ? APB_INK_DARK : APB_WHITE;
		if (NormalTex)
		{
			Style.Normal = bLightButton
				? APB_TiledTexBrush(NormalTex, FLinearColor::White)
				: APB_TexBrush(NormalTex, FLinearColor::White);
			Style.Hovered = (bLightButton || HoverTex == TexBtnLight.Get())
				? APB_TiledTexBrush(HoverTex ? HoverTex : NormalTex, FLinearColor::White)
				: APB_TexBrush(HoverTex ? HoverTex : NormalTex, FLinearColor::White);
			Style.Pressed = bLightButton
				? APB_TiledTexBrush(NormalTex, FLinearColor(0.82f, 0.82f, 0.82f, 1.f))
				: APB_TexBrush(NormalTex, FLinearColor(0.82f, 0.82f, 0.82f, 1.f));
			Style.Disabled = bLightButton
				? APB_TiledTexBrush(NormalTex, FLinearColor(0.45f, 0.45f, 0.45f, 0.6f))
				: APB_TexBrush(NormalTex, FLinearColor(0.45f, 0.45f, 0.45f, 0.6f));
		}
		else
		{
			Button->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogTemp, Error, TEXT("APBFrontend CHARSELECT_ART_MISSING button=%s"), *Name);
			APB_MakeBoxBrush(Style.Normal, FLinearColor(0.f, 0.f, 0.f, 0.f));
			APB_MakeBoxBrush(Style.Hovered, FLinearColor(0.f, 0.f, 0.f, 0.f));
			APB_MakeBoxBrush(Style.Pressed, FLinearColor(0.f, 0.f, 0.f, 0.f));
			APB_MakeBoxBrush(Style.Disabled, FLinearColor(0.f, 0.f, 0.f, 0.f));
		}
		Style.NormalPadding = FMargin(4.f, 2.f);
		Style.PressedPadding = FMargin(4.f, 2.f);
		Button->SetStyle(Style);
		Button->SetIsEnabled(bEnabled);
		Button->OnHovered.AddDynamic(this, &UAPBFrontendWidget::OnAnyHover);
		UTextBlock* ButtonLabel = MakeLabel(Name + TEXT("_Label"), Label.ToUpper(), 11, TextColor);
		ButtonLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
		ButtonLabel->SetJustification(ETextJustify::Center);
		ButtonLabel->SetAutoWrapText(false);
		Button->AddChild(ButtonLabel);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(ButtonLabel->Slot))
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
			ButtonSlot->SetPadding(FMargin(0.f));
		}
		return Button;
	};

	UButton* AccountButton = MakeLobbyButton(TEXT("CSAccountButton"),
		S2011(TEXT("APBLoginScreen.AccountManagement"), TEXT("ACCOUNT")), TexBtnOff.Get(), TexBtnOn.Get());
	AccountButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnAccountLink);
	Place("UILabelButton_AccMgmt", AccountButton, 3);

	UButton* CreateB = MakeLobbyButton(TEXT("CSCreateBtn"),
		S2011(TEXT("CharacterSelectScreen.CreateCharacter"), TEXT("CREATE CHARACTER")), TexBtnLight.Get(), TexBtnLight.Get(), !bHas);
	CreateB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCreateCharOpen);
	CreateB->SetIsEnabled(!bHas);
	Place("UILabelButton_CreateCharacter", CreateB, 3);

	UButton* CharEntry = MakeLobbyButton(TEXT("CSCharEntry"), CharacterName.ToUpper(), TexBtnLight.Get(), TexBtnLight.Get(), bHas);
	CharEntry->SetIsEnabled(bHas);
	CharEntry->SetVisibility(bHas && TexBtnLight ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CharEntry->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnSelectExistingChar);
	Place("UIImage_Characterheader2", CharEntry, 3);

	UTexture2D* ListNormal = bHas ? TexListCellActive.Get() : TexListCell.Get();
	UTexture2D* ListHover = TexListCellActive.Get() ? TexListCellActive.Get() : ListNormal;
	UButton* CharacterRow = MakeLobbyButton(TEXT("CSCharacterRow"),
		bHas ? CharacterName : S2011(TEXT("CharacterSelectScreen.NoCharacterAvail"), TEXT("NO CHARACTER AVAILABLE")),
		ListNormal, ListHover, bHas);
	CharacterRow->SetIsEnabled(bHas);
	CharacterRow->SetVisibility(bHas && ListNormal ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CharacterRow->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnSelectExistingChar);
	PlaceRect(CharacterRow, 10.f, 226.f, 299.f, 20.f, 3);

	// Two-step delete: first click arms the confirm, second click deletes (auto-disarms after 4s).
	CharSelectDeleteBtn = MakeLobbyButton(TEXT("CSDeleteBtn"),
		S2011(TEXT("CharacterSelectScreen.DeleteCharacter"), TEXT("DELETE CHARACTER")), TexBtnOff.Get(), TexBtnOn.Get(), bHas);
	CharSelectDeleteBtn->SetIsEnabled(bHas);
	CharSelectDeleteBtn->SetVisibility(bHas && TexBtnOff ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CharSelectDeleteBtn->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnDeleteCharClicked);
	CharSelectDeleteLabel = Cast<UTextBlock>(CharSelectDeleteBtn->GetContent());
	Place("UILabelButton_DeleteCharacter", CharSelectDeleteBtn, 4);

	// 3D studio viewer: saved character mesh + clothing, auto-spin turntable, LMB drag to rotate.
	CharPreviewSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CSViewerSize"));
	CharPreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CSViewerImage"));
	CharPreviewImage->SetVisibility(bHas ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CharPreviewSizeBox->AddChild(CharPreviewImage);
	PlaceRect(CharPreviewSizeBox, 329.f, 40.f, 471.f, 462.f, 2);
	PlaceRect(MakeLabel(TEXT("CSLore"),
		TEXT("SAN PARO - TWO FACTIONS. ONE CITY. CHOOSE YOUR SIDE."), 11, APB_AMBER),
		340.f, 48.f, 460.f, 22.f, 3);
	ViewerHint = MakeLabel(TEXT("CSViewerHint"),
		TEXT("LMB DRAG TO ROTATE - AUTO-SPIN"), 9, APB_MUTED);
	ViewerHint->SetJustification(ETextJustify::Center);
	ViewerHint->SetVisibility(bHas ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PlaceRect(ViewerHint, 340.f, 476.f, 460.f, 20.f, 3);

	Place("UIImage_threatbackground", TexPanel(TEXT("CSThreatBG"), TexGenericContent, true), 2);
	UTexture2D* FactionTex = bEnforcer ? TexFactionEnf : TexFactionCrim;
	if (bHas && FactionTex)
	{
		UImage* FactionBadge = MakeImage(TEXT("CSFactionBadge"), FactionTex);
		Place("UIImage_Threat", FactionBadge, 4);
	}

	UTextBlock* NameVal = MakeLabel(TEXT("CSCharName"), CharacterName.ToUpper(), 18, bHas ? APB_INK : APB_INK_SEC);
	NameVal->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
	NameVal->SetAutoWrapText(false);
	Place("UILabel_CharacterName", NameVal, 4);
	Place("UILabel_Cash", MakeLabel(TEXT("CSCash"),
		bHas ? FactionName : TEXT("-"), 12, APB_INK_SEC), 4);
	Place("UIImage_threatbackground3", TexPanel(TEXT("CSRatingBG"), TexGenericContent, true), 3);
	if (TexRing)
	{
		PlaceRect(MakeImage(TEXT("CSRatingRing"), TexRing.Get()), 678.f, 502.f, 40.f, 40.f, 4);
	}
	UTextBlock* Rating = MakeLabel(TEXT("CSRating"), bHas ? FString::FromInt(ThreatRating) : TEXT("-"), 18, APB_AMBER_HI);
	Rating->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
	Rating->SetJustification(ETextJustify::Center);
	Place("UILabel_Rating", Rating, 4);

	UTextBlock* PlaytimeFooter = MakeLabel(TEXT("CSTotalPlaytime"),
		S2011(TEXT("CharacterSelectScreen.TotalPlaytime"), TEXT("Total Playtime: <Time>")).Replace(TEXT("<Time>"), TEXT("—")),
		9, APB_INK_SEC);
	PlaytimeFooter->SetAutoWrapText(false);
	PlaytimeFooter->SetJustification(ETextJustify::Center);
	Place("UILabel_CharacterPlayedTime", PlaytimeFooter, 4);

	if (TexDropShadow)
	{
		PlaceRect(TexPanel(TEXT("CSQuitShadow"), TexDropShadow, false), -3.f, 552.f, 183.f, 51.f, 3);
		PlaceRect(TexPanel(TEXT("CSOptionsShadow"), TexDropShadow, false), 159.f, 552.f, 183.f, 51.f, 3);
		PlaceRect(TexPanel(TEXT("CSLogoutShadow"), TexDropShadow, false), 388.f, 552.f, 218.f, 51.f, 3);
		PlaceRect(TexPanel(TEXT("CSPlayShadow"), TexDropShadow, false), 585.f, 552.f, 218.f, 51.f, 3);
	}
	UButton* PlayB = MakeLobbyButton(TEXT("CSPlayBtn"), S2011(TEXT("CharacterSelectScreen.Play"), TEXT("PLAY")), TexBtnOn.Get(), TexBtnLight.Get(), bHas);
	PlayB->SetIsEnabled(bHas);
	PlayB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnSelectExistingChar);
	Place("cUILabelButton_Play", PlayB, 4);
	UButton* LogoutB = MakeLobbyButton(TEXT("CSLogoutBtn"), S2011(TEXT("CharacterSelectScreen.Logout"), TEXT("Logout")), TexBtnOff.Get(), TexBtnOn.Get());
	LogoutB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnBackToLogin);
	Place("UILabelButton_Logout", LogoutB, 4);
	UButton* OptionsB = MakeLobbyButton(TEXT("CSOptionsBtn"), S2011(TEXT("APBLoginScreen.Options"), TEXT("Options")), TexBtnOff.Get(), TexBtnOn.Get());
	OptionsB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnOpenSettings);
	Place("UILabelButton_Options", OptionsB, 4);
	UButton* QuitB = MakeLobbyButton(TEXT("CSQuitBtn"), S2011(TEXT("CharacterSelectScreen.ExitGame"), TEXT("Quit")), TexBtnOff.Get(), TexBtnOn.Get());
	QuitB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnExitDesktop);
	Place("UILabelButton_Quit", QuitB, 4);
}

void UAPBFrontendWidget::SetStage(EAPBFrontendStage Stage)
{
	// Any stage change dismisses a fullscreen replay overlay first (defensive;
	// the overlay already swallows all input while active).
	if (bReplayActive)
	{
		StopReplayMovie();
	}
	const bool bLeavePreview = (CurrentStage == EAPBFrontendStage::CharacterSelect || CurrentStage == EAPBFrontendStage::CharacterCreate)
		&& Stage != EAPBFrontendStage::CharacterSelect && Stage != EAPBFrontendStage::CharacterCreate;
	if (bLeavePreview)
	{
		DestroyCharacterPreview();
	}
	// A stage change cancels any pending two-step delete confirm (defensive; the
	// stale label pointer is already replaced by the rebuild below).
	bDeleteArmed = false;
	CurrentStage = Stage;
	RebuildStageBody();
	if (Stage != EAPBFrontendStage::Splash)
	{
		if (BgArt) BgArt->SetVisibility(ESlateVisibility::Collapsed);
		ApplyStageBackgroundVideo(Stage);
	}
	// Theme plays whenever the login screen is shown (splash-continue and
	// -APBHoldStage=Login both route through here; bMusicStarted guards re-entry).
	if (Stage == EAPBFrontendStage::Login)
	{
		StartLoginMusic();
	}
	LogStage();
	UpdateViewportScale();
	if (Stage == EAPBFrontendStage::CharacterCreate)
	{
		EnsureCharacterPreview();
		RefreshCharacterPreviewFromUI();
	}
	else if (Stage == EAPBFrontendStage::CharacterSelect)
	{
		EnsureCharacterPreview();
		RefreshCharacterPreviewFromSaved();
	}
	if (LogoImage)
	{
		LogoImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (LogoSizeBox)
	{
		// No in-window wordmark on any dialog stage (spec §3.1/§4.1 — logo is Splash/Loading only)
		LogoSizeBox->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAPBFrontendWidget::RebuildStageBody()
{
	if (!BodyBox || !WidgetTree) return;		UserBox = nullptr;
		PassBox = nullptr;
		LoginBtn = nullptr;
		LoginLabel = nullptr;
		CharNameBox = nullptr;
	RegisterEmailBox = RegisterPasswordBox = RegisterConfirmBox = nullptr;
	RegisterTosCheck = RegisterPrivacyCheck = RegisterCaptchaCheck = nullptr;
	RememberCheck = nullptr;
	EnforcerCheck = nullptr;
	FactionCriminalBtn = FactionEnforcerBtn = nullptr;
	WardrobeItemCombo = nullptr;
	SymbolCountLabel = nullptr;
	PaletteGrid = nullptr;
	DistrictCombo = nullptr;
	CharHeightSlider = CharBulkSlider = nullptr;
	CharHeightLabel = CharBulkLabel = nullptr;
	GenderCombo = SkinToneCombo = FacePresetCombo = HairStyleCombo = HairColorCombo = EyeColorCombo = AgeGroupCombo = nullptr;
	MakeupCombo = nullptr;
	MakeupChannelLabel = nullptr;
	ModeBasicBtn = ModeAdvancedBtn = nullptr;
	AdvancedPanel = nullptr;
	ScarAddBtn = ScarRemoveBtn = nullptr;
	ScarCountLabel = nullptr;
	PreviewSummary = nullptr;
	CharPreviewImage = nullptr;
	CharPreviewSizeBox = nullptr;
	MenuVolumeSlider = nullptr;
	VolumeValueText = nullptr;
	ResolutionCombo = nullptr;
	DisplayModeLabel = nullptr;

	const FLinearColor PanelCol = APB_PANEL;

	if (DesignScale) DesignScale->SetVisibility(ESlateVisibility::Collapsed);

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
		if (PanelSizeBox) PanelSizeBox->SetVisibility(ESlateVisibility::Collapsed);
		if (FooterBar) FooterBar->SetVisibility(ESlateVisibility::Collapsed);
		if (SplashLogo) SplashLogo->SetVisibility(ESlateVisibility::Collapsed);
		if (LogoImage) LogoImage->SetVisibility(ESlateVisibility::Collapsed);
		if (DesignScale) DesignScale->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		BuildLoginDesign();
		break;
	}
		case EAPBFrontendStage::Register:
	{
		BeginStageContent(true);
		ApplyPanelChrome(true, PanelCol);
		BuildRegistrationBody();
		break;
	}
	case EAPBFrontendStage::Credits:
	{
		BeginStageContent(true);
		ApplyPanelChrome(true, PanelCol);
		if (TitleText) TitleText->SetText(FText::FromString(TEXT("CREDITS")));
		if (SubtitleText) SubtitleText->SetText(FText::FromString(TEXT("ALL POINTS BULLETIN")));
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("2011 RTW frontend presentation")));
		AddToScroll(MakeLabel(TEXT("CreditsBody"), TEXT("All menu presentation, strings, sounds, and movies are sourced from the 2011 RTW frontend reference."), 13, APB_INK), 12.f);
		AddToScroll(MakeLabel(TEXT("CreditsSource"), TEXT("Source: APBMenus_GameFlowScenes, APBUserInterface, frontend Wwise banks, and the preserved intro movie conversion."), 11, APB_MUTED), 8.f);
		UButton* CreditsBack = MakeAccentButton(TEXT("CreditsBack"), TEXT("BACK TO LOGIN"), APB_BTN);
		CreditsBack->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnRegistrationBack);
		AddToScroll(CreditsBack, 16.f);
		break;
	}
	case EAPBFrontendStage::ReplayVideos:
	{
		BeginStageContent(true);
		ApplyPanelChrome(true, PanelCol);
		if (TitleText) TitleText->SetText(FText::FromString(TEXT("REPLAY VIDEOS")));
		if (SubtitleText) SubtitleText->SetText(FText::FromString(TEXT("APB MOVIE LIBRARY")));
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Select a movie to replay; any key / click stops it.")));
		ReplayMovieButtons.Reset();
		ReplayMoviePaths.Reset();
		// Dynamic library: every staged movie under Content/Movies (retail BIK
		// conversions + baked login/background beds), sorted, mp4 preferred.
		CollectReplayMovies(ReplayMoviePaths);
		for (const FString& MoviePath : ReplayMoviePaths)
		{
			const FString Label = FriendlyMovieName(MoviePath);
			UButton* Btn = MakeButton(TEXT("ReplayMovie") + FString::FromInt(ReplayMovieButtons.Num()), Label);
			// Dynamic multicast OnClicked takes no payload: the shared UFUNCTION
			// handler finds the clicked button by hover, then plays its path.
			Btn->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnReplayMovieChosen);
			Btn->SetToolTipText(FText::FromString(MoviePath));
			ReplayMovieButtons.Add(Btn);
			AddToScroll(Btn, 6.f);
		}
		UButton* ReplayIntro = MakeAccentButton(TEXT("ReplayIntro"), TEXT("PLAY INTRO MOVIE"), APB_AMBER);
		ReplayIntro->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnReplayIntroMovie);
		AddToScroll(ReplayIntro, 12.f);
		UButton* ReplayBack = MakeButton(TEXT("ReplayBack"), TEXT("BACK TO LOGIN"));
		ReplayBack->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnRegistrationBack);
		AddToScroll(ReplayBack, 8.f);
		if (ReplayMoviePaths.Num() == 0 && StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("No movies found under Content/Movies.")));
		}
		break;
	}
	case EAPBFrontendStage::CharacterSelect:
		{
			if (PanelSizeBox) PanelSizeBox->SetVisibility(ESlateVisibility::Collapsed);
			if (FooterBar) FooterBar->SetVisibility(ESlateVisibility::Collapsed);
			if (SplashLogo) SplashLogo->SetVisibility(ESlateVisibility::Collapsed);
			if (LogoImage) LogoImage->SetVisibility(ESlateVisibility::Collapsed);
			if (DesignScale) DesignScale->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			BuildCharacterSelectDesign();
			LogStage(TEXT("char_select_ui_built"));
			break;
		}
	case EAPBFrontendStage::CharacterCreate:
	{
		BeginStageContent(true); // long form may scroll
		ApplyPanelChrome(true, PanelCol);
		if (TitleText) TitleText->SetText(FText::FromString(SRetail(TEXT("CharacterCreateScreen.CharacterCreationTitle"), TEXT("CREATE"))));
		if (SubtitleText) SubtitleText->SetText(FText::FromString(SRetail(TEXT("CharacterCreateScreen.CharacterCreationSubtitle"), TEXT("CHARACTER"))));
		if (StatusText) StatusText->SetText(FText::FromString(SRetail(TEXT("CharacterCreateScreen.ChooseFaction"), TEXT("Choose Your Faction"))));
		if (HintText) HintText->SetText(FText::FromString(SRetail(TEXT("CharacterCreateScreen.ToolTip_Exit_Creation"), TEXT("Discard appearance and return to Faction Select"))));

		// 3D studio — full-body viewer with retail camera presets
		CharPreviewSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CharPreviewSizeBox"));
		CharPreviewSizeBox->SetHeightOverride(380.f);
		CharPreviewSizeBox->SetWidthOverride(300.f);
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
		AddToScroll(MakeLabel(TEXT("Prev3DLabel"), SRetail(TEXT("CharacterCustomisationScreens.Character"), TEXT("CHARACTER")), 11, APB_AMBER), 2.f);
		UHorizontalBox* CamRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CamRow"));
		UButton* CamFull = MakeButton(TEXT("CamFull"), SRetail(TEXT("CharacterCustomisationScreens.ToolTipCamFullBody"), TEXT("FULL BODY")));
		CamFull->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCamFullBody);
		UButton* CamFaceB = MakeButton(TEXT("CamFaceB"), SRetail(TEXT("CharacterCustomisationScreens.ToolTipCamFace"), TEXT("FACE")));
		CamFaceB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCamFace);
		UButton* CamDollyB = MakeButton(TEXT("CamDollyB"), SRetail(TEXT("CharacterCustomisationScreens.ToolTipCamDolly"), TEXT("DOLLY")));
		CamDollyB->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCamDolly);
		for (UButton* B : { CamFull, CamFaceB, CamDollyB })
		{
			if (UHorizontalBoxSlot* HS = CamRow->AddChildToHorizontalBox(B))
			{
				HS->SetPadding(FMargin(2.f));
				HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		AddToScroll(CamRow, 2.f);

		AddToScroll(MakeLabel(TEXT("cn"), SRetail(TEXT("CharacterCreateScreen.CharacterName"), TEXT("NAME")), 12, APB_AMBER), 6.f);
		CharNameBox = MakeTextField(TEXT("CharCreateNameBox"), SRetail(TEXT("CharacterCreateScreen.NameYourCharacter"), TEXT("Name Your Character")), false);
		CharNameBox->SetText(FText::GetEmpty());
		AddToScroll(CharNameBox, 2.f);

		// GENDER — retail first step (ChooseGender / Male / Female)
		AddToScroll(MakeLabel(TEXT("genderTitle"), SRetail(TEXT("CharacterCreateScreen.ChooseGender"), TEXT("GENDER")), 12, APB_AMBER), 10.f);
		GenderCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("GenderCombo"));
		GenderCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
		GenderCombo->AddOption(SRetail(TEXT("CharacterCreateScreen.Male"), TEXT("MALE")));
		GenderCombo->AddOption(SRetail(TEXT("CharacterCreateScreen.Female"), TEXT("FEMALE")));
		GenderCombo->SetSelectedIndex(0);
		GenderCombo->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnGenderComboChanged);
		AddToScroll(GenderCombo, 2.f);

		// FACTION — icon row + buttons (kept from prior flow)
		AddToScroll(MakeLabel(TEXT("facTitle"), SRetail(TEXT("CharacterCreateScreen.Faction"), TEXT("faction:")), 12, APB_AMBER), 10.f);
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
		FactionCriminalBtn = MakeAccentButton(TEXT("FacCrim"), SRetail(TEXT("CharacterCreateScreen.Criminal"), TEXT("criminal")), FLinearColor(0.55f, 0.12f, 0.12f, 1.f));
		FactionEnforcerBtn = MakeAccentButton(TEXT("FacEnf"), SRetail(TEXT("CharacterCreateScreen.Enforcement"), TEXT("Enforcer")), FLinearColor(0.12f, 0.14f, 0.18f, 1.f));
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

		// BODY SHAPE — live sliders (retail BodyShape_caps), no more unavailable stub
		AddToScroll(MakeLabel(TEXT("BodyShapeTitle"), SRetail(TEXT("CharacterCustomisationScreens.BodyShape_caps"), TEXT("BODY SHAPE")), 12, APB_AMBER), 8.f);
		CharHeightLabel = MakeLabel(TEXT("CharHeightLabel"), TEXT("HEIGHT  1.00"), 11, APB_MUTED);
		AddToScroll(CharHeightLabel, 2.f);
		CharHeightSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("CharHeightSlider"));
		CharHeightSlider->SetMinValue(0.8f);
		CharHeightSlider->SetMaxValue(1.2f);
		CharHeightSlider->SetValue(1.0f);
		CharHeightSlider->SetStepSize(0.01f);
		CharHeightSlider->OnValueChanged.AddDynamic(this, &UAPBFrontendWidget::OnCharHeightChanged);
		AddToScroll(CharHeightSlider, 2.f);
		CharBulkLabel = MakeLabel(TEXT("CharBulkLabel"), TEXT("BUILD  0.95"), 11, APB_MUTED);
		AddToScroll(CharBulkLabel, 2.f);
		CharBulkSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("CharBulkSlider"));
		CharBulkSlider->SetMinValue(0.8f);
		CharBulkSlider->SetMaxValue(1.2f);
		CharBulkSlider->SetValue(0.95f);
		CharBulkSlider->SetStepSize(0.01f);
		CharBulkSlider->OnValueChanged.AddDynamic(this, &UAPBFrontendWidget::OnCharBulkChanged);
		AddToScroll(CharBulkSlider, 2.f);
		UHorizontalBox* BuildRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuildRow"));
		UButton* Bskinny = MakeButton(TEXT("Bskinny"), SRetail(TEXT("CharacterCustomisationScreens.BuildSkinny"), TEXT("SKINNY")));
		Bskinny->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnBuildPresetSkinny);
		UButton* Bavg = MakeButton(TEXT("Bavg"), SRetail(TEXT("CharacterCustomisationScreens.BuildAverage"), TEXT("AVERAGE")));
		Bavg->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnBuildPresetAverage);
		UButton* Bbulky = MakeButton(TEXT("Bbulky"), SRetail(TEXT("CharacterCustomisationScreens.BuildBulky"), TEXT("BULKY")));
		Bbulky->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnBuildPresetBulky);
		UButton* Bmusc = MakeButton(TEXT("Bmusc"), SRetail(TEXT("CharacterCustomisationScreens.BuildFat"), TEXT("MUSCULAR")));
		Bmusc->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnBuildPresetMuscular);
		for (UButton* B : { Bskinny, Bavg, Bbulky, Bmusc })
		{
			if (UHorizontalBoxSlot* HS = BuildRow->AddChildToHorizontalBox(B))
			{
				HS->SetPadding(FMargin(2.f));
				HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		AddToScroll(BuildRow, 2.f);

		// SKIN TONE (retail SkinTone_caps)
		AddToScroll(MakeLabel(TEXT("SkinToneTitle"), SRetail(TEXT("CharacterCustomisationScreens.SkinTone_caps"), TEXT("SKIN TONE")), 12, APB_AMBER), 8.f);
		SkinToneCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("SkinToneCombo"));
		SkinToneCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
		SkinToneCombo->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnSkinToneComboChanged);
		AddToScroll(SkinToneCombo, 2.f);

		// FACE (retail FACE / face_preset)
		AddToScroll(MakeLabel(TEXT("FaceTitle"), SRetail(TEXT("CharacterCustomisationScreens.FACE"), TEXT("FACE")), 12, APB_AMBER), 8.f);
		FacePresetCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("FacePresetCombo"));
		FacePresetCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
		FacePresetCombo->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnFacePresetComboChanged);
		AddToScroll(FacePresetCombo, 2.f);

		// HAIR (retail Hair / Hair_Style / Hair_Color)
		AddToScroll(MakeLabel(TEXT("HairTitle"), SRetail(TEXT("CharacterCustomisationScreens.Hair"), TEXT("HAIR")), 12, APB_AMBER), 8.f);
		HairStyleCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("HairStyleCombo"));
		HairStyleCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
		HairStyleCombo->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnHairStyleComboChanged);
		AddToScroll(HairStyleCombo, 2.f);
		HairColorCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("HairColorCombo"));
		HairColorCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
		HairColorCombo->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnHairColorComboChanged);
		AddToScroll(HairColorCombo, 2.f);

		// EYES (retail Eyes / IrisColor)
		AddToScroll(MakeLabel(TEXT("EyesTitle"), SRetail(TEXT("CharacterCustomisationScreens.Eyes"), TEXT("EYES")), 12, APB_AMBER), 8.f);
		EyeColorCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("EyeColorCombo"));
		EyeColorCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
		EyeColorCombo->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnEyeColorComboChanged);
		AddToScroll(EyeColorCombo, 2.f);

		// Basic / Advanced mode toggle (retail QuickOptions / AdvancedScreenButton)
		UHorizontalBox* ModeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ModeRow"));
		ModeBasicBtn = MakeAccentButton(TEXT("ModeBasic"), SRetail(TEXT("CharacterCustomisationScreens.QuickOptions"), TEXT("QUICK MODE")), APB_BTN_OK);
		ModeBasicBtn->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnEditorModeBasic);
		ModeAdvancedBtn = MakeAccentButton(TEXT("ModeAdvanced"), SRetail(TEXT("CharacterCustomisationScreens.AdvancedScreenButton"), TEXT("ADVANCED MODE")), APB_BTN);
		ModeAdvancedBtn->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnEditorModeAdvanced);
		for (UButton* B : { ModeBasicBtn, ModeAdvancedBtn })
		{
			if (UHorizontalBoxSlot* HS = ModeRow->AddChildToHorizontalBox(B))
			{
				HS->SetPadding(FMargin(2.f));
				HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		AddToScroll(ModeRow, 4.f);

		// ADVANCED PANEL — age, makeup, scars, tattoos (retail sections)
		AdvancedPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AdvancedPanel"));
		{
			FSlateBrush Brush;
			APB_MakeBoxBrush(Brush, APB_WELL_DARK);
			AdvancedPanel->SetBrush(Brush);
			AdvancedPanel->SetBrushColor(FLinearColor::White);
			AdvancedPanel->SetPadding(FMargin(10.f));
		}
		UVerticalBox* AdvBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AdvBox"));
		AdvancedPanel->AddChild(AdvBox);
		UTextBlock* AgeTitle = MakeLabel(TEXT("AgeTitle"), SRetail(TEXT("CharacterCustomisationScreens.AgeGroup"), TEXT("AGE GROUP")), 11, APB_AMBER);
		AdvBox->AddChildToVerticalBox(AgeTitle);
		AgeGroupCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("AgeGroupCombo"));
		AgeGroupCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
		AgeGroupCombo->AddOption(SRetail(TEXT("CharacterCustomisationScreens.AgeTeenager"), TEXT("18 - 20 Years")));
		AgeGroupCombo->AddOption(SRetail(TEXT("CharacterCustomisationScreens.AgeTwenties"), TEXT("20 - 30 Years")));
		AgeGroupCombo->AddOption(SRetail(TEXT("CharacterCustomisationScreens.AgeThirties"), TEXT("30 - 40 Years")));
		AgeGroupCombo->AddOption(SRetail(TEXT("CharacterCustomisationScreens.AgeForties"), TEXT("40 - 50 Years")));
		AgeGroupCombo->SetSelectedIndex(0);
		AgeGroupCombo->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnAgeGroupComboChanged);
		AdvBox->AddChildToVerticalBox(AgeGroupCombo);

		MakeupChannelLabel = MakeLabel(TEXT("MakeupChannelLabel"), SRetail(TEXT("CharacterCustomisationScreens.MakeUp_Lipstick"), TEXT("LIPSTICK")), 11, APB_AMBER);
		AdvBox->AddChildToVerticalBox(MakeupChannelLabel);
		UHorizontalBox* MupRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MupRow"));
		UButton* MCh1 = MakeButton(TEXT("MCh1"), SRetail(TEXT("CharacterCustomisationScreens.MakeUp_Lipstick"), TEXT("LIPSTICK")));
		MCh1->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnMakeupChannel1);
		UButton* MCh2 = MakeButton(TEXT("MCh2"), SRetail(TEXT("CharacterCustomisationScreens.MakeUp_EyeShadow"), TEXT("EYE SHADOW")));
		MCh2->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnMakeupChannel2);
		UButton* MCh3 = MakeButton(TEXT("MCh3"), SRetail(TEXT("CharacterCustomisationScreens.MakeUp_EyeLiner"), TEXT("EYE LINER")));
		MCh3->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnMakeupChannel3);
		UButton* MCh4 = MakeButton(TEXT("MCh4"), SRetail(TEXT("CharacterCustomisationScreens.Blusher"), TEXT("BLUSHER")));
		MCh4->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnMakeupChannel4);
		for (UButton* B : { MCh1, MCh2, MCh3, MCh4 })
		{
			if (UHorizontalBoxSlot* HS = MupRow->AddChildToHorizontalBox(B))
			{
				HS->SetPadding(FMargin(1.f));
				HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		AdvBox->AddChildToVerticalBox(MupRow);
		MakeupCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("MakeupCombo"));
		MakeupCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
		MakeupCombo->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnMakeupComboChanged);
		AdvBox->AddChildToVerticalBox(MakeupCombo);

		UTextBlock* ScarTitle = MakeLabel(TEXT("ScarTitle"), SRetail(TEXT("CharacterCustomisationScreens.Scars"), TEXT("SCARS")), 11, APB_AMBER);
		AdvBox->AddChildToVerticalBox(ScarTitle);
		UHorizontalBox* ScarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ScarRow"));
		ScarAddBtn = MakeButton(TEXT("ScarAdd"), SRetail(TEXT("CharacterCustomisationScreens.AddScar"), TEXT("ADD SCAR")));
		ScarAddBtn->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnAddScarClicked);
		ScarRemoveBtn = MakeButton(TEXT("ScarRemove"), SRetail(TEXT("CharacterCustomisationScreens.RemoveScar"), TEXT("REMOVE SCAR")));
		ScarRemoveBtn->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnRemoveScarClicked);
		for (UButton* B : { ScarAddBtn, ScarRemoveBtn })
		{
			if (UHorizontalBoxSlot* HS = ScarRow->AddChildToHorizontalBox(B))
			{
				HS->SetPadding(FMargin(1.f));
				HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		AdvBox->AddChildToVerticalBox(ScarRow);
		ScarCountLabel = MakeLabel(TEXT("ScarCount"), TEXT("SCARS: 0"), 10, APB_MUTED);
		AdvBox->AddChildToVerticalBox(ScarCountLabel);

		UTextBlock* TattooTitle = MakeLabel(TEXT("TattooTitle"), SRetail(TEXT("CharacterCustomisationScreens.Tattoos"), TEXT("TATTOOS")), 11, APB_AMBER);
		AdvBox->AddChildToVerticalBox(TattooTitle);
		UHorizontalBox* TattooRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TattooRow"));
		UButton* AddSymbol = MakeButton(TEXT("AddSymbol"), SRetail(TEXT("CharacterCustomisationScreens.ChoosePrimitive"), TEXT("CHANGE TATTOO")));
		AddSymbol->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnAddSymbol);
		TattooRow->AddChildToHorizontalBox(AddSymbol);
		UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
		SymbolCountLabel = MakeLabel(TEXT("SymbolCount"), APB ? FString::Printf(TEXT("%d layers"), APB->GetSymbolLayerCount()) : TEXT("0 layers"), 10, APB_MUTED);
		if (UHorizontalBoxSlot* HS = TattooRow->AddChildToHorizontalBox(SymbolCountLabel)) HS->SetPadding(FMargin(8.f, 0.f));
		AdvBox->AddChildToVerticalBox(TattooRow);
		AddToScroll(AdvancedPanel, 4.f);
		AdvancedPanel->SetVisibility(bEditorAdvancedMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

		// WARDROBE — secondary (clothes), after the character editor
		static const TCHAR* WardrobeLabels[] = { TEXT("Tops"), TEXT("Bottoms"), TEXT("Footwear"), TEXT("Headwear"), TEXT("Handwear"), TEXT("Facewear"), TEXT("Underwear"), TEXT("Outerwear"), TEXT("Skirts & Dresses"), TEXT("Jewellery"), TEXT("Belts"), TEXT("Accessories"), TEXT("Webbing"), TEXT("Armour"), TEXT("Body & Facial Hair") };
		AddToScroll(MakeLabel(TEXT("WardrobeTitle"), SRetail(TEXT("CharacterCustomisationScreens.CustomizeCharacter"), TEXT("CUSTOMIZATION")), 12, APB_AMBER), 8.f);
		UUniformGridPanel* WardrobeTabs = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("WardrobeTabs"));
		for (int32 TabId = 1; TabId <= 15; ++TabId)
		{
			UButton* TabButton = MakeButton(FString::Printf(TEXT("WardrobeTab%d"), TabId), WardrobeLabels[TabId - 1]);
			switch (TabId)
			{
			case 1: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab1); break;
			case 2: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab2); break;
			case 3: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab3); break;
			case 4: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab4); break;
			case 5: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab5); break;
			case 6: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab6); break;
			case 7: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab7); break;
			case 8: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab8); break;
			case 9: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab9); break;
			case 10: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab10); break;
			case 11: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab11); break;
			case 12: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab12); break;
			case 13: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab13); break;
			case 14: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab14); break;
			case 15: TabButton->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnWardrobeTab15); break;
			default: break;
			}
			USizeBox* TabSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("WardrobeTabSize%d"), TabId));
			TabSize->SetWidthOverride(100.f);
			TabSize->SetHeightOverride(44.f);
			TabSize->AddChild(TabButton);
			WardrobeTabs->AddChildToUniformGrid(TabSize, (TabId - 1) / 5, (TabId - 1) % 5);
		}
		AddToScroll(WardrobeTabs, 4.f);
		WardrobeItemCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("WardrobeItemCombo"));
		WardrobeItemCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
		WardrobeItemCombo->OnSelectionChanged.AddDynamic(this, &UAPBFrontendWidget::OnClothingSelectionChanged);
		AddToScroll(WardrobeItemCombo, 2.f);
		AddToScroll(MakeLabel(TEXT("PaletteLabel"), SRetail(TEXT("CharacterCustomisationScreens.SkinColor"), TEXT("Skin Color")), 11, APB_AMBER), 4.f);
		PaletteGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("PaletteGrid"));
		WardrobePaletteColors = APB ? APB->GetPaletteColors(TEXT("Clothing"), 0) : TArray<FLinearColor>();
		const int32 PaletteCount = FMath::Min(WardrobePaletteColors.Num(), 24);
		for (int32 ColorIndex = 0; ColorIndex < PaletteCount; ++ColorIndex)
		{
			UButton* Swatch = MakeButton(FString::Printf(TEXT("PaletteSwatch%d"), ColorIndex), TEXT(" "));
			Swatch->SetBackgroundColor(WardrobePaletteColors[ColorIndex]);
			switch (ColorIndex)
			{
			#define APB_SWATCH_HANDLER(Index) case Index: Swatch->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnPaletteSwatch##Index); break;
			APB_SWATCH_HANDLER(0) APB_SWATCH_HANDLER(1) APB_SWATCH_HANDLER(2) APB_SWATCH_HANDLER(3) APB_SWATCH_HANDLER(4) APB_SWATCH_HANDLER(5)
			APB_SWATCH_HANDLER(6) APB_SWATCH_HANDLER(7) APB_SWATCH_HANDLER(8) APB_SWATCH_HANDLER(9) APB_SWATCH_HANDLER(10) APB_SWATCH_HANDLER(11)
			APB_SWATCH_HANDLER(12) APB_SWATCH_HANDLER(13) APB_SWATCH_HANDLER(14) APB_SWATCH_HANDLER(15) APB_SWATCH_HANDLER(16) APB_SWATCH_HANDLER(17)
			APB_SWATCH_HANDLER(18) APB_SWATCH_HANDLER(19) APB_SWATCH_HANDLER(20) APB_SWATCH_HANDLER(21) APB_SWATCH_HANDLER(22) APB_SWATCH_HANDLER(23)
			#undef APB_SWATCH_HANDLER
			default: break;
			}
			PaletteGrid->AddChildToUniformGrid(Swatch, ColorIndex / 8, ColorIndex % 8);
		}
		if (PaletteCount > 0) AddToScroll(PaletteGrid, 2.f);
		UButton* Randomize = MakeAccentButton(TEXT("RandomizeAppearance"), SRetail(TEXT("CharacterCustomisationScreens.Randomize"), TEXT("Randomize")), APB_AMBER);
		Randomize->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnRandomizeAppearance);
		AddToScroll(Randomize, 8.f);
		ActiveWardrobeTab = 1;
		WardrobeItemIds.SetNum(15);
		if (APB)
		{
			for (int32 TabId = 1; TabId <= 15; ++TabId)
			{
				WardrobeItemIds[TabId - 1].Reset();
			}
		}
		RefreshWardrobeItems();

		PreviewSummary = MakeLabel(TEXT("PrevSummary"), TEXT("RETAIL PREVIEW"), 11, APB_MUTED);
		AddToScroll(PreviewSummary, 6.f);

		UButton* Confirm = MakeAccentButton(TEXT("ConfirmChar"), SRetail(TEXT("CharacterCreateScreen.CreateCharacter"), TEXT("Create Character")), FLinearColor(0.12f, 0.42f, 0.22f, 1.f));
		Confirm->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCharCreateConfirm);
		AddToScroll(Confirm, 10.f);
		UButton* Back = MakeButton(TEXT("CreateBack"), SRetail(TEXT("CharacterCreateScreen.BackToFactionSelect"), TEXT("Back To Faction Select")));
		Back->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnCharCreateBack);
		AddToScroll(Back, 6.f);

		RefreshEditorCombos();
		RefreshCharacterEditorFromUI();
		LogStage(TEXT("char_create_ui_character_editor"));
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
		DistrictAbbrevs.Reset();
		DistrictCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("DistCombo"));
		DistrictCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
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
					DistrictAbbrevs.Add(Parts.Num() > 3 && !Parts[3].IsEmpty() ? Parts[3] : Parts[0]);
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
				USizeBox* PhotoSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("DistrictPhotoSize_%d"), i));
				PhotoSize->SetWidthOverride(256.f);
				PhotoSize->SetHeightOverride(195.f);
				if (DistrictPhoto)
				{
					UImage* Photo = MakeImage(FString::Printf(TEXT("DistrictPhoto_%d"), i), DistrictPhoto);
					PhotoSize->AddChild(Photo);
				}
				else
				{
					// No staged 2011 photo for this district: branded flat plate with the
					// source abbrev so the row never renders an empty gap.
					UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("DistrictPlate_%d"), i));
					FSlateBrush PlateBrush;
					APB_MakeBoxBrush(PlateBrush, APB_WELL_DARK);
					Plate->SetBrush(PlateBrush);
					Plate->SetBrushColor(FLinearColor::White);
					Plate->SetPadding(FMargin(0.f));
					UTextBlock* Tag = MakeLabel(FString::Printf(TEXT("DistrictPlateTag_%d"), i),
						DistrictAbbrevs.IsValidIndex(i) ? DistrictAbbrevs[i].ToUpper() : TEXT("?"), 26, APB_AMBER);
					Tag->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 26));
					Tag->SetJustification(ETextJustify::Center);
					if (UBorderSlot* PlateSlot = Cast<UBorderSlot>(Plate->AddChild(Tag)))
					{
						PlateSlot->SetVerticalAlignment(VAlign_Center);
						PlateSlot->SetHorizontalAlignment(HAlign_Center);
					}
					PhotoSize->AddChild(Plate);
				}
				if (UHorizontalBoxSlot* HS = DistrictRowH->AddChildToHorizontalBox(PhotoSize))
				{
					HS->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
					HS->SetVerticalAlignment(VAlign_Center);
				}

				UVerticalBox* DistrictInfo = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("DistrictInfo_%d"), i));
				UTextBlock* DistrictName = MakeLabel(FString::Printf(TEXT("DistrictName_%d"), i), DistrictNames[i].ToUpper(), 20, APB_WHITE);
				DistrictName->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
				if (UVerticalBoxSlot* VS = DistrictInfo->AddChildToVerticalBox(DistrictName)) VS->SetPadding(FMargin(0.f, 12.f, 0.f, 4.f));
				DistrictInfo->AddChildToVerticalBox(MakeLabel(FString::Printf(TEXT("DistrictId_%d"), i),
					FString::Printf(TEXT("%s - ONLINE"), *DistrictAbbrevs[i]), 13, APB_MUTED));

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
		if (TitleText) TitleText->SetText(FText::FromString(TEXT("OPTIONS")));
		if (SubtitleText) SubtitleText->SetText(FText::FromString(TEXT("VIDEO SETTINGS - AUDIO SETTINGS")));
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Applies resolution + aspect scaling")));
		if (HintText) HintText->SetText(FText::FromString(TEXT("Fit works on 4:3 / 16:9 / 16:10")));

		// --- Video settings ---
		AddToScroll(MakeLabel(TEXT("VideoSect"), TEXT("VIDEO SETTINGS"), 14, APB_AMBER_HI), 4.f);
		AddToScroll(MakeLabel(TEXT("resL"), TEXT("RESOLUTION"), 12, APB_AMBER), 4.f);
		ResolutionCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ResCombo"));
		ResolutionCombo->SetMaxListHeight(apb_layout::SafePopupMaxHeight(1080.f));
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

		// --- Audio settings ---
		AddToScroll(MakeLabel(TEXT("AudioSect"), TEXT("AUDIO SETTINGS"), 14, APB_AMBER_HI), 12.f);
		AddToScroll(MakeLabel(TEXT("audL"), TEXT("MENU VOLUME"), 12, APB_AMBER), 4.f);
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
		UHorizontalBox* VolPresetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VolPresetRow"));
		UButton* VolMute = MakeAccentButton(TEXT("VolMute"), TEXT(" Mute "), APB_BTN);
		UButton* VolLow = MakeAccentButton(TEXT("VolLow"), TEXT(" Low "), APB_BTN);
		UButton* VolMed = MakeAccentButton(TEXT("VolMed"), TEXT(" Medium "), APB_BTN);
		UButton* VolHigh = MakeAccentButton(TEXT("VolHigh"), TEXT(" High "), APB_BTN);
		VolMute->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnVolPresetMute);
		VolLow->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnVolPresetLow);
		VolMed->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnVolPresetMed);
		VolHigh->OnClicked.AddDynamic(this, &UAPBFrontendWidget::OnVolPresetHigh);
		for (UButton* B : { VolMute, VolLow, VolMed, VolHigh })
		{
			if (UHorizontalBoxSlot* HS = VolPresetRow->AddChildToHorizontalBox(B))
			{
				HS->SetPadding(FMargin(2.f));
				HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		AddToScroll(VolPresetRow, 4.f);

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
	RefreshWardrobeItems();
}

void UAPBFrontendWidget::SelectWardrobeTab(int32 TabId)
{
	ActiveWardrobeTab = FMath::Clamp(TabId, 1, 15);
	RefreshWardrobeItems();
	if (UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr)
	{
		float PosY = 0.f, PosZ = 0.f, TargetZ = 0.f, Fov = 0.f;
		if (CharPreviewActor && APB->GetCameraFrameForTab(ActiveWardrobeTab, PosY, PosZ, TargetZ, Fov)) CharPreviewActor->FrameCamera(PosY, PosZ, TargetZ, Fov);
	}
	RefreshCharacterPreviewFromUI();
}

void UAPBFrontendWidget::RefreshWardrobeItems()
{
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB || !WardrobeItemCombo) return;
	bSuppressPreviewRefresh = true;
	WardrobeItemCombo->ClearOptions();
	for (const FAPBClothingChoice& Choice : APB->GetClothingForTab(ActiveWardrobeTab, 60)) WardrobeItemCombo->AddOption(FString::Printf(TEXT("%s | %s"), *Choice.Id, *Choice.Name));
	if (WardrobeItemCombo->GetOptionCount() > 0)
	{
		const FString SelectedId = WardrobeItemIds.IsValidIndex(ActiveWardrobeTab - 1) ? WardrobeItemIds[ActiveWardrobeTab - 1] : FString();
		int32 SelectedOption = 0;
		for (int32 OptionIndex = 0; OptionIndex < WardrobeItemCombo->GetOptionCount(); ++OptionIndex)
		{
			if (WardrobeItemCombo->GetOptionAtIndex(OptionIndex).StartsWith(SelectedId + TEXT(" |"))) { SelectedOption = OptionIndex; break; }
		}
		WardrobeItemCombo->SetSelectedIndex(SelectedOption);
	}
	bSuppressPreviewRefresh = false;
}

void UAPBFrontendWidget::OnSplashContinue()
{
	if (!bStartupReady) return;
	SetStage(EAPBFrontendStage::Login);
	StartLoginMusic();
}
void UAPBFrontendWidget::OnBackToLogin() { SetStage(EAPBFrontendStage::Login); }
void UAPBFrontendWidget::OnCharCreateBack() { SetStage(EAPBFrontendStage::CharacterSelect); }	void UAPBFrontendWidget::SetLoginCredentials(const FString& User, const FString& Pass)
	{
		if (UserBox) UserBox->SetText(FText::FromString(User));
		if (PassBox) PassBox->SetText(FText::FromString(Pass));
		OnLoginFieldsChanged(FText::GetEmpty());
	}

	void UAPBFrontendWidget::OnLoginFieldsChanged(const FText& Text)
	{
		if (!LoginBtn) return;
		const FString User = UserBox ? UserBox->GetText().ToString().TrimStartAndEnd() : FString();
		const FString Pass = PassBox ? PassBox->GetText().ToString() : FString();
		const bool bReady = !User.IsEmpty() && !Pass.IsEmpty();
		LoginBtn->SetIsEnabled(bReady);
		if (LoginLabel)
		{
			// Disabled: legible gray on the dark plate (white+shadow reads muddy);
			// enabled: full white with the soft drop shadow back.
			LoginLabel->SetColorAndOpacity(bReady ? APB_WHITE : FLinearColor(0.58f, 0.58f, 0.58f, 1.f));
			LoginLabel->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, bReady ? 0.55f : 0.20f));
		}
	}

void UAPBFrontendWidget::OnLoginClicked()
{
	PlayUiSfx(TEXT("UI_Click"));
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
	if (APB->bWorldServerMode)
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Connecting to world server...")));
		APB->Login(User, Pass);
		GetWorld()->GetTimerManager().SetTimer(WorldAuthPollTimer, [this, APB]()
		{
			const UWorld* W = GetWorld();
			if (!W) return;
			for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
			{
				if (const APlayerController* PC = It->Get())
				{
					if (const AAPBPlayerState* PS = PC->GetPlayerState<AAPBPlayerState>())
					{
						if (PS->bWorldAuthOk)
						{
							GetWorld()->GetTimerManager().ClearTimer(WorldAuthPollTimer);
							WorldAuthTimeout = 0.f;
							bFirstRunTOS = false;
							LogStage(TEXT("login_ok_world"));
							SetStage(EAPBFrontendStage::CharacterSelect);
							return;
						}
					}
				}
			}
			WorldAuthTimeout += 0.5f;
			if (WorldAuthTimeout >= 10.f)
			{
				GetWorld()->GetTimerManager().ClearTimer(WorldAuthPollTimer);
				WorldAuthTimeout = 0.f;
				if (StatusText) StatusText->SetText(FText::FromString(TEXT("World server login timed out")));
				LogStage(TEXT("login_fail_timeout"));
			}
		}, 0.5f, true);
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
	OnAccountLink();
}

void UAPBFrontendWidget::OnRegistrationSubmit()
{
	PlayUiSfx(TEXT("UI_Click"));
	const FString Email = RegisterEmailBox ? RegisterEmailBox->GetText().ToString().TrimStartAndEnd() : FString();
	const FString Password = RegisterPasswordBox ? RegisterPasswordBox->GetText().ToString() : FString();
	const FString ConfirmPassword = RegisterConfirmBox ? RegisterConfirmBox->GetText().ToString() : FString();
	const bool bTos = RegisterTosCheck && RegisterTosCheck->IsChecked();
	const bool bPrivacy = RegisterPrivacyCheck && RegisterPrivacyCheck->IsChecked();
	const bool bCaptcha = RegisterCaptchaCheck && RegisterCaptchaCheck->IsChecked();
	const bool bValid = apb_layout::RegistrationFieldsValid(
		TCHAR_TO_UTF8(*Email), TCHAR_TO_UTF8(*Password), TCHAR_TO_UTF8(*ConfirmPassword), bTos, bPrivacy, bCaptcha);
	if (!bValid)
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Enter a valid email, matching passwords, accept both policies, and complete reCAPTCHA.")));
		LogStage(TEXT("register_fail_validation"));
		return;
	}
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	if (APB->RegisterAccount(Email, Password) && APB->Login(Email, Password))
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

void UAPBFrontendWidget::OnRegistrationBack()
{
	PlayUiSfx(TEXT("UI_Back"));
	SetStage(EAPBFrontendStage::Login);
}

void UAPBFrontendWidget::StartReplayMovie(const FString& MoviePath)
{
	const FString ContentRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	FString ReplayPath = MoviePath;
	if (ReplayPath.IsEmpty() || !FPaths::FileExists(ReplayPath))
	{
		ReplayPath = FirstExistingVideo({
			ContentRoot / TEXT("Movies/IntroTitles.mp4"),
			ContentRoot / TEXT("Movies/SplashScreen.mp4")
		});
	}
	if (ReplayPath.IsEmpty())
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Replay movie is unavailable.")));
		LogStage(TEXT("replay_missing"));
		return;
	}
	// Task-18 gate: replay media must be registry-verified before open.
	if (!VerifyMediaFile(ReplayPath, TEXT("REPLAY_MOVIE")))
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Replay movie is unavailable.")));
		LogStage(TEXT("replay_denied"));
		return;
	}
	// Halt the bed video + its audio and the theme first so the intro's own
	// soundtrack plays cleanly instead of doubling over the bed/theme mix.
	StopLoginBackgroundVideo();
	StopLoginMusic();
	// Dedicated player + texture + sound so the intro can sit in the top-layer
	// overlay without touching the stage bed that owns LoginMediaPlayer.
	if (!ReplayMediaPlayer)
	{
		ReplayMediaPlayer = NewObject<UMediaPlayer>(this, TEXT("ReplayMediaPlayer"));
		ReplayMediaPlayer->PlayOnOpen = false;
		ReplayMediaPlayer->SetLooping(false);
	}
	if (!ReplayMediaTexture)
	{
		ReplayMediaTexture = NewObject<UMediaTexture>(this, TEXT("ReplayMediaTexture"));
		ReplayMediaTexture->SetMediaPlayer(ReplayMediaPlayer);
		ReplayMediaTexture->UpdateResource();
	}
	if (!ReplayMediaSoundComp)
	{
		if (UWorld* SoundWorld = GetWorld())
		{
			ReplayMediaSoundComp = NewObject<UMediaSoundComponent>(this, TEXT("ReplayMediaSound"));
			ReplayMediaSoundComp->bIsUISound = true;
			ReplayMediaSoundComp->bAutoActivate = false;
			ReplayMediaSoundComp->SetMediaPlayer(ReplayMediaPlayer);
			ReplayMediaSoundComp->RegisterComponentWithWorld(SoundWorld);
			ReplayMediaSoundComp->SetVolumeMultiplier(MenuAudioVolume);
			ReplayMediaSoundComp->Start();
		}
	}
	ReplayMediaPlayer->SetLooping(false);
	ReplayMediaPlayer->OnEndReached.RemoveDynamic(this, &UAPBFrontendWidget::OnReplayMovieEnded);
	ReplayMediaPlayer->OnEndReached.AddDynamic(this, &UAPBFrontendWidget::OnReplayMovieEnded);
	// Arm the restore path before opening: if OpenFile fails, StopReplayMovie()
	// re-applies the stage bed + theme (they were halted above) so a broken
	// movie can never strand the menu in silence.
	bReplayActive = true;
	if (!ReplayMediaPlayer->OpenFile(ReplayPath))
	{
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Replay movie could not be opened.")));
		LogStage(TEXT("replay_open_failed"));
		StopReplayMovie();
		return;
	}
	if (ReplayImage)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Brush.SetResourceObject(ReplayMediaTexture);
		Brush.ImageSize = FVector2D(1920.f, 1080.f);
		ReplayImage->SetBrush(Brush);
		ReplayImage->SetColorAndOpacity(FLinearColor::White);
	}
	if (ReplayOverlay)
	{
		ReplayOverlay->SetVisibility(ESlateVisibility::Visible);
		ReplayOverlay->SetRenderOpacity(1.f);
	}
	ReplayMediaPlayer->Play();
	bReplayActive = true;
	ReplayStartedTime = FPlatformTime::Seconds();
	// Grab keyboard + mouse focus so any key (or click, via the catcher button)
	// reliably reaches this widget's NativeOnKeyDown/NativeOnPreviewKeyDown.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(true);
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetWidgetToFocus(TakeWidget());
		PC->SetInputMode(Mode);
	}
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("Playing preserved APB movie. Press any key / click to stop.")));
	LogStage(FString::Printf(TEXT("replay_play=%s"), *ReplayPath));
}

void UAPBFrontendWidget::OnReplayMovieChosen()
{
	PlayUiSfx(TEXT("UI_Click"));
	APlayerController* PC = GetOwningPlayer();
	for (int32 i = 0; i < ReplayMovieButtons.Num(); ++i)
	{
		// Mouse click: the pressed button is hovered. Keyboard activation
		// (Tab + Enter/Space): the focused button holds user focus instead.
		if (ReplayMovieButtons[i] && ReplayMoviePaths.IsValidIndex(i)
			&& (ReplayMovieButtons[i]->IsHovered()
				|| (PC && ReplayMovieButtons[i]->HasUserFocus(PC))))
		{
			StartReplayMovie(ReplayMoviePaths[i]);
			return;
		}
	}
	StartReplayMovie();
}

void UAPBFrontendWidget::OnReplayStopClicked()
{
	PlayUiSfx(TEXT("UI_Back"));
	StopReplayMovie();
}

void UAPBFrontendWidget::StopReplayMovie()
{
	if (!bReplayActive) return;
	bReplayActive = false;
	if (ReplayMediaPlayer)
	{
		ReplayMediaPlayer->Close();
	}
	if (ReplayMediaSoundComp)
	{
		ReplayMediaSoundComp->Stop();
	}
	if (ReplayOverlay)
	{
		ReplayOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	// Restore the stage bed behind the menu (same-path guard keeps it quiet if
	// the bed movie never changed); resume the theme on the Login stage only.
	ApplyStageBackgroundVideo(CurrentStage);
	if (CurrentStage == EAPBFrontendStage::Login)
	{
		StartLoginMusic();
	}
	LogStage(TEXT("replay_stop"));
}

void UAPBFrontendWidget::OnReplayMovieEnded()
{
	if (bReplayActive)
	{
		StopReplayMovie();
	}
}

FReply UAPBFrontendWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bReplayActive)
	{
		StopReplayMovie();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UAPBFrontendWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bReplayActive)
	{
		StopReplayMovie();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UAPBFrontendWidget::OnCreateCharOpen() { SetStage(EAPBFrontendStage::CharacterCreate); }

void UAPBFrontendWidget::OnDeleteCharClicked()
{
	PlayUiSfx(TEXT("UI_Click"));
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	if (!bDeleteArmed)
	{
		bDeleteArmed = true;
		DeleteArmedTime = FPlatformTime::Seconds();
		if (CharSelectDeleteLabel)
		{
			CharSelectDeleteLabel->SetText(FText::FromString(TEXT("CONFIRM DELETE?")));
			CharSelectDeleteLabel->SetColorAndOpacity(FLinearColor(1.f, 0.35f, 0.35f, 1.f));
		}
		LogStage(TEXT("delete_armed"));
		return;
	}
	bDeleteArmed = false;
	if (CharSelectDeleteLabel)
	{
		CharSelectDeleteLabel->SetText(FText::FromString(S2011(TEXT("CharacterSelectScreen.DeleteCharacter"), TEXT("DELETE CHARACTER"))));
		CharSelectDeleteLabel->SetColorAndOpacity(APB_WHITE);
	}
	if (APB->DeleteCharacter())
	{
		LogStage(TEXT("char_deleted"));
		SetStage(EAPBFrontendStage::CharacterSelect);
	}
	else
	{
		LogStage(TEXT("char_delete_failed"));
	}
}

void UAPBFrontendWidget::OnSelectExistingChar()
{
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	if (APB->bWorldServerMode)
	{
		APlayerController* PlayerController = GetOwningPlayer();
		AAPBPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AAPBPlayerState>() : nullptr;
		if (!PlayerState)
		{
			CompleteWorldTravelFailure(TEXT("no_ticket"));
			return;
		}
		StopLoginMusic();
		SetStage(EAPBFrontendStage::Loading);
		PendingTravelPreviousTicket = PlayerState->IssuedTicketJson;
		PendingTravelReservationId.Empty();
		bWorldTravelPending = true;
		WorldAuthTimeout = 0.f;
		const FString CharacterName = CharNameBox ? CharNameBox->GetText().ToString().TrimStartAndEnd() : TEXT("Operative");
		PlayerState->Server_IssueTicket(CharacterName.IsEmpty() ? TEXT("Operative") : CharacterName, SelectedDistrictId);
		GetWorld()->GetTimerManager().SetTimer(WorldAuthPollTimer,
			FTimerDelegate::CreateUObject(this, &UAPBFrontendWidget::PollWorldTravelReservation), 0.25f, true);
		return;
	}
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

void UAPBFrontendWidget::OnWardrobeTab1() { SelectWardrobeTab(1); }
void UAPBFrontendWidget::OnWardrobeTab2() { SelectWardrobeTab(2); }
void UAPBFrontendWidget::OnWardrobeTab3() { SelectWardrobeTab(3); }
void UAPBFrontendWidget::OnWardrobeTab4() { SelectWardrobeTab(4); }
void UAPBFrontendWidget::OnWardrobeTab5() { SelectWardrobeTab(5); }
void UAPBFrontendWidget::OnWardrobeTab6() { SelectWardrobeTab(6); }
void UAPBFrontendWidget::OnWardrobeTab7() { SelectWardrobeTab(7); }
void UAPBFrontendWidget::OnWardrobeTab8() { SelectWardrobeTab(8); }
void UAPBFrontendWidget::OnWardrobeTab9() { SelectWardrobeTab(9); }
void UAPBFrontendWidget::OnWardrobeTab10() { SelectWardrobeTab(10); }
void UAPBFrontendWidget::OnWardrobeTab11() { SelectWardrobeTab(11); }
void UAPBFrontendWidget::OnWardrobeTab12() { SelectWardrobeTab(12); }
void UAPBFrontendWidget::OnWardrobeTab13() { SelectWardrobeTab(13); }
void UAPBFrontendWidget::OnWardrobeTab14() { SelectWardrobeTab(14); }
void UAPBFrontendWidget::OnWardrobeTab15() { SelectWardrobeTab(15); }

void UAPBFrontendWidget::OnPaletteSwatch0() { SelectedColorIndex = 0; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch1() { SelectedColorIndex = 1; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch2() { SelectedColorIndex = 2; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch3() { SelectedColorIndex = 3; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch4() { SelectedColorIndex = 4; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch5() { SelectedColorIndex = 5; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch6() { SelectedColorIndex = 6; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch7() { SelectedColorIndex = 7; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch8() { SelectedColorIndex = 8; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch9() { SelectedColorIndex = 9; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch10() { SelectedColorIndex = 10; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch11() { SelectedColorIndex = 11; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch12() { SelectedColorIndex = 12; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch13() { SelectedColorIndex = 13; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch14() { SelectedColorIndex = 14; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch15() { SelectedColorIndex = 15; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch16() { SelectedColorIndex = 16; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch17() { SelectedColorIndex = 17; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch18() { SelectedColorIndex = 18; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch19() { SelectedColorIndex = 19; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch20() { SelectedColorIndex = 20; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch21() { SelectedColorIndex = 21; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch22() { SelectedColorIndex = 22; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }
void UAPBFrontendWidget::OnPaletteSwatch23() { SelectedColorIndex = 23; OnClothingSelectionChanged(FString(), ESelectInfo::Direct); }

void UAPBFrontendWidget::OnRandomizeAppearance()
{
	if (UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr)
	{
		APB->RandomizeAppearance(AppearanceRandomSeed++);
		RefreshWardrobeItems();
		RefreshCharacterEditorFromUI();
	}
}

// ---- Retail character editor handlers (Basic + Advanced) ----

void UAPBFrontendWidget::OnEditorModeBasic()
{
	bEditorAdvancedMode = false;
	RefreshAdvancedPanel();
	LogStage(TEXT("editor_mode=basic"));
}

void UAPBFrontendWidget::OnEditorModeAdvanced()
{
	bEditorAdvancedMode = true;
	RefreshAdvancedPanel();
	LogStage(TEXT("editor_mode=advanced"));
}

void UAPBFrontendWidget::RefreshAdvancedPanel()
{
	if (AdvancedPanel)
	{
		AdvancedPanel->SetVisibility(bEditorAdvancedMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ModeBasicBtn) ModeBasicBtn->SetBackgroundColor(bEditorAdvancedMode ? APB_BTN : APB_BTN_OK);
	if (ModeAdvancedBtn) ModeAdvancedBtn->SetBackgroundColor(bEditorAdvancedMode ? APB_BTN_OK : APB_BTN);
	if (MakeupChannelLabel)
	{
		const TCHAR* Name = (ActiveMakeupChannel == 1) ? TEXT("EYE SHADOW")
			: (ActiveMakeupChannel == 2) ? TEXT("EYE LINER")
			: (ActiveMakeupChannel == 3) ? TEXT("BLUSHER") : TEXT("LIPSTICK");
		MakeupChannelLabel->SetText(FText::FromString(FString::Printf(TEXT("MAKEUP - %s"), Name)));
	}
	if (ScarCountLabel) ScarCountLabel->SetText(FText::FromString(FString::Printf(TEXT("SCARS: %d"), EditorScarCount)));
	// Re-sync the makeup combo to the active channel's stored value on channel switch
	if (MakeupCombo && MakeupCombo->GetOptionCount() > 0)
	{
		if (UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr)
		{
			float H, B; int32 Skin, FP, HS, HC, EC, G, AG, ML, MS, ME, MB, SC;
			if (APB->GetCharacterProfile(H, B, Skin, FP, HS, HC, EC, G, AG, ML, MS, ME, MB, SC))
			{
				const int32 MIdx = (ActiveMakeupChannel == 1) ? MS : (ActiveMakeupChannel == 2) ? ME : (ActiveMakeupChannel == 3) ? MB : ML;
				MakeupCombo->SetSelectedIndex(FMath::Clamp(MIdx, 0, MakeupCombo->GetOptionCount() - 1));
			}
		}
	}
}

void UAPBFrontendWidget::OnCharHeightChanged(float Value)
{
	if (CharHeightLabel) CharHeightLabel->SetText(FText::FromString(FString::Printf(TEXT("HEIGHT  %.2f"), Value)));
	ApplyCharacterEditorToDomain();
}

void UAPBFrontendWidget::OnCharBulkChanged(float Value)
{
	if (CharBulkLabel) CharBulkLabel->SetText(FText::FromString(FString::Printf(TEXT("BUILD  %.2f"), Value)));
	ApplyCharacterEditorToDomain();
}

void UAPBFrontendWidget::OnBuildPresetSkinny() { if (CharBulkSlider) CharBulkSlider->SetValue(0.8f); OnCharBulkChanged(0.8f); }
void UAPBFrontendWidget::OnBuildPresetAverage() { if (CharBulkSlider) CharBulkSlider->SetValue(0.95f); OnCharBulkChanged(0.95f); }
void UAPBFrontendWidget::OnBuildPresetBulky() { if (CharBulkSlider) CharBulkSlider->SetValue(1.1f); OnCharBulkChanged(1.1f); }
void UAPBFrontendWidget::OnBuildPresetMuscular() { if (CharBulkSlider) CharBulkSlider->SetValue(1.2f); OnCharBulkChanged(1.2f); }

void UAPBFrontendWidget::OnGenderComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!bSuppressPreviewRefresh) ApplyCharacterEditorToDomain();
}

void UAPBFrontendWidget::OnSkinToneComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!bSuppressPreviewRefresh) ApplyCharacterEditorToDomain();
}

void UAPBFrontendWidget::OnFacePresetComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!bSuppressPreviewRefresh) ApplyCharacterEditorToDomain();
}

void UAPBFrontendWidget::OnHairStyleComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!bSuppressPreviewRefresh) ApplyCharacterEditorToDomain();
}

void UAPBFrontendWidget::OnHairColorComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!bSuppressPreviewRefresh) ApplyCharacterEditorToDomain();
}

void UAPBFrontendWidget::OnEyeColorComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!bSuppressPreviewRefresh) ApplyCharacterEditorToDomain();
}

void UAPBFrontendWidget::OnAgeGroupComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!bSuppressPreviewRefresh) ApplyCharacterEditorToDomain();
}

void UAPBFrontendWidget::OnMakeupChannel1() { ActiveMakeupChannel = 0; RefreshAdvancedPanel(); }
void UAPBFrontendWidget::OnMakeupChannel2() { ActiveMakeupChannel = 1; RefreshAdvancedPanel(); }
void UAPBFrontendWidget::OnMakeupChannel3() { ActiveMakeupChannel = 2; RefreshAdvancedPanel(); }
void UAPBFrontendWidget::OnMakeupChannel4() { ActiveMakeupChannel = 3; RefreshAdvancedPanel(); }

void UAPBFrontendWidget::OnMakeupComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!bSuppressPreviewRefresh) ApplyCharacterEditorToDomain();
}

void UAPBFrontendWidget::OnAddScarClicked()
{
	SetCharacterCreateUnavailable(TEXT("RETAIL_SOURCE_REQUIRED SCAR_DECAL_PAYLOAD"));
	LogStage(TEXT("scar_edit_blocked source_payload_missing"));
}

void UAPBFrontendWidget::OnRemoveScarClicked()
{
	SetCharacterCreateUnavailable(TEXT("RETAIL_SOURCE_REQUIRED SCAR_DECAL_PAYLOAD"));
	LogStage(TEXT("scar_edit_blocked source_payload_missing"));
}

void UAPBFrontendWidget::OnCamFullBody()
{
	if (CharPreviewActor) CharPreviewActor->FrameCamera(280.f, 95.f, 95.f, 55.f);
}

void UAPBFrontendWidget::OnCamFace()
{
	if (CharPreviewActor) CharPreviewActor->FrameCamera(140.f, 160.f, 140.f, 45.f);
}

void UAPBFrontendWidget::OnCamDolly()
{
	if (CharPreviewActor) CharPreviewActor->FrameCamera(200.f, 95.f, 95.f, 50.f);
}

bool UAPBFrontendWidget::ApplyCharacterEditorToDomain()
{
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return false;
	auto SelectedOrUnavailable = [this](UComboBoxString* Combo, const TCHAR* Control) -> int32
	{
		if (!Combo || Combo->GetOptionCount() <= 0 || Combo->GetSelectedIndex() < 0)
		{
			SetCharacterCreateUnavailable(FString::Printf(TEXT("RETAIL_SOURCE_REQUIRED %s"), Control));
			return INDEX_NONE;
		}
		return FMath::Clamp(Combo->GetSelectedIndex(), 0, Combo->GetOptionCount() - 1);
	};
	const int32 Gender = SelectedOrUnavailable(GenderCombo, TEXT("GENDER"));
	const int32 SkinTone = SelectedOrUnavailable(SkinToneCombo, TEXT("SKIN_TONE"));
	const int32 FacePreset = SelectedOrUnavailable(FacePresetCombo, TEXT("FACE"));
	const int32 HairStyle = SelectedOrUnavailable(HairStyleCombo, TEXT("HAIR_STYLE"));
	const int32 HairColor = SelectedOrUnavailable(HairColorCombo, TEXT("HAIR_COLOR"));
	const int32 EyeColor = SelectedOrUnavailable(EyeColorCombo, TEXT("EYE_COLOR"));
	const int32 AgeGroup = SelectedOrUnavailable(AgeGroupCombo, TEXT("AGE_GROUP"));
	const int32 MIdx = SelectedOrUnavailable(MakeupCombo, TEXT("MAKEUP"));
	if (Gender == INDEX_NONE || SkinTone == INDEX_NONE || FacePreset == INDEX_NONE
		|| HairStyle == INDEX_NONE || HairColor == INDEX_NONE || EyeColor == INDEX_NONE
		|| AgeGroup == INDEX_NONE || MIdx == INDEX_NONE)
	{
		return false;
	}
	const float Height = CharHeightSlider ? FMath::Clamp(CharHeightSlider->GetValue(), 0.8f, 1.2f) : 1.0f;
	const float Bulk = CharBulkSlider ? FMath::Clamp(CharBulkSlider->GetValue(), 0.8f, 1.2f) : 0.95f;
	if (!APB->ApplyCharacterProfile(Height, Bulk, SkinTone, FacePreset, HairStyle, HairColor, EyeColor))
	{
		return false;
	}
	int32 Lipstick = 0, EyeShadow = 0, Eyeliner = 0, Blusher = 0;
	switch (ActiveMakeupChannel)
	{
	case 0: Lipstick = MIdx; break;
	case 1: EyeShadow = MIdx; break;
	case 2: Eyeliner = MIdx; break;
	case 3: Blusher = MIdx; break;
	default: break;
	}
	if (!APB->ApplyAdvancedProfile(Gender, AgeGroup, Lipstick, EyeShadow, Eyeliner, Blusher, EditorScarCount))
	{
		return false;
	}
	RefreshCharacterPreviewFromUI();
	return true;
}

void UAPBFrontendWidget::RefreshEditorCombos()
{
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	const TArray<FLinearColor> SkinTones = APB->GetPaletteColors(TEXT("StandardPaint"), 0);
	if (SkinToneCombo)
	{
		SkinToneCombo->ClearOptions();
		for (int32 i = 0; i < FMath::Min(SkinTones.Num(), 24); ++i)
		{
			SkinToneCombo->AddOption(FString::Printf(TEXT("Tone %02d"), i + 1));
		}
		if (SkinToneCombo->GetOptionCount() == 0) SkinToneCombo->AddOption(TEXT("Default"));
		SkinToneCombo->SetSelectedIndex(0);
	}
	if (FacePresetCombo)
	{
		FacePresetCombo->ClearOptions();
		for (int32 i = 1; i <= 12; ++i) FacePresetCombo->AddOption(FString::Printf(TEXT("Preset %02d"), i));
		FacePresetCombo->SetSelectedIndex(0);
	}
	if (HairStyleCombo)
	{
		HairStyleCombo->ClearOptions();
		for (int32 i = 1; i <= 12; ++i) HairStyleCombo->AddOption(FString::Printf(TEXT("Style %02d"), i));
		HairStyleCombo->SetSelectedIndex(0);
	}
	const TArray<FLinearColor> HairColors = APB->GetPaletteColors(TEXT("Hair"), 0);
	if (HairColorCombo)
	{
		HairColorCombo->ClearOptions();
		for (int32 i = 0; i < FMath::Min(HairColors.Num(), 24); ++i)
		{
			HairColorCombo->AddOption(FString::Printf(TEXT("Color %02d"), i + 1));
		}
		if (HairColorCombo->GetOptionCount() == 0) HairColorCombo->AddOption(TEXT("Default"));
		HairColorCombo->SetSelectedIndex(0);
	}
	const TArray<FLinearColor> EyeColors = APB->GetPaletteColors(TEXT("Hair_Dyes"), 0);
	if (EyeColorCombo)
	{
		EyeColorCombo->ClearOptions();
		for (int32 i = 0; i < FMath::Min(EyeColors.Num(), 24); ++i)
		{
			EyeColorCombo->AddOption(FString::Printf(TEXT("Color %02d"), i + 1));
		}
		if (EyeColorCombo->GetOptionCount() == 0) EyeColorCombo->AddOption(TEXT("Default"));
		EyeColorCombo->SetSelectedIndex(0);
	}
	const TArray<FLinearColor> MakeupColors = APB->GetPaletteColors(TEXT("StandardPaint"), 0);
	if (MakeupCombo)
	{
		MakeupCombo->ClearOptions();
		for (int32 i = 0; i < FMath::Min(MakeupColors.Num(), 24); ++i)
		{
			MakeupCombo->AddOption(FString::Printf(TEXT("Color %02d"), i + 1));
		}
		if (MakeupCombo->GetOptionCount() == 0) MakeupCombo->AddOption(TEXT("Default"));
		MakeupCombo->SetSelectedIndex(0);
	}
}

void UAPBFrontendWidget::RefreshCharacterEditorFromUI()
{
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	float H = 1.f, B = 0.95f;
	int32 Skin = 0, FP = 0, HS = 0, HC = 0, EC = 0, G = 0, AG = 0, ML = 0, MS = 0, ME = 0, MB = 0, SC = 0;
	APB->GetCharacterProfile(H, B, Skin, FP, HS, HC, EC, G, AG, ML, MS, ME, MB, SC);
	bSuppressPreviewRefresh = true;
	if (CharHeightSlider) CharHeightSlider->SetValue(H);
	if (CharBulkSlider) CharBulkSlider->SetValue(B);
	if (GenderCombo && GenderCombo->GetOptionCount() > 0) GenderCombo->SetSelectedIndex(FMath::Clamp(G, 0, GenderCombo->GetOptionCount() - 1));
	if (SkinToneCombo && SkinToneCombo->GetOptionCount() > 0) SkinToneCombo->SetSelectedIndex(FMath::Clamp(Skin, 0, SkinToneCombo->GetOptionCount() - 1));
	if (FacePresetCombo && FacePresetCombo->GetOptionCount() > 0) FacePresetCombo->SetSelectedIndex(FMath::Clamp(FP, 0, FacePresetCombo->GetOptionCount() - 1));
	if (HairStyleCombo && HairStyleCombo->GetOptionCount() > 0) HairStyleCombo->SetSelectedIndex(FMath::Clamp(HS, 0, HairStyleCombo->GetOptionCount() - 1));
	if (HairColorCombo && HairColorCombo->GetOptionCount() > 0) HairColorCombo->SetSelectedIndex(FMath::Clamp(HC, 0, HairColorCombo->GetOptionCount() - 1));
	if (EyeColorCombo && EyeColorCombo->GetOptionCount() > 0) EyeColorCombo->SetSelectedIndex(FMath::Clamp(EC, 0, EyeColorCombo->GetOptionCount() - 1));
	if (AgeGroupCombo && AgeGroupCombo->GetOptionCount() > 0) AgeGroupCombo->SetSelectedIndex(FMath::Clamp(AG, 0, AgeGroupCombo->GetOptionCount() - 1));
	int32 MIdx = ML;
	if (ActiveMakeupChannel == 1) MIdx = MS;
	else if (ActiveMakeupChannel == 2) MIdx = ME;
	else if (ActiveMakeupChannel == 3) MIdx = MB;
	if (MakeupCombo && MakeupCombo->GetOptionCount() > 0) MakeupCombo->SetSelectedIndex(FMath::Clamp(MIdx, 0, MakeupCombo->GetOptionCount() - 1));
	EditorScarCount = SC;
	bSuppressPreviewRefresh = false;
	if (CharHeightLabel) CharHeightLabel->SetText(FText::FromString(FString::Printf(TEXT("HEIGHT  %.2f"), H)));
	if (CharBulkLabel) CharBulkLabel->SetText(FText::FromString(FString::Printf(TEXT("BUILD  %.2f"), B)));
	RefreshAdvancedPanel();
	RefreshCharacterPreviewFromUI();
}

void UAPBFrontendWidget::OnAddSymbol()
{
	SetCharacterCreateUnavailable(TEXT("RETAIL_SOURCE_REQUIRED TATTOO_SYMBOL_PAYLOAD"));
	LogStage(TEXT("tattoo_edit_blocked source_payload_missing"));
}

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
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;

	const bool bEnf = bCreateAsEnforcer || (EnforcerCheck && EnforcerCheck->IsChecked());
	const FString MeshPath = CharPreviewActor->ApplyBaseMesh(bEnf);

	float Height = 1.05f, Bulk = 0.95f;
	if (CharHeightSlider) Height = CharHeightSlider->GetValue();
	if (CharBulkSlider) Bulk = CharBulkSlider->GetValue();
	CharPreviewActor->ApplyBodyProfile(Height, Bulk);

	int32 Bound = 0;
	for (int32 TabId = 1; TabId <= 15; ++TabId)
	{
		const FString ItemId = WardrobeItemIds.IsValidIndex(TabId - 1) ? WardrobeItemIds[TabId - 1] : FString();
		if (!ItemId.IsEmpty() && CharPreviewActor->ApplyClothingSlotVisual(APB->GetSlotForTab(TabId), ItemId))
		{
			++Bound;
		}
	}
	BindPreviewImageToRT();
	const bool bBaseMeshOk = MeshPath != TEXT("missing");
	const TCHAR* PreviewState = (bBaseMeshOk && Bound > 0) ? TEXT("PREVIEW_OK") :
		(bBaseMeshOk ? TEXT("PREVIEW_BLOCKED clothing_mesh_unavailable") : TEXT("PREVIEW_BLOCKED base_mesh_missing"));
	const FString Line = FString::Printf(
		TEXT("%s mesh=%s slots_bound=%d height=%.2f build=%.2f enf=%d"),
		PreviewState, *MeshPath, Bound, Height, Bulk, bEnf ? 1 : 0);
	if (PreviewSummary) PreviewSummary->SetText(FText::FromString(Line));
	LogStage(Line);
	UE_LOG(LogTemp, Warning, TEXT("APBFrontend %s"), *Line);
}

void UAPBFrontendWidget::RefreshCharacterPreviewFromSaved()
{
	EnsureCharacterPreview();
	if (!CharPreviewActor) return;
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return;
	const auto Snap = APB->CaptureDomainSnapshot();
	if (!Snap.bHasCharacter)
	{
		CharPreviewActor->SetAutoSpin(false);
		return;
	}
	const FString MeshPath = CharPreviewActor->ApplyBaseMesh(Snap.bEnforcer);
	float Height = 1.f, Bulk = 1.f;
	APB->GetBodyProfile(Height, Bulk);
	CharPreviewActor->ApplyBodyProfile(Height, Bulk);
	const TArray<FString> Rows = APB->GetEquippedClothingRows();
	int32 Bound = 0;
	for (const FString& Row : Rows)
	{
		FString SlotName, ItemId;
		if (Row.Split(TEXT("|"), &SlotName, &ItemId) && CharPreviewActor->ApplyClothingSlotVisual(SlotName, ItemId))
		{
			++Bound;
		}
	}
	CharPreviewActor->FrameCamera(280.f, 95.f, 95.f, 55.f);
	CharPreviewActor->SetAutoSpin(true);
	BindPreviewImageToRT();
	LogStage(FString::Printf(TEXT("select_preview mesh=%s slots=%d"), *MeshPath, Bound));
}

void UAPBFrontendWidget::OnClothingSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bSuppressPreviewRefresh) return;
	if (CurrentStage != EAPBFrontendStage::CharacterCreate) return;
	if (UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr)
	{
		const FString Id = ComboId(WardrobeItemCombo);
		if (!Id.IsEmpty())
		{
			if (WardrobeItemIds.IsValidIndex(ActiveWardrobeTab - 1)) WardrobeItemIds[ActiveWardrobeTab - 1] = Id;
			APB->EquipClothingColored(APB->GetSlotForTab(ActiveWardrobeTab), Id, SelectedColorIndex, SelectedColorIndex);
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

void UAPBFrontendWidget::OnPreviewRefreshClicked()
{
	RefreshCharacterPreviewFromUI();
}

bool UAPBFrontendWidget::ApplyAppearanceFromEditor()
{
	UAPBGameInstanceSubsystem* APB = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	if (!APB) return false;
	if (!ApplyCharacterEditorToDomain()) return false;
	const float Height = CharHeightSlider ? FMath::Clamp(CharHeightSlider->GetValue(), 0.8f, 1.2f) : 1.0f;
	const float Bulk = CharBulkSlider ? FMath::Clamp(CharBulkSlider->GetValue(), 0.8f, 1.2f) : 0.95f;
	LogStage(FString::Printf(TEXT("body height=%.3f bulk=%.3f"), Height, Bulk));
	FString Summary = FString::Printf(TEXT("body H=%.2f B=%.2f;"), Height, Bulk);
	for (int32 TabId = 1; TabId <= 15; ++TabId)
	{
		const FString ItemId = WardrobeItemIds.IsValidIndex(TabId - 1) ? WardrobeItemIds[TabId - 1] : FString();
		if (!ItemId.IsEmpty())
		{
			const FString ClothingSlot = APB->GetSlotForTab(TabId);
			APB->EquipClothingColored(ClothingSlot, ItemId, SelectedColorIndex, SelectedColorIndex);
			Summary += FString::Printf(TEXT("%s=%s;"), *ClothingSlot, *ItemId);
		}
	}
	if (PreviewSummary) PreviewSummary->SetText(FText::FromString(TEXT("Equipped: ") + Summary));
	LogStage(TEXT("appearance=") + Summary);
	RefreshCharacterPreviewFromUI();
	return true;
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
	if (!ApplyAppearanceFromEditor())
	{
		APB->DeleteCharacter();
		if (StatusText) StatusText->SetText(FText::FromString(TEXT("Character appearance is unavailable until retail source payloads are imported.")));
		LogStage(TEXT("char_create_fail appearance_source_unavailable"));
		return;
	}
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
	FString Opts = TEXT("listen?game=/Script/APBReloaded.APBFreeroamGameMode");
	UGameplayStatics::OpenLevel(this, FName(*MapName), true, Opts);
}

void UAPBFrontendWidget::PollWorldTravelReservation()
{
	if (!bWorldTravelPending)
	{
		return;
	}
	UWorld* World = GetWorld();
	APlayerController* PlayerController = GetOwningPlayer();
	AAPBPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AAPBPlayerState>() : nullptr;
	if (!World || !PlayerState)
	{
		CompleteWorldTravelFailure(TEXT("no_ticket"));
		return;
	}
	const FString ReservationJson = PlayerState->IssuedTicketJson;
	if (!ReservationJson.IsEmpty() && ReservationJson != PendingTravelPreviousTicket)
	{
		World->GetTimerManager().ClearTimer(WorldAuthPollTimer);
		TSharedPtr<FJsonObject> Reservation;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReservationJson);
		if (!FJsonSerializer::Deserialize(Reader, Reservation) || !Reservation.IsValid())
		{
			CompleteWorldTravelFailure(TEXT("no_ticket"));
			return;
		}
		FString Error;
		if (Reservation->TryGetStringField(TEXT("error"), Error))
		{
			CompleteWorldTravelFailure(Error == TEXT("over_capacity") || Error == TEXT("unknown_district") ||
				Error == TEXT("no_live_node") ? Error : TEXT("no_ticket"));
			return;
		}
		FString Ticket;
		FString Host;
		FString ReservationId;
		double PortNumber = 0;
		if (!Reservation->TryGetStringField(TEXT("ticket"), Ticket) || Ticket.IsEmpty() ||
			!Reservation->TryGetStringField(TEXT("host"), Host) || Host.IsEmpty() ||
			!Reservation->TryGetNumberField(TEXT("port"), PortNumber) || PortNumber < 1 || PortNumber > 65535 ||
			!Reservation->TryGetStringField(TEXT("reservation_id"), ReservationId) || ReservationId.IsEmpty())
		{
			CompleteWorldTravelFailure(TEXT("no_ticket"));
			return;
		}
		PendingTravelReservationId = ReservationId;
		bWorldTravelPending = false;
		const int32 Port = static_cast<int32>(PortNumber);
		if (UAPBGameInstanceSubsystem* APB = GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			APB->StartDistrictTravel(PlayerController, SelectedDistrictId, Host, Port, Ticket, ReservationId);
		}
		else
		{
			CompleteWorldTravelFailure(TEXT("travel_error"));
		}
		return;
	}
	WorldAuthTimeout += 0.25f;
	if (WorldAuthTimeout >= 10.f)
	{
		CompleteWorldTravelFailure(TEXT("timeout"));
	}
}

void UAPBFrontendWidget::CompleteWorldTravelFailure(const FString& Reason, const bool bEmitMarker)
{
	if (!bWorldTravelPending && PendingTravelReservationId.IsEmpty())
	{
		return;
	}
	bWorldTravelPending = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WorldAuthPollTimer);
	}
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (AAPBPlayerState* PlayerState = PlayerController->GetPlayerState<AAPBPlayerState>())
		{
			if (!PendingTravelReservationId.IsEmpty())
			{
				PlayerState->Server_ReleaseTravelReservation(PendingTravelReservationId);
			}
		}
	}
	PendingTravelReservationId.Empty();
	WorldAuthTimeout = 0.f;
	if (StatusText) StatusText->SetText(FText::FromString(TEXT("District travel failed")));
	SetStage(EAPBFrontendStage::DistrictSelect);
	if (bEmitMarker)
	{
		UE_LOG(LogTemp, Warning, TEXT("TRAVEL_FAIL reason=%s"), *Reason);
	}
}

void UAPBFrontendWidget::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	CompleteWorldTravelFailure(TEXT("travel_error"), false);
}
