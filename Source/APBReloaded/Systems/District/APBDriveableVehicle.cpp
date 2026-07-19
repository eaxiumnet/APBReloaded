#include "APBDriveableVehicle.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/SkeletalMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AAPBDriveableVehicle::AAPBDriveableVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	// Collision body — blocks world geometry (not query-only skate)
	UBoxComponent* Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->InitBoxExtent(FVector(180.f, 90.f, 60.f));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionObjectType(ECC_Vehicle);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Body->SetSimulatePhysics(false);
	Body->SetCanEverAffectNavigation(false);
	RootComponent = Body;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // body handles collision
	// APB coupe only — never seed /Engine/BasicShapes/Cube
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Coupe(
		TEXT("/Game/Imported/Vehicles/V_A_2DrCoupe/PartMesh.PartMesh"));
	if (Coupe.Succeeded())
	{
		Mesh->SetStaticMesh(Coupe.Object);
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	}

	ChassisMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChassisMesh"));
	ChassisMesh->SetupAttachment(RootComponent);
	ChassisMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChassisMesh->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	ChassisMesh->SetVisibility(false);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);

	// Real movement component (not raw no-sweep world offset)
	UFloatingPawnMovement* Move = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Move->SetUpdatedComponent(Body);
	Move->MaxSpeed = DriveSpeed;
	Move->Acceleration = 8000.f;
	Move->Deceleration = 4000.f;
	Move->TurningBoost = 8.f;
	// Keep free XY motion; gravity not required (ground plane is large box)
	Move->bConstrainToPlane = false;
	CatalogVehicleId = TEXT("Vehicle_V_A_2DrCoupe");
}

bool AAPBDriveableVehicle::ApplyCatalogVisualMesh()
{
	// Map catalog ids / package families onto imported glTF chassis under /Game/Imported/Vehicles
	static const TCHAR* Families[] = {
		TEXT("V_A_2DrCoupe"), TEXT("V_A_2DrVan"), TEXT("V_A_Taxi"), TEXT("V_A_SUV"),
		TEXT("V_A_Hatchback"), TEXT("V_A_Pickup"), TEXT("V_A_Saloon"), TEXT("V_A_Roadster"),
	};
	const FString Id = CatalogVehicleId;
	const TCHAR* Pick = Families[0];
	for (const TCHAR* Fam : Families)
	{
		if (Id.Contains(Fam, ESearchCase::IgnoreCase) ||
			Id.Contains(FString(Fam).RightChop(4), ESearchCase::IgnoreCase)) // strip V_A_
		{
			Pick = Fam;
			break;
		}
	}
	// Heuristic: van/truck/taxi keywords
	if (Id.Contains(TEXT("Van"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Utility"), ESearchCase::IgnoreCase))
	{
		Pick = TEXT("V_A_2DrVan");
	}
	else if (Id.Contains(TEXT("Taxi"), ESearchCase::IgnoreCase))
	{
		Pick = TEXT("V_A_Taxi");
	}
	else if (Id.Contains(TEXT("SUV"), ESearchCase::IgnoreCase))
	{
		Pick = TEXT("V_A_SUV");
	}
	else if (Id.Contains(TEXT("Pickup"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Truck"), ESearchCase::IgnoreCase))
	{
		Pick = TEXT("V_A_Pickup");
	}
	else if (Id.Contains(TEXT("Saloon"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Sedan"), ESearchCase::IgnoreCase))
	{
		Pick = TEXT("V_A_Saloon");
	}
	else if (Id.Contains(TEXT("Hatch"), ESearchCase::IgnoreCase))
	{
		Pick = TEXT("V_A_Hatchback");
	}
	else if (Id.Contains(TEXT("Roadster"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Sports"), ESearchCase::IgnoreCase))
	{
		Pick = TEXT("V_A_Roadster");
	}

	const FString Path = FString::Printf(
		TEXT("/Game/Imported/Vehicles/%s/LOD_Base_Mesh/SkeletalMeshes/LOD_Base_Mesh.LOD_Base_Mesh"), Pick);
	if (USkeletalMesh* Sk = LoadObject<USkeletalMesh>(nullptr, *Path))
	{
		if (ChassisMesh)
		{
			ChassisMesh->SetSkeletalMesh(Sk);
			ChassisMesh->SetVisibility(true);
			if (Mesh)
			{
				Mesh->SetVisibility(false);
			}
			return true;
		}
	}
	return false;
}

void AAPBDriveableVehicle::BeginPlay()
{
	Super::BeginPlay();
	ApplyCatalogVisualMesh();
	if (UFloatingPawnMovement* Move = FindComponentByClass<UFloatingPawnMovement>())
	{
		if (USceneComponent* Root = GetRootComponent())
		{
			Move->SetUpdatedComponent(Root);
		}
		Move->MaxSpeed = DriveSpeed;
		Move->SetActive(true);
		Move->SetComponentTickEnabled(true);
	}
}

void AAPBDriveableVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAPBDriveableVehicle, bHasDriver);
	DOREPLIFETIME(AAPBDriveableVehicle, DistanceDriven);
}

void AAPBDriveableVehicle::SetDriverCharacter(APawn* Char)
{
	DriverChar = Char;
	bHasDriver = Char != nullptr;
	LastLoc = GetActorLocation();
	DistanceDriven = 0.f;
	ForceNetUpdate();
}

void AAPBDriveableVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindAxis("MoveForward", this, &AAPBDriveableVehicle::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AAPBDriveableVehicle::MoveRight);
	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AAPBDriveableVehicle::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &AAPBDriveableVehicle::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
}

void AAPBDriveableVehicle::MoveForward(float V)
{
	Throttle = V;
	if (FMath::Abs(V) > KINDA_SMALL_NUMBER)
	{
		AddMovementInput(GetActorForwardVector(), V);
	}
}

void AAPBDriveableVehicle::MoveRight(float V)
{
	Steering = V;
	if (FMath::Abs(V) > KINDA_SMALL_NUMBER)
	{
		// Yaw via controller / rotation
		AddActorWorldRotation(FRotator(0.f, V * 60.f * GetWorld()->GetDeltaSeconds(), 0.f));
	}
}

void AAPBDriveableVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Steering assist while throttling
	if (FMath::Abs(Steering) > KINDA_SMALL_NUMBER)
	{
		AddActorWorldRotation(FRotator(0.f, Steering * 90.f * DeltaSeconds, 0.f));
	}
	const FVector Now = GetActorLocation();
	const float Step = FVector::Dist(Now, LastLoc);
	if (Step > KINDA_SMALL_NUMBER)
	{
		DistanceDriven += Step;
		LastLoc = Now;
	}
}

void AAPBDriveableVehicle::ApplyThrottleInput(float ForwardAxis)
{
	// Same path as axis binding → FloatingPawnMovement via AddMovementInput
	MoveForward(ForwardAxis);
}

void AAPBDriveableVehicle::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	bHasDriver = true;
	LastLoc = GetActorLocation();
	DistanceDriven = 0.f;
	ForceNetUpdate();
}

void AAPBDriveableVehicle::UnPossessed()
{
	Super::UnPossessed();
	bHasDriver = false;
	Throttle = 0.f;
	ForceNetUpdate();
}
