#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "APBDriveableVehicle.generated.h"

UCLASS()
class APBRELOADED_API AAPBDriveableVehicle : public APawn
{
	GENERATED_BODY()
public:
	AAPBDriveableVehicle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UStaticMeshComponent* Mesh;

	/** Imported APB chassis (glTF fleet under /Game/Imported/Vehicles/*). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class USkeletalMeshComponent* ChassisMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UCameraComponent* Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB|Vehicle")
	float DriveSpeed = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB|Vehicle")
	FString CatalogVehicleId;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="APB|Vehicle")
	bool bHasDriver = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="APB|Vehicle")
	float DistanceDriven = 0.f;

	void SetDriverCharacter(APawn* Char);

	/** Bind skeletal chassis from CatalogVehicleId family if present. */
	UFUNCTION(BlueprintCallable, Category="APB|Vehicle")
	bool ApplyCatalogVisualMesh();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Apply throttle like axis input (used by playable probe). */
	UFUNCTION(BlueprintCallable, Category="APB|Vehicle")
	void ApplyThrottleInput(float ForwardAxis);

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void BeginPlay() override;

protected:
	void MoveForward(float V);
	void MoveRight(float V);
	float Throttle = 0.f;
	float Steering = 0.f;
	TWeakObjectPtr<APawn> DriverChar;
	FVector LastLoc;
};
