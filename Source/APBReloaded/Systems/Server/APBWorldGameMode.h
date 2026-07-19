#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "APBWorldGameMode.generated.h"

/** World/lobby routing host: character select + district list before travel. */
UCLASS()
class APBRELOADED_API AAPBWorldGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AAPBWorldGameMode();
};
