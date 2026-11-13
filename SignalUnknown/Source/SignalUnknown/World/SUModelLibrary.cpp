#include "World/SUModelLibrary.h"
#include "SignalUnknown.h"
#include "ProceduralMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

USUModelLibrary* USUModelLibrary::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<USUModelLibrary>() : nullptr;
}

int32 USUModelLibrary::GetNumFiles()
{
	EnsureLoaded();
	return Files.Num();
}

void USUModelLibrary::EnsureLoaded()
{
	if (bLoaded)
	{
		return;
	}
	bLoaded = true;
	BaseMaterials.SetNum(4);
	const FString Dir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("SUModels"));
	TArray<FString> Found;
	IFileManager::Get().FindFilesRecursive(Found, *Dir, TEXT("*.sumodel"), true, false);
	Found.Sort();
	for (const FString& Path : Found)
	{
		LoadFile(Path);
	}
	UE_LOG(LogSignalUnknown, Log, TEXT("Model library: %d files, %d meshes from %s"), Files.Num(), MeshIndex.Num(), *Dir);
	if (Files.Num() == 0)
	{
		UE_LOG(LogSignalUnknown, Warning, TEXT("No .sumodel files found - the game falls back to placeholder shapes."));
	}
}

void USUModelLibrary::LoadFile(const FString& Path)
{
	TSharedPtr<FSUModelFile> File = MakeShared<FSUModelFile>();
	File->Name = FPaths::GetBaseFilename(Path);
	if (!FFileHelper::LoadFileToArray(File->Bytes, *Path))
	{
		UE_LOG(LogSignalUnknown, Error, TEXT("Cannot read model file %s"), *Path);
		return;
	}
	std::string Error;
	if (!SUModel::Parse(File->Bytes.GetData(), static_cast<size_t>(File->Bytes.Num()), File->Data, Error))
	{
		UE_LOG(LogSignalUnknown, Error, TEXT("Model file %s is invalid: %s"), *Path, UTF8_TO_TCHAR(Error.c_str()));
		return;
	}
	File->MaterialSlots.Init(-1, static_cast<int32>(File->Data.Materials.size()));
	File->TextureSlots.Init(-1, static_cast<int32>(File->Data.Textures.size()));
	const int32 FileIndex = Files.Add(File);
	for (const SUModel::Mesh& Mesh : File->Data.Meshes)
	{
		FSUModelMeshRef Ref;
		Ref.File = File.Get();
		Ref.Mesh = &Mesh;
		Ref.FileIndex = FileIndex;
		MeshIndex.Add(FName(UTF8_TO_TCHAR(Mesh.Name.c_str())), Ref);
	}
}

bool USUModelLibrary::HasMesh(FName MeshName)
{
	EnsureLoaded();
	return MeshIndex.Contains(MeshName);
}

bool USUModelLibrary::FindMesh(FName MeshName, FSUModelMeshRef& OutRef)
{
	EnsureLoaded();
	if (const FSUModelMeshRef* Found = MeshIndex.Find(MeshName))
	{
		OutRef = *Found;
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------ textures / materials

UTexture2D* USUModelLibrary::CreateTexture(const SUModel::Texture& Source)
{
	if (!Source.Data || Source.Size == 0)
	{
		return nullptr;
	}
	IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(Source.Encoding == 0 ? EImageFormat::PNG : EImageFormat::JPEG);
	if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Source.Data, static_cast<int64>(Source.Size)))
	{
		return nullptr;
	}
	TArray64<uint8> Raw;
	if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
	{
		return nullptr;
	}
	const int32 Width = static_cast<int32>(Wrapper->GetWidth());
	const int32 Height = static_cast<int32>(Wrapper->GetHeight());
	if (Width <= 0 || Height <= 0 || Raw.Num() < static_cast<int64>(Width) * Height * 4)
	{
		return nullptr;
	}
	const bool bNormal = Source.Kind == 1;
#if WITH_EDITOR
	// Editor build: go through the texture source so the engine builds mip maps and compresses.
	UTexture2D* Texture = NewObject<UTexture2D>(this, NAME_None, RF_Transient);
	Texture->Source.Init(Width, Height, 1, 1, TSF_BGRA8, Raw.GetData());
	Texture->SRGB = !bNormal;
	Texture->CompressionSettings = bNormal ? TC_Normalmap : TC_Default;
	Texture->MipGenSettings = TMGS_FromTextureGroup;
	Texture->LODGroup = bNormal ? TEXTUREGROUP_WorldNormalMap : TEXTUREGROUP_World;
	Texture->UpdateResource();
	return Texture;
#else
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}
	void* Data = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Raw.GetData(), static_cast<SIZE_T>(Width) * Height * 4);
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->SRGB = !bNormal;
	Texture->UpdateResource();
	return Texture;
