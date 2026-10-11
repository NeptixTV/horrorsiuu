#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumRange.h"
#include "HHTypes.generated.h"

/** Where a cosmetic is worn. Order defines the order of the customization tabs. */
UENUM(BlueprintType)
enum class EHHCosmeticSlot : uint8
{
	Body		UMETA(DisplayName = "Skin Tone"),
	Hair		UMETA(DisplayName = "Hair"),
	Hat			UMETA(DisplayName = "Headwear"),
	Mask		UMETA(DisplayName = "Mask"),
	Top			UMETA(DisplayName = "Top"),
	Jacket		UMETA(DisplayName = "Jacket"),
	Gloves		UMETA(DisplayName = "Gloves"),
	Pants		UMETA(DisplayName = "Pants"),
	Shoes		UMETA(DisplayName = "Shoes"),
	Backpack	UMETA(DisplayName = "Back"),
	Accessory	UMETA(DisplayName = "Accessory"),
	MAX			UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EHHCosmeticSlot, EHHCosmeticSlot::MAX);

UENUM(BlueprintType)
enum class EHHItemRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Epic,
	Legendary
};

/** Loadout slots a player brings into a job. */
UENUM(BlueprintType)
enum class EHHEquipmentSlot : uint8
{
	Light		UMETA(DisplayName = "Light"),
	Entry		UMETA(DisplayName = "Entry Tool"),
	Utility		UMETA(DisplayName = "Utility"),
	Gadget		UMETA(DisplayName = "Gadget"),
	Bag			UMETA(DisplayName = "Bag"),
	MAX			UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EHHEquipmentSlot, EHHEquipmentSlot::MAX);

UENUM(BlueprintType)
enum class EHHMissionDifficulty : uint8
{
	Quiet		UMETA(DisplayName = "Quiet"),
	Uneasy		UMETA(DisplayName = "Uneasy"),
	Restless	UMETA(DisplayName = "Restless"),
	Malevolent	UMETA(DisplayName = "Malevolent")
};

/** Screens of the lobby. Each screen can be bound to a physical station in the hideout. */
UENUM(BlueprintType)
enum class EHHLobbyScreen : uint8
{
	None			UMETA(DisplayName = "Exploring"),
	MainMenu,
	Play,
	Loadout,
	Customization,
	Store,
	Settings
};

UENUM(BlueprintType)
enum class EHHLobbyPhase : uint8
{
	Gathering,
	Countdown,
	Departing
};

UENUM(BlueprintType)
enum class EHHNotifyType : uint8
{
	Info,
	Success,
	Warning,
	Error,
	Crew
};

/** UI sound cues routed through UHHAudioSubsystem. */
UENUM(BlueprintType)
enum class EHHUISound : uint8
{
	Hover,
	Click,
	Back,
	Confirm,
	Error,
	OpenPanel,
	Equip,
	Purchase,
	Ready,
	Unready,
	CountdownTick,
	Notify,
	LevelUp,
	InventorySelect,
	ObjectiveComplete
};

/**
 * Cosmetic selection, one item id per slot (NAME_None = slot empty).
 * Replicated through AHHPlayerState and persisted in the profile save.
 */
USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHCosmeticLoadout
{
	GENERATED_BODY()

	FHHCosmeticLoadout() { Items.SetNum(static_cast<int32>(EHHCosmeticSlot::MAX)); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TArray<FName> Items;

	FName Get(EHHCosmeticSlot Slot) const
	{
		const int32 Index = static_cast<int32>(Slot);
		return Items.IsValidIndex(Index) ? Items[Index] : NAME_None;
	}

	void Set(EHHCosmeticSlot Slot, FName ItemId)
	{
		const int32 Index = static_cast<int32>(Slot);
		if (Items.Num() < static_cast<int32>(EHHCosmeticSlot::MAX))
		{
			Items.SetNum(static_cast<int32>(EHHCosmeticSlot::MAX));
		}
		if (Items.IsValidIndex(Index))
		{
			Items[Index] = ItemId;
		}
	}

	bool operator==(const FHHCosmeticLoadout& Other) const { return Items == Other.Items; }
	bool operator!=(const FHHCosmeticLoadout& Other) const { return !(*this == Other); }
};

/** Equipment selection, one item id per EHHEquipmentSlot. */
USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHEquipmentLoadout
{
	GENERATED_BODY()

	FHHEquipmentLoadout() { Items.SetNum(static_cast<int32>(EHHEquipmentSlot::MAX)); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	TArray<FName> Items;

	FName Get(EHHEquipmentSlot Slot) const
	{
		const int32 Index = static_cast<int32>(Slot);
		return Items.IsValidIndex(Index) ? Items[Index] : NAME_None;
	}

	void Set(EHHEquipmentSlot Slot, FName ItemId)
	{
		const int32 Index = static_cast<int32>(Slot);
		if (Items.Num() < static_cast<int32>(EHHEquipmentSlot::MAX))
		{
			Items.SetNum(static_cast<int32>(EHHEquipmentSlot::MAX));
		}
		if (Items.IsValidIndex(Index))
		{
			Items[Index] = ItemId;
		}
	}

	bool operator==(const FHHEquipmentLoadout& Other) const { return Items == Other.Items; }
	bool operator!=(const FHHEquipmentLoadout& Other) const { return !(*this == Other); }
};

// ---------------------------------------------------------------------------------------
// Heists

/** What an inventory slot holds. */
UENUM(BlueprintType)
enum class EHHInventoryKind : uint8
{
	Empty,
	Equipment,
	Loot
};

/**
 * One of the five carry slots. Equipment brought along occupies slots too, so a crew member
 * decides what to leave in the van to make room for loot.
 */
USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EHHInventoryKind Kind = EHHInventoryKind::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName ItemId;

	/** Rolled value of this particular piece of loot (0 for equipment). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Value = 0;

	bool IsEmpty() const { return Kind == EHHInventoryKind::Empty || ItemId.IsNone(); }
	bool IsLoot() const { return Kind == EHHInventoryKind::Loot && !ItemId.IsNone(); }

	bool operator==(const FHHInventorySlot& Other) const { return Kind == Other.Kind && ItemId == Other.ItemId && Value == Other.Value; }
	bool operator!=(const FHHInventorySlot& Other) const { return !(*this == Other); }
};

UENUM(BlueprintType)
enum class EHHHeistPhase : uint8
{
	/** Arrived by van, scouting the outside. */
	Approach,
	/** Someone got into the house. */
	Inside,
	/** The minimum haul is in the van: everyone back to the van. */
	Escape,
	/** Van left / crew caught. Summary on screen. */
	Ended
};

UENUM(BlueprintType)
enum class EHHMissionOutcome : uint8
{
	None,
	/** Drove away with at least the minimum haul. */
	Success,
	/** Drove away, but with less than the minimum haul. */
	Partial,
	/** The police arrived before the van left. */
	Caught,
	/** The crew leader called the job off. */
	Abandoned
};

USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHObjective
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	bool bOptional = false;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	bool bDone = false;
};

/** One stolen item in the end-of-job summary. */
USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHLootRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 Value = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	FString StolenBy;
};

USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHMissionSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	EHHMissionOutcome Outcome = EHHMissionOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	FName MissionId;

	/** Value of everything stashed in the van. */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 SecuredValue = 0;

	/** Loot that was still in pockets when the crew was caught / left behind. */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 LostValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 PayoutPerPlayer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 XPPerPlayer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	float DurationSeconds = 0.f;

	/** Highest haunting stage the house reached (0..4). */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 HauntStage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	TArray<FHHLootRecord> Loot;
};

/** Shared helpers for display names and colors of the enums above. */
namespace HHText
{
	HORRORHEIST_API FText CosmeticSlotName(EHHCosmeticSlot Slot);
	HORRORHEIST_API FText EquipmentSlotName(EHHEquipmentSlot Slot);
	HORRORHEIST_API FText RarityName(EHHItemRarity Rarity);
	HORRORHEIST_API FText DifficultyName(EHHMissionDifficulty Difficulty);
	HORRORHEIST_API FLinearColor RarityColor(EHHItemRarity Rarity);
	HORRORHEIST_API FLinearColor DifficultyColor(EHHMissionDifficulty Difficulty);
	/** "$12,450" */
	HORRORHEIST_API FText Money(int64 Amount);
	HORRORHEIST_API FText OutcomeName(EHHMissionOutcome Outcome);
	/** "22:43" from hours since midnight (0..24). */
	HORRORHEIST_API FText ClockTime(float Hours);
}
