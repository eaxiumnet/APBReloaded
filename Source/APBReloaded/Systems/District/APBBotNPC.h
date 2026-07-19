#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "APBBotNPC.generated.h"

/** Simple freeroam opposition / ambient bot. */
UCLASS()
class APBRELOADED_API AAPBBotNPC : public ACharacter
{
	GENERATED_BODY()
public:
	AAPBBotNPC();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB")
	bool bHostile = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB")
	FString DisplayLabel = TEXT("Opposition");

	UFUNCTION(BlueprintCallable, Category="APB")
	float ApplyDamagePoints(float Amount);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	FVector Home = FVector::ZeroVector;
	float WanderPhase = 0.f;
};
