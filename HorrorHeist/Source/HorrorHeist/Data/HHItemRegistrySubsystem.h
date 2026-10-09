#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/HHTypes.h"
#include "HHItemRegistrySubsystem.generated.h"

class UHHItemDefinition;
class UHHCosmeticDefinition;
class UHHEquipmentDefinition;
class UHHMissionDefinition;
class UTexture2D;

/**
 * Catalog of every cosmetic, equipment item and mission. Discovers data assets through the
 * Asset Manager (falls back to the Asset Registry) so content can be added without code.
 * Small catalogs are loaded up-front; the design keeps lookups by id so it scales to hundreds
 * of items and can move to async bundles later without touching callers.
 */
UCLASS()
class HORRORHEIST_API UHHItemRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UHHItemRegistrySubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Re-scan content (used after the editor setup script created new assets). */
	void Reload();

	const UHHItemDefinition* FindItem(FName ItemId) const;
	const UHHCosmeticDefinition* FindCosmetic(FName ItemId) const;
	const UHHEquipmentDefinition* FindEquipment(FName ItemId) const;
	const UHHMissionDefinition* FindMission(FName MissionId) const;

	TArray<const UHHCosmeticDefinition*> GetCosmeticsForSlot(EHHCosmeticSlot Slot) const;
	TArray<const UHHEquipmentDefinition*> GetEquipmentForSlot(EHHEquipmentSlot Slot) const;
	TArray<const UHHMissionDefinition*> GetMissions() const;

	/** Everything that can appear in the store, sorted by slot then price. */
	TArray<const UHHItemDefinition*> GetStoreItems() const;

	/** Loads (and keeps alive) an icon for UI use. */
	UTexture2D* GetIcon(const UHHItemDefinition* Item) const;
	UTexture2D* GetMissionPhoto(const UHHMissionDefinition* Mission) const;

	int32 NumItems() const { return ItemsById.Num(); }

private:
	template <typename T>
	void GatherAssets(const FName PrimaryAssetType, TArray<TObjectPtr<T>>& Out);

	UTexture2D* PinTexture(const TSoftObjectPtr<UTexture2D>& Texture) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHHCosmeticDefinition>> Cosmetics;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHHEquipmentDefinition>> Equipment;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHHMissionDefinition>> Missions;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UHHItemDefinition>> ItemsById;

	/** Icons and photos handed to Slate brushes must stay referenced. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> PinnedAssets;
};
