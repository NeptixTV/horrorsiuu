#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HHNoiseSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_FourParams(FHHOnNoise, const FVector& /*Location*/, float /*Loudness*/, AActor* /*Instigator*/, FName /*Tag*/);

/**
 * Server-side noise bus. Footsteps, doors, dropped loot and (later) voice chat loudness
 * report here; resident and ghost AI will subscribe and run spatial propagation (rooms,
 * floors, doors, materials) on top of this single entry point.
 */
UCLASS()
class HORRORHEIST_API UHHNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Loudness 0..1 (whisper .. glass breaking). */
	void ReportNoise(const FVector& Location, float Loudness, AActor* Instigator, FName Tag);

	FHHOnNoise OnNoise;
};
