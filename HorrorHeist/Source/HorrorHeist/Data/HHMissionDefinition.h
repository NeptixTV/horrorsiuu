#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/HHTypes.h"
#include "HHMissionDefinition.generated.h"

class UTexture2D;
class UWorld;

/**
 * One thing the crew knows (or can find out) about a job: who lives there, where the valuables
 * are, ways in. Intel that is not known from the start is learned by scouting the house by day
 * (AHHIntelSpot in the town) and stays in the crew's notebook (host profile).
 */
USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHJobIntel
{
	GENERATED_BODY()

	/** Unique across all jobs ("Vance_SpareKey"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intel")
	FName IntelId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intel")
	EHHIntelKind Kind = EHHIntelKind::Note;

	/** "Coin collection" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intel")
	FText Title;

	/** "Master bedroom, top of the dresser (upstairs, front)." */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intel", meta = (MultiLine = true))
	FText Detail;

	/** Rough street value for valuables (0 = not a valuable / unknown). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intel", meta = (ClampMin = 0))
	int32 ValueMin = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intel", meta = (ClampMin = 0))
	int32 ValueMax = 0;

	/** On the board from the start (the client's tip, the blueprint notes). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intel")
	bool bKnownFromStart = false;

	/** Hint shown while unknown: "Watch the house from across the lane by day." */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intel")
	FText HowToLearn;

	/** Entry intel that is a key: the crew starts the job holding it (and it is gone from its hiding place). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intel")
	FName KeyId;
};

/**
 * A job on the board: a house, its rumours and its payout.
 * Future house systems (residents, routines, supernatural rules) hang off this asset.
 */
UCLASS(BlueprintType)
class HORRORHEIST_API UHHMissionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	FName MissionId;

	/** "The Vance Residence" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	FText DisplayName;

	/** "14 Ashgrove Lane" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	FText Address;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission", meta = (MultiLine = true))
	FText Briefing;

	/** What the neighbours say. Shown handwritten on the board. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission", meta = (MultiLine = true))
	FText Rumor;

	/** The thing the client actually wants. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	FText PrimaryTarget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	EHHMissionDifficulty Difficulty = EHHMissionDifficulty::Quiet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = 0))
	int32 Residents = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = 1, ClampMax = 4))
	int32 RecommendedCrew = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
	int32 PayoutMin = 2000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
	int32 PayoutMax = 5000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = 1))
	int32 UnlockLevel = 1;

	/** Polaroid shown on the job board. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UTexture2D> Photo;

	/** The crew's annotated floor plan (dossier). Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UTexture2D> FloorPlan;

	/** Owners, valuables, ways in, security - see FHHJobIntel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Intel")
	TArray<FHHJobIntel> Intel;

	/** Level the crew travels to. Empty = job not playable yet (the board says so). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Travel")
	TSoftObjectPtr<UWorld> Map;

	/**
	 * False while the house exists (map prepared) but its job is not finished: the board shows
	 * "being cased" and the van will not leave. Lets level designers walk the map early.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Travel")
	bool bAvailable = true;

	/** The van only leaves for this job after dark (see Project Settings > Horror Heist). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Travel")
	bool bRequiresNight = true;

	/** Value the crew must stash in the van for a clean result ("Success" instead of "Partial"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = 0))
	int32 MinimumHaul = 1500;

	/** Loot id of PrimaryTarget (optional objective; stashing it adds a bonus to the payout). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	FName PrimaryTargetItem;

	/**
	 * Actor tag of this job's house in the shared town level (doors, lights, haunt points).
	 * Doors that carry a different house tag stay shut during this job.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	FName HouseTag;

	/** Scales how fast the house turns hostile (1 = normal). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Haunting", meta = (ClampMin = 0.1))
	float HauntAggression = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission")
	int32 SortOrder = 0;

	FName GetMissionId() const { return MissionId.IsNone() ? GetFName() : MissionId; }
	const FHHJobIntel* FindIntel(FName IntelId) const
	{
		return Intel.FindByPredicate([IntelId](const FHHJobIntel& Entry) { return Entry.IntelId == IntelId; });
	}
	bool IsPlayable() const { return !Map.IsNull() && bAvailable; }
	/** Map exists but the job is not released yet. */
	bool IsPrepared() const { return !Map.IsNull() && !bAvailable; }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("HHMission")), GetFName());
	}
};
