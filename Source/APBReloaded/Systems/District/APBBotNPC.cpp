#include "APBBotNPC.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

AAPBBotNPC::AAPBBotNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	GetCharacterMovement()->MaxWalkSpeed = 280.f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	bUseControllerRotationYaw = false;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AAPBBotNPC::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	WanderPhase = FMath::FRandRange(0.f, 6.28f);
	if (USkeletalMeshComponent* SM = GetMesh())
	{
		SM->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		SM->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}
}

void AAPBBotNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Health <= 0.f) return;
	WanderPhase += DeltaSeconds;
	const FVector Offset(FMath::Sin(WanderPhase) * 400.f, FMath::Cos(WanderPhase * 0.7f) * 400.f, 0.f);
	const FVector Target = Home + Offset;
	const FVector Dir = (Target - GetActorLocation()).GetSafeNormal2D();
	AddMovementInput(Dir, 0.6f);
}

float AAPBBotNPC::ApplyDamagePoints(float Amount)
{
	Health = FMath::Max(0.f, Health - Amount);
	if (Health <= 0.f)
	{
		GetCharacterMovement()->DisableMovement();
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
	}
	return Health;
}
