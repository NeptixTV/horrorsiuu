#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HHFootstepComponent.generated.h"

class USoundBase;

/**
 * Distance-driven footsteps (no animation notifies needed): the stride length follows the
 * speed, the surface under the foot picks the sound set, and every step is reported to the
 * noise subsystem so future AI can hear the crew.
 */
UCLASS(ClassGroup = (HorrorHeist), meta = (BlueprintSpawnableComponent))
class HORRORHEIST_API UHHFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHHFootstepComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void PlayLanding(float ImpactSpeed);

	UPROPERTY(EditAnywhere, Category = "Footsteps")
	float WalkStride = 78.f;

	UPROPERTY(EditAnywhere, Category = "Footsteps")
	float SprintStride = 118.f;

	UPROPERTY(EditAnywhere, Category = "Footsteps")
	float CrouchStride = 58.f;

private:
	void PlayStep(float Loudness);
	USoundBase* PickSound(int32 SurfaceType);
	int32 TraceSurface(FVector& OutLocation) const;

	float DistanceAccumulator = 0.f;
	int32 LastIndex = INDEX_NONE;
};
