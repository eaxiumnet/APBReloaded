#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "APBFrontendGameMode.generated.h"

/** Boot map: splash/login/character/district UI only (no freeroam pawn). */
UCLASS()
class APBRELOADED_API AAPBFrontendGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AAPBFrontendGameMode();
	virtual void BeginPlay() override;
};
