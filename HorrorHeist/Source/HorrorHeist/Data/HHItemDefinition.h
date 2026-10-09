#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/HHTypes.h"
#include "HHItemDefinition.generated.h"

class UTexture2D;

/**
 * Base for everything a player can own: cosmetics and equipment.
 * Items are Primary Data Assets discovered by the Asset Manager (see DefaultGame.ini),
 * so adding content never requires code changes: create a new data asset in the right folder.
 */
UCLASS(Abstract, BlueprintType)
class HORRORHEIST_API UHHItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id used in save games and replication. Must be unique across all items. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	/** Short in-world flavour line, shown in a typewriter font. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText FlavorText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EHHItemRarity Rarity = EHHItemRarity::Common;

	/** Theme / set name, e.g. "Old-School Burglar". Purely presentational. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText Theme;

	/** Store price. 0 together with bOwnedByDefault = starter item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = 0))
	int32 Price = 0;

	/** Player level required before the item can be bought. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = 1))
	int32 UnlockLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
	bool bOwnedByDefault = false;

	/** Hidden items can be owned (rewards, events) but never appear in the store. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
	bool bHiddenInStore = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 SortOrder = 0;

	/** Primary asset type, e.g. "HHCosmetic". */
	virtual FPrimaryAssetType GetItemAssetType() const { return FPrimaryAssetType(); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Id falls back to the asset name so a forgotten ItemId never breaks saves. */
	FName GetItemId() const { return ItemId.IsNone() ? GetFName() : ItemId; }
};
