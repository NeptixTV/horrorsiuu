#include "World/SUWorldBuilder.h"
#include "SignalUnknown.h"
#include "Core/SUTypes.h"
#include "Core/SUGameState.h"
#include "World/SUAssetLibrary.h"
#include "World/SUSkySystem.h"
#include "World/Sites/SUSiteBase.h"
#include "World/Sites/SUSiteTower.h"
#include "World/Sites/SUSiteGasStation.h"
#include "World/Sites/SUSiteSettlement.h"
#include "World/Sites/SUSiteForsthaus.h"
#include "World/Sites/SUSiteM02.h"
#include "World/Sites/SUSiteTown.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/KismetRenderingLibrary.h"

using namespace SULogic;

namespace
{
	constexpr float M = 100.f; // meters -> centimeters

	ESUMat RoadMaterial(RoadSurface Surface)
	{
		switch (Surface)
		{
		case RoadSurface::Asphalt: return ESUMat::Asphalt;
		case RoadSurface::Gravel: return ESUMat::Gravel;
		default: return ESUMat::Dirt;
		}
	}

	FName RoadTag(RoadSurface Surface)
	{
		switch (Surface)
		{
		case RoadSurface::Asphalt: return SUSurfaceTag::Asphalt;
		case RoadSurface::Gravel: return SUSurfaceTag::Gravel;
		default: return SUSurfaceTag::Dirt;
		}
	}
}

ASUWorldBuilder::ASUWorldBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	RootComponent = SceneRoot;
}

