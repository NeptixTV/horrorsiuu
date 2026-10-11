#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "HHVanPoint.generated.h"

class UBoxComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EHHVanPointType : uint8
{
	/** Rear doors: everything in your bag that is loot goes into the van (secured). */
	Stash,
	/** Driver's door: leave with the crew (ends the job). */
	Drive
};

/**
 * Usable spots on the getaway van (the van body itself is plain scenery). Two of these sit at
 * the rear doors and the driver's door. The Drive point also defines "at the van" for the crew
 * count: everybody has to be within CrewRadius before the van can leave.
 */
UCLASS()
class HORRORHEIST_API AHHVanPoint : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHVanPoint();

	virtual void BeginPlay() override;

	// IHHInteractable
	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual FText GetInteractionDetail(AActor* Interactor) const override;
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
	virtual float GetInteractDuration(AActor* Interactor) const override;

	EHHVanPointType GetPointType() const { return PointType; }
	float GetCrewRadius() const { return CrewRadius; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UBoxComponent> UseVolume;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van")
	EHHVanPointType PointType = EHHVanPointType::Stash;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van")
	FVector UseExtent = FVector(45.f, 70.f, 90.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van")
	float CrewRadius = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van")
	float DriveHoldSeconds = 1.5f;
};
