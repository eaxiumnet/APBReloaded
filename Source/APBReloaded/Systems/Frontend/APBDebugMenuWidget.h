#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "APBFrontendTypes.h"
#include "APBDebugMenuWidget.generated.h"

class UBorder;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UButton;
class UCheckBox;
class UAPBFrontendWidget;
class UCanvasPanel;
class UScrollBox;

/**
 * Runtime debug overlay (F8): jump frontend stages, FPS cap, FPS/unit stats, menu volume.
 * Pure C++ UMG — no Blueprint asset required.
 */
UCLASS()
class APBRELOADED_API UAPBDebugMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "APB|Debug")
	void ToggleVisible();

	UFUNCTION(BlueprintCallable, Category = "APB|Debug")
	void SetMenuVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "APB|Debug")
	bool IsMenuVisible() const { return bMenuOpen; }

	/** Always-on corner FPS (independent of panel open). */
	UFUNCTION(BlueprintCallable, Category = "APB|Debug")
	void SetShowFpsOverlay(bool bShow);

	UFUNCTION(BlueprintCallable, Category = "APB|Debug")
	bool GetShowFpsOverlay() const { return bShowFpsOverlay; }

	void BindFrontend(UAPBFrontendWidget* InFrontend);

protected:
	void BuildLayout();
	void RefreshReadouts();
	void ApplyFpsCap(float CapOrZero);
	void ExecConsole(const FString& Cmd);
	UAPBFrontendWidget* ResolveFrontend() const;

	UButton* MakeBtn(const FString& Name, const FString& Label, float Width = 0.f);
	UTextBlock* MakeLbl(const FString& Name, const FString& Text, int32 Size, FLinearColor Color);
	void AddRow(UWidget* W, float PadY = 3.f);

	UFUNCTION() void OnStageSplash();
	UFUNCTION() void OnStageLogin();
	UFUNCTION() void OnStageCharSelect();
	UFUNCTION() void OnStageCharCreate();
	UFUNCTION() void OnStageDistrict();
	UFUNCTION() void OnStageLoading();
	UFUNCTION() void OnStageSettings();

	UFUNCTION() void OnFpsUncap();
	UFUNCTION() void OnFps30();
	UFUNCTION() void OnFps60();
	UFUNCTION() void OnFps120();
	UFUNCTION() void OnFps144();

	UFUNCTION() void OnToggleFpsOverlay();
	UFUNCTION() void OnToggleStatFps();
	UFUNCTION() void OnToggleStatUnit();
	UFUNCTION() void OnToggleStatUnitGraph();
	UFUNCTION() void OnVolMute();
	UFUNCTION() void OnVolHalf();
	UFUNCTION() void OnVolFull();
	UFUNCTION() void OnHidePanel();
	UFUNCTION() void OnCloseMenu();

	void JumpStage(EAPBFrontendStage Stage);

	UPROPERTY() TObjectPtr<UAPBFrontendWidget> FrontendRef = nullptr;

	UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas = nullptr;
	UPROPERTY() TObjectPtr<UBorder> PanelBorder = nullptr;
	UPROPERTY() TObjectPtr<UScrollBox> BodyScroll = nullptr;
	UPROPERTY() TObjectPtr<UVerticalBox> BodyBox = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> TitleText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> StatusText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> FpsCornerText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> HintText = nullptr;

	bool bMenuOpen = false;
	bool bShowFpsOverlay = true;
	bool bStatFpsOn = false;
	bool bStatUnitOn = false;
	bool bStatUnitGraphOn = false;
	float CurrentFpsCap = 60.f; // default 60; 0 = uncapped
	float SmoothedFps = 0.f;
	float ReadoutAccum = 0.f;
};
