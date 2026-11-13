#include "World/SUSkinnedModelComponent.h"
#include "SignalUnknown.h"
#include "Engine/World.h"

USUSkinnedModelComponent::USUSkinnedModelComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bUseAsyncCooking = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(true);
}

bool USUSkinnedModelComponent::SetModel(FName MeshName)
{
	USUModelLibrary* Library = USUModelLibrary::Get(this);
	FSUModelMeshRef Ref;
	if (!Library || !Library->FindMesh(MeshName, Ref) || !Ref.Mesh->bSkinned || Ref.File->Data.Joints.empty())
	{
		return false;
	}
	ModelRef = Ref;
	ClearAllMeshSections();
	Library->FillMesh(this, ModelRef, false);
	Buffers.SetNum(static_cast<int32>(Ref.Mesh->Sections.size()));
	for (int32 S = 0; S < Buffers.Num(); ++S)
	{
		const SUModel::Section& Section = Ref.Mesh->Sections[static_cast<size_t>(S)];
		const int32 Count = static_cast<int32>(Section.NumVertices);
		FSectionBuffers& B = Buffers[S];
		B.Vertices.SetNumUninitialized(Count);
		B.Normals.SetNumUninitialized(Count);
		B.UVs.SetNumUninitialized(Count);
		B.Tangents.SetNum(Count);
		for (int32 V = 0; V < Count; ++V)
		{
			const size_t I = static_cast<size_t>(V);
			B.UVs[V] = FVector2D(Section.UVs[I * 2], Section.UVs[I * 2 + 1]);
			B.Tangents[V].bFlipTangentY = Section.Tangents[I * 4 + 3] < 0.f;
		}
	}
	CurrentClip = -1;
	PreviousClip = -1;
	CurrentName = NAME_None;
	PlayClip(TEXT("Idle"), 0.f, true);
	UpdateSkin();
	return true;
}

float USUSkinnedModelComponent::GetModelValue(const char* Key, float Default) const
{
	return ModelRef.IsValid() ? ModelRef.File->Data.GetMeta(Key, Default) : Default;
}

float USUSkinnedModelComponent::GetClipDuration(FName ClipName) const
{
	if (!ModelRef.IsValid())
	{
		return 0.f;
	}
	const int32 Index = ModelRef.File->Data.FindClip(TCHAR_TO_UTF8(*ClipName.ToString()));
	return Index >= 0 ? ModelRef.File->Data.Clips[static_cast<size_t>(Index)].Duration : 0.f;
}

bool USUSkinnedModelComponent::IsClipFinished() const
{
	if (!ModelRef.IsValid() || CurrentClip < 0)
	{
		return true;
	}
	const SUModel::Clip& Clip = ModelRef.File->Data.Clips[static_cast<size_t>(CurrentClip)];
	return !Clip.bLoop && CurrentTime >= Clip.Duration;
}

void USUSkinnedModelComponent::PlayClip(FName ClipName, float BlendTime, bool bRestart)
{
	if (!ModelRef.IsValid() || (ClipName == CurrentName && !bRestart))
	{
		return;
	}
	const int32 Index = ModelRef.File->Data.FindClip(TCHAR_TO_UTF8(*ClipName.ToString()));
	if (Index < 0)
	{
		return;
	}
	PreviousClip = CurrentClip;
	PreviousTime = CurrentTime;
	CurrentClip = Index;
	CurrentName = ClipName;
	CurrentTime = 0.f;
	BlendDuration = BlendTime;
	BlendAlpha = (PreviousClip >= 0 && BlendTime > 0.f) ? 0.f : 1.f;
}

void USUSkinnedModelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!ModelRef.IsValid() || CurrentClip < 0)
	{
		return;
	}
	CurrentTime += DeltaTime * PlayRate;
	PreviousTime += DeltaTime * PlayRate;
	if (BlendAlpha < 1.f)
	{
		BlendAlpha = BlendDuration > 0.f ? FMath::Min(1.f, BlendAlpha + DeltaTime / BlendDuration) : 1.f;
	}
	// deform at 30 Hz and only while the creature is on screen (or was a moment ago)
	SkinAccumulator += DeltaTime;
	const bool bOnScreen = GetWorld() && (GetWorld()->GetTimeSeconds() - GetLastRenderTime() < 0.5f || GetWorld()->GetTimeSeconds() < 2.f);
	if (SkinAccumulator >= (bOnScreen ? 1.f / 30.f : 0.5f))
	{
		SkinAccumulator = 0.f;
		UpdateSkin();
	}
}

void USUSkinnedModelComponent::UpdateSkin()
{
	if (!ModelRef.IsValid())
	{
		return;
	}
	const SUModel::File& File = ModelRef.File->Data;
	SUModel::SampleClip(File, CurrentClip, CurrentTime, PoseA);
	if (BlendAlpha < 1.f && PreviousClip >= 0)
	{
		SUModel::SampleClip(File, PreviousClip, PreviousTime, PoseB);
		const float Smooth = BlendAlpha * BlendAlpha * (3.f - 2.f * BlendAlpha);
		SUModel::BlendPose(PoseB, PoseA, Smooth);
		SUModel::ComputeMatrices(File, PoseB, WorldMatrices, SkinMatrices);
	}
	else
	{
		SUModel::ComputeMatrices(File, PoseA, WorldMatrices, SkinMatrices);
	}
	const TArray<FLinearColor> NoColors;
	for (int32 S = 0; S < Buffers.Num(); ++S)
	{
		const SUModel::Section& Section = ModelRef.Mesh->Sections[static_cast<size_t>(S)];
		SUModel::SkinSection(Section, SkinMatrices, SkinnedPositions, SkinnedNormals, SkinnedTangents);
		FSectionBuffers& B = Buffers[S];
		const int32 Count = B.Vertices.Num();
		for (int32 V = 0; V < Count; ++V)
		{
			const size_t I = static_cast<size_t>(V) * 3;
			B.Vertices[V] = FVector(SkinnedPositions[I], SkinnedPositions[I + 1], SkinnedPositions[I + 2]);
			B.Normals[V] = FVector(SkinnedNormals[I], SkinnedNormals[I + 1], SkinnedNormals[I + 2]).GetSafeNormal();
			B.Tangents[V].TangentX = FVector(SkinnedTangents[I], SkinnedTangents[I + 1], SkinnedTangents[I + 2]).GetSafeNormal();
		}
		UpdateMeshSection_LinearColor(S, B.Vertices, B.Normals, B.UVs, NoColors, B.Tangents);
	}
}
