#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHBoundaryWall.generated.h"

class UBoxComponent;

/**
 * Invisible wall that keeps players inside the playable part of a job map (the rest of the
 * town is scenery at night). Blocks pawns only: traces, the camera and items pass through.
 */
UCLASS()
class HORRORHEIST_API AHHBoundaryWall : public AActor
{
	GENERATED_BODY()

public:
	AHHBoundaryWall();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boundary")
	TObjectPtr<UBoxComponent> Wall;
};
