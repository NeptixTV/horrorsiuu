// SIGNAL UNKNOWN – loads the model files (Content/SUModels/*.sumodel) at runtime.
//
// The project has no binary Unreal assets: models are read from the .sumodel files that
// Tools/AssetPipeline produces, and turned into procedural meshes (level geometry, creatures)
// or runtime static meshes (trees and other instanced things). Textures and materials are
// created on the fly.
//
// LIMITATION: textured materials are generated with editor functionality, so they exist when the
// game runs from the editor build (START.bat, "Spielen"). A packaged build would show the models
// with plain colours until real material assets are added.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Logic/SUModelData.h"
#include "SUModelLibrary.generated.h"

class UProceduralMeshComponent;
class UStaticMesh;
class UTexture2D;
class UMaterial;
class UMaterialInterface;

/** One loaded .sumodel file. */
struct FSUModelFile
{
	FString Name;
	TArray<uint8> Bytes;
	SUModel::File Data;
	/** Index into USUModelLibrary::Materials per material of the file (-1 = not created yet). */
	TArray<int32> MaterialSlots;
	TArray<int32> TextureSlots;
};

/** Reference to a mesh inside a loaded file. */
struct FSUModelMeshRef
{
	const FSUModelFile* File = nullptr;
	const SUModel::Mesh* Mesh = nullptr;
	int32 FileIndex = -1;

	bool IsValid() const { return File != nullptr && Mesh != nullptr; }
};

UCLASS()
class SIGNALUNKNOWN_API USUModelLibrary : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static USUModelLibrary* Get(const UObject* WorldContext);

	/** True if a mesh with this name exists in any model file. */
	bool HasMesh(FName MeshName);
	bool FindMesh(FName MeshName, FSUModelMeshRef& OutRef);

	/** Creates a registered procedural mesh component showing a static mesh of the library. */
	UProceduralMeshComponent* SpawnMesh(AActor* Owner, USceneComponent* Parent, FName MeshName, const FTransform& RelativeTransform, bool bCollision, FName SurfaceTag = NAME_None);

	/** Fills an (unregistered or registered) procedural mesh with the sections of a mesh in bind pose. */
	void FillMesh(UProceduralMeshComponent* Component, const FSUModelMeshRef& Ref, bool bCollision);

	/** Runtime static mesh for instancing (trees). nullptr if the mesh is missing or cannot be built. */
	UStaticMesh* GetStaticMesh(FName MeshName);

	UMaterialInterface* GetMaterial(int32 FileIndex, int32 MaterialIndex);

	int32 GetNumFiles();

private:
	void EnsureLoaded();
	void LoadFile(const FString& Path);
	UTexture2D* GetTexture(int32 FileIndex, int32 TextureIndex);
	UTexture2D* CreateTexture(const SUModel::Texture& Source);
	UMaterial* GetBaseMaterial(bool bMasked, bool bTwoSided, float Cutoff);
	/** Plain coloured material owned by the library (never by a world, which would keep that world alive). */
	UMaterialInterface* MakeColorMaterial(const FLinearColor& Color);

	TArray<TSharedPtr<FSUModelFile>> Files;
	TMap<FName, FSUModelMeshRef> MeshIndex;
	bool bLoaded = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> Textures;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterial>> BaseMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UStaticMesh>> StaticMeshes;
};
