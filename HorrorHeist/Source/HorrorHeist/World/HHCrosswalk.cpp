#include "World/HHCrosswalk.h"
#include "NPC/HHTownsperson.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"

AHHCrosswalk::AHHCrosswalk()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	SetRootComponent(Area);
	Area->InitBoxExtent(FVector(400.f, 150.f, 150.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetGenerateOverlapEvents(false);
}

bool AHHCrosswalk::Contains(const FVector& Location) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(Location);
	const FVector Extent = Area->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z + 100.f;
}

bool AHHCrosswalk::IsBusy() const
{
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden() && Contains(It->GetActorLocation()))
		{
			return true;
		}
	}
	for (TActorIterator<AHHTownsperson> It(GetWorld()); It; ++It)
	{
		if (It->IsPresent() && It->IsWalking() && Contains(It->GetActorLocation()))
		{
			return true;
		}
	}
	return false;
}

AHHCrosswalk* AHHCrosswalk::FindAhead(const UObject* WorldContextObject, const FVector& Location, const FVector& Direction, float Distance, float* OutAlong)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	const FVector Forward = Direction.GetSafeNormal2D();
	AHHCrosswalk* Best = nullptr;
	float BestAlong = Distance;
	for (TActorIterator<AHHCrosswalk> It(World); It; ++It)
	{
		for (float Along = 0.f; Along <= Distance; Along += 50.f)
		{
			if (It->Contains(Location + Forward * Along))
			{
				if (Along < BestAlong)
				{
					BestAlong = Along;
					Best = *It;
				}
				break;
			}
		}
	}
	if (OutAlong)
	{
		*OutAlong = BestAlong;
	}
	return Best;
}
