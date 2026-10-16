#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/HHTypes.h"
#include "HHProfileSaveGame.generated.h"

/**
 * Local player profile: identity, economy, unlocks and selections.
 * Versioned so later milestones can migrate old saves.
 */
UCLASS()
class HORRORHEIST_API UHHProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY(SaveGame)
	int32 Version = CurrentVersion;

	UPROPERTY(SaveGame)
	FString PlayerName;

	UPROPERTY(SaveGame)
	int64 Cash = 0;

	UPROPERTY(SaveGame)
	int64 TotalXP = 0;

	UPROPERTY(SaveGame)
	TArray<FName> OwnedItems;

	UPROPERTY(SaveGame)
	FHHCosmeticLoadout Cosmetics;

	UPROPERTY(SaveGame)
	FHHEquipmentLoadout Equipment;

	UPROPERTY(SaveGame)
	FName LastMission;

	UPROPERTY(SaveGame)
	int32 JobsCompleted = 0;

	UPROPERTY(SaveGame)
	int64 LifetimeEarnings = 0;

	/** Hollowmere Savings & Loan: money in the bank. Safe from bail, earns interest every night. */
	UPROPERTY(SaveGame)
	int64 Savings = 0;

	/** The town's calendar: one day per job night (interest, the morning papers). */
	UPROPERTY(SaveGame)
	int32 Day = 1;

	/** Job intel learned by scouting (FHHJobIntel::IntelId). */
	UPROPERTY(SaveGame)
	TArray<FName> KnownIntel;

	UPROPERTY(SaveGame)
	FDateTime CreatedAt;

	UPROPERTY(SaveGame)
	bool bInitialized = false;
};
