#include "APBCharacterCreatePreviewActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Materials/MaterialInterface.h"
#include "APBVerifiedAssetRegistry.h"

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
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->ShowOnlyComponent(BodyMesh);

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetRelativeRotation(FRotator(-35.f, 40.f, 0.f));
	KeyLight->SetIntensity(2.f);

	FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetRelativeRotation(FRotator(-10.f, -140.f, 0.f));
	FillLight->SetIntensity(0.8f);
	FillLight->SetLightColor(FLinearColor(0.72f, 0.82f, 1.f));
}

void AAPBCharacterCreatePreviewActor::BeginPlay()
{
	Super::BeginPlay();
	// Far below world so freeroam/frontend camera never sees the studio by accident
	SetActorLocation(FVector(0.f, 0.f, -50000.f));

	if (!RenderTarget)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("CharCreateRT"));
		RenderTarget->InitCustomFormat(512, 640, PF_B8G8R8A8, false);
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
	// Gentle studio turntable (paused while the user drags to rotate)
	if (bAutoSpin)
	{
		YawSpin += DeltaSeconds * 25.f;
	}
	if (BodyMesh)
	{
		BodyMesh->SetRelativeRotation(FRotator(0.f, YawSpin + ManualYaw, 0.f));
	}
}

void AAPBCharacterCreatePreviewActor::SetAutoSpin(bool bSpin)
{
	bAutoSpin = bSpin;
}

void AAPBCharacterCreatePreviewActor::AddYaw(float Degrees)
{
	ManualYaw += Degrees;
}

UStaticMesh* AAPBCharacterCreatePreviewActor::LoadMesh(const TCHAR* Path) const
{
	UGameInstance* GI = GetGameInstance();
	UAPBVerifiedAssetRegistry* Registry = GI ? GI->GetSubsystem<UAPBVerifiedAssetRegistry>() : nullptr;
	// Character-create content is retail-sourced; the registry rejects a 2011 match.
	return Registry ? Registry->LoadStaticMesh(GetWorld(), Path, TEXT("character_create_preview_mesh"), TEXT("retail")) : nullptr;
}

FString AAPBCharacterCreatePreviewActor::ApplyBaseMesh(bool bEnforcer)
{
	const TCHAR* Path = bEnforcer
		? TEXT("/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha.m_contact_enforcement_larocha")
		: TEXT("/Game/Imported/Characters/Contact_Bloodrose/F_Contact_Criminal_Bloodrose.F_Contact_Criminal_Bloodrose");
	UStaticMesh* Mesh = LoadMesh(Path);
	if (Mesh && BodyMesh)
	{
		BodyMesh->SetStaticMesh(Mesh);
		const TCHAR* MaterialPath = TEXT("/Game/Imported/MaterialDatabase/DisplayPoint_CharacterMesh/MI_DisplyPoint_CharacterMesh.MI_DisplyPoint_CharacterMesh");
		UGameInstance* GI = GetGameInstance();
		UAPBVerifiedAssetRegistry* Registry = GI ? GI->GetSubsystem<UAPBVerifiedAssetRegistry>() : nullptr;
		UMaterialInterface* PreviewMaterial = Registry
			? Registry->LoadMaterialInterface(GetWorld(), MaterialPath, TEXT("character_create_preview_material"), TEXT("retail"))
			: nullptr;
		if (PreviewMaterial)
		{
			const int32 MaterialSlots = FMath::Max(1, BodyMesh->GetNumMaterials());
			for (int32 Slot = 0; Slot < MaterialSlots; ++Slot)
			{
				BodyMesh->SetMaterial(Slot, PreviewMaterial);
			}
			UE_LOG(LogTemp, Warning, TEXT("APBCharCreate PREVIEW_MATERIAL_OK path=%s slots=%d"), MaterialPath, MaterialSlots);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("APBCharCreate PREVIEW_MATERIAL_BLOCKED path=%s reason=source_material_missing"), MaterialPath);
		}
		LastMeshPath = Path;
		UE_LOG(LogTemp, Warning, TEXT("APBCharCreate PREVIEW_OK mesh=%s enf=%d"), *LastMeshPath, bEnforcer ? 1 : 0);
	}
	else
	{
		LastMeshPath = TEXT("missing");
		UE_LOG(LogTemp, Warning, TEXT("APBCharCreate PREVIEW_BLOCKED base_mesh_missing enf=%d path=%s"), bEnforcer ? 1 : 0, Path);
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
	if (UStaticMeshComponent* C = SlotComp(Slot))
	{
		C->SetVisibility(false);
		C->SetRelativeScale3D(FVector(0.12f));
	}
	BoundSlotCount = 0;
	UE_LOG(LogTemp, Warning, TEXT("APBCharCreate PREVIEW_SLOT_BLOCKED slot=%s item=%s reason=item_mesh_unavailable"),
		*Slot, *ItemId);
	CaptureNow();
	return false;
}

void AAPBCharacterCreatePreviewActor::CaptureNow()
{
	if (Capture)
	{
		Capture->CaptureScene();
	}
}