#endif
}

UTexture2D* USUModelLibrary::GetTexture(int32 FileIndex, int32 TextureIndex)
{
	if (!Files.IsValidIndex(FileIndex) || !Files[FileIndex]->TextureSlots.IsValidIndex(TextureIndex))
	{
		return nullptr;
	}
	FSUModelFile& File = *Files[FileIndex];
	int32& Slot = File.TextureSlots[TextureIndex];
	if (Slot < 0)
	{
		UTexture2D* Texture = CreateTexture(File.Data.Textures[static_cast<size_t>(TextureIndex)]);
		if (!Texture)
		{
			UE_LOG(LogSignalUnknown, Warning, TEXT("Texture %d of %s could not be decoded"), TextureIndex, *File.Name);
		}
		Slot = Textures.Add(Texture);
	}
	return Textures[Slot];
}

UMaterial* USUModelLibrary::GetBaseMaterial(bool bMasked, bool bTwoSided, float Cutoff)
{
#if WITH_EDITOR
	const int32 Key = (bMasked ? 1 : 0) | (bTwoSided ? 2 : 0);
	if (BaseMaterials.IsValidIndex(Key) && BaseMaterials[Key])
	{
		return BaseMaterials[Key];
	}
	UTexture* DefaultColor = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	UTexture* DefaultNormal = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
	if (!DefaultColor || !DefaultNormal)
	{
		return nullptr;
	}
	UMaterial* Material = NewObject<UMaterial>(this, NAME_None, RF_Transient);
	UMaterialEditorOnlyData* Inputs = Material->GetEditorOnlyData();
	if (!Inputs)
	{
		return nullptr;
	}
	UMaterialExpressionTextureSampleParameter2D* Base = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
	Base->ParameterName = TEXT("BaseColor");
	Base->Texture = DefaultColor;
	Base->SamplerType = SAMPLERTYPE_Color;
	Material->GetExpressionCollection().AddExpression(Base);
	Inputs->BaseColor.Connect(0, Base);

	UMaterialExpressionTextureSampleParameter2D* Normal = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
	Normal->ParameterName = TEXT("Normal");
	Normal->Texture = DefaultNormal;
	Normal->SamplerType = SAMPLERTYPE_Normal;
	Material->GetExpressionCollection().AddExpression(Normal);
	Inputs->Normal.Connect(0, Normal);

	UMaterialExpressionScalarParameter* Roughness = NewObject<UMaterialExpressionScalarParameter>(Material);
	Roughness->ParameterName = TEXT("Roughness");
	Roughness->DefaultValue = 0.9f;
	Material->GetExpressionCollection().AddExpression(Roughness);
	Inputs->Roughness.Connect(0, Roughness);

	UMaterialExpressionScalarParameter* Metallic = NewObject<UMaterialExpressionScalarParameter>(Material);
	Metallic->ParameterName = TEXT("Metallic");
	Metallic->DefaultValue = 0.f;
	Material->GetExpressionCollection().AddExpression(Metallic);
	Inputs->Metallic.Connect(0, Metallic);

	if (bMasked)
	{
		Material->BlendMode = BLEND_Masked;
		Material->OpacityMaskClipValue = Cutoff;
		Inputs->OpacityMask.Connect(4, Base);   // alpha channel of the colour texture
	}
	Material->TwoSided = bTwoSided;
	Material->PostEditChange();
	BaseMaterials[Key] = Material;
	return Material;
#else
	return nullptr;
#endif
}

