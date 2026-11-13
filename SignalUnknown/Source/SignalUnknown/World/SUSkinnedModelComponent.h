// SIGNAL UNKNOWN – animated model: a skinned mesh from a .sumodel file, deformed on the CPU.
//
// Plays the clips stored in the file (Idle, Walk, Run, Attack, Scream) with a cross fade between
// them. Runs on every machine from replicated state; nothing here is replicated itself.
#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "World/SUModelLibrary.h"
#include "SUSkinnedModelComponent.generated.h"

UCLASS()
class SIGNALUNKNOWN_API USUSkinnedModelComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	USUSkinnedModelComponent(const FObjectInitializer& ObjectInitializer);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Selects the mesh. Returns false if the library has no skinned mesh with this name. */
	bool SetModel(FName MeshName);
	bool HasModel() const { return ModelRef.IsValid(); }

	/** Starts a clip (no-op if it is already playing, unless bRestart). */
	void PlayClip(FName ClipName, float BlendTime = 0.25f, bool bRestart = false);
	void SetPlayRate(float InRate) { PlayRate = InRate; }
	FName GetClipName() const { return CurrentName; }
	/** Non-looping clip reached its end. */
	bool IsClipFinished() const;
	float GetClipDuration(FName ClipName) const;
	/** Value stored with the model (speeds in cm/s). */
	float GetModelValue(const char* Key, float Default) const;

private:
	FSUModelMeshRef ModelRef;
	FName CurrentName;
	int32 CurrentClip = -1;
	int32 PreviousClip = -1;
	float CurrentTime = 0.f;
	float PreviousTime = 0.f;
	float BlendAlpha = 1.f;
	float BlendDuration = 0.25f;
	float PlayRate = 1.f;
	float SkinAccumulator = 1.f;

	SUModel::Pose PoseA;
	SUModel::Pose PoseB;
	std::vector<float> WorldMatrices;
	std::vector<float> SkinMatrices;
	std::vector<float> SkinnedPositions;
	std::vector<float> SkinnedNormals;
	std::vector<float> SkinnedTangents;

	struct FSectionBuffers
	{
		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
	};
	TArray<FSectionBuffers> Buffers;

	void UpdateSkin();
};