ASUWorldBuilder* ASUWorldBuilder::Get(const UObject* WorldContext)
{
	if (const ASUGameState* GS = ASUGameState::Get(WorldContext))
	{
		if (GS->GetWorldBuilder())
		{
			return GS->GetWorldBuilder();
		}
	}
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (World)
	{
		for (TActorIterator<ASUWorldBuilder> It(World); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ASUWorldBuilder::BuildWorld(int32 Seed)
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	SetActorTransform(FTransform::Identity);

	Layout = MakeUnique<WorldLayout>(static_cast<uint32_t>(Seed));

	double T0 = FPlatformTime::Seconds();
	BuildTerrain();
	UE_LOG(LogSignalUnknown, Log, TEXT("  terrain %.2fs"), FPlatformTime::Seconds() - T0);
	T0 = FPlatformTime::Seconds();
	BuildWater();
	BuildRoads();
	UE_LOG(LogSignalUnknown, Log, TEXT("  roads %.2fs"), FPlatformTime::Seconds() - T0);
	T0 = FPlatformTime::Seconds();
	BuildForest();
	UE_LOG(LogSignalUnknown, Log, TEXT("  forest %.2fs"), FPlatformTime::Seconds() - T0);
	BuildBoundaries();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	SkySystem = GetWorld()->SpawnActor<ASUSkySystem>(ASUSkySystem::StaticClass(), FTransform::Identity, Params);

	T0 = FPlatformTime::Seconds();
	BuildSites();
	UE_LOG(LogSignalUnknown, Log, TEXT("  sites %.2fs"), FPlatformTime::Seconds() - T0);

	if (GetNetMode() != NM_DedicatedServer)
	{
		SetupMapCapture();
	}
}

// ------------------------------------------------------------------------------------ queries

float ASUWorldBuilder::GetGroundHeight(float X, float Y) const
{
	return Layout ? static_cast<float>(Layout->Height(X / M, Y / M) * M) : 0.f;
}

FVector ASUWorldBuilder::GroundPoint(float X, float Y, float ZOffset) const
{
	return FVector(X, Y, GetGroundHeight(X, Y) + ZOffset);
}

float ASUWorldBuilder::DistanceToRoad(const FVector& Location) const
{
	if (!Layout)
	{
		return 1e9f;
	}
	const RoadQuery Q = Layout->NearestRoad(Location.X / M, Location.Y / M, 60.0);
	return Q.RoadIndex >= 0 ? static_cast<float>(Q.Distance * M) : 1e9f;
}

bool ASUWorldBuilder::FindRoadPointAhead(const FVector& Location, const FVector& Direction, float AheadDistance, FVector& OutPoint, FVector& OutRoadDir) const
{
	if (!Layout)
	{
		return false;
	}
	const RoadQuery Q = Layout->NearestRoad(Location.X / M, Location.Y / M, 40.0);
	if (Q.RoadIndex < 0)
	{
		return false;
	}
	const Road& R = Layout->Roads[static_cast<size_t>(Q.RoadIndex)];
	const RoadSample& A = R.Samples[static_cast<size_t>(Q.SegmentIndex)];
	const RoadSample& B = R.Samples[static_cast<size_t>(Q.SegmentIndex + 1)];
	const double Along = A.Along + Q.SegmentT * (B.Along - A.Along);
	const FVector2D Dir2(Direction.X, Direction.Y);
	const double Sign = (Dir2.X * Q.Tangent.X + Dir2.Y * Q.Tangent.Y) >= 0.0 ? 1.0 : -1.0;
	const double Target = Along + Sign * (AheadDistance / M);
	if (Target < 0.0 || Target > R.Length)
	{
		return false;
	}
	const RoadSample S = Layout->SampleRoadAt(Q.RoadIndex, Target);
	OutPoint = FVector(S.P.X * M, S.P.Y * M, S.Z * M);
	OutRoadDir = FVector(S.Tangent.X * Sign, S.Tangent.Y * Sign, 0.0).GetSafeNormal();
	return true;
}

bool ASUWorldBuilder::IsInsidePlayableArea(const FVector& Location) const
{
	return Layout && Layout->IsInsideCore(Location.X / M, Location.Y / M, 30.0);
}

void ASUWorldBuilder::RegisterPoint(FName Name, const FTransform& Transform)
{
	Points.Add(Name, Transform);
}

bool ASUWorldBuilder::GetPoint(FName Name, FTransform& OutTransform) const
{
	if (const FTransform* Found = Points.Find(Name))
	{
		OutTransform = *Found;
		return true;
	}
	if (Layout)
	{
		if (const SitePad* Pad = Layout->FindPad(TCHAR_TO_ANSI(*Name.ToString())))
		{
			OutTransform = FTransform(FRotator(0.f, static_cast<float>(Pad->FacingYawDeg), 0.f), FVector(Pad->Center.X * M, Pad->Center.Y * M, Pad->Z * M));
			return true;
		}
	}
	return false;
}

FVector ASUWorldBuilder::GetPointLocation(FName Name) const
{
	FTransform T;
	return GetPoint(Name, T) ? T.GetLocation() : FVector::ZeroVector;
}

FTransform ASUWorldBuilder::GetPlayerSpawnTransform(int32 Index) const
{
	const FName Key(*FString::Printf(TEXT("Spawn%d"), FMath::Abs(Index) % 4));
	FTransform T;
	if (GetPoint(Key, T))
	{
		return T;
	}
	return FTransform(FRotator::ZeroRotator, GroundPoint(500.f, 500.f, 200.f));
}

FTransform ASUWorldBuilder::GetMenuCameraTransform() const
{
	FTransform T;
	if (GetPoint(TEXT("MenuCamera"), T))
	{
		return T;
	}
	return FTransform(FRotator(-6.f, 215.f, 0.f), GroundPoint(-900.f, -700.f, 1600.f));
}

// ------------------------------------------------------------------------------------ terrain

void ASUWorldBuilder::BuildTerrain()
{
	// playable core: 10 m grid
	const float CoreHalf = static_cast<float>((WorldLayout::CoreHalfSize + 100.0) * M);
	const int32 CoreChunks = 12;
	const int32 CoreCells = 35;
	const float CoreSpacing = (CoreHalf * 2.f) / (CoreChunks * CoreCells);
	for (int32 CY = 0; CY < CoreChunks; ++CY)
	{
		for (int32 CX = 0; CX < CoreChunks; ++CX)
		{
			BuildTerrainChunk(CX, CY, -CoreHalf, CoreSpacing, CoreCells, true);
		}
	}

	// surrounding mountains: 50 m grid, sunk below the core to avoid overlap
	const float OuterHalf = static_cast<float>(WorldLayout::WorldHalfSize * M);
	const int32 OuterChunks = 6;
	const int32 OuterCells = 28;
	const float OuterSpacing = (OuterHalf * 2.f) / (OuterChunks * OuterCells);
	for (int32 CY = 0; CY < OuterChunks; ++CY)
	{
		for (int32 CX = 0; CX < OuterChunks; ++CX)
		{
			BuildTerrainChunk(CX, CY, -OuterHalf, OuterSpacing, OuterCells, false);
		}
	}
}

void ASUWorldBuilder::BuildTerrainChunk(int32 ChunkX, int32 ChunkY, float Origin, float Spacing, int32 Cells, bool bCore)
{
	USUAssetLibrary* Lib = USUAssetLibrary::Get(this);
	const int32 N = Cells + 1;
	const float X0 = Origin + ChunkX * Cells * Spacing;
	const float Y0 = Origin + ChunkY * Cells * Spacing;
	const float CoreLimit = static_cast<float>((WorldLayout::CoreHalfSize + 60.0) * M);

	// heights with a one-vertex border for normals
	const int32 NB = N + 2;
	TArray<float> Heights;
	Heights.SetNumUninitialized(NB * NB);
	for (int32 J = 0; J < NB; ++J)
	{
		for (int32 I = 0; I < NB; ++I)
		{
			const float X = X0 + (I - 1) * Spacing;
			const float Y = Y0 + (J - 1) * Spacing;
			float H = static_cast<float>(Layout->Height(X / M, Y / M) * M);
			if (!bCore && FMath::Abs(X) < CoreLimit && FMath::Abs(Y) < CoreLimit)
			{
				H -= 3000.f; // hidden under the detailed core terrain
			}
			Heights[J * NB + I] = H;
		}
	}
	auto HeightAt = [&](int32 I, int32 J) { return Heights[(J + 1) * NB + (I + 1)]; };

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	Vertices.Reserve(N * N + N * 4);
	Normals.Reserve(N * N + N * 4);
	UVs.Reserve(N * N + N * 4);
	for (int32 J = 0; J < N; ++J)
	{
		for (int32 I = 0; I < N; ++I)
		{
			const float X = X0 + I * Spacing;
			const float Y = Y0 + J * Spacing;
			Vertices.Add(FVector(X, Y, HeightAt(I, J)));
			const float DX = HeightAt(I + 1, J) - HeightAt(I - 1, J);
			const float DY = HeightAt(I, J + 1) - HeightAt(I, J - 1);
			Normals.Add(FVector(-DX, -DY, 2.f * Spacing).GetSafeNormal());
			UVs.Add(FVector2D(X / 400.f, Y / 400.f));
		}
	}

	// sections: 0 grass, 1 forest floor, 2 rock, 3 snow, 4 dirt (road verges)
	TArray<int32> Tris[5];
	auto Classify = [&](const FVector& A, const FVector& B, const FVector& C) -> int32
	{
		const FVector FaceN = FVector::CrossProduct(C - A, B - A).GetSafeNormal();
		const FVector Mid = (A + B + C) / 3.f;
		const double MX = Mid.X / M;
		const double MY = Mid.Y / M;
		if (Mid.Z > 62000.f) { return 3; }
		if (FMath::Abs(FaceN.Z) < 0.72f) { return 2; }
		if (bCore)
		{
			const RoadQuery Q = Layout->NearestRoad(MX, MY, 12.0);
			if (Q.RoadIndex >= 0) { return 4; }
			if (Layout->ForestFactor(MX, MY) > 0.45) { return 1; }
		}
		else if (Mid.Z > 30000.f)
		{
			return 2;
		}
		return 0;
	};
	for (int32 J = 0; J < Cells; ++J)
	{
		for (int32 I = 0; I < Cells; ++I)
		{
			const int32 V00 = J * N + I;
			const int32 V10 = V00 + 1;
			const int32 V01 = V00 + N;
			const int32 V11 = V01 + 1;
			// winding: counter-clockwise seen from above in Unreal's left-handed space
			const int32 S1 = Classify(Vertices[V00], Vertices[V11], Vertices[V10]);
			Tris[S1].Append({V00, V11, V10});
			const int32 S2 = Classify(Vertices[V00], Vertices[V01], Vertices[V11]);
			Tris[S2].Append({V00, V01, V11});
		}
	}

	// skirts around core chunks hide cracks between chunks and the coarse outer terrain
	if (bCore)
	{
		auto AddSkirt = [&](int32 A, int32 B)
		{
			const int32 SA = Vertices.Add(Vertices[A] - FVector(0.f, 0.f, 800.f));
			Normals.Add(Normals[A]);
			UVs.Add(UVs[A]);
			const int32 SB = Vertices.Add(Vertices[B] - FVector(0.f, 0.f, 800.f));
			Normals.Add(Normals[B]);
			UVs.Add(UVs[B]);
			Tris[0].Append({A, SA, B});
			Tris[0].Append({B, SA, SB});
			Tris[0].Append({A, B, SA});
			Tris[0].Append({B, SB, SA});
		};
		for (int32 I = 0; I < Cells; ++I)
		{
			AddSkirt(I, I + 1);
			AddSkirt((N - 1) * N + I, (N - 1) * N + I + 1);
			AddSkirt(I * N, (I + 1) * N);
			AddSkirt(I * N + N - 1, (I + 1) * N + N - 1);
		}
	}

	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, FName(*FString::Printf(TEXT("Terrain_%s_%d_%d"), bCore ? TEXT("Core") : TEXT("Outer"), ChunkX, ChunkY)));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->bUseAsyncCooking = false;
	Mesh->SetCastShadow(true);
	Mesh->ComponentTags.Add(SUSurfaceTag::Grass);
	const TArray<FLinearColor> NoColors;
	const TArray<FProcMeshTangent> NoTangents;
	static const ESUMat SectionMats[5] = {ESUMat::Grass, ESUMat::ForestFloor, ESUMat::Rock, ESUMat::Snow, ESUMat::Dirt};
	int32 Section = 0;
	for (int32 S = 0; S < 5; ++S)
	{
		if (Tris[S].Num() == 0)
		{
			continue;
		}
		Mesh->CreateMeshSection_LinearColor(Section, Vertices, Tris[S], Normals, UVs, NoColors, NoTangents, bCore);
		if (Lib)
		{
			Mesh->SetMaterial(Section, Lib->GetMaterial(SectionMats[S]));
		}
		++Section;
	}
	if (bCore)
	{
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Mesh->RegisterComponent();
	TerrainChunks.Add(Mesh);
}

// ------------------------------------------------------------------------------------ water

void ASUWorldBuilder::BuildWater()
{
	USUAssetLibrary* Lib = USUAssetLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	// One large water plane at the water table: the lake and small ponds appear wherever the
	// terrain dips below it. PLACEHOLDER: flat dark material, no water shader yet.
	UStaticMeshComponent* Water = NewObject<UStaticMeshComponent>(this, TEXT("WaterPlane"));
	Water->SetupAttachment(SceneRoot);
	Water->SetMobility(EComponentMobility::Static);
	Water->SetStaticMesh(Lib->GetMesh(ESUShape::Plane));
	Water->SetMaterial(0, Lib->GetMaterial(ESUMat::Water));
	const float Size = static_cast<float>(WorldLayout::CoreHalfSize * 2.0 * M + 40000.0);
	Water->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, static_cast<float>(Layout->TheLake.WaterZ * M)), FVector(Size / 100.f, Size / 100.f, 1.f)));
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCastShadow(false);
	Water->RegisterComponent();
	WaterPlanes.Add(Water);
}

