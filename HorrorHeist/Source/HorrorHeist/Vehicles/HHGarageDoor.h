#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "HHGarageDoor.generated.h"

class USoundBase;

/**
 * A roll-up garage door (the hideout's door onto the ramp). Opens and closes when used directly
 * or from inside the van (garage remote). The slats roll up into the drum at the top: the mesh is
 * squashed upwards, its collision with it, so the opening frees from the bottom.
 */
UCLASS()
class HORRORHEIST_API AHHGarageDoor : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHGarageDoor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Nearest garage door within Range of Location, or null. */
	static AHHGarageDoor* FindNearest(const UObject* WorldContextObject, const FVector& Location, float Range);

	// IHHInteractable
	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual FText GetInteractionDetail(AActor* Interactor) const override;
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
	virtual float GetInteractDuration(AActor* Interactor) const override { return 0.4f; }

	/** False (with the reason) where the door must stay shut (on a job nothing is behind it). */
	bool CanOperate(FText& OutReason) const;
	/** Server. */
	void Toggle();
	bool IsOpen() const { return bOpen; }

protected:
	UFUNCTION()
	void OnRep_Open();

	/** Rails and drum (static); Mesh is the curtain that rolls up. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Garage")
	TObjectPtr<UStaticMeshComponent> Frame;

	void ApplyAlpha();

	/** Seconds for a full open / close. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garage")
	float TravelSeconds = 3.5f;

	/** The door only works in the hideout / town hub, not on a job. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garage")
	bool bHubOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garage")
	TObjectPtr<USoundBase> MotorSound;

	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;

	float Alpha = 0.f;
	float DoorHeight = 290.f;
	FVector BaseScale = FVector::OneVector;
	FVector BaseLocation = FVector::ZeroVector;
};
