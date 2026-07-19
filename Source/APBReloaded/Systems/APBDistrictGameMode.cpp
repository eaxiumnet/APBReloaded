#include "APBDistrictGameMode.h"
#include "APBPlayerState.h"
#include "APBGameInstanceSubsystem.h"
#include "APBDistrictStreamer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AAPBDistrictGameMode::AAPBDistrictGameMode()
{
	PlayerStateClass = AAPBPlayerState::StaticClass();
	bUseSeamlessTravel = true;
	// Listen-server freeroam: host acts as district authority when dedicated packaging unavailable.
	bUseSeamlessTravel = true;
}

void AAPBDistrictGameMode::BeginPlay()
{
	Super::BeginPlay();
	SessionId = FString::Printf(TEXT("DS-%s-1"), *DistrictId);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			if (APB->GetPhase() != TEXT("District"))
			{
				APB->RegisterAccount(TEXT("host"), TEXT("host"));
				APB->Login(TEXT("host"), TEXT("host"));
				APB->EnterWorld(TEXT("W1"));
				APB->CreateCharacter(TEXT("Host"), false);
				APB->JoinDistrict(DistrictId);
			}
			SessionId = APB->GetSessionId();
			// Spawn streamer for chunked freeroam
			if (UWorld* World = GetWorld())
			{
				FActorSpawnParameters Sp;
				Sp.Name = TEXT("APBDistrictStreamer");
				World->SpawnActor<AAPBDistrictStreamer>(AAPBDistrictStreamer::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Sp);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("APB District session %s district=%s (listen-server freeroam)"), *SessionId, *DistrictId);
}

void AAPBDistrictGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			const FString Name = NewPlayer && NewPlayer->PlayerState ? NewPlayer->PlayerState->GetPlayerName() : TEXT("Peer");
			APB->JoinDistrictAsPeer(SessionId, Name);
			if (AAPBPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AAPBPlayerState>() : nullptr)
			{
				// Server authority: seed currency/threat from host service
				PS->ThreatPoints = APB->GetThreatPoints();
			}
		}
	}
}

