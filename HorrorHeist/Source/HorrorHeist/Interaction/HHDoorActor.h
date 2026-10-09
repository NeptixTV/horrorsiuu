#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "HHDoorActor.generated.h"

class USoundBase;

/**
 * Hinged door / locker door. The server owns the target angle; clients swing smoothly.
 * Opens away from whoever uses it. The ambience director can nudge doors ajar while
 * nobody is looking.
 */
UCLASS()
class HORRORHEIST_API AHHDoorActor : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHDoorActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

	/** Server: move the door to an angle at a given speed (deg/s) without user input. */
	void SetTargetAngle(float Angle, float Speed, bool bPlayCreak);

	bool IsOpen() const { return !FMath::IsNearlyZero(TargetAngle, 1.f); }
	float GetCurrentAngle() const { return CurrentAngle; }

protected:
	UFUNCTION()
	void OnRep_TargetAngle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	float OpenAngle = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	float SwingSpeed = 160.f;

	/** Only opens in one direction (lockers, cabinets). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	bool bOneWay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FText OpenPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FText ClosePrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> CloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> CreakSound;

private:
	UPROPERTY(ReplicatedUsing = OnRep_TargetAngle)
	float TargetAngle = 0.f;

	UPROPERTY(Replicated)
	float ReplicatedSpeed = 160.f;

	UPROPERTY(Replicated)
	bool bReplicatedCreak = false;

	float CurrentAngle = 0.f;
	float PreviousTarget = 0.f;
};
