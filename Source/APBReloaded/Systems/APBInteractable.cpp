#include "APBInteractable.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "APBGameInstanceSubsystem.h"
#include "Engine/GameInstance.h"

AAPBInteractable::AAPBInteractable()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->InitBoxExtent(FVector(60.f, 60.f, 80.f));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Overlap);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.2f));
	}
	DisplayName = TEXT("Interactable");
}

void AAPBInteractable::BeginPlay()
{
	Super::BeginPlay();
	switch (Kind)
	{
	case EAPBInteractableKind::Mailbox:
		DisplayName = TEXT("Mailbox");
		Mesh->SetRelativeScale3D(FVector(0.6f, 0.4f, 0.9f));
		break;
	case EAPBInteractableKind::AmmoBox:
		DisplayName = TEXT("Ammo Box");
		Mesh->SetRelativeScale3D(FVector(0.9f, 0.7f, 0.5f));
		break;
	case EAPBInteractableKind::Resupply:
		DisplayName = TEXT("Resupply Station");
		Mesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.0f));
		break;
	case EAPBInteractableKind::Contact:
		DisplayName = TEXT("Contact");
		Mesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.8f));
		break;
	}
#if WITH_EDITOR
	SetActorLabel(DisplayName);
#endif
}

FString AAPBInteractable::Interact(APlayerController* PC)
{
	++UseCount;
	UGameInstance* GI = GetGameInstance();
	UAPBGameInstanceSubsystem* APB = GI ? GI->GetSubsystem<UAPBGameInstanceSubsystem>() : nullptr;
	FString Result = FString::Printf(TEXT("%s used=%d"), *DisplayName, UseCount);
	if (APB && Kind == EAPBInteractableKind::AmmoBox)
	{
		// Grant a primary weapon ammo proxy via Armas-listed catalog if possible
		FString Err;
		if (APB->ArmasPurchase(TEXT("Weapon_OCA-EW 20 Super"), Err) || APB->ArmasPurchase(TEXT("Weapon_NTEC-5"), Err))
		{
			Result += TEXT(" +ammo_restock");
		}
		else
		{
			Result += TEXT(" +ammo_check");
		}
		APB->PushDomainSnapshotToAllPlayerStates();
	}
	else if (APB && Kind == EAPBInteractableKind::Resupply)
	{
		Result += TEXT(" +resupply_ok");
		APB->PushDomainSnapshotToAllPlayerStates();
	}
	else if (APB && Kind == EAPBInteractableKind::Mailbox)
	{
		Result += TEXT(" +mail_stub_ok");
	}
	else if (APB && Kind == EAPBInteractableKind::Contact)
	{
		APB->StartOppositionMission();
		APB->PushDomainSnapshotToAllPlayerStates();
		Result += TEXT(" +mission_offer");
	}
	UE_LOG(LogTemp, Log, TEXT("APB INTERACT kind=%d %s"), (int32)Kind, *Result);
	return Result;
}
