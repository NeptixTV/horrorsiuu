#include "World/SUAssetLibrary.h"
#include "SignalUnknown.h"
#include "Core/SUDeveloperSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const TCHAR* MeshPaths[] = {
		TEXT("/Engine/BasicShapes/Cube.Cube"),
		TEXT("/Engine/BasicShapes/Sphere.Sphere"),
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		TEXT("/Engine/BasicShapes/Cone.Cone"),
		TEXT("/Engine/BasicShapes/Plane.Plane"),
	};

	const TCHAR* BaseMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	template <typename T>
	T* LoadOverride(const TSoftObjectPtr<T>& Ptr)
	{
		return Ptr.IsNull() ? nullptr : Ptr.LoadSynchronous();
	}
}

void USUAssetLibrary::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

USUAssetLibrary* USUAssetLibrary::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	USUAssetLibrary* Lib = World ? World->GetSubsystem<USUAssetLibrary>() : nullptr;
	if (Lib)
	{
		Lib->EnsureLoaded();
	}
	return Lib;
}

void USUAssetLibrary::EnsureLoaded()
{
	if (bLoaded)
	{
		return;
	}
	bLoaded = true;

	for (const TCHAR* Path : MeshPaths)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
		if (!Mesh)
		{
			UE_LOG(LogSignalUnknown, Error, TEXT("Missing engine mesh %s"), Path);
		}
		Meshes.Add(Mesh);
	}
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, BaseMaterialPath);
	if (!BaseMaterial)
	{
		UE_LOG(LogSignalUnknown, Error, TEXT("Missing engine material %s"), BaseMaterialPath);
	}

	Materials.SetNum(static_cast<int32>(ESUMat::Count));

	// art overrides
	const USUArtSettings* Art = GetDefault<USUArtSettings>();
	Materials[static_cast<int32>(ESUMat::Grass)] = LoadOverride(Art->TerrainGrassMaterial);
	Materials[static_cast<int32>(ESUMat::ForestFloor)] = LoadOverride(Art->TerrainForestFloorMaterial);
	Materials[static_cast<int32>(ESUMat::Rock)] = LoadOverride(Art->TerrainRockMaterial);
	Materials[static_cast<int32>(ESUMat::Asphalt)] = LoadOverride(Art->AsphaltMaterial);
	Materials[static_cast<int32>(ESUMat::Gravel)] = LoadOverride(Art->GravelMaterial);
	Materials[static_cast<int32>(ESUMat::Water)] = LoadOverride(Art->WaterMaterial);
	Materials[static_cast<int32>(ESUMat::WoodLight)] = LoadOverride(Art->WoodMaterial);
	for (const TSoftObjectPtr<UStaticMesh>& Ptr : Art->TreeMeshes)
	{
		if (UStaticMesh* M = LoadOverride(Ptr)) { TreeOverrides.Add(M); }
	}
	for (const TSoftObjectPtr<UStaticMesh>& Ptr : Art->RockMeshes)
	{
		if (UStaticMesh* M = LoadOverride(Ptr)) { RockOverrides.Add(M); }
	}
	for (const TSoftObjectPtr<UStaticMesh>& Ptr : Art->BushMeshes)
	{
		if (UStaticMesh* M = LoadOverride(Ptr)) { BushOverrides.Add(M); }
	}
}

UStaticMesh* USUAssetLibrary::GetMesh(ESUShape Shape) const
{
	const int32 Index = static_cast<int32>(Shape);
	return Meshes.IsValidIndex(Index) ? Meshes[Index].Get() : nullptr;
}

UMaterialInterface* USUAssetLibrary::GetMaterial(ESUMat Mat)
{
	EnsureLoaded();
	const int32 Index = static_cast<int32>(Mat);
	if (!Materials.IsValidIndex(Index))
	{
		return BaseMaterial;
	}
	if (!Materials[Index])
	{
		Materials[Index] = GetColorMaterial(GetBaseColor(Mat));
	}
	return Materials[Index];
}

