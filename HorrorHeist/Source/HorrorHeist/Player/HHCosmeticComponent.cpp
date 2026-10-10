#include "Player/HHCosmeticComponent.h"
#include "Data/HHCosmeticDefinition.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "HorrorHeist.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ReferenceSkeleton.h"

namespace
{
	const FName TintParam(TEXT("Tint"));
	const FName Tint2Param(TEXT("Tint2"));
	const FName SkinTintParam(TEXT("SkinTint"));
}

UHHCosmeticComponent::UHHCosmeticComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Parts.SetNum(static_cast<int32>(EHHCosmeticSlot::MAX));
}

void UHHCosmeticComponent::SetBodyMesh(USkeletalMeshComponent* InBody)
{
	if (Body.Get() == InBody)
	{
		return;
	}
	for (EHHCosmeticSlot Slot : TEnumRange<EHHCosmeticSlot>())
	{
		ClearSlot(Slot);
	}
	Body = InBody;
	if (bHasApplied)
	{
		const FHHCosmeticLoadout Previous = Applied;
		Applied = FHHCosmeticLoadout();
		bHasApplied = false;
		ApplyLoadout(Previous);
	}
}

void UHHCosmeticComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (EHHCosmeticSlot Slot : TEnumRange<EHHCosmeticSlot>())
	{
		ClearSlot(Slot);
	}
	Super::EndPlay(EndPlayReason);
}

void UHHCosmeticComponent::ClearSlot(EHHCosmeticSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (Parts.Num() < static_cast<int32>(EHHCosmeticSlot::MAX))
	{
		Parts.SetNum(static_cast<int32>(EHHCosmeticSlot::MAX));
	}
	if (UPrimitiveComponent* Part = Parts[Index])
	{
		Part->DestroyComponent();
	}
	Parts[Index] = nullptr;
}

void UHHCosmeticComponent::ApplyLoadout(const FHHCosmeticLoadout& Loadout)
{
	USkeletalMeshComponent* BodyMesh = Body.Get();
	if (!BodyMesh)
	{
		// Remember it; SetBodyMesh() re-applies.
		Applied = Loadout;
		bHasApplied = true;
		return;
	}

	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	if (!Registry)
	{
		return;
	}

	bool bHideHair = false;
	for (EHHCosmeticSlot Slot : TEnumRange<EHHCosmeticSlot>())
	{
		if (const UHHCosmeticDefinition* Item = Registry->FindCosmetic(Loadout.Get(Slot)))
		{
			bHideHair |= Item->bHidesHair;
		}
	}

	for (EHHCosmeticSlot Slot : TEnumRange<EHHCosmeticSlot>())
	{
		const int32 Index = static_cast<int32>(Slot);
		const FName ItemId = Loadout.Get(Slot);
		const bool bUnchanged = bHasApplied && Applied.Get(Slot) == ItemId;
		if (bUnchanged && (Parts[Index] || ItemId.IsNone()) && Slot != EHHCosmeticSlot::Body)
		{
			continue;
		}

		ClearSlot(Slot);
		const UHHCosmeticDefinition* Item = Registry->FindCosmetic(ItemId);
		if (!Item || Item->Slot != Slot)
		{
			continue;
		}

		if (Item->bIsSkinTone)
		{
			ApplySkinTone(Item);
			continue;
		}

		if (UPrimitiveComponent* Part = CreatePart(Item))
		{
			ApplyLook(Part, Item);
			Parts[Index] = Part;
		}
	}

	if (UPrimitiveComponent* Hair = Parts[static_cast<int32>(EHHCosmeticSlot::Hair)])
	{
		Hair->SetVisibility(!bHideHair);
	}

	Applied = Loadout;
	bHasApplied = true;
	SetHiddenForOwner(bHiddenForOwner);
}

