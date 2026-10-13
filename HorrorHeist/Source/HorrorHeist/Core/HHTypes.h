#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumRange.h"
#include "Engine/NetSerialization.h"
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
	Settings,
	/** On a job: the five carry slots in detail (Tab). */
	Inventory
};

UENUM(BlueprintType)
enum class EHHLobbyPhase : uint8
{
	Gathering,
	Countdown,
	Departing
};

/** Why the van cannot leave yet although the crew may be ready (shown on the job board). */
UENUM(BlueprintType)
enum class EHHDepartureBlock : uint8
{
	None,
	/** Nobody pinned a job to the board. */
	NoJob,
	/** The pinned job has no map / is not released (content outdated or still being cased). */
	NotPlayable,
	/** The job only starts after dark. */
	NotNight
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
/** Full-body state of a crew member that overrides locomotion. */
UENUM(BlueprintType)
enum class EHHBodyPose : uint8
{
	Normal,
	/** On the floor, crawling; a crewmate can get them back up. */
	Downed,
	/** Gone. The body stays where it fell. */
	Dead,
	/** Kneeling over a downed crewmate, getting them back up. */
	Kneeling
};

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

	/** Loot: its rolled value. Equipment: charges left (salt, film, first aid), 0 if it has none. */
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
	/** Clean getaway: the van reached the safehouse with at least the minimum haul, no police. */
	Success,
	/** Got away, but with less than the minimum haul. */
	Partial,
	/** The police arrived at the house before the van left. */
	Caught,
	/** The crew leader called the job off. */
	Abandoned,
	/** Reached the safehouse with the full haul after the police had been called. */
	HotGetaway,
	/** The police boxed the van in during the chase. */
	ChaseBusted,
	/** Nobody was left standing: the house took them (or the police took the rest). */
	CrewLost
};

/** What became of one crew member when the job ended (end-of-job roll call). */
UENUM(BlueprintType)
enum class EHHCrewFate : uint8
{
	/** In the van when it reached the safehouse. */
	Escaped,
	/** Not in the van when it left - walked home, lost what they carried. */
	LeftBehind,
	/** Picked up by the police. */
	Caught,
	/** Left the session during the job. */
	Gone,
	/** Died in the house. Whatever they carried, gear included, stays there. */
	Dead
};

/** Police activity on a job (replicated for the HUD). */
UENUM(BlueprintType)
enum class EHHPoliceState : uint8
{
	None,
	/** A patrol car drives through the street (it reacts to what it sees). */
	Patrol,
	/** Called: a car is on its way to the house. */
	Responding,
	/** At the house, searching. */
	AtHouse,
	/** Following the van. */
	Chasing,
	/** Lost the van; driving around looking for it. */
	Searching
};

/** Kinds of job intel on the board (dossier sections). */
UENUM(BlueprintType)
enum class EHHIntelKind : uint8
{
	/** Who lives there and how they behave. */
	Owner,
	/** Something worth stealing and where it is. */
	Valuable,
	/** A way in (keys, weak doors, windows). */
	Entry,
	/** Alarms, dogs, neighbours, patrols. */
	Security,
	/** Anything else worth knowing. */
	Note
};

/** When an intel spot can be scouted. */
UENUM(BlueprintType)
enum class EHHIntelTime : uint8
{
	Any,
	/** Needs daylight (looking through windows, watching the owners). */
	DayOnly,
	NightOnly
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

/** One crew member in the end-of-job roll call. */
USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHCrewRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 CrewSlot = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	EHHCrewFate Fate = EHHCrewFate::Escaped;

	/** Value this member put into the van. */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 Stashed = 0;

	/** Value still in their bag when the job ended without them in the van. */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 Lost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	bool bDrove = false;

	/** Was down at some point and got back up thanks to a crewmate. */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	bool bRevived = false;
};

/** A spot a crew member marked with the walkie (shown to the whole crew for a few seconds). */
USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHPing
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ping")
	FVector_NetQuantize Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ping")
	int32 CrewSlot = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ping")
	FString Name;

	/** Game-state clock (server world time) when it was placed. */
	UPROPERTY(BlueprintReadOnly, Category = "Ping")
	float Time = 0.f;
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

	/** Bonus for the client's primary target (already part of the payout). */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 TargetBonus = 0;

	/** What the fence keeps for hot goods / lying low (already taken off the payout). */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 FenceCut = 0;

	/** Flash photos of the thing in the house, and what a magazine pays for them (part of the payout). */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 Photos = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 PhotoBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 PayoutPerPlayer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 XPPerPlayer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	float DurationSeconds = 0.f;

	/** Highest haunting stage the house reached (0..4). */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 HauntStage = 0;

	/** The police were called during the job. */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	bool bPoliceCalled = false;

	/** Where the van went (escape zone id) and its name for the summary. */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	FName Safehouse;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	FText SafehouseName;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	TArray<FHHLootRecord> Loot;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	TArray<FHHCrewRecord> Crew;

	/** Personal copy only: equipment this player lost (they died with it in the house). */
	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	TArray<FName> LostGear;
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
	/** One line under the ending stamp ("Clean getaway", "They boxed the van in") */
	HORRORHEIST_API FText OutcomeLine(EHHMissionOutcome Outcome);
	HORRORHEIST_API bool IsGetaway(EHHMissionOutcome Outcome);
	HORRORHEIST_API FText CrewFateName(EHHCrewFate Fate);
	HORRORHEIST_API FText IntelKindName(EHHIntelKind Kind);
	/** "22:43" from hours since midnight (0..24). */
	HORRORHEIST_API FText ClockTime(float Hours);
}
