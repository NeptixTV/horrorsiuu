#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HHDeveloperSettings.generated.h"

class UHHGameData;
class UWorld;

/** Project Settings > Game > Horror Heist */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Horror Heist"))
class HORRORHEIST_API UHHDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Data")
	TSoftObjectPtr<UHHGameData> GameData;

	/** Hideout / lobby map. Players return here after a job. */
	UPROPERTY(Config, EditAnywhere, Category = "Lobby")
	TSoftObjectPtr<UWorld> LobbyMap;

	UPROPERTY(Config, EditAnywhere, Category = "Lobby", meta = (ClampMin = 1, ClampMax = 8))
	int32 MaxCrewSize = 4;

	UPROPERTY(Config, EditAnywhere, Category = "Lobby", meta = (ClampMin = 0.0))
	float DepartureCountdownSeconds = 6.f;

	UPROPERTY(Config, EditAnywhere, Category = "Save")
	FString ProfileSlotName = TEXT("HH_Profile");

	// ---- Time of day -------------------------------------------------------------------

	/** Real minutes for one full in-game day at time speed 1x (players can scale it). */
	UPROPERTY(Config, EditAnywhere, Category = "Time of Day", meta = (ClampMin = 1.0))
	float DayLengthMinutes = 48.f;

	/** In-game hour at which a new session starts in the hub (0..24). */
	UPROPERTY(Config, EditAnywhere, Category = "Time of Day", meta = (ClampMin = 0.0, ClampMax = 24.0))
	float StartHour = 16.5f;

	/** Jobs that require darkness can leave from this hour ... */
	UPROPERTY(Config, EditAnywhere, Category = "Time of Day", meta = (ClampMin = 0.0, ClampMax = 24.0))
	float NightStartHour = 20.f;

	/** ... until this hour. */
	UPROPERTY(Config, EditAnywhere, Category = "Time of Day", meta = (ClampMin = 0.0, ClampMax = 24.0))
	float NightEndHour = 5.f;

	/** Hour the crew returns to the hideout after a job. */
	UPROPERTY(Config, EditAnywhere, Category = "Time of Day", meta = (ClampMin = 0.0, ClampMax = 24.0))
	float ReturnHour = 4.5f;

	// ---- Heists ------------------------------------------------------------------------

	/** Seconds the summary stays on screen before the crew is sent back to the hideout. */
	UPROPERTY(Config, EditAnywhere, Category = "Heist", meta = (ClampMin = 3.0))
	float SummarySeconds = 22.f;

	/** Seconds between the police being called and their arrival. */
	UPROPERTY(Config, EditAnywhere, Category = "Heist", meta = (ClampMin = 10.0))
	float PoliceResponseSeconds = 120.f;

	/** XP per $1000 of secured loot (plus a flat bonus for finishing). */
	UPROPERTY(Config, EditAnywhere, Category = "Heist", meta = (ClampMin = 0))
	int32 XPPerThousand = 60;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UHHDeveloperSettings* Get() { return GetDefault<UHHDeveloperSettings>(); }
};
