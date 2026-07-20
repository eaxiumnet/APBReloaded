#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APBCharacterCreatePreviewActor.generated.h"

class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UDirectionalLightComponent;

/**
 * Off-screen studio actor for CharacterCreate: body mesh + SceneCapture2D → RT for UMG.
 * Uses real Imported character static meshes (Steam-derived exports).
 */
UCLASS()
class APBRELOADED_API AAPBCharacterCreatePreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AAPBCharacterCreatePreviewActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Load base body from faction (Criminal Bloodrose / Enforcer LaRocha). Returns mesh path applied. */
	UFUNCTION(BlueprintCallable, Category = "APB|Create")
	FString ApplyBaseMesh(bool bEnforcer);

	/** Scale body from height/build (0.8–1.2 domain range). */
	UFUNCTION(BlueprintCallable, Category = "APB|Create")
	void ApplyBodyProfile(float Height, float Build);

	/** Frame the capture per retail APBLCC values: distance PosY, height PosZ, aim TargetZ, FOV. */
	UFUNCTION(BlueprintCallable, Category = "APB|Create")
	void FrameCamera(float PosY, float PosZ, float TargetZ, float Fov);

	/**
	 * Visual bind for a clothing slot using catalog item id.
	 * Torso/face can swap body mesh from wardrobe pool; other slots toggle markers.
	 * Returns true if a mesh bind succeeded.
	 */
	UFUNCTION(BlueprintCallable, Category = "APB|Create")
	bool ApplyClothingSlotVisual(const FString& Slot, const FString& ItemId);

	UFUNCTION(BlueprintCallable, Category = "APB|Create")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	UFUNCTION(BlueprintCallable, Category = "APB|Create")
	FString GetLastMeshPath() const { return LastMeshPath; }

	UFUNCTION(BlueprintCallable, Category = "APB|Create")
	int32 GetBoundSlotCount() const { return BoundSlotCount; }

	/** Force a capture for the UI image. */
	void CaptureNow();

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BodyMesh = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SlotHeadMesh = nullptr;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SlotTorsoMesh = nullptr;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SlotLegsMesh = nullptr;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SlotFeetMesh = nullptr;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SlotHandsMesh = nullptr;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SlotAccessoryMesh = nullptr;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SlotFaceMesh = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneCaptureComponent2D> Capture = nullptr;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> KeyLight = nullptr;

	FString LastMeshPath;
	int32 BoundSlotCount = 0;
	float YawSpin = 0.f;

	UStaticMeshComponent* SlotComp(const FString& Slot) const;
	static UStaticMesh* LoadMesh(const TCHAR* Path);
};
