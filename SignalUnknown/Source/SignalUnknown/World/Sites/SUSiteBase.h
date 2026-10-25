// SIGNAL UNKNOWN – base for procedurally built locations (tower, gas station, Forsthaus ...).
//
// A site builds its static geometry locally on every machine. Interactive objects are spawned
// only when bServer is true (they replicate to the clients).
#pragma once

#include "CoreMinimal.h"
#include "World/SUProceduralActor.h"
#include "Engine/World.h"
#include "Templates/Function.h"
#include "SUSiteBase.generated.h"

class ASUWorldBuilder;
class ASUAreaTrigger;
class ASUPickupItem;
class USUEvidenceComponent;
class ASUDocument;

UCLASS(Abstract)
class SIGNALUNKNOWN_API ASUSiteBase : public ASUProceduralActor
{
	GENERATED_BODY()

public:
	ASUSiteBase();

	void SetSiteId(FName InId) { SiteId = InId; }
	FName GetSiteId() const { return SiteId; }

	/** Build everything. Called once by the world builder right after spawning. */
	virtual void BuildSite(ASUWorldBuilder* InBuilder, bool bInServer);

protected:
	FName SiteId;
	TWeakObjectPtr<ASUWorldBuilder> Builder;
	bool bServer = false;

	/** Local -> world. */
	FVector ToWorld(const FVector& Local) const { return GetActorTransform().TransformPosition(Local); }
	FTransform ToWorld(const FVector& Local, float LocalYaw) const;
	/** Terrain height below a local point, in local Z. */
	float GroundZ(float LocalX, float LocalY) const;

	/** Spawns a replicated interactive actor (server only); Configure runs before BeginPlay. */
	template <typename T>
	T* SpawnInteractable(const FVector& LocalPos, float LocalYaw, TFunctionRef<void(T&)> Configure)
	{
		if (!bServer)
		{
			return nullptr;
		}
		const FTransform WorldT = ToWorld(LocalPos, LocalYaw);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bDeferConstruction = true;
		T* Actor = GetWorld()->SpawnActor<T>(T::StaticClass(), WorldT, Params);
		if (Actor)
		{
			Configure(*Actor);
			Actor->FinishSpawning(WorldT);
		}
		return Actor;
	}

	template <typename T>
	T* SpawnInteractable(const FVector& LocalPos, float LocalYaw)
	{
		return SpawnInteractable<T>(LocalPos, LocalYaw, [](T&) {});
	}

	ASUPickupItem* SpawnItem(FName ItemId, const FVector& LocalPos, float LocalYaw, int32 Count = 1, AActor* AttachTo = nullptr);
	ASUDocument* SpawnDocument(const FVector& LocalPos, float LocalYaw, const TCHAR* Title, const TCHAR* Body, int32 Look, FName DocumentId = NAME_None, FName TakeItemId = NAME_None);

	/** Local area trigger (exists on every machine, posts events on the server). */
	ASUAreaTrigger* AddArea(FName AreaId, const FVector& LocalCenter, const FVector& Extent, float LocalYaw = 0.f);

	/** Static evidence that belongs to this site (exists on every machine). */
	USUEvidenceComponent* AddEvidence(FName EvidenceId, const TCHAR* DisplayName, FName Category, const FVector& LocalCenter, const FVector& Extent, bool bRequiresLight, float MaxDistance = 1500.f, const FRotator& LocalRot = FRotator::ZeroRotator);

	/** Register a named world point with the builder (markers, spawns). */
	void RegisterPoint(FName Name, const FVector& LocalPos, float LocalYaw = 0.f);

	/** Simple box house used by settlements and the town. */
	void AddSimpleHouse(const FVector& Center, float SizeX, float SizeY, float WallHeight, ESUMat Walls, ESUMat Roof, bool bWindows, int32 Seed);
	/** Concrete barriers, sign and warning lights across a road. */
	void AddRoadBlock(const FVector& Center, float Width, const TCHAR* SignText);
};