UMaterialInterface* USUAssetLibrary::GetColorMaterial(const FLinearColor& Color)
{
	EnsureLoaded();
	if (!BaseMaterial)
	{
		return nullptr;
	}
	const FColor Key8 = Color.ToFColor(false);
	const uint32 Key = Key8.DWColor();
	if (TObjectPtr<UMaterialInterface>* Found = ColorCache.Find(Key))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (MID)
	{
		// BasicShapeMaterial exposes a "Color" vector parameter
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
	ColorCache.Add(Key, MID);
	return MID;
}

FLinearColor USUAssetLibrary::GetBaseColor(ESUMat Mat)
{
	switch (Mat)
	{
	case ESUMat::Grass: return FLinearColor(0.065f, 0.10f, 0.035f);
	case ESUMat::ForestFloor: return FLinearColor(0.055f, 0.045f, 0.028f);
	case ESUMat::Rock: return FLinearColor(0.17f, 0.165f, 0.155f);
	case ESUMat::Snow: return FLinearColor(0.82f, 0.84f, 0.88f);
	case ESUMat::Asphalt: return FLinearColor(0.032f, 0.032f, 0.034f);
	case ESUMat::Gravel: return FLinearColor(0.2f, 0.18f, 0.155f);
	case ESUMat::Dirt: return FLinearColor(0.11f, 0.08f, 0.05f);
	case ESUMat::RoadLineWhite: return FLinearColor(0.7f, 0.7f, 0.68f);
	case ESUMat::RoadLineYellow: return FLinearColor(0.6f, 0.43f, 0.04f);
	case ESUMat::WoodLight: return FLinearColor(0.34f, 0.23f, 0.13f);
	case ESUMat::WoodDark: return FLinearColor(0.12f, 0.075f, 0.042f);
	case ESUMat::WoodPainted: return FLinearColor(0.3f, 0.1f, 0.07f);
	case ESUMat::WoodGrey: return FLinearColor(0.21f, 0.2f, 0.18f);
	case ESUMat::Concrete: return FLinearColor(0.3f, 0.3f, 0.29f);
	case ESUMat::MetalDark: return FLinearColor(0.045f, 0.045f, 0.05f);
	case ESUMat::MetalPainted: return FLinearColor(0.22f, 0.24f, 0.17f);
	case ESUMat::MetalWhite: return FLinearColor(0.58f, 0.58f, 0.56f);
	case ESUMat::Rust: return FLinearColor(0.19f, 0.075f, 0.03f);
	case ESUMat::Glass: return FLinearColor(0.02f, 0.028f, 0.032f);
	case ESUMat::Water: return FLinearColor(0.008f, 0.022f, 0.028f);
	case ESUMat::Fabric: return FLinearColor(0.15f, 0.12f, 0.09f);
	case ESUMat::FabricBlue: return FLinearColor(0.05f, 0.08f, 0.15f);
	case ESUMat::Paper: return FLinearColor(0.74f, 0.71f, 0.6f);
	case ESUMat::Blood: return FLinearColor(0.11f, 0.0f, 0.0f);
	case ESUMat::BloodDry: return FLinearColor(0.06f, 0.008f, 0.006f);
	case ESUMat::Black: return FLinearColor(0.01f, 0.01f, 0.01f);
	case ESUMat::White: return FLinearColor(0.78f, 0.78f, 0.76f);
	case ESUMat::Red: return FLinearColor(0.5f, 0.02f, 0.02f);
	case ESUMat::Yellow: return FLinearColor(0.7f, 0.5f, 0.02f);
	case ESUMat::Green: return FLinearColor(0.05f, 0.24f, 0.06f);
	case ESUMat::Foliage: return FLinearColor(0.018f, 0.052f, 0.022f);
	case ESUMat::FoliageDark: return FLinearColor(0.011f, 0.032f, 0.015f);
	case ESUMat::FoliageLight: return FLinearColor(0.045f, 0.085f, 0.028f);
	case ESUMat::Bark: return FLinearColor(0.065f, 0.045f, 0.03f);
	case ESUMat::BarkBirch: return FLinearColor(0.58f, 0.56f, 0.52f);
	case ESUMat::Lamp: return FLinearColor(1.f, 0.92f, 0.75f);
	case ESUMat::Screen: return FLinearColor(0.08f, 0.25f, 0.2f);
	case ESUMat::CarBody: return FLinearColor(0.15f, 0.13f, 0.09f);
	case ESUMat::CarDark: return FLinearColor(0.025f, 0.025f, 0.025f);
	case ESUMat::Tire: return FLinearColor(0.015f, 0.015f, 0.015f);
	case ESUMat::Chrome: return FLinearColor(0.5f, 0.5f, 0.5f);
	case ESUMat::Brick: return FLinearColor(0.24f, 0.095f, 0.055f);
	case ESUMat::Roof: return FLinearColor(0.065f, 0.065f, 0.075f);
	case ESUMat::Shingle: return FLinearColor(0.09f, 0.055f, 0.045f);
	case ESUMat::Creature: return FLinearColor(0.007f, 0.008f, 0.009f);
	case ESUMat::Skin: return FLinearColor(0.5f, 0.34f, 0.27f);
	case ESUMat::Cloth: return FLinearColor(0.12f, 0.15f, 0.2f);
	case ESUMat::GasRed: return FLinearColor(0.48f, 0.03f, 0.02f);
	case ESUMat::Mud: return FLinearColor(0.06f, 0.042f, 0.025f);
	default: return FLinearColor(0.5f, 0.5f, 0.5f);
	}
}
