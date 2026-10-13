#include "Heist/HHEscapeZone.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AHHEscapeZone::AHHEscapeZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetBoxExtent(FVector(300.f, 300.f, 200.f));
	Zone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Zone->SetGenerateOverlapEvents(false);
	Zone->SetHiddenInGame(true);
	Zone->ShapeColor = FColor(80, 200, 120);
	SetRootComponent(Zone);
}

TArray<AHHEscapeZone*> AHHEscapeZone::GetAll(const UObject* WorldContextObject)
{
	TArray<AHHEscapeZone*> Result;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return Result;
	}
	for (TActorIterator<AHHEscapeZone> It(World); It; ++It)
	{
		if (!It->ZoneId.IsNone())
		{
			Result.Add(*It);
		}
	}
	Result.Sort([](const AHHEscapeZone& A, const AHHEscapeZone& B)
	{
		return A.SortOrder != B.SortOrder ? A.SortOrder < B.SortOrder : A.ZoneId.LexicalLess(B.ZoneId);
	});
	return Result;
}

AHHEscapeZone* AHHEscapeZone::Find(const UObject* WorldContextObject, FName InZoneId)
{
	for (AHHEscapeZone* Candidate : GetAll(WorldContextObject))
	{
		if (Candidate->ZoneId == InZoneId)
		{
			return Candidate;
		}
	}
	return nullptr;
}

bool AHHEscapeZone::ContainsPoint(const FVector& Point) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(Point);
	const FVector Extent = Zone->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}
