#pragma once

#include "CoreMinimal.h"

class USkeletalMeshComponent;
class UHHCosmeticComponent;
class UHHCharacterDefinition;
class UAnimSequence;

/** Townspeople (residents at windows, the neighbour, police) wear the crew's body and cosmetics. */
namespace HHNpcBody
{
	/**
	 * Puts the default body mesh on Body (with the definition's yaw offset), dresses it with the
	 * given cosmetic item ids (slot taken from each item) and returns the definition for its
	 * animations. Null when the game data has no character (content not built).
	 */
	HORRORHEIST_API const UHHCharacterDefinition* Setup(USkeletalMeshComponent* Body, UHHCosmeticComponent* Cosmetics, const TArray<FName>& CosmeticIds);

	/** Plays a looping single-node animation unless it is already playing. */
	HORRORHEIST_API void PlayLoop(USkeletalMeshComponent* Body, UAnimSequence* Anim);
}