UMaterialInterface* USUModelLibrary::MakeColorMaterial(const FLinearColor& Color)
{
	if (!ShapeMaterial)
	{
		ShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (!ShapeMaterial)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ShapeMaterial, this);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return MID;
}

UMaterialInterface* USUModelLibrary::GetMaterial(int32 FileIndex, int32 MaterialIndex)
{
	EnsureLoaded();
	if (!Files.IsValidIndex(FileIndex) || !Files[FileIndex]->MaterialSlots.IsValidIndex(MaterialIndex))
	{
		return nullptr;
	}
	FSUModelFile& File = *Files[FileIndex];
	int32& Slot = File.MaterialSlots[MaterialIndex];
	if (Slot >= 0)
	{
		return Materials[Slot];
	}
	const SUModel::Material& Def = File.Data.Materials[static_cast<size_t>(MaterialIndex)];
	UMaterialInterface* Result = nullptr;
	UTexture2D* BaseTexture = Def.BaseTexture >= 0 ? GetTexture(FileIndex, Def.BaseTexture) : nullptr;
	if (BaseTexture)
	{
		if (UMaterial* Parent = GetBaseMaterial(Def.bMasked, Def.bTwoSided, Def.Cutoff))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
			if (MID)
			{
				MID->SetTextureParameterValue(TEXT("BaseColor"), BaseTexture);
				if (UTexture2D* NormalTexture = Def.NormalTexture >= 0 ? GetTexture(FileIndex, Def.NormalTexture) : nullptr)
				{
					MID->SetTextureParameterValue(TEXT("Normal"), NormalTexture);
				}
				MID->SetScalarParameterValue(TEXT("Roughness"), Def.Roughness);
				MID->SetScalarParameterValue(TEXT("Metallic"), Def.Metallic);
				Result = MID;
			}
		}
	}
	if (!Result)
	{
		// no texture (or no editor functionality): plain colour
		const FLinearColor Color = Def.BaseTexture >= 0 ? FLinearColor(0.2f, 0.18f, 0.16f) : FLinearColor(Def.Color[0], Def.Color[1], Def.Color[2]);
		Result = MakeColorMaterial(Color);
	}
	Slot = Materials.Add(Result);
	return Result;
}

// ------------------------------------------------------------------------------------ meshes

void USUModelLibrary::FillMesh(UProceduralMeshComponent* Component, const FSUModelMeshRef& Ref, bool bCollision)
{
	if (!Component || !Ref.IsValid())
	{
		return;
	}
	const TArray<FLinearColor> NoColors;
	int32 SectionIndex = 0;
	for (const SUModel::Section& Section : Ref.Mesh->Sections)
	{
		const int32 Count = static_cast<int32>(Section.NumVertices);
		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Triangles;
		Vertices.SetNumUninitialized(Count);
		Normals.SetNumUninitialized(Count);
		UVs.SetNumUninitialized(Count);
		Tangents.SetNum(Count);
		for (int32 V = 0; V < Count; ++V)
		{
			const size_t I = static_cast<size_t>(V);
			Vertices[V] = FVector(Section.Positions[I * 3], Section.Positions[I * 3 + 1], Section.Positions[I * 3 + 2]);
			Normals[V] = FVector(Section.Normals[I * 3], Section.Normals[I * 3 + 1], Section.Normals[I * 3 + 2]);
			UVs[V] = FVector2D(Section.UVs[I * 2], Section.UVs[I * 2 + 1]);
			Tangents[V] = FProcMeshTangent(FVector(Section.Tangents[I * 4], Section.Tangents[I * 4 + 1], Section.Tangents[I * 4 + 2]), Section.Tangents[I * 4 + 3] < 0.f);
		}
		Triangles.SetNumUninitialized(static_cast<int32>(Section.Indices.size()));
		for (int32 T = 0; T < Triangles.Num(); ++T)
		{
			Triangles[T] = static_cast<int32>(Section.Indices[static_cast<size_t>(T)]);
		}
		Component->CreateMeshSection_LinearColor(SectionIndex, Vertices, Triangles, Normals, UVs, NoColors, Tangents, bCollision);
		Component->SetMaterial(SectionIndex, GetMaterial(Ref.FileIndex, Section.MaterialIndex));
		++SectionIndex;
	}
}

