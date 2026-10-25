// SIGNAL UNKNOWN – central access to meshes and materials.
//
// PLACEHOLDER ART: the prototype uses the engine's basic shapes with coloured materials.
// Every lookup goes through this class, so real assets can be swapped in later
// (see USUArtSettings and Docs/ASSETS.md).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SUAssetLibrary.generated.h"

class UStaticMesh;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ESUShape : uint8
{
	Cube,
	Sphere,
	Cylinder,
	Cone,
	Plane
};

UENUM(BlueprintType)
enum class ESUMat : uint8
{
	Grass,
	ForestFloor,
	Rock,
	Snow,
	Asphalt,
	Gravel,
	Dirt,
	RoadLineWhite,
	RoadLineYellow,
	WoodLight,
	WoodDark,
	WoodPainted,
	WoodGrey,
	Concrete,
	MetalDark,
	MetalPainted,
	MetalWhite,
	Rust,
	Glass,
	Water,
	Fabric,
	FabricBlue,
	Paper,
	Blood,
	BloodDry,
	Black,
	White,
	Red,
	Yellow,
	Green,
	Foliage,
	FoliageDark,
	FoliageLight,
	Bark,
	BarkBirch,
	Lamp,
	Screen,
	CarBody,
	CarDark,
	Tire,
	Chrome,
	Brick,
	Roof,
	Shingle,
	Creature,
	Skin,
	Cloth,
	GasRed,
	Mud,
	Count UMETA(Hidden)
};

UCLASS()
class SIGNALUNKNOWN_API USUAssetLibrary : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static USUAssetLibrary* Get(const UObject* WorldContext);

	UStaticMesh* GetMesh(ESUShape Shape) const;
	UMaterialInterface* GetMaterial(ESUMat Mat);
	/** A plain coloured material (cached per colour). */
	UMaterialInterface* GetColorMaterial(const FLinearColor& Color);
	static FLinearColor GetBaseColor(ESUMat Mat);

	/** Optional replacement meshes from the art settings (empty if none are configured). */
	const TArray<TObjectPtr<UStaticMesh>>& GetTreeOverrides() const { return TreeOverrides; }
	const TArray<TObjectPtr<UStaticMesh>>& GetRockOverrides() const { return RockOverrides; }
	const TArray<TObjectPtr<UStaticMesh>>& GetBushOverrides() const { return BushOverrides; }

private:
	void EnsureLoaded();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(Transient)
	TMap<uint32, TObjectPtr<UMaterialInterface>> ColorCache;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> TreeOverrides;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> RockOverrides;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> BushOverrides;

	bool bLoaded = false;
};
