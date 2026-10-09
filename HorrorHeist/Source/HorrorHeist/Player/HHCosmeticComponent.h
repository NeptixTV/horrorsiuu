#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HHTypes.h"
#include "HHCosmeticComponent.generated.h"

class USkeletalMeshComponent;
class UPrimitiveComponent;
class UHHCosmeticDefinition;

/**
 * Dresses a body skeletal mesh from a cosmetic loadout. Works for the networked character
 * and the local preview mannequin alike. Skinned parts follow the body via leader pose,
 * rigid parts are attached to bones using the reference pose so artists can model them
 * directly on the character.
 */
UCLASS(ClassGroup = (HorrorHeist), meta = (BlueprintSpawnableComponent))
class HORRORHEIST_API UHHCosmeticComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHHCosmeticComponent();

	void SetBodyMesh(USkeletalMeshComponent* InBody);
	USkeletalMeshComponent* GetBodyMesh() const { return Body.Get(); }

	void ApplyLoadout(const FHHCosmeticLoadout& Loadout);
	const FHHCosmeticLoadout& GetAppliedLoadout() const { return Applied; }

	/** First person: the owner sees only the shadow of their own outfit. */
	void SetHiddenForOwner(bool bHidden);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ClearSlot(EHHCosmeticSlot Slot);
	UPrimitiveComponent* CreatePart(const UHHCosmeticDefinition* Item);
	void ApplyLook(UPrimitiveComponent* Part, const UHHCosmeticDefinition* Item);
	void ApplySkinTone(const UHHCosmeticDefinition* Item);
	FTransform GetBoneRefPoseComponentTransform(FName Bone) const;

	TWeakObjectPtr<USkeletalMeshComponent> Body;

	/** One spawned component per slot (index = EHHCosmeticSlot). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Parts;

	FHHCosmeticLoadout Applied;
	bool bHiddenForOwner = false;
	bool bHasApplied = false;
};
