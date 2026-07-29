#include "APBFreeroamCharacter.h"
#include "APBDriveableVehicle.h"
#include "APBGameInstanceSubsystem.h"
#include "APBPlayerState.h"
#include "APBInteractable.h"
#include "APBBotNPC.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "Engine/DamageEvents.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"

AAPBFreeroamCharacter::AAPBFreeroamCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = 600.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Imported APB contact meshes as freeroam clothing/visual baseline (faction-specific)
	ClothingVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClothingVisualMesh"));
	ClothingVisualMesh->SetupAttachment(GetCapsuleComponent());
	ClothingVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClothingVisualMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	ClothingVisualMesh->SetRelativeScale3D(FVector(1.f));

	// Multi-slot wardrobe markers (catalog-driven equip toggles visibility/scale)
	auto MakeSlot = [this](const TCHAR* Name, FVector Rel) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(GetCapsuleComponent());
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetRelativeLocation(Rel);
		C->SetRelativeScale3D(FVector(0.12f));
		C->SetVisibility(false);
		// Engine cube as slot marker until per-piece clothing meshes bulk-imported
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (Cube.Succeeded()) C->SetStaticMesh(Cube.Object);
		return C;
	};
	SlotHeadMesh = MakeSlot(TEXT("SlotHeadMesh"), FVector(0.f, 0.f, 70.f));
	SlotTorsoMesh = MakeSlot(TEXT("SlotTorsoMesh"), FVector(0.f, 0.f, 20.f));
	SlotLegsMesh = MakeSlot(TEXT("SlotLegsMesh"), FVector(0.f, 0.f, -30.f));
	SlotFeetMesh = MakeSlot(TEXT("SlotFeetMesh"), FVector(0.f, 0.f, -75.f));
	SlotHandsMesh = MakeSlot(TEXT("SlotHandsMesh"), FVector(0.f, 25.f, 10.f));
	SlotAccessoryMesh = MakeSlot(TEXT("SlotAccessoryMesh"), FVector(15.f, 0.f, 55.f));
	SlotFaceMesh = MakeSlot(TEXT("SlotFaceMesh"), FVector(12.f, 0.f, 65.f));

	CatalogWeaponId = TEXT("");
}

void AAPBFreeroamCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyFactionVisualMesh();
}

bool AAPBFreeroamCharacter::ApplyFactionVisualMesh()
{
	if (!ClothingVisualMesh) return false;
	bool bEnforcer = false;
	if (const AAPBPlayerState* PS = GetPlayerState<AAPBPlayerState>())
	{
		// EAPBFaction: assume Enforcer enum non-zero / named — read via subsystem snapshot when available
		bEnforcer = false;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			const FAPBDomainSnapshotUE S = APB->CaptureDomainSnapshot();
			bEnforcer = S.bEnforcer;
		}
	}
	// Prefer wardrobe LC studio mesh when present, else faction contact heroes
	UStaticMesh* ContactMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Imported/Characters/Wardrobe/StudioCharacter.StudioCharacter"));
	const TCHAR* Path = bEnforcer
		? TEXT("/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha.m_contact_enforcement_larocha")
		: TEXT("/Game/Imported/Characters/Contact_Bloodrose/F_Contact_Criminal_Bloodrose.F_Contact_Criminal_Bloodrose");
	if (!ContactMesh)
	{
		ContactMesh = LoadObject<UStaticMesh>(nullptr, Path);
	}
	if (!ContactMesh)
	{
		Path = bEnforcer
			? TEXT("/Game/Imported/Characters/Contact_Sofia/F_Contact_Enforcement_Sofia.F_Contact_Enforcement_Sofia")
			: TEXT("/Game/Imported/Characters/Contact_Bloodrose/F_Contact_Criminal_Bloodrose.F_Contact_Criminal_Bloodrose");
		ContactMesh = LoadObject<UStaticMesh>(nullptr, Path);
	}
	if (!ContactMesh) return false;
	ClothingVisualMesh->SetStaticMesh(ContactMesh);
	AppliedClothingSummary = bEnforcer ? TEXT("enforcer_contact_mesh") : TEXT("criminal_contact_mesh");
	return true;
}

