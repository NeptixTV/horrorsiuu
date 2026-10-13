#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "Core/HHTypes.h"
#include "HHIntelSpot.generated.h"

class UBoxComponent;

/**
 * Day scouting: a place in town where the crew learns something about a job - the mailbox,
 * a window to peek through, the gnome the spare key hides under, a bench to watch the owners
 * from. Holding interact adds its intel to the crew's notebook (AHHLobbyGameState), which the
 * job board's dossier shows. Placed in L_Town; idle during a job unless bUsableOnJobs.
 */
UCLASS()
class HORRORHEIST_API AHHIntelSpot : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHIntelSpot();

	virtual void BeginPlay() override;

	// IHHInteractable
	virtual FText GetInteractionDetail(AActor* Interactor) const override;
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
	virtual float GetInteractDuration(AActor* Interactor) const override { return HoldSeconds; }

protected:
	bool IsTimeRight() const;
	bool HasSomethingNew() const;
	bool IsOnJob() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Intel")
	TObjectPtr<UBoxComponent> UseVolume;

	/** FHHJobIntel ids learned here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intel")
	TArray<FName> IntelIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intel")
	EHHIntelTime TimeRule = EHHIntelTime::Any;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intel", meta = (ClampMin = 0.0))
	float HoldSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intel")
	FVector UseExtent = FVector(40.f, 40.f, 60.f);

	/** What the scout sees, shown to them ("Letters for Harold Vance - and for a Thomas Vance, aged 9."). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intel", meta = (MultiLine = true))
	FText FoundText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intel")
	bool bUsableOnJobs = false;
};