// ------------------------------------------------------------------------------------ roads

void ASUWorldBuilder::BuildRoads()
{
	USUAssetLibrary* Lib = USUAssetLibrary::Get(this);
	const TArray<FLinearColor> NoColors;
	const TArray<FProcMeshTangent> NoTangents;

	MapRoads.Reset();
	for (size_t RI = 0; RI < Layout->Roads.size(); ++RI)
	{
		const Road& R = Layout->Roads[RI];
		const int32 Count = static_cast<int32>(R.Samples.size());
		if (Count < 2)
		{
			continue;
		}
		const float HalfW = static_cast<float>(R.Width * 0.5 * M);
		const float Shoulder = 160.f;
		const float Lift = 8.f;

		TArray<FVector> V;
		TArray<FVector> Nrm;
		TArray<FVector2D> UV;
		TArray<int32> SurfaceTris;
		TArray<int32> ShoulderTris;
		TArray<int32> LineTris;
		TArray<FVector2D>& MapLine = MapRoads.AddDefaulted_GetRef();

		for (int32 I = 0; I < Count; ++I)
		{
			const RoadSample& S = R.Samples[static_cast<size_t>(I)];
			const FVector C(S.P.X * M, S.P.Y * M, S.Z * M + Lift);
			const FVector T(S.Tangent.X, S.Tangent.Y, 0.0);
			const FVector Side = FVector::CrossProduct(FVector::UpVector, T).GetSafeNormal(); // right side
			const float U = static_cast<float>(S.Along / 6.0);
			// 0: outer left, 1: edge left, 2: edge right, 3: outer right
			V.Add(C - Side * (HalfW + Shoulder) - FVector(0, 0, 45.f)); UV.Add(FVector2D(0.f, U));
			V.Add(C - Side * HalfW); UV.Add(FVector2D(0.2f, U));
			V.Add(C + Side * HalfW); UV.Add(FVector2D(0.8f, U));
			V.Add(C + Side * (HalfW + Shoulder) - FVector(0, 0, 45.f)); UV.Add(FVector2D(1.f, U));
			for (int32 K = 0; K < 4; ++K)
			{
				Nrm.Add(FVector::UpVector);
			}
			if (I % 3 == 0 || I == Count - 1)
			{
				MapLine.Add(WorldToMapUV(C));
			}
		}
		for (int32 I = 0; I + 1 < Count; ++I)
		{
			const int32 A = I * 4;
			const int32 B = (I + 1) * 4;
			// Front faces: Cross(C - A, B - A) points along the face normal (up).
			// surface (1-2)
			SurfaceTris.Append({A + 1, B + 2, B + 1});
			SurfaceTris.Append({A + 1, A + 2, B + 2});
			// shoulders
			ShoulderTris.Append({A + 0, B + 1, B + 0});
			ShoulderTris.Append({A + 0, A + 1, B + 1});
			ShoulderTris.Append({A + 2, B + 3, B + 2});
			ShoulderTris.Append({A + 2, A + 3, B + 3});
		}

		// centre line (dashed) on asphalt, edge lines too
		if (R.Surface == RoadSurface::Asphalt)
		{
			auto AddStrip = [&](float Offset, float Width, bool bDashed)
			{
				for (int32 I = 0; I + 1 < Count; ++I)
				{
					if (bDashed && (I % 4) >= 2)
					{
						continue;
					}
					const RoadSample& S0 = R.Samples[static_cast<size_t>(I)];
					const RoadSample& S1 = R.Samples[static_cast<size_t>(I + 1)];
					const FVector C0(S0.P.X * M, S0.P.Y * M, S0.Z * M + Lift + 1.5f);
					const FVector C1(S1.P.X * M, S1.P.Y * M, S1.Z * M + Lift + 1.5f);
					const FVector Side0 = FVector::CrossProduct(FVector::UpVector, FVector(S0.Tangent.X, S0.Tangent.Y, 0.0)).GetSafeNormal();
					const FVector Side1 = FVector::CrossProduct(FVector::UpVector, FVector(S1.Tangent.X, S1.Tangent.Y, 0.0)).GetSafeNormal();
					const int32 Base = V.Num();
					V.Add(C0 + Side0 * (Offset - Width * 0.5f)); V.Add(C0 + Side0 * (Offset + Width * 0.5f));
					V.Add(C1 + Side1 * (Offset - Width * 0.5f)); V.Add(C1 + Side1 * (Offset + Width * 0.5f));
					for (int32 K = 0; K < 4; ++K)
					{
						Nrm.Add(FVector::UpVector);
						UV.Add(FVector2D(0.f, 0.f));
					}
					LineTris.Append({Base, Base + 3, Base + 2});
					LineTris.Append({Base, Base + 1, Base + 3});
				}
			};
			AddStrip(0.f, 12.f, true);
			AddStrip(-HalfW + 25.f, 12.f, false);
			AddStrip(HalfW - 25.f, 12.f, false);
		}

		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, FName(*FString::Printf(TEXT("Road_%d"), static_cast<int32>(RI))));
		Mesh->SetupAttachment(SceneRoot);
		Mesh->bUseAsyncCooking = false;
		Mesh->ComponentTags.Add(RoadTag(R.Surface));
		Mesh->CreateMeshSection_LinearColor(0, V, SurfaceTris, Nrm, UV, NoColors, NoTangents, true);
		Mesh->CreateMeshSection_LinearColor(1, V, ShoulderTris, Nrm, UV, NoColors, NoTangents, true);
		if (LineTris.Num() > 0)
		{
			Mesh->CreateMeshSection_LinearColor(2, V, LineTris, Nrm, UV, NoColors, NoTangents, false);
		}
		if (Lib)
		{
			Mesh->SetMaterial(0, Lib->GetMaterial(RoadMaterial(R.Surface)));
			Mesh->SetMaterial(1, Lib->GetMaterial(R.Surface == RoadSurface::Asphalt ? ESUMat::Gravel : ESUMat::Dirt));
			Mesh->SetMaterial(2, Lib->GetMaterial(ESUMat::RoadLineWhite));
		}
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Mesh->RegisterComponent();
		RoadMeshes.Add(Mesh);
	}
}

