#include "APBCharacterCreatePreviewActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AAPBCharacterCreatePreviewActor::AAPBCharacterCreatePreviewActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCastShadow(true);

	auto MakeSlot = [this](const TCHAR* Name, FVector Rel) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(BodyMesh);
		C->SetRelativeLocation(Rel);
		C->SetRelativeScale3D(FVector(0.12f));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetVisibility(false);
		return C;
	};
	SlotHeadMesh = MakeSlot(TEXT("SlotHeadMesh"), FVector(0, 0, 90));
	SlotTorsoMesh = MakeSlot(TEXT("SlotTorsoMesh"), FVector(0, 0, 40));
	SlotLegsMesh = MakeSlot(TEXT("SlotLegsMesh"), FVector(0, 0, -20));
	SlotFeetMesh = MakeSlot(TEXT("SlotFeetMesh"), FVector(0, 0, -70));
	SlotHandsMesh = MakeSlot(TEXT("SlotHandsMesh"), FVector(35, 0, 30));
	SlotAccessoryMesh = MakeSlot(TEXT("SlotAccessoryMesh"), FVector(0, 20, 50));
	SlotFaceMesh = MakeSlot(TEXT("SlotFaceMesh"), FVector(15, 0, 80));

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	Capture->SetRelativeLocation(FVector(220.f, 0.f, 60.f));
	Capture->SetRelativeRotation(FRotator(-8.f, 180.f, 0.f));
	Capture->bCaptureEveryFrame = true;
	Capture->bCaptureOnMovement = true;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	Capture->ShowOnlyComponent(BodyMesh);

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetRelativeRotation(FRotator(-35.f, 40.f, 0.f));
	KeyLight->SetIntensity(8.f);
}

void AAPBCharacterCreatePreviewActor::BeginPlay()
{
	Super::BeginPlay();
	// Far below world so freeroam/frontend camera never sees the studio by accident
	SetActorLocation(FVector(0.f, 0.f, -50000.f));

	if (!RenderTarget)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("CharCreateRT"));
		RenderTarget->InitAutoFormat(512, 640);
		RenderTarget->ClearColor = FLinearColor(0.02f, 0.05f, 0.09f, 1.f);
		RenderTarget->UpdateResourceImmediate(true);
	}
	if (Capture && RenderTarget)
	{
		Capture->TextureTarget = RenderTarget;
		Capture->ClearShowOnlyComponents();
		Capture->ShowOnlyComponent(BodyMesh);
		for (UStaticMeshComponent* C : { SlotHeadMesh, SlotTorsoMesh, SlotLegsMesh, SlotFeetMesh,
			SlotHandsMesh, SlotAccessoryMesh, SlotFaceMesh })
		{
			if (C) Capture->ShowOnlyComponent(C);
		}
		Capture->CaptureScene();
	}
	FrameCamera(280.f, 95.f, 95.f, 55.f);
	ApplyBaseMesh(false);
	UE_LOG(LogTemp, Warning, TEXT("APBCharCreate PREVIEW_ACTOR_SPAWN path=%s"), *LastMeshPath);
}

void AAPBCharacterCreatePreviewActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Gentle studio turntable
	YawSpin += DeltaSeconds * 25.f;
	if (BodyMesh)
	{
		BodyMesh->SetRelativeRotation(FRotator(0.f, YawSpin, 0.f));
	}
}

UStaticMesh* AAPBCharacterCreatePreviewActor::LoadMesh(const TCHAR* Path)
{
	return LoadObject<UStaticMesh>(nullptr, Path);
}

FString AAPBCharacterCreatePreviewActor::ApplyBaseMesh(bool bEnforcer)
{
	const TCHAR* Paths[] = {
		bEnforcer
			? TEXT("/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha.m_contact_enforcement_larocha")
			: TEXT("/Game/Imported/Characters/Contact_Bloodrose/F_Contact_Criminal_Bloodrose.F_Contact_Criminal_Bloodrose"),
		TEXT("/Game/Imported/Characters/Wardrobe/StudioCharacter.StudioCharacter"),
		TEXT("/Game/Imported/Characters/Contact_Sofia/F_Contact_Enforcement_Sofia.F_Contact_Enforcement_Sofia"),
	};
	UStaticMesh* Mesh = nullptr;
	FString Used;
	for (const TCHAR* P : Paths)
	{
		Mesh = LoadMesh(P);
		if (Mesh)
		{
			Used = P;
			break;
		}
	}
	if (Mesh && BodyMesh)
	{
		BodyMesh->SetStaticMesh(Mesh);
		LastMeshPath = Used;
		UE_LOG(LogTemp, Warning, TEXT("APBCharCreate PREVIEW_OK mesh=%s enf=%d"), *LastMeshPath, bEnforcer ? 1 : 0);
	}
	else
	{
		LastMeshPath = TEXT("missing");
		UE_LOG(LogTemp, Warning, TEXT("APBCharCreate PREVIEW_FAIL no_mesh enf=%d"), bEnforcer ? 1 : 0);
	}
	CaptureNow();
	return LastMeshPath;
}