UProceduralMeshComponent* USUModelLibrary::SpawnMesh(AActor* Owner, USceneComponent* Parent, FName MeshName, const FTransform& RelativeTransform, bool bCollision, FName SurfaceTag)
{
	FSUModelMeshRef Ref;
	if (!Owner || !FindMesh(MeshName, Ref))
	{
		return nullptr;
	}
	UProceduralMeshComponent* Component = NewObject<UProceduralMeshComponent>(Owner);
	Component->SetupAttachment(Parent);
	Component->SetRelativeTransform(RelativeTransform);
	Component->bUseComplexAsSimpleCollision = true;
	Component->bUseAsyncCooking = true;
	Component->SetCastShadow(true);
	if (!SurfaceTag.IsNone())
	{
		Component->ComponentTags.Add(SurfaceTag);
	}
	FillMesh(Component, Ref, bCollision);
	if (bCollision)
	{
		Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Component->RegisterComponent();
	return Component;
}

UStaticMesh* USUModelLibrary::GetStaticMesh(FName MeshName)
{
	EnsureLoaded();
	if (TObjectPtr<UStaticMesh>* Found = StaticMeshes.Find(MeshName))
	{
		return *Found;
	}
	FSUModelMeshRef Ref;
	if (!FindMesh(MeshName, Ref))
	{
		StaticMeshes.Add(MeshName, nullptr);
		return nullptr;
	}
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(this, NAME_None, RF_Transient);
	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();

	int32 SectionIndex = 0;
	for (const SUModel::Section& Section : Ref.Mesh->Sections)
	{
		const FName SlotName(*FString::Printf(TEXT("Section%d"), SectionIndex));
		StaticMesh->GetStaticMaterials().Add(FStaticMaterial(GetMaterial(Ref.FileIndex, Section.MaterialIndex), SlotName));
		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		SlotNames[Group] = SlotName;
		TArray<FVertexInstanceID> Instances;
		Instances.Reserve(static_cast<int32>(Section.NumVertices));
		for (size_t V = 0; V < Section.NumVertices; ++V)
		{
			const FVertexID Vertex = Description.CreateVertex();
			Positions[Vertex] = FVector3f(Section.Positions[V * 3], Section.Positions[V * 3 + 1], Section.Positions[V * 3 + 2]);
			const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
			Normals[Instance] = FVector3f(Section.Normals[V * 3], Section.Normals[V * 3 + 1], Section.Normals[V * 3 + 2]);
			Tangents[Instance] = FVector3f(Section.Tangents[V * 4], Section.Tangents[V * 4 + 1], Section.Tangents[V * 4 + 2]);
			BinormalSigns[Instance] = Section.Tangents[V * 4 + 3];
			UVs.Set(Instance, 0, FVector2f(Section.UVs[V * 2], Section.UVs[V * 2 + 1]));
			Instances.Add(Instance);
		}
		TArray<FVertexInstanceID> Corner;
		Corner.SetNum(3);
		for (size_t T = 0; T + 2 < Section.Indices.size(); T += 3)
		{
			Corner[0] = Instances[static_cast<int32>(Section.Indices[T])];
			Corner[1] = Instances[static_cast<int32>(Section.Indices[T + 1])];
			Corner[2] = Instances[static_cast<int32>(Section.Indices[T + 2])];
			Description.CreateTriangle(Group, Corner);
		}
		++SectionIndex;
	}
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bBuildSimpleCollision = false;
	Params.bFastBuild = true;
	Params.bMarkPackageDirty = false;
	TArray<const FMeshDescription*> Descriptions;
	Descriptions.Add(&Description);
	if (!StaticMesh->BuildFromMeshDescriptions(Descriptions, Params))
	{
		UE_LOG(LogSignalUnknown, Warning, TEXT("Static mesh %s could not be built"), *MeshName.ToString());
		StaticMesh = nullptr;
	}
	StaticMeshes.Add(MeshName, StaticMesh);
	return StaticMesh;
}