// ------------------------------------------------------------------------------------ forest

UHierarchicalInstancedStaticMeshComponent* ASUWorldBuilder::MakeFoliageComponent(const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Material, bool bCollision, int32 CullDistance)
{
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(Name));
	H->SetupAttachment(SceneRoot);
	H->SetMobility(EComponentMobility::Static);
	H->SetStaticMesh(Mesh);
	if (Material)
	{
		H->SetMaterial(0, Material);
	}
	if (bCollision)
	{
		H->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (CullDistance > 0)
	{
		H->SetCullDistances(CullDistance / 2, CullDistance);
	}
	H->RegisterComponent();
	Foliage.Add(H);
	return H;
}

void ASUWorldBuilder::BuildForest()
{
	USUAssetLibrary* Lib = USUAssetLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	UStaticMesh* Cyl = Lib->GetMesh(ESUShape::Cylinder);
	UStaticMesh* Cone = Lib->GetMesh(ESUShape::Cone);
	UStaticMesh* Sphere = Lib->GetMesh(ESUShape::Sphere);
	const uint32 Seed = Layout->GetSeed();

	TArray<FTransform> Trunks, BirchTrunks, Crown1, Crown2, PineCrowns, BirchCrowns, Rocks, Bushes, Logs, FarCrowns, Ferns;
	TArray<TArray<FTransform>> OverrideTrees;
	const TArray<TObjectPtr<UStaticMesh>>& TreeOverrides = Lib->GetTreeOverrides();
	OverrideTrees.SetNum(TreeOverrides.Num());
	const float TreeMeshHeight = 1500.f;

	// ---- core forest: jittered 9 m grid
	const double Cell = 9.0;
	const double Half = WorldLayout::CoreHalfSize;
	const int32 Steps = static_cast<int32>(2.0 * Half / Cell);
	for (int32 J = 0; J < Steps; ++J)
	{
		for (int32 I = 0; I < Steps; ++I)
		{
			const double JX = Hash2D01(I, J, Seed + 11u);
			const double JY = Hash2D01(I, J, Seed + 12u);
			const double X = -Half + (I + JX) * Cell;
			const double Y = -Half + (J + JY) * Cell;
			const double Roll = Hash2D01(I, J, Seed + 13u);
			// bushes and ferns at forest edges / clearings
			const double Bush = Hash2D01(I, J, Seed + 21u);
			const double Density = Layout->TreeDensity(X, Y);
			if (Roll < Density)
			{
				const double Kind = Hash2D01(I, J, Seed + 14u);
				const float H = static_cast<float>((13.0 + 15.0 * Hash2D01(I, J, Seed + 15u)) * M);
				const double Z = Layout->Height(X, Y) * M - 20.0;
				const float Yaw = static_cast<float>(Hash2D01(I, J, Seed + 16u) * 360.0);
				const FVector Base(X * M, Y * M, Z);
				if (OverrideTrees.Num() > 0)
				{
					const int32 Variant = static_cast<int32>(Kind * OverrideTrees.Num()) % OverrideTrees.Num();
					const float S = H / TreeMeshHeight;
					OverrideTrees[Variant].Add(FTransform(FRotator(0.f, Yaw, 0.f), Base, FVector(S)));
					continue;
				}
				const float TrunkD = H * 0.022f;
				if (Kind < 0.08 && Layout->ForestFactor(X, Y) < 0.5)
				{
					// birch
					const float BH = H * 0.8f;
					BirchTrunks.Add(FTransform(FRotator(0.f, Yaw, 0.f), Base + FVector(0, 0, BH * 0.4f), FVector(TrunkD * 0.7f / 100.f, TrunkD * 0.7f / 100.f, BH * 0.8f / 100.f)));
					BirchCrowns.Add(FTransform(FRotator(0.f, Yaw, 0.f), Base + FVector(0, 0, BH * 0.72f), FVector(BH * 0.32f / 100.f, BH * 0.32f / 100.f, BH * 0.5f / 100.f)));
				}
				else if (Kind < 0.2)
				{
					// pine: tall bare trunk, round crown
					Trunks.Add(FTransform(FRotator(0.f, Yaw, 0.f), Base + FVector(0, 0, H * 0.42f), FVector(TrunkD / 100.f, TrunkD / 100.f, H * 0.84f / 100.f)));
					PineCrowns.Add(FTransform(FRotator(0.f, Yaw, 0.f), Base + FVector(0, 0, H * 0.82f), FVector(H * 0.24f / 100.f, H * 0.24f / 100.f, H * 0.3f / 100.f)));
				}
				else
				{
					// spruce / fir: two stacked cones
					const float Slim = Kind < 0.5 ? 0.34f : 0.4f;
					Trunks.Add(FTransform(FRotator(0.f, Yaw, 0.f), Base + FVector(0, 0, H * 0.35f), FVector(TrunkD / 100.f, TrunkD / 100.f, H * 0.7f / 100.f)));
					Crown1.Add(FTransform(FRotator(0.f, Yaw, 0.f), Base + FVector(0, 0, H * 0.43f), FVector(H * Slim / 100.f, H * Slim / 100.f, H * 0.62f / 100.f)));
					Crown2.Add(FTransform(FRotator(0.f, Yaw + 30.f, 0.f), Base + FVector(0, 0, H * 0.73f), FVector(H * Slim * 0.62f / 100.f, H * Slim * 0.62f / 100.f, H * 0.54f / 100.f)));
				}
			}
			else if (Density > 0.0 && Bush < 0.18)
			{
				const double Z = Layout->Height(X, Y) * M;
				const float S = static_cast<float>(0.6 + Hash2D01(I, J, Seed + 22u) * 1.2);
				Bushes.Add(FTransform(FRotator(0.f, static_cast<float>(Bush * 2000.0), 0.f), FVector(X * M, Y * M, Z + 20.f * S), FVector(1.4f * S, 1.2f * S, 0.8f * S)));
				if (Bush < 0.05)
				{
					Ferns.Add(FTransform(FRotator(0.f, static_cast<float>(Bush * 5000.0), 0.f), FVector(X * M + 120.0, Y * M, Z + 25.f), FVector(0.9f, 0.9f, 0.5f)));
				}
			}

			// rocks: steep spots and random boulders
			if ((I % 3 == 0) && (J % 3 == 0))
			{
				const double RockRoll = Hash2D01(I, J, Seed + 31u);
				const double Slope = Layout->Slope(X, Y, 4.0);
				if (RockRoll < 0.03 + Slope * 0.6 && Layout->TreeDensity(X, Y) > 0.0)
				{
					const double Z = Layout->Height(X, Y) * M;
					const float S = static_cast<float>(0.6 + Hash2D01(I, J, Seed + 32u) * 2.4);
					Rocks.Add(FTransform(FRotator(static_cast<float>(RockRoll * 300.0), static_cast<float>(RockRoll * 900.0), 0.f),
						FVector(X * M, Y * M, Z + 10.f * S), FVector(1.6f * S, 1.2f * S, 0.9f * S)));
				}
				else if (RockRoll > 0.985 && Density > 0.3)
				{
					// fallen log
					const double Z = Layout->Height(X, Y) * M;
					const float Len = static_cast<float>(5.0 + RockRoll * 6.0);
					Logs.Add(FTransform(FRotator(90.f, static_cast<float>(Hash2D01(I, J, Seed + 33u) * 360.0), 0.f), FVector(X * M, Y * M, Z + 18.f), FVector(0.4f, 0.4f, Len)));
				}
			}
		}
	}

	// ---- distant forest on the mountains (no collision, single cones)
	const double FarCell = 34.0;
	const double Outer = WorldLayout::WorldHalfSize;
	const int32 FarSteps = static_cast<int32>(2.0 * Outer / FarCell);
	for (int32 J = 0; J < FarSteps; ++J)
	{
		for (int32 I = 0; I < FarSteps; ++I)
		{
			const double X = -Outer + (I + Hash2D01(I, J, Seed + 41u)) * FarCell;
			const double Y = -Outer + (J + Hash2D01(I, J, Seed + 42u)) * FarCell;
			if (Layout->IsInsideCore(X, Y, -40.0))
			{
				continue;
			}
			if (Hash2D01(I, J, Seed + 43u) > Layout->TreeDensity(X, Y) * 1.3)
			{
				continue;
			}
			const float H = static_cast<float>((15.0 + 12.0 * Hash2D01(I, J, Seed + 44u)) * M);
			const double Z = Layout->Height(X, Y) * M;
			FarCrowns.Add(FTransform(FRotator::ZeroRotator, FVector(X * M, Y * M, Z + H * 0.45f), FVector(H * 0.4f / 100.f, H * 0.4f / 100.f, H * 0.9f / 100.f)));
		}
	}

	auto Add = [&](const TCHAR* Name, UStaticMesh* Mesh, ESUMat Mat, bool bCollision, int32 Cull, const TArray<FTransform>& Instances)
	{
		if (Instances.Num() == 0 || !Mesh)
		{
			return;
		}
		UHierarchicalInstancedStaticMeshComponent* H = MakeFoliageComponent(Name, Mesh, Lib->GetMaterial(Mat), bCollision, Cull);
		H->AddInstances(Instances, false);
	};
	Add(TEXT("Trees_Trunks"), Cyl, ESUMat::Bark, true, 250000, Trunks);
	Add(TEXT("Trees_BirchTrunks"), Cyl, ESUMat::BarkBirch, true, 200000, BirchTrunks);
	Add(TEXT("Trees_Crown1"), Cone, ESUMat::Foliage, false, 400000, Crown1);
	Add(TEXT("Trees_Crown2"), Cone, ESUMat::FoliageDark, false, 400000, Crown2);
	Add(TEXT("Trees_PineCrowns"), Sphere, ESUMat::FoliageDark, false, 400000, PineCrowns);
	Add(TEXT("Trees_BirchCrowns"), Sphere, ESUMat::FoliageLight, false, 200000, BirchCrowns);
	Add(TEXT("Rocks"), Sphere, ESUMat::Rock, true, 200000, Rocks);
	Add(TEXT("Logs"), Cyl, ESUMat::Bark, true, 60000, Logs);
	Add(TEXT("Bushes"), Sphere, ESUMat::FoliageLight, false, 20000, Bushes);
	Add(TEXT("Ferns"), Cone, ESUMat::Foliage, false, 12000, Ferns);
	Add(TEXT("FarForest"), Cone, ESUMat::FoliageDark, false, 0, FarCrowns);
	for (int32 V = 0; V < OverrideTrees.Num(); ++V)
	{
		if (OverrideTrees[V].Num() > 0)
		{
			UHierarchicalInstancedStaticMeshComponent* H = MakeFoliageComponent(*FString::Printf(TEXT("Trees_Override_%d"), V), TreeOverrides[V], nullptr, true, 300000);
			H->AddInstances(OverrideTrees[V], false);
		}
	}
	UE_LOG(LogSignalUnknown, Log, TEXT("  forest: %d trees, %d rocks, %d bushes, %d distant"), Trunks.Num() + BirchTrunks.Num(), Rocks.Num(), Bushes.Num(), FarCrowns.Num());
}

