#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "APBFreeroamCharacter.generated.h"

class AAPBDriveableVehicle;

/** Playable third-person freeroam character: catalog weapon fire + enter vehicle. */
UCLASS()
class APBRELOADED_API AAPBFreeroamCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AAPBFreeroamCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	class UCameraComponent* FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB|Combat")
	float WeaponRange = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB|Combat")
	FString CatalogWeaponId;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="APB|Combat")
	float LastShotDamage = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="APB|Combat")
	int32 ShotsFired = 0;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Server, Unreliable, Category="APB|Combat")
	void ServerFireWeapon();

	UFUNCTION(BlueprintCallable, Category="APB|Combat")
	float FireWeaponLocal();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="APB|Vehicle")
	void ServerEnterNearestVehicle();

	UFUNCTION(BlueprintCallable, Category="APB|Vehicle")
	bool EnterNearestVehicle();

	/** Use nearest mailbox / ammo / resupply / contact. */
	UFUNCTION(BlueprintCallable, Category="APB|World")
	FString InteractNearest();

	/** Forward for input binding / probes — uses CharacterMovement, not teleport. */
	UFUNCTION(BlueprintCallable, Category="APB|Input")
	void ApplyMoveInput(float Forward, float Right);

	/** Apply imported contact mesh from faction (clothing visual baseline). */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	bool ApplyFactionVisualMesh();

	/** Equip clothing slot from catalog id; re-applies faction mesh + slot wardrobe markers. */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	bool EquipClothingVisual(const FString& Slot, const FString& ItemId);

	/** Equip full default multi-slot wardrobe for current faction (head/torso/legs/feet/hands/accessory/face). */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	int32 EquipFullWardrobeVisual();

	/** Number of wardrobe slot components with an active item marker. */
	UFUNCTION(BlueprintCallable, Category="APB|Customization")
	int32 GetActiveWardrobeSlotCount() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="APB|Customization")
	class UStaticMeshComponent* ClothingVisualMesh;

	/** Per-slot wardrobe markers (full clothing set depth). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="APB|Customization")
	class UStaticMeshComponent* SlotHeadMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="APB|Customization")
	class UStaticMeshComponent* SlotTorsoMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="APB|Customization")
	class UStaticMeshComponent* SlotLegsMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="APB|Customization")
	class UStaticMeshComponent* SlotFeetMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="APB|Customization")
	class UStaticMeshComponent* SlotHandsMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="APB|Customization")
	class UStaticMeshComponent* SlotAccessoryMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="APB|Customization")
	class UStaticMeshComponent* SlotFaceMesh;

	UPROPERTY(BlueprintReadOnly, Category="APB|Customization")
	FString AppliedClothingSummary;

	UPROPERTY(BlueprintReadOnly, Category="APB|Customization")
	int32 WardrobeSlotCount = 0;

	virtual void BeginPlay() override;

protected:
	void MoveForward(float V);
	void MoveRight(float V);
	void Turn(float V);
	void LookUp(float V);
	void OnFirePressed();
	void OnEnterVehiclePressed();
	void OnInteractPressed();
};
