#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "HHSearchableActor.generated.h"

class UBoxComponent;

/**
 * A drawer, cupboard, wardrobe or box that can be searched (hold the interact key). Searching
 * reveals the loot placed inside it (loot actors with the same ContainerId), slides the
 * optional drawer mesh open and makes a little noise. Each container is searched once.
 */
UCLASS()
class HORRORHEIST_API AHHSearchableActor : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHSearchableActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IHHInteractable
	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual FText GetInteractionDetail(AActor* Interactor) const override;
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
	virtual float GetInteractDuration(AActor* Interactor) const override { return SearchSeconds; }

	bool IsSearched() const { return bSearched; }

protected:
	UFUNCTION()
	void OnRep_Searched();

	/** What the trace hits: put it over the drawer front / cupboard doors. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Search")
	TObjectPtr<UBoxComponent> SearchVolume;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search")
	FName ContainerId;

	/** "dresser", "nightstand" ... used in the prompt ("Search the dresser"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search")
	FText ContainerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search", meta = (ClampMin = 0.0))
	float SearchSeconds = 1.6f;

	/** Local offset the Mesh (a drawer) slides to when opened. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search")
	FVector OpenOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search")
	FVector SearchVolumeExtent = FVector(30.f, 20.f, 20.f);

private:
	UPROPERTY(ReplicatedUsing = OnRep_Searched)
	bool bSearched = false;

	FVector ClosedLocation = FVector::ZeroVector;
	float OpenAlpha = 0.f;
};