// ------------------------------------------------------------------------------------ boundaries

void ASUWorldBuilder::BuildBoundaries()
{
	// Invisible walls at the edge of the playable core (mountains beyond are scenery for now).
	const float Half = static_cast<float>(WorldLayout::CoreHalfSize * M) - 3000.f;
	const float Height = 100000.f;
	const float Thick = 500.f;
	const FVector Positions[4] = {FVector(Half, 0, 0), FVector(-Half, 0, 0), FVector(0, Half, 0), FVector(0, -Half, 0)};
	const FVector Extents[4] = {FVector(Thick, Half, Height), FVector(Thick, Half, Height), FVector(Half, Thick, Height), FVector(Half, Thick, Height)};
	for (int32 I = 0; I < 4; ++I)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(this, FName(*FString::Printf(TEXT("Boundary_%d"), I)));
		Box->SetupAttachment(SceneRoot);
		Box->SetMobility(EComponentMobility::Static);
		Box->SetRelativeLocation(Positions[I]);
		Box->SetBoxExtent(Extents[I]);
		Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Box->SetHiddenInGame(true);
		Box->RegisterComponent();
		Boundaries.Add(Box);
	}
}

// ------------------------------------------------------------------------------------ sites

template <typename T>
T* ASUWorldBuilder::SpawnSite(FName PadId, float ExtraYaw)
{
	const SitePad* Pad = Layout->FindPad(TCHAR_TO_ANSI(*PadId.ToString()));
	if (!Pad)
	{
		UE_LOG(LogSignalUnknown, Error, TEXT("Missing site pad %s"), *PadId.ToString());
		return nullptr;
	}
	const FTransform Transform(FRotator(0.f, static_cast<float>(Pad->FacingYawDeg) + ExtraYaw, 0.f), FVector(Pad->Center.X * M, Pad->Center.Y * M, Pad->Z * M));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	T* Site = GetWorld()->SpawnActor<T>(T::StaticClass(), Transform, Params);
	if (Site)
	{
		Site->SetSiteId(PadId);
		Site->BuildSite(this, GetNetMode() != NM_Client);
		Sites.Add(Site);
		RegisterPoint(PadId, Transform);
	}
	return Site;
}

