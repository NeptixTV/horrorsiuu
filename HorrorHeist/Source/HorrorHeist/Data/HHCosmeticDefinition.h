#pragma once

#include "CoreMinimal.h"
#include "Data/HHItemDefinition.h"
#include "HHCosmeticDefinition.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UMaterialInterface;

/**
 * A wearable item. Two ways to build one:
 *  - SkeletalMesh: skinned to the character skeleton; follows the body through the leader pose
 *    (jackets, pants, shoes, gloves...).
 *  - StaticMesh: rigidly attached to a bone (hats, masks, backpacks, watches...).
 *    Static meshes are authored in character space (same pivot as the body), the cosmetic
 *    component computes the bone-relative offset from the reference pose automatically.
 * Colour variants share meshes and only change the tint parameters.
 */
UCLASS(BlueprintType)
class HORRORHEIST_API UHHCosmeticDefinition : public UHHItemDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
	EHHCosmeticSlot Slot = EHHCosmeticSlot::Top;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Mesh")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Mesh")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	/** Bone a static mesh follows. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Mesh")
	FName AttachBone = TEXT("head");

	/** Extra offset applied after the automatic character-space alignment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Mesh")
	FTransform AttachOffset;

	/** Optional per-slot material overrides (index = material slot of the mesh). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Look")
	TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

	/** Drives the "Tint" vector parameter of every material on the mesh. Alpha 0 = keep material colour. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Look")
	FLinearColor PrimaryTint = FLinearColor(1.f, 1.f, 1.f, 0.f);

	/** Drives the "Tint2" vector parameter (stripes, trims, laces). Alpha 0 = unused. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Look")
	FLinearColor SecondaryTint = FLinearColor(1.f, 1.f, 1.f, 0.f);

	/** Hides the equipped hair (beanies, balaclavas, bag masks...). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Look")
	bool bHidesHair = false;

	/** Skin tone items only change the body material ("SkinTint" parameter). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Look")
	bool bIsSkinTone = false;

	/** Whether the slot may be left empty (e.g. no mask). Pants or shoes usually cannot. */
	static bool IsSlotOptional(EHHCosmeticSlot InSlot);

	virtual FPrimaryAssetType GetItemAssetType() const override { return FPrimaryAssetType(TEXT("HHCosmetic")); }
};
