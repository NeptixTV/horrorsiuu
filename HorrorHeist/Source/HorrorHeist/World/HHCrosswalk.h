#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHCrosswalk.generated.h"

class UBoxComponent;

/**
 * The area of a zebra crossing. Cars stop in front of it while anyone - a crew member or a
 * townsperson - is on it. Placed with the painted stripes by the world builder.
 */
UCLASS()
class HORRORHEIST_API AHHCrosswalk : public AActor
{
	GENERATED_BODY()

public:
	AHHCrosswalk();

	/** Someone on the crossing (characters and visible townspeople). */
	bool IsBusy() const;
	bool Contains(const FVector& Location) const;

	/** The first crossing within Distance ahead of Location along Direction (null if none); OutAlong = how far ahead it starts. */
	static AHHCrosswalk* FindAhead(const UObject* WorldContextObject, const FVector& Location, const FVector& Direction, float Distance, float* OutAlong = nullptr);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crosswalk")
	TObjectPtr<UBoxComponent> Area;
};