void ASUWorldBuilder::BuildSites()
{
	SpawnSite<ASUSiteTower>(TEXT("Tower"));
	SpawnSite<ASUSiteGasStation>(TEXT("GasStation"));
	SpawnSite<ASUSiteForsthaus>(TEXT("Forsthaus"));
	SpawnSite<ASUSiteM02>(TEXT("M02Car"));
	SpawnSite<ASUSiteTown>(TEXT("Town"));
	for (int32 I = 1; I <= 6; ++I)
	{
		SpawnSite<ASUSiteSettlement>(FName(*FString::Printf(TEXT("House%d"), I)));
	}

	// road blocks where roads leave the playable area
	int32 BlockIndex = 0;
	for (const Road& R : Layout->Roads)
	{
		for (size_t I = 1; I < R.Samples.size(); ++I)
		{
			const RoadSample& A = R.Samples[I - 1];
			const RoadSample& B = R.Samples[I];
			const bool bInA = Layout->IsInsideCore(A.P.X, A.P.Y, 70.0);
			const bool bInB = Layout->IsInsideCore(B.P.X, B.P.Y, 70.0);
			if (bInA == bInB)
			{
				continue;
			}
			const RoadSample& Inside = bInA ? A : B;
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Inside.Tangent.Y), static_cast<float>(Inside.Tangent.X)));
			const FTransform T(FRotator(0.f, Yaw, 0.f), FVector(Inside.P.X * M, Inside.P.Y * M, Inside.Z * M));
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.Owner = this;
			if (ASUSiteRoadBlock* Block = GetWorld()->SpawnActor<ASUSiteRoadBlock>(ASUSiteRoadBlock::StaticClass(), T, Params))
			{
				Block->RoadWidth = static_cast<float>(R.Width * M);
				Block->SetSiteId(FName(*FString::Printf(TEXT("RoadBlock%d"), BlockIndex++)));
				Block->BuildSite(this, GetNetMode() != NM_Client);
				Sites.Add(Block);
			}
		}
	}
}

