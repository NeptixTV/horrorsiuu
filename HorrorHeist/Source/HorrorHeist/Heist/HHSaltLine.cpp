#include "Heist/HHSaltLine.h"
#include "Core/HHGameData.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AHHSaltLine::AHHSaltLine()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->SetMobility(EComponentMobility::Movable);
}

void AHHSaltLine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHSaltLine, bScattered);
}

void AHHSaltLine::BeginPlay()
{
	Super::BeginPlay();
	if (!Mesh->GetStaticMesh())
	{
		if (const UHHGameData* GameData = UHHGameData::Get(this))
		{
			Mesh->SetStaticMesh(GameData->SaltLineMesh.LoadSynchronous());
		}
	}
	OnRep_Scattered();
}

void AHHSaltLine::OnRep_Scattered()
{
	// Crossed: the line is kicked apart - wider, flatter, broken up.
	Mesh->SetRelativeScale3D(bScattered ? FVector(1.15f, 3.2f, 0.45f) : FVector::OneVector);
}

bool AHHSaltLine::TryCross(const UObject* WorldContextObject, const FVector& Location, float Radius)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AHHSaltLine> It(World); It; ++It)
	{
		AHHSaltLine* Line = *It;
		if (!Line->IsIntact() || !Line->HasAuthority())
		{
			continue;
		}
		// Distance to the segment of the line, on the floor plane (height within a storey).
		const FVector Axis = Line->GetActorForwardVector().GetSafeNormal2D();
		const FVector A = Line->GetActorLocation() - Axis * Line->HalfLength;
		const FVector B = Line->GetActorLocation() + Axis * Line->HalfLength;
		const FVector Closest = FMath::ClosestPointOnSegment(FVector(Location.X, Location.Y, A.Z), A, B);
		if (FVector::Dist2D(Closest, Location) < Radius && FMath::Abs(Location.Z - A.Z) < 180.f)
		{
			Line->bScattered = true;
			Line->OnRep_Scattered();
			Line->ForceNetUpdate();
			return true;
		}
	}
	return false;
}
