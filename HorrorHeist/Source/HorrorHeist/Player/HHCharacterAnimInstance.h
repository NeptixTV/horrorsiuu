#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "HHCharacterAnimInstance.generated.h"

class UAnimSequence;
class UHHCharacterDefinition;

/** One weighted sequence sample, filled on the game thread, evaluated on the worker thread. */
struct FHHAnimSample
{
	const UAnimSequence* Sequence = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
};

/**
 * Native locomotion blender: idle / walk / run / crouch / fall, distance-matched playback
 * rate and a replicated look-pitch spread over the spine. Lets the project run without
 * an Animation Blueprint; a designer can still swap in one via the character definition.
 */
struct FHHAnimInstanceProxy : public FAnimInstanceProxy
{
	FHHAnimInstanceProxy() = default;
	explicit FHHAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	static constexpr int32 MaxSamples = 4;

	FHHAnimSample Samples[MaxSamples];
	int32 NumSamples = 0;
	float AimPitch = 0.f;
	TArray<FName> AimBones;
	TArray<float> AimWeights;

protected:
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	void ApplyAimPitch(FPoseContext& Output) const;
};

UCLASS(Transient, NotBlueprintable)
class HORRORHEIST_API UHHCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	friend struct FHHAnimInstanceProxy;

public:
	void InitializeFromDefinition(const UHHCharacterDefinition* Definition);

	/** Preview mannequins can force an idle pose without a moving pawn. */
	void SetPreviewMode(bool bInPreview) { bPreviewMode = bInPreview; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> RunAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CrouchIdleAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CrouchWalkAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> FallAnim;

	float WalkSpeedRef = 150.f;
	float RunSpeedRef = 400.f;
	float CrouchSpeedRef = 120.f;

	TArray<FName> AimBones;
	TArray<float> AimWeights;

	// Smoothed state
	float Speed = 0.f;
	float CrouchAlpha = 0.f;
	float FallAlpha = 0.f;
	float AimPitch = 0.f;

	// Playback clocks
	float IdleTime = 0.f;
	float CrouchIdleTime = 0.f;
	float FallTime = 0.f;
	/** Shared normalized gait phase keeps walk/run/crouch-walk feet in sync while blending. */
	float GaitPhase = 0.f;

	bool bPreviewMode = false;

	FHHAnimSample PendingSamples[FHHAnimInstanceProxy::MaxSamples];
	int32 PendingNumSamples = 0;
};