// ------------------------------------------------------------------------------------ map

FVector2D ASUWorldBuilder::WorldToMapUV(const FVector& Location) const
{
	return FVector2D((Location.Y + MapHalfSize) / (2.f * MapHalfSize), (MapHalfSize - Location.X) / (2.f * MapHalfSize));
}

FVector ASUWorldBuilder::MapUVToWorld(const FVector2D& UV) const
{
	const float X = MapHalfSize - UV.Y * 2.f * MapHalfSize;
	const float Y = UV.X * 2.f * MapHalfSize - MapHalfSize;
	return GroundPoint(X, Y);
}

void ASUWorldBuilder::SetupMapCapture()
{
	MapTexture = UKismetRenderingLibrary::CreateRenderTarget2D(this, 2048, 2048, RTF_RGBA8, FLinearColor::Black);
	MapCapture = NewObject<USceneCaptureComponent2D>(this, TEXT("MapCapture"));
	MapCapture->SetupAttachment(SceneRoot);
	MapCapture->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 150000.f), FRotator(-90.f, 0.f, 0.f));
	MapCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	MapCapture->OrthoWidth = MapHalfSize * 2.f;
	MapCapture->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
	MapCapture->bCaptureEveryFrame = false;
	MapCapture->bCaptureOnMovement = false;
	MapCapture->TextureTarget = MapTexture;
	MapCapture->RegisterComponent();

	// foliage clusters are built asynchronously – capture after they are visible
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, this, &ASUWorldBuilder::CaptureMap, 3.f, false);
	FTimerHandle Handle2;
	GetWorldTimerManager().SetTimer(Handle2, this, &ASUWorldBuilder::CaptureMap, 12.f, false);
}

void ASUWorldBuilder::CaptureMap()
{
	if (MapCapture && MapTexture)
	{
		MapCapture->CaptureScene();
	}
}