static UStaticMeshComponent* SlotComponentFor(AAPBFreeroamCharacter* Self, const FString& Slot)
{
	if (!Self) return nullptr;
	const FString S = Slot.ToLower();
	if (S == TEXT("head")) return Self->SlotHeadMesh;
	if (S == TEXT("torso")) return Self->SlotTorsoMesh;
	if (S == TEXT("legs")) return Self->SlotLegsMesh;
	if (S == TEXT("feet")) return Self->SlotFeetMesh;
	if (S == TEXT("hands")) return Self->SlotHandsMesh;
	if (S == TEXT("accessory")) return Self->SlotAccessoryMesh;
	if (S == TEXT("face")) return Self->SlotFaceMesh;
	return nullptr;
}

bool AAPBFreeroamCharacter::EquipClothingVisual(const FString& Slot, const FString& ItemId)
{
	bool bOk = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			// Domain equip is authoritative; visual re-applies faction mesh + records slot
			bOk = APB->EquipClothingItem(Slot, ItemId);
		}
	}
	ApplyFactionVisualMesh();
	// Map catalog clothing items onto imported wardrobe/body meshes (hash pick among pool)
	static const TCHAR* WardrobePool[] = {
		TEXT("/Game/Imported/Characters/Wardrobe/StudioCharacter.StudioCharacter"),
		TEXT("/Game/Imported/Characters/Wardrobe/Contact_Bloodrose_F_Contact_Criminal_Bloodrose.Contact_Bloodrose_F_Contact_Criminal_Bloodrose"),
		TEXT("/Game/Imported/Characters/Wardrobe/Contact_Chaos_Bloodrose_Male_Contact_Chaos_Bloodrose_Male.Contact_Chaos_Bloodrose_Male_Contact_Chaos_Bloodrose_Male"),
		TEXT("/Game/Imported/Characters/Wardrobe/Contact_GKing_DoubleB_contact_gking_doubleb.Contact_GKing_DoubleB_contact_gking_doubleb"),
		TEXT("/Game/Imported/Characters/Wardrobe/LC_M_Clubber_Casual_CaucasianLight_03_StudioCharacter.LC_M_Clubber_Casual_CaucasianLight_03_StudioCharacter"),
		TEXT("/Game/Imported/Characters/Wardrobe/LC_M_Business_City_CaucasianLight_04_StudioCharacter.LC_M_Business_City_CaucasianLight_04_StudioCharacter"),
		TEXT("/Game/Imported/Characters/Contact_Bloodrose/F_Contact_Criminal_Bloodrose.F_Contact_Criminal_Bloodrose"),
		TEXT("/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha.m_contact_enforcement_larocha"),
		TEXT("/Game/Imported/Characters/Contact_Sofia/F_Contact_Enforcement_Sofia.F_Contact_Enforcement_Sofia"),
	};
	if (!ItemId.IsEmpty() && ClothingVisualMesh)
	{
		const uint32 H = GetTypeHash(ItemId + Slot);
		const int32 N = UE_ARRAY_COUNT(WardrobePool);
		const TCHAR* Pick = WardrobePool[H % N];
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Pick))
		{
			// Torso/face drive body mesh; other slots still show markers
			if (Slot.Equals(TEXT("torso"), ESearchCase::IgnoreCase) || Slot.Equals(TEXT("face"), ESearchCase::IgnoreCase))
			{
				ClothingVisualMesh->SetStaticMesh(M);
			}
		}
	}
	if (UStaticMeshComponent* SlotComp = SlotComponentFor(this, Slot))
	{
		const bool bHasItem = !ItemId.IsEmpty() && ItemId != TEXT("None");
		SlotComp->SetVisibility(bHasItem);
		// Slight scale bump so equipped slots read in freeroam
		SlotComp->SetRelativeScale3D(bHasItem ? FVector(0.18f) : FVector(0.12f));
		if (bHasItem) ++WardrobeSlotCount;
	}
	AppliedClothingSummary = FString::Printf(TEXT("%s=%s visual=%s wardrobe=%d"),
		*Slot, *ItemId, *AppliedClothingSummary, WardrobeSlotCount);
	return bOk || ClothingVisualMesh != nullptr;
}