void AAPBCharacterCreatePreviewActor::ApplyBodyProfile(float Height, float Build)
{
	Height = FMath::Clamp(Height, 0.8f, 1.2f);
	Build = FMath::Clamp(Build, 0.8f, 1.2f);
	if (BodyMesh)
	{
		BodyMesh->SetRelativeScale3D(FVector(Build, Build, Height));
	}
	UE_LOG(LogTemp, Warning, TEXT("APBCharCreate PREVIEW_BODY height=%.3f build=%.3f"), Height, Build);
	CaptureNow();
}

void AAPBCharacterCreatePreviewActor::FrameCamera(float PosY, float PosZ, float TargetZ, float Fov)
{
	if (!Capture) return;
	Capture->SetRelativeLocation(FVector(PosY, 0.f, PosZ));
	// Camera faces -X (yaw 180); pitch up/down to aim at TargetZ over horizontal distance PosY.
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(TargetZ - PosZ, FMath::Max(PosY, 1.f)));
	Capture->SetRelativeRotation(FRotator(Pitch, 180.f, 0.f));
	Capture->FOVAngle = FMath::Clamp(Fov, 10.f, 120.f);
	UE_LOG(LogTemp, Warning, TEXT("APBCharCreate PREVIEW_FRAME posY=%.1f posZ=%.1f targetZ=%.1f fov=%.1f pitch=%.2f"),
		PosY, PosZ, TargetZ, Fov, Pitch);
	CaptureNow();
}

UStaticMeshComponent* AAPBCharacterCreatePreviewActor::SlotComp(const FString& Slot) const
{
	const FString S = Slot.ToLower();
	if (S == TEXT("head")) return SlotHeadMesh;
	if (S == TEXT("torso")) return SlotTorsoMesh;
	if (S == TEXT("legs")) return SlotLegsMesh;
	if (S == TEXT("feet")) return SlotFeetMesh;
	if (S == TEXT("hands")) return SlotHandsMesh;
	if (S == TEXT("accessory")) return SlotAccessoryMesh;
	if (S == TEXT("face")) return SlotFaceMesh;
	return nullptr;
}

bool AAPBCharacterCreatePreviewActor::ApplyClothingSlotVisual(const FString& Slot, const FString& ItemId)
{
	static const TCHAR* WardrobePool[] = {
		TEXT("/Game/Imported/Characters/Wardrobe/StudioCharacter.StudioCharacter"),
		TEXT("/Game/Imported/Characters/Contact_Bloodrose/F_Contact_Criminal_Bloodrose.F_Contact_Criminal_Bloodrose"),
		TEXT("/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha.m_contact_enforcement_larocha"),
		TEXT("/Game/Imported/Characters/Contact_Sofia/F_Contact_Enforcement_Sofia.F_Contact_Enforcement_Sofia"),
	};
	bool bBound = false;
	if (!ItemId.IsEmpty() && BodyMesh)
	{
		const uint32 H = GetTypeHash(ItemId + Slot);
		const TCHAR* Pick = WardrobePool[H % UE_ARRAY_COUNT(WardrobePool)];
		if (Slot.Equals(TEXT("torso"), ESearchCase::IgnoreCase) || Slot.Equals(TEXT("face"), ESearchCase::IgnoreCase))
		{
			if (UStaticMesh* M = LoadMesh(Pick))
			{
				BodyMesh->SetStaticMesh(M);
				LastMeshPath = Pick;
				bBound = true;
			}
		}
	}
	if (UStaticMeshComponent* C = SlotComp(Slot))
	{
		const bool bHas = !ItemId.IsEmpty() && ItemId != TEXT("None");
		C->SetVisibility(bHas);
		C->SetRelativeScale3D(bHas ? FVector(0.18f) : FVector(0.12f));
		// Reuse body mesh as simple slot marker when available
		if (bHas && BodyMesh && BodyMesh->GetStaticMesh())
		{
			C->SetStaticMesh(BodyMesh->GetStaticMesh());
			bBound = true;
		}
	}
	BoundSlotCount = 0;
	for (UStaticMeshComponent* C : { SlotHeadMesh, SlotTorsoMesh, SlotLegsMesh, SlotFeetMesh,
		SlotHandsMesh, SlotAccessoryMesh, SlotFaceMesh })
	{
		if (C && C->IsVisible()) ++BoundSlotCount;
	}
	UE_LOG(LogTemp, Warning, TEXT("APBCharCreate PREVIEW_SLOT slot=%s item=%s bound=%d body=%s"),
		*Slot, *ItemId, bBound ? 1 : 0, *LastMeshPath);
	CaptureNow();
	return bBound || !LastMeshPath.IsEmpty();
}

void AAPBCharacterCreatePreviewActor::CaptureNow()
{
	if (Capture)
	{
		Capture->CaptureScene();
	}
}
