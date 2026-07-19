#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "APBDistrictGameMode.generated.h"

/** Authoritative district session (freeroam multiplayer map host). */
UCLASS()
class APBRELOADED_API AAPBDistrictGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AAPBDistrictGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB")
	FString DistrictId = TEXT("Financial");

	UPROPERTY(BlueprintReadOnly, Category="APB")
	FString SessionId;

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
