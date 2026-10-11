#include "Heist/HHHauntProp.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AHHHauntProp::AHHHauntProp()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void AHHHauntProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHHauntProp, bMoved);
}

void AHHHauntProp::BeginPlay()
{
	Super::BeginPlay();
	Original = GetActorTransform();
	ApplyTransform();
}

void AHHHauntProp::MoveToAlternate()
{
	if (HasAuthority() && !bMoved && !AltLocation.IsZero())
	{
		bMoved = true;
		ApplyTransform();
		ForceNetUpdate();
	}
}

void AHHHauntProp::OnRep_Moved()
{
	ApplyTransform();
}

void AHHHauntProp::ApplyTransform()
{
	if (bMoved)
	{
		SetActorLocationAndRotation(AltLocation, AltRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (HasActorBegunPlay())
	{
		SetActorTransform(Original, false, nullptr, ETeleportType::TeleportPhysics);
	}
}
