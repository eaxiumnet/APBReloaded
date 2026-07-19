#include "APBFrontendHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void AAPBFrontendHUD::SetHudStage(const FString& Stage, const FString& Status)
{
	StageLine = Stage;
	StatusLine = Status;
}

void AAPBFrontendHUD::DrawHUD()
{
	Super::DrawHUD();
	// UMG owns the frontend (video + form + F8 debug). No canvas overlays.
}
