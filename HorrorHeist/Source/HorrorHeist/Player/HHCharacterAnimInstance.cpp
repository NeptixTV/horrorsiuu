#include "Player/HHCharacterAnimInstance.h"
#include "Data/HHCharacterDefinition.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "Algo/Sort.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ReferenceSkeleton.h"

// ---------------------------------------------------------------------------------------
// Game thread

void UHHCharacterAnimInstance::InitializeFromDefinition(const UHHCharacterDefinition* Definition)
{
	if (!Definition)
	{
		return;
	}
	IdleAnim = Definition->Idle.LoadSynchronous();
	WalkAnim = Definition->Walk.LoadSynchronous();
	RunAnim = Definition->Run.LoadSynchronous();
	CrouchIdleAnim = Definition->CrouchIdle.LoadSynchronous();
	CrouchWalkAnim = Definition->CrouchWalk.LoadSynchronous();
	FallAnim = Definition->Fall.LoadSynchronous();

	WalkSpeedRef = FMath::Max(Definition->WalkAnimSpeed, 1.f);
	RunSpeedRef = FMath::Max(Definition->RunAnimSpeed, WalkSpeedRef + 1.f);
	CrouchSpeedRef = FMath::Max(Definition->CrouchWalkAnimSpeed, 1.f);

	AimBones = Definition->AimBones;
	AimWeights = Definition->AimBoneWeights;
}

FAnimInstanceProxy* UHHCharacterAnimInstance::CreateAnimInstanceProxy()
{
	return new FHHAnimInstanceProxy(this);
}

void UHHCharacterAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	Super::DestroyAnimInstanceProxy(InProxy);
}

namespace
{
	float LoopTime(const UAnimSequence* Sequence, float Time)
	{
		const float Length = Sequence ? static_cast<float>(Sequence->GetPlayLength()) : 0.f;
		return Length > UE_KINDA_SMALL_NUMBER ? FMath::Fmod(Time, Length) : 0.f;
	}

	float PhaseTime(const UAnimSequence* Sequence, float Phase)
	{
		const float Length = Sequence ? static_cast<float>(Sequence->GetPlayLength()) : 0.f;
		return FMath::Clamp(Phase, 0.f, 0.9999f) * Length;
	}

	float CycleDistance(const UAnimSequence* Sequence, float ReferenceSpeed)
	{
		const float Length = Sequence ? static_cast<float>(Sequence->GetPlayLength()) : 1.f;
		return FMath::Max(Length * ReferenceSpeed, 1.f);
	}
}

void UHHCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	float TargetSpeed = 0.f;
	bool bCrouched = false;
	bool bFalling = false;
	float TargetPitch = 0.f;

	if (const APawn* Pawn = TryGetPawnOwner())
	{
		if (!bPreviewMode)
		{
			TargetSpeed = Pawn->GetVelocity().Size2D();
			TargetPitch = FRotator::NormalizeAxis(Pawn->GetBaseAimRotation().Pitch);
			if (const ACharacter* Character = Cast<ACharacter>(Pawn))
			{
				bCrouched = Character->bIsCrouched;
				bFalling = Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling();
			}
		}
	}

	Speed = FMath::FInterpTo(Speed, TargetSpeed, DeltaSeconds, 10.f);
	CrouchAlpha = FMath::FInterpTo(CrouchAlpha, bCrouched ? 1.f : 0.f, DeltaSeconds, 9.f);
	FallAlpha = FMath::FInterpTo(FallAlpha, bFalling ? 1.f : 0.f, DeltaSeconds, 7.f);
	AimPitch = FMath::FInterpTo(AimPitch, FMath::Clamp(TargetPitch, -70.f, 70.f), DeltaSeconds, 12.f);

	// Fallbacks so a partial animation set still looks sensible.
	const UAnimSequence* Idle = IdleAnim;
	const UAnimSequence* Walk = WalkAnim ? WalkAnim.Get() : Idle;
	const UAnimSequence* Run = RunAnim ? RunAnim.Get() : Walk;
	const UAnimSequence* CrouchIdle = CrouchIdleAnim ? CrouchIdleAnim.Get() : Idle;
	const UAnimSequence* CrouchWalk = CrouchWalkAnim ? CrouchWalkAnim.Get() : Walk;
	const UAnimSequence* Fall = FallAnim ? FallAnim.Get() : Idle;

	const float MoveAlpha = FMath::Clamp(Speed / WalkSpeedRef, 0.f, 1.f);
	const float RunAlpha = FMath::Clamp((Speed - WalkSpeedRef) / (RunSpeedRef - WalkSpeedRef), 0.f, 1.f);
	const float CrouchMoveAlpha = FMath::Clamp(Speed / CrouchSpeedRef, 0.f, 1.f);

	// Distance-matched gait: advance the shared phase by distance travelled.
	const float StandDistance = FMath::Lerp(CycleDistance(Walk, WalkSpeedRef), CycleDistance(Run, RunSpeedRef), RunAlpha);
	const float Distance = FMath::Lerp(StandDistance, CycleDistance(CrouchWalk, CrouchSpeedRef), CrouchAlpha);
	if (Speed > 1.f)
	{
		GaitPhase = FMath::Fmod(GaitPhase + DeltaSeconds * Speed / Distance, 1.f);
	}
	IdleTime += DeltaSeconds;
	CrouchIdleTime += DeltaSeconds;
	FallTime = bFalling ? FallTime + DeltaSeconds : 0.f;

	const float Stand = (1.f - CrouchAlpha) * (1.f - FallAlpha);
	const float Crouch = CrouchAlpha * (1.f - FallAlpha);

	FHHAnimSample Candidates[6];
	Candidates[0] = { Idle,			LoopTime(Idle, IdleTime),				Stand * (1.f - MoveAlpha) };
	Candidates[1] = { Walk,			PhaseTime(Walk, GaitPhase),				Stand * MoveAlpha * (1.f - RunAlpha) };
	Candidates[2] = { Run,			PhaseTime(Run, GaitPhase),				Stand * MoveAlpha * RunAlpha };
	Candidates[3] = { CrouchIdle,	LoopTime(CrouchIdle, CrouchIdleTime),	Crouch * (1.f - CrouchMoveAlpha) };
	Candidates[4] = { CrouchWalk,	PhaseTime(CrouchWalk, GaitPhase),		Crouch * CrouchMoveAlpha };
	Candidates[5] = { Fall,			LoopTime(Fall, FallTime),				FallAlpha };

	// Keep the strongest samples (at most MaxSamples) and renormalize.
	Algo::Sort(Candidates, [](const FHHAnimSample& A, const FHHAnimSample& B) { return A.Weight > B.Weight; });
	PendingNumSamples = 0;
	float Total = 0.f;
	for (const FHHAnimSample& Candidate : Candidates)
	{
		if (Candidate.Sequence && Candidate.Weight > 0.01f && PendingNumSamples < FHHAnimInstanceProxy::MaxSamples)
		{
			PendingSamples[PendingNumSamples++] = Candidate;
			Total += Candidate.Weight;
		}
	}
	for (int32 Index = 0; Index < PendingNumSamples; ++Index)
	{
		PendingSamples[Index].Weight /= FMath::Max(Total, UE_KINDA_SMALL_NUMBER);
	}

	// Hand the frame to the proxy (evaluated after this on a worker thread).
	FHHAnimInstanceProxy& Proxy = GetProxyOnGameThread<FHHAnimInstanceProxy>();
	Proxy.NumSamples = PendingNumSamples;
	for (int32 Index = 0; Index < PendingNumSamples; ++Index)
	{
		Proxy.Samples[Index] = PendingSamples[Index];
	}
	Proxy.AimPitch = AimPitch;
	Proxy.AimBones = AimBones;
	Proxy.AimWeights = AimWeights;
}

