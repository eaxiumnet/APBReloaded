#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "APBInteractable.generated.h"

UENUM(BlueprintType)
enum class EAPBInteractableKind : uint8
{
	Mailbox = 0,
	AmmoBox,
	Resupply,
	Contact
};

/** World fixture: mailbox / ammo / resupply (APB freeroam loop props). */
UCLASS()
class APBRELOADED_API AAPBInteractable : public AActor
{
	GENERATED_BODY()
public:
	AAPBInteractable();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UBoxComponent* Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB")
	EAPBInteractableKind Kind = EAPBInteractableKind::Mailbox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="APB")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="APB")
	int32 UseCount = 0;

	UFUNCTION(BlueprintCallable, Category="APB")
	FString Interact(class APlayerController* PC);

	virtual void BeginPlay() override;
};
