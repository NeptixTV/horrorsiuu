// SIGNAL UNKNOWN – versioned save game.
//
// Rules for future changes (avoid data loss):
//  * never remove or rename existing UPROPERTYs – add new ones and bump CurrentVersion,
//  * migrate old data in USUSaveSubsystem::MigrateSave,
//  * saves from a newer game version are refused instead of being overwritten.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/SUTypes.h"
#include "Mission/SUMissionTypes.h"
#include "Evidence/SUPhotoTypes.h"
#include "Player/SUInventoryComponent.h"
#include "SUSaveGame.generated.h"

USTRUCT()
struct FSUMissionSave
{
	GENERATED_BODY()

	UPROPERTY() FName ActiveMissionId;
	UPROPERTY() ESUMissionPhase Phase = ESUMissionPhase::None;
	UPROPERTY() TArray<FSUObjectiveState> Objectives;
	UPROPERTY() TArray<FName> CompletedMissions;
	UPROPERTY() TArray<FName> DocumentedEvidence;
	UPROPERTY() TArray<FName> ReportedEvidence;
	UPROPERTY() FName QueuedMissionId;
	UPROPERTY() float QueuedDelay = 0.f;
};

USTRUCT()
struct FSUPlayerSave
{
	GENERATED_BODY()

	UPROPERTY() FString PlayerName;
	UPROPERTY() TArray<FSUInventorySlot> Items;
	UPROPERTY() float Health = 100.f;
	UPROPERTY() TArray<FSUPhotoMeta> Photos;
};

USTRUCT()
struct FSUVehicleSave
{
	GENERATED_BODY()

	UPROPERTY() bool bValid = false;
	UPROPERTY() FTransform Transform;
	UPROPERTY() float FuelLiters = 0.f;
	UPROPERTY() float EngineHealth = 100.f;
	UPROPERTY() TArray<FSUInventorySlot> Trunk;
};

UCLASS()
class SIGNALUNKNOWN_API USUSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY() int32 SaveVersion = CurrentVersion;
	UPROPERTY() FString GameVersion;
	UPROPERTY() FDateTime SavedAt;

	UPROPERTY() int32 MoneyCents = 0;
	UPROPERTY() TArray<FSUTransaction> Transactions;
	UPROPERTY() int32 Day = 1;
	UPROPERTY() float Hours = 8.f;
	UPROPERTY() int32 StoryStage = 0;
	UPROPERTY() TArray<FName> WorldFlags;
	UPROPERTY() int32 UnpaidFuelIncidents = 0;

	UPROPERTY() FSUMissionSave Mission;
	UPROPERTY() TArray<FSUPlayerSave> Players;
	UPROPERTY() FSUVehicleSave Vehicle;

	/** Reserved for base upgrades (not implemented yet, kept for save compatibility). */
	UPROPERTY() TArray<FName> BaseUpgrades;
};
