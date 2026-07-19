#pragma once
#include "CoreMinimal.h"
#include "APBFrontendTypes.generated.h"

UENUM(BlueprintType)
enum class EAPBFrontendStage : uint8
{
	Splash = 0,
	Login,
	CharacterSelect,
	CharacterCreate,
	DistrictSelect,
	Settings,
	Loading,
	InDistrict
};

USTRUCT(BlueprintType)
struct FAPBClothingChoice
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Slot;
	UPROPERTY(BlueprintReadOnly) int64 ArmasPrice = 0;
};
