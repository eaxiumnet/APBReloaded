#include "APBDistrictStreamer.h"
#include "APBGameInstanceSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AAPBDistrictStreamer::AAPBDistrictStreamer()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AAPBDistrictStreamer::BeginPlay()
{
	Super::BeginPlay();
	RefreshAroundPlayer();
}

void AAPBDistrictStreamer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Accumul += DeltaSeconds;
	if (Accumul >= UpdateIntervalSec)
	{
		Accumul = 0.f;
		RefreshAroundPlayer();
	}
}

void AAPBDistrictStreamer::RefreshAroundPlayer()
{
	UWorld* World = GetWorld();
	if (!World) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	FVector Loc = FVector::ZeroVector;
	if (PC && PC->GetPawn()) Loc = PC->GetPawn()->GetActorLocation();
	else if (PC) Loc = PC->GetSpawnLocation();

	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			TArray<FString> Chunks = APB->GetStreamChunksNear(Loc.X * 0.01f, Loc.Y * 0.01f); // cm -> m-ish
			LoadedChunkIds = Chunks;
			UE_LOG(LogTemp, Verbose, TEXT("APB stream chunks loaded=%d around (%.1f,%.1f)"), Chunks.Num(), Loc.X, Loc.Y);
		}
	}
}
