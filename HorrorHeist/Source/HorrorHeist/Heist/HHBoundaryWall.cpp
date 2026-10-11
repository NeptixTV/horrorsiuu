#include "Heist/HHBoundaryWall.h"
#include "Components/BoxComponent.h"

AHHBoundaryWall::AHHBoundaryWall()
{
	PrimaryActorTick.bCanEverTick = false;
	Wall = CreateDefaultSubobject<UBoxComponent>(TEXT("Wall"));
	SetRootComponent(Wall);
	Wall->SetBoxExtent(FVector(500.f, 20.f, 400.f));
	Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Wall->SetCollisionObjectType(ECC_WorldStatic);
	Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
	Wall->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Wall->SetGenerateOverlapEvents(false);
	Wall->ShapeColor = FColor(255, 60, 60);
	SetActorHiddenInGame(true);
}
