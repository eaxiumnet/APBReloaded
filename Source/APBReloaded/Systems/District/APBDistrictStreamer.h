#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APBDistrictStreamer.generated.h"

/** Chunked freeroam load planner driven by Content/Data/district_stream.json via WorldService. */
UCLASS()
class APBRELOADED_API AAPBDistrictStreamer : public AActor
{
	GENERATED_BODY()
public:
	AAPBDistrictStreamer();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB|Stream")
	float UpdateIntervalSec = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category="APB|Stream")
	TArray<FString> LoadedChunkIds;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="APB|Stream")
	void RefreshAroundPlayer();

private:
	float Accumul = 0.f;
};
