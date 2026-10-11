#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHHouseVolume.generated.h"

class UBoxComponent;

/**
 * Marks the inside of the target house (one or more boxes per house). The heist rules test
 * player positions against these twice a second: "inside" drives the entry objective, psyche
 * drain and who the house pays attention to. No overlap events, no tick.
 */
UCLASS()
class HORRORHEIST_API AHHHouseVolume : public AActor
{
	GENERATED_BODY()

public:
	AHHHouseVolume();

	bool ContainsPoint(const FVector& Point) const;
	FName GetHouseTag() const { return HouseTag; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "House")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "House")
	FName HouseTag;
};