int32 AAPBFreeroamCharacter::GetActiveWardrobeSlotCount() const
{
	int32 N = 0;
	auto Count = [&](UStaticMeshComponent* C) { if (C && C->IsVisible()) ++N; };
	Count(SlotHeadMesh); Count(SlotTorsoMesh); Count(SlotLegsMesh); Count(SlotFeetMesh);
	Count(SlotHandsMesh); Count(SlotAccessoryMesh); Count(SlotFaceMesh);
	return N;
}

int32 AAPBFreeroamCharacter::EquipFullWardrobeVisual()
{
	WardrobeSlotCount = 0;
	bool bEnforcer = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			bEnforcer = APB->CaptureDomainSnapshot().bEnforcer;
		}
	}
	// Full slot set: head/torso/legs/feet/hands/accessory/face (catalog IDs from clothing.json)
	struct FPair { const TCHAR* Slot; const TCHAR* Crim; const TCHAR* Enf; };
	const FPair Sets[] = {
		{ TEXT("head"), TEXT("Clothing_Cap_Flex_T1"), TEXT("Clothing_Head_Cap_T1") },
		{ TEXT("torso"), TEXT("Clothing_Crim_Hoodie_T1"), TEXT("Clothing_Enf_Jacket_T1") },
		{ TEXT("legs"), TEXT("Clothing_Crim_Jeans_T1"), TEXT("Clothing_Enf_Pants_T1") },
		{ TEXT("feet"), TEXT("Clothing_Boots_Urban_T1"), TEXT("Clothing_Feet_Sneakers_T1") },
		{ TEXT("hands"), TEXT("Clothing_Gloves_Tactical"), TEXT("Clothing_Hands_Gloves_T1") },
		{ TEXT("accessory"), TEXT("Clothing_Accessory_Chain_T1"), TEXT("Clothing_Accessory_Comm") },
		{ TEXT("face"), TEXT("Clothing_Face_Mask_T1"), TEXT("Clothing_Face_Mask_T1") },
	};
	int32 Equipped = 0;
	for (const FPair& P : Sets)
	{
		const FString Item = bEnforcer ? P.Enf : P.Crim;
		if (EquipClothingVisual(P.Slot, Item)) ++Equipped;
	}
	WardrobeSlotCount = GetActiveWardrobeSlotCount();
	AppliedClothingSummary = FString::Printf(TEXT("full_wardrobe slots=%d/%d faction=%s"),
		WardrobeSlotCount, 7, bEnforcer ? TEXT("Enforcer") : TEXT("Criminal"));
	return WardrobeSlotCount;
}

void AAPBFreeroamCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAPBFreeroamCharacter, LastShotDamage);
	DOREPLIFETIME(AAPBFreeroamCharacter, ShotsFired);
}

void AAPBFreeroamCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveForward", this, &AAPBFreeroamCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AAPBFreeroamCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &AAPBFreeroamCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AAPBFreeroamCharacter::LookUp);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AAPBFreeroamCharacter::OnFirePressed);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AAPBFreeroamCharacter::OnInteractPressed);
	// Key fallbacks (works even if project Input.ini lacks named actions)
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AAPBFreeroamCharacter::OnEnterVehiclePressed);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AAPBFreeroamCharacter::OnInteractPressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AAPBFreeroamCharacter::OnFirePressed);
	// Axis fallbacks for legacy mapping
	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AAPBFreeroamCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &AAPBFreeroamCharacter::MoveRight);
}

void AAPBFreeroamCharacter::MoveForward(float V)
{
	if (Controller && V != 0.f)
	{
		const FRotator Yaw(0, Controller->GetControlRotation().Yaw, 0);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), V);
	}
}

void AAPBFreeroamCharacter::MoveRight(float V)
{
	if (Controller && V != 0.f)
	{
		const FRotator Yaw(0, Controller->GetControlRotation().Yaw, 0);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), V);
	}
}

void AAPBFreeroamCharacter::Turn(float V) { AddControllerYawInput(V); }
void AAPBFreeroamCharacter::LookUp(float V) { AddControllerPitchInput(V); }

