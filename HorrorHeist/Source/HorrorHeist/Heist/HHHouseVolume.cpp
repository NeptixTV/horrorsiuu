#include "Heist/HHHouseVolume.h"
#include "Components/BoxComponent.h"

AHHHouseVolume::AHHHouseVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(500.f, 500.f, 300.f));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(false);
	Box->ShapeColor = FColor(200, 120, 60);
	SetActorHiddenInGame(true);
}

bool AHHHouseVolume::ContainsPoint(const FVector& Point) const
{
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(Point);
	const FVector Extent = Box->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}
