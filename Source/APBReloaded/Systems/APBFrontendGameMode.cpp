#include "APBFrontendGameMode.h"
#include "APBFrontendPlayerController.h"
#include "APBFrontendHUD.h"
#include "APBPlayerState.h"
#include "GameFramework/SpectatorPawn.h"
#include "APBGameInstanceSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

AAPBFrontendGameMode::AAPBFrontendGameMode()
{
	PlayerControllerClass = AAPBFrontendPlayerController::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	PlayerStateClass = AAPBPlayerState::StaticClass();
	HUDClass = AAPBFrontendHUD::StaticClass();
	bStartPlayersAsSpectators = true;
}

void AAPBFrontendGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			APB->InitCatalogFromProjectData();
		}
	}

	// Place a simple camera so the empty map is not an undefined view
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ACameraActor* Cam = World->SpawnActor<ACameraActor>(FVector(0, 0, 200), FRotator(-10.f, 0, 0), Sp))
		{
			if (UCameraComponent* C = Cam->GetCameraComponent())
			{
				C->bConstrainAspectRatio = false;
				C->SetFieldOfView(90.f);
			}
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->SetViewTarget(Cam);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("APBFrontend GameMode ready UI_STAGE=Splash (HUD+Widget)"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Cyan,
			TEXT("APB Frontend GameMode — Splash UI loading..."));
	}
}
