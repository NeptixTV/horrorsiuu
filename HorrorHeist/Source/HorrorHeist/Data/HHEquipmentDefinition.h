#pragma once

#include "CoreMinimal.h"
#include "Data/HHItemDefinition.h"
#include "HHEquipmentDefinition.generated.h"

class UStaticMesh;

/** One data-driven gameplay property of a piece of equipment (battery, noise, capacity...). */
USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHItemStat
{
	GENERATED_BODY()

	/** Lookup key for gameplay code, e.g. "Battery", "Noise", "Capacity". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	FName StatId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	FText Label;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	float Value = 0.f;

	/** Used to draw the stat bar in the UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	float DisplayMax = 100.f;

	/** True when a lower value is better (noise, weight) so the UI colours it correctly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	bool bLowerIsBetter = false;
};

/**
 * Burglary equipment brought into a job. Gameplay code reads values through GetStat(), so
 * balancing happens entirely in data assets.
 */
UCLASS(BlueprintType)
class HORRORHEIST_API UHHEquipmentDefinition : public UHHItemDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	EHHEquipmentSlot Slot = EHHEquipmentSlot::Utility;

	/** World / hand mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TArray<FHHItemStat> Stats;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	float GetStat(FName StatId, float DefaultValue = 0.f) const;

	virtual FPrimaryAssetType GetItemAssetType() const override { return FPrimaryAssetType(TEXT("HHEquipment")); }
};
