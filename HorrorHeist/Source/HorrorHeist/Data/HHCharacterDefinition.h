#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HHCharacterDefinition.generated.h"

class USkeletalMesh;
class UAnimSequence;
class UMaterialInterface;
class UAnimInstance;

/**
 * Body + locomotion set of a playable character. Animations are blended natively by
 * UHHCharacterAnimInstance, so no Animation Blueprint is required. A designer can still
 * replace it with an Animation Blueprint through AnimClassOverride.
 */
UCLASS(BlueprintType)
class HORRORHEIST_API UHHCharacterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	/** Optional: an Animation Blueprint to use instead of the native blender. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftClassPtr<UAnimInstance> AnimClassOverride;

	/** Offset of the mesh relative to the capsule bottom (mesh pivot at the feet). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	float MeshYawOffset = -90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> Idle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> Walk;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> Run;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> CrouchIdle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> CrouchWalk;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> Fall;

	/** Ground speed (cm/s) the walk cycle was authored for. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	float WalkAnimSpeed = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	float RunAnimSpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	float CrouchWalkAnimSpeed = 120.f;

	/** Bones that receive the replicated look pitch (spine -> head), with weights. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TArray<FName> AimBones = { TEXT("spine_02"), TEXT("spine_03"), TEXT("neck_01"), TEXT("head") };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TArray<float> AimBoneWeights = { 0.2f, 0.25f, 0.25f, 0.3f };
};
