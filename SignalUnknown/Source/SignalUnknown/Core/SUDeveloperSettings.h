// SIGNAL UNKNOWN – project settings (Project Settings -> Game -> Signal Unknown ...).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SUDeveloperSettings.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UDataTable;

/** Gameplay tuning values. Stored in Config/DefaultGame.ini. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Signal Unknown - Gameplay"))
class SIGNALUNKNOWN_API USUGameplaySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Seed of the procedural world. All co-op players must use the same value. */
	UPROPERTY(Config, EditAnywhere, Category = "World")
	int32 WorldSeed = 7071;

	/** Time of day when a new game starts (17.6 = 17:36). */
	UPROPERTY(Config, EditAnywhere, Category = "World", meta = (ClampMin = "0", ClampMax = "24"))
	float StartHour = 17.6f;

	/** Game minutes that pass per real minute. */
	UPROPERTY(Config, EditAnywhere, Category = "World", meta = (ClampMin = "0.1", ClampMax = "120"))
	float GameMinutesPerRealMinute = 6.f;

	UPROPERTY(Config, EditAnywhere, Category = "Economy")
	int32 StartMoneyCents = 15000;

	UPROPERTY(Config, EditAnywhere, Category = "Economy")
	int32 FuelPriceCentsPerLiter = 179;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle")
	float StartFuelLiters = 24.f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle")
	float FuelTankLiters = 70.f;

	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "1", ClampMax = "500"))
	int32 CameraCapacity = 36;

	/** Saved photos are scaled down to this width. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "320", ClampMax = "3840"))
	int32 PhotoMaxWidth = 1600;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	bool bHorrorDirectorEnabled = true;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	float DirectorMinIntervalSeconds = 18.f;

	UPROPERTY(Config, EditAnywhere, Category = "Horror")
	float DirectorMaxIntervalSeconds = 38.f;

	UPROPERTY(Config, EditAnywhere, Category = "Missions")
	float NextMissionDelaySeconds = 45.f;

	/** Optional: missions from a DataTable (row type FSUMissionDef). Empty = built-in missions. */
	UPROPERTY(Config, EditAnywhere, Category = "Missions")
	TSoftObjectPtr<UDataTable> MissionTable;

	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }
};

/**
 * Art overrides. Everything in the prototype is built from engine primitives; as soon as real
 * assets (e.g. Fab/Megascans) are imported, assign them here – no code changes needed.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Signal Unknown - Art"))
class SIGNALUNKNOWN_API USUArtSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Replacement conifer meshes (pivot at the trunk base, ~1 m = 100 units tall is scaled up). */
	UPROPERTY(Config, EditAnywhere, Category = "Vegetation")
	TArray<TSoftObjectPtr<UStaticMesh>> TreeMeshes;

	/** Native height of the tree meshes above (cm) so they can be scaled to the target height. */
	UPROPERTY(Config, EditAnywhere, Category = "Vegetation")
	float TreeMeshHeight = 1500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Vegetation")
	TArray<TSoftObjectPtr<UStaticMesh>> RockMeshes;

	UPROPERTY(Config, EditAnywhere, Category = "Vegetation")
	TArray<TSoftObjectPtr<UStaticMesh>> BushMeshes;

	UPROPERTY(Config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> TerrainGrassMaterial;

	UPROPERTY(Config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> TerrainForestFloorMaterial;

	UPROPERTY(Config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> TerrainRockMaterial;

	UPROPERTY(Config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> AsphaltMaterial;

	UPROPERTY(Config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> GravelMaterial;

	UPROPERTY(Config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> WaterMaterial;

	UPROPERTY(Config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> WoodMaterial;

	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }
};