void AAPBFreeroamCharacter::OnFirePressed()
{
	if (HasAuthority()) FireWeaponLocal();
	else ServerFireWeapon();
}

void AAPBFreeroamCharacter::ServerFireWeapon_Implementation()
{
	FireWeaponLocal();
}

float AAPBFreeroamCharacter::FireWeaponLocal()
{
	if (!HasAuthority()) return 0.f;
	UWorld* World = GetWorld();
	if (!World) return 0.f;

	FVector Start = FollowCamera ? FollowCamera->GetComponentLocation() : GetActorLocation() + FVector(0, 0, 64);
	FVector Dir = FollowCamera ? FollowCamera->GetForwardVector() : GetActorForwardVector();
	FVector End = Start + Dir * WeaponRange;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(APBWeapon), true, this);
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Red : FColor::Green, false, 2.f, 0, 1.5f);

	float Dmg = 0.f;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
		{
			float Hp = 0.f; bool bKill = false;
			Dmg = APB->FireCatalogWeapon(CatalogWeaponId, 3.f, 0.f, Hp, bKill);
			// Apply physical damage / bot health
			if (bHit && Hit.GetActor())
			{
				if (AAPBBotNPC* Bot = Cast<AAPBBotNPC>(Hit.GetActor()))
				{
					Bot->ApplyDamagePoints(Dmg);
				}
				else
				{
					FDamageEvent DmgEvent;
					Hit.GetActor()->TakeDamage(Dmg, DmgEvent, GetController(), this);
				}
			}
			// D16b: meta state syncs only via the Domain bridge, never direct PlayerState writes.
			APB->SyncPlayerStateFromDomain(GetPlayerState<AAPBPlayerState>());
		}
	}
	LastShotDamage = Dmg;
	++ShotsFired;
	ForceNetUpdate();
	return Dmg;
}

void AAPBFreeroamCharacter::OnEnterVehiclePressed()
{
	if (HasAuthority()) EnterNearestVehicle();
	else ServerEnterNearestVehicle();
}

void AAPBFreeroamCharacter::ServerEnterNearestVehicle_Implementation()
{
	EnterNearestVehicle();
}

bool AAPBFreeroamCharacter::EnterNearestVehicle()
{
	if (!HasAuthority()) return false;
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAPBDriveableVehicle::StaticClass(), Found);
	AAPBDriveableVehicle* Best = nullptr;
	float BestDist = 5000.f;
	for (AActor* A : Found)
	{
		const float D = FVector::Dist(A->GetActorLocation(), GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = Cast<AAPBDriveableVehicle>(A);
		}
	}
	if (!Best) return false;
	if (AController* C = GetController())
	{
		C->Possess(Best);
		Best->SetDriverCharacter(this);
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UAPBGameInstanceSubsystem* APB = GI->GetSubsystem<UAPBGameInstanceSubsystem>())
			{
				// Catalog ids from vehicles.json (apbdb); empty id uses Domain first-Vehicle fallback
				if (!APB->SpawnCatalogVehicle(TEXT("Vehicle_Car_A_UtilityEstate")))
				{
					APB->SpawnCatalogVehicle(TEXT(""));
				}
				APB->PossessCatalogVehicle();
			}
		}
		return true;
	}
	return false;
}

void AAPBFreeroamCharacter::ApplyMoveInput(float Forward, float Right)
{
	MoveForward(Forward);
	MoveRight(Right);
}

void AAPBFreeroamCharacter::OnInteractPressed()
{
	InteractNearest();
}

FString AAPBFreeroamCharacter::InteractNearest()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAPBInteractable::StaticClass(), Found);
	AAPBInteractable* Best = nullptr;
	float BestDist = 400.f;
	for (AActor* A : Found)
	{
		const float D = FVector::Dist(A->GetActorLocation(), GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = Cast<AAPBInteractable>(A);
		}
	}
	if (!Best) return TEXT("none");
	APlayerController* PC = Cast<APlayerController>(GetController());
	const FString R = Best->Interact(PC);
	UE_LOG(LogTemp, Log, TEXT("APB INTERACT_RESULT %s"), *R);
	return R;
}
