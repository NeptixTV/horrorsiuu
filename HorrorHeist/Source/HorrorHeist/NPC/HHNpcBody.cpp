#include "NPC/HHNpcBody.h"
#include "Core/HHGameData.h"
#include "Data/HHCharacterDefinition.h"
#include "Data/HHCosmeticDefinition.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Player/HHCosmeticComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

namespace HHNpcBody
{
	const UHHCharacterDefinition* Setup(USkeletalMeshComponent* Body, UHHCosmeticComponent* Cosmetics, const TArray<FName>& CosmeticIds)
	{
		const UHHGameData* GameData = Body ? UHHGameData::Get(Body) : nullptr;
		const UHHCharacterDefinition* Definition = GameData ? GameData->DefaultCharacter.LoadSynchronous() : nullptr;
		USkeletalMesh* Mesh = Definition ? Definition->BodyMesh.LoadSynchronous() : nullptr;
		if (!Mesh)
		{
			return nullptr;
		}
		Body->SetSkeletalMeshAsset(Mesh);
		Body->SetRelativeRotation(FRotator(0.f, Definition->MeshYawOffset, 0.f));

		if (Cosmetics)
		{
			FHHCosmeticLoadout Loadout;
			if (const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(Body))
			{
				for (const FName Id : CosmeticIds)
				{
					if (const UHHCosmeticDefinition* Item = Registry->FindCosmetic(Id))
					{
						Loadout.Set(Item->Slot, Id);
					}
				}
			}
			Cosmetics->SetBodyMesh(Body);
			Cosmetics->ApplyLoadout(Loadout);
		}
		return Definition;
	}

	void PlayLoop(USkeletalMeshComponent* Body, UAnimSequence* Anim)
	{
		if (!Body || !Anim)
		{
			return;
		}
		if (Body->GetAnimationMode() == EAnimationMode::AnimationSingleNode && Body->GetSingleNodeInstance()
			&& Body->GetSingleNodeInstance()->GetCurrentAsset() == Anim)
		{
			return;
		}
		Body->PlayAnimation(Anim, true);
	}
}
