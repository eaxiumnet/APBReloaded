#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "APBPlayerController.generated.h"

UCLASS()
class APBRELOADED_API AAPBPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	UFUNCTION(Exec)
	void APBChat(int32 DelayMs, const FString& RawLine);

	UFUNCTION(Exec)
	void APBChatTravel(int32 DelayMs, const FString& DistrictId);
};
