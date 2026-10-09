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

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UHHDeveloperSettings* Get() { return GetDefault<UHHDeveloperSettings>(); }
};