UPrimitiveComponent* UHHCosmeticComponent::CreatePart(const UHHCosmeticDefinition* Item)
{
	USkeletalMeshComponent* BodyMesh = Body.Get();
	AActor* Owner = GetOwner();
	if (!BodyMesh || !Owner)
	{
		return nullptr;
	}

	if (USkeletalMesh* SkinnedMesh = Item->SkeletalMesh.LoadSynchronous())
	{
		USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(Owner, MakeUniqueObjectName(Owner, USkeletalMeshComponent::StaticClass(), TEXT("Cosmetic")));
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->bCastHiddenShadow = true;
		Part->SetupAttachment(BodyMesh);
		Part->RegisterComponent();
		Part->SetSkeletalMeshAsset(SkinnedMesh);
		Part->SetLeaderPoseComponent(BodyMesh);
		return Part;
	}

	if (UStaticMesh* RigidMesh = Item->StaticMesh.LoadSynchronous())
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner, MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), TEXT("Cosmetic")));
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->bCastHiddenShadow = true;
		Part->SetStaticMesh(RigidMesh);
		Part->SetMobility(EComponentMobility::Movable);
		Part->RegisterComponent();

		// The mesh is authored in character space: cancel the bone's reference pose so it
		// sits exactly where the artist modelled it, then follows the bone when animated.
		const FTransform BoneRefPose = GetBoneRefPoseComponentTransform(Item->AttachBone);
		Part->AttachToComponent(BodyMesh, FAttachmentTransformRules::KeepRelativeTransform, Item->AttachBone);
		Part->SetRelativeTransform(Item->AttachOffset * BoneRefPose.Inverse());
		return Part;
	}

	UE_LOG(LogHorrorHeist, Warning, TEXT("Cosmetic '%s' has no mesh."), *Item->GetItemId().ToString());
	return nullptr;
}

FTransform UHHCosmeticComponent::GetBoneRefPoseComponentTransform(FName Bone) const
{
	const USkeletalMeshComponent* BodyMesh = Body.Get();
	const USkeletalMesh* Mesh = BodyMesh ? BodyMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Mesh)
	{
		return FTransform::Identity;
	}

	const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
	int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
	if (BoneIndex == INDEX_NONE)
	{
		return FTransform::Identity;
	}

	const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
	FTransform ComponentSpace = FTransform::Identity;
	while (BoneIndex != INDEX_NONE)
	{
		ComponentSpace = ComponentSpace * RefPose[BoneIndex];
		BoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
	}
	return ComponentSpace;
}

void UHHCosmeticComponent::ApplyLook(UPrimitiveComponent* Part, const UHHCosmeticDefinition* Item)
{
	UMeshComponent* Mesh = Cast<UMeshComponent>(Part);
	if (!Mesh)
	{
		return;
	}

	for (int32 Index = 0; Index < Item->MaterialOverrides.Num(); ++Index)
	{
		if (UMaterialInterface* Material = Item->MaterialOverrides[Index].LoadSynchronous())
		{
			Mesh->SetMaterial(Index, Material);
		}
	}

	const bool bTint = Item->PrimaryTint.A > 0.f;
	const bool bTint2 = Item->SecondaryTint.A > 0.f;
	if (!bTint && !bTint2)
	{
		return;
	}
	for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
	{
		if (UMaterialInstanceDynamic* Dynamic = Mesh->CreateDynamicMaterialInstance(Index))
		{
			if (bTint)
			{
				Dynamic->SetVectorParameterValue(TintParam, Item->PrimaryTint);
			}
			if (bTint2)
			{
				Dynamic->SetVectorParameterValue(Tint2Param, Item->SecondaryTint);
			}
		}
	}
}

void UHHCosmeticComponent::ApplySkinTone(const UHHCosmeticDefinition* Item)
{
	USkeletalMeshComponent* BodyMesh = Body.Get();
	if (!BodyMesh)
	{
		return;
	}
	for (int32 Index = 0; Index < BodyMesh->GetNumMaterials(); ++Index)
	{
		if (UMaterialInstanceDynamic* Dynamic = BodyMesh->CreateDynamicMaterialInstance(Index))
		{
			Dynamic->SetVectorParameterValue(SkinTintParam, Item->PrimaryTint);
		}
	}
}

void UHHCosmeticComponent::SetHiddenForOwner(bool bHidden)
{
	bHiddenForOwner = bHidden;
	for (UPrimitiveComponent* Part : Parts)
	{
		if (Part)
		{
			Part->SetOwnerNoSee(bHidden);
		}
	}
}
