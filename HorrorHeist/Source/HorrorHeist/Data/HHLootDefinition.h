#pragma once

#include "CoreMinimal.h"
#include "Data/HHItemDefinition.h"
#include "HHLootDefinition.generated.h"

class UStaticMesh;
class USoundBase;

/**
 * Something worth stealing. Each placed piece rolls its value between ValueMin and ValueMax,
 * so the same house pays a little differently every night. Taking loot angers the house
 * (HauntAnger); cursed pieces anger it a lot. Primary asset type "HHLoot" (DefaultGame.ini).
 */
UCLASS(BlueprintType)
class HORRORHEIST_API UHHLootDefinition : public UHHItemDefinition
{
	GENERATED_BODY()

public:
	UHHLootDefinition() { bHiddenInStore = true; }

	/** World mesh (also shown in the hand while selected). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0))
	int32 ValueMin = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0))
	int32 ValueMax = 120;

	/** Bulky: slows the carrier and blocks sprinting while it is in the bag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	bool bHeavy = false;

	/** Anger added to the house when this is taken (0..100 scale). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0.0))
	float HauntAnger = 2.f;

	/** The house wants this back: big anger spike and an immediate reaction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	bool bCursed = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TSoftObjectPtr<USoundBase> PickupSound;

	int32 RollValue(FRandomStream& Stream) const;

	virtual FPrimaryAssetType GetItemAssetType() const override { return FPrimaryAssetType(TEXT("HHLoot")); }
};
