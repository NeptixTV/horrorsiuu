// SIGNAL UNKNOWN – builds the first playable region at runtime.
//
// Runs on every machine (server and clients) from ASUGameState::BeginPlay. Static geometry is
// generated locally from the deterministic SULogic::WorldLayout; interactive actors (doors,
// radio, vehicle, items...) are spawned by the sites on the server only and replicate.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Logic/SUWorldLayout.h"
#include "SUWorldBuilder.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UBoxComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class ASUSkySystem;
class ASUSiteBase;
class ASUVehicle;

UCLASS()
class SIGNALUNKNOWN_API ASUWorldBuilder : public AActor
{
	GENERATED_BODY()

public:
	ASUWorldBuilder();

	static ASUWorldBuilder* Get(const UObject* WorldContext);

	/** Generates everything. Safe to call once; later calls do nothing. */
	void BuildWorld(int32 Seed);
	bool IsBuilt() const { return bBuilt; }

	const SULogic::WorldLayout* GetLayout() const { return Layout.Get(); }

	// ---------------------------------------------------------------- queries (Unreal units, cm)
	float GetGroundHeight(float X, float Y) const;
	/** Ground position (with Z) for a 2D location. */
	FVector GroundPoint(float X, float Y, float ZOffset = 0.f) const;
	float DistanceToRoad(const FVector& Location) const;
	/** Point on the nearest road about AheadDistance in front of Location along Direction. */
	bool FindRoadPointAhead(const FVector& Location, const FVector& Direction, float AheadDistance, FVector& OutPoint, FVector& OutRoadDir) const;
	bool IsInsidePlayableArea(const FVector& Location) const;

	/** Named points registered by the sites ("Tower", "TowerParking", "Forsthaus", ...). */
	void RegisterPoint(FName Name, const FTransform& Transform);
	bool GetPoint(FName Name, FTransform& OutTransform) const;
	FVector GetPointLocation(FName Name) const;

	FTransform GetPlayerSpawnTransform(int32 Index) const;
	FTransform GetMenuCameraTransform() const;

	ASUSkySystem* GetSkySystem() const { return SkySystem; }

	/** The team vehicle spawned by the tower site (server only). */
	ASUVehicle* GetMainVehicle() const { return MainVehicle; }
	void SetMainVehicle(ASUVehicle* Vehicle) { MainVehicle = Vehicle; }

	// ---------------------------------------------------------------- map
	UTextureRenderTarget2D* GetMapTexture() const { return MapTexture; }
	/** 0..1 map coordinates (u = east, v = south). */
	FVector2D WorldToMapUV(const FVector& Location) const;
	FVector MapUVToWorld(const FVector2D& UV) const;
	/** Road polylines in map UV space (for drawing). */
	const TArray<TArray<FVector2D>>& GetMapRoads() const { return MapRoads; }
	float GetMapHalfSize() const { return MapHalfSize; }

private:
	void BuildTerrain();
	void BuildTerrainChunk(int32 ChunkX, int32 ChunkY, float Origin, float Spacing, int32 Cells, bool bCore);
	void BuildWater();
	void BuildRoads();
	void BuildForest();
	void BuildBoundaries();
	void BuildSites();
	void SetupMapCapture();
	void CaptureMap();

	template <typename T>
	T* SpawnSite(FName PadId, float ExtraYaw = 0.f);

	TUniquePtr<SULogic::WorldLayout> Layout;

	UPROPERTY(VisibleAnywhere, Category = "Signal Unknown")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> TerrainChunks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> RoadMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Foliage;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WaterPlanes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> Boundaries;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASUSiteBase>> Sites;

	UPROPERTY(Transient)
	TObjectPtr<ASUSkySystem> SkySystem;

	UPROPERTY(Transient)
	TObjectPtr<ASUVehicle> MainVehicle;

	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> MapCapture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> MapTexture;

	TMap<FName, FTransform> Points;
	TArray<TArray<FVector2D>> MapRoads;
	float MapHalfSize = 220000.f;
	bool bBuilt = false;

	UHierarchicalInstancedStaticMeshComponent* MakeFoliageComponent(const TCHAR* Name, class UStaticMesh* Mesh, class UMaterialInterface* Material, bool bCollision, int32 CullDistance);
};