// ---------------------------------------------------------------------------------------
// Worker thread

void FHHAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
}

bool FHHAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (NumSamples <= 0 || !Samples[0].Sequence)
	{
		Output.ResetToRefPose();
		ApplyAimPitch(Output);
		return true;
	}

	{
		FAnimationPoseData PoseData(Output);
		Samples[0].Sequence->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Samples[0].Time)));
	}

	float Accumulated = Samples[0].Weight;
	for (int32 Index = 1; Index < NumSamples; ++Index)
	{
		const FHHAnimSample& Sample = Samples[Index];
		if (!Sample.Sequence || Sample.Weight <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FPoseContext SamplePose(Output);
		{
			FAnimationPoseData SampleData(SamplePose);
			Sample.Sequence->GetAnimationPose(SampleData, FAnimExtractContext(static_cast<double>(Sample.Time)));
		}

		FPoseContext Blended(Output);
		{
			const FAnimationPoseData CurrentData(Output);
			const FAnimationPoseData SampleData(SamplePose);
			FAnimationPoseData BlendedData(Blended);
			const float WeightOfCurrent = Accumulated / (Accumulated + Sample.Weight);
			FAnimationRuntime::BlendTwoPosesTogether(CurrentData, SampleData, WeightOfCurrent, BlendedData);
		}
		Output = Blended;
		Accumulated += Sample.Weight;
	}

	ApplyAimPitch(Output);
	return true;
}

void FHHAnimInstanceProxy::ApplyAimPitch(FPoseContext& Output) const
{
	if (FMath::IsNearlyZero(AimPitch, 0.1f) || AimBones.Num() == 0)
	{
		return;
	}

	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
	const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();

	// The body faces +Y in mesh space, so pitch is a rotation around mesh +X.
	const FVector PitchAxis(1.f, 0.f, 0.f);

	for (int32 Index = 0; Index < AimBones.Num(); ++Index)
	{
		const int32 SkeletonIndex = RefSkeleton.FindBoneIndex(AimBones[Index]);
		if (SkeletonIndex == INDEX_NONE)
		{
			continue;
		}
		const FCompactPoseBoneIndex PoseIndex = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SkeletonIndex));
		if (!PoseIndex.IsValid())
		{
			continue;
		}

		// Reference-pose rotation of the bone in component space (root * ... * parent * bone).
		FQuat RefComponentRotation = FQuat::Identity;
		for (int32 Bone = SkeletonIndex; Bone != INDEX_NONE; Bone = RefSkeleton.GetParentIndex(Bone))
		{
			RefComponentRotation = RefPose[Bone].GetRotation() * RefComponentRotation;
		}
		const FVector LocalAxis = RefComponentRotation.UnrotateVector(PitchAxis).GetSafeNormal();
		const float Weight = AimWeights.IsValidIndex(Index) ? AimWeights[Index] : 1.f / AimBones.Num();
		const FQuat Delta(LocalAxis, FMath::DegreesToRadians(AimPitch * Weight));

		FTransform& BoneTransform = Output.Pose[PoseIndex];
		BoneTransform.SetRotation((BoneTransform.GetRotation() * Delta).GetNormalized());
	}
}
