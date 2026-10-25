#include "Mission/SUAreaTrigger.h"
#include "Core/SUTypes.h"
#include "Core/SUEventSubsystem.h"
#include "Player/SUCharacter.h"
#include "Vehicle/SUVehicle.h"
#include "Components/BoxComponent.h"

ASUAreaTrigger::ASUAreaTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);
	Box->SetHiddenInGame(true);
	RootComponent = Box;
}

void ASUAreaTrigger::Setup(FName InAreaId, const FVector& Extent)
{
	AreaId = InAreaId;
	Box->SetBoxExtent(Extent, true);
	Box->OnComponentBeginOverlap.AddDynamic(this, &ASUAreaTrigger::HandleBeginOverlap);
	Box->OnComponentEndOverlap.AddDynamic(this, &ASUAreaTrigger::HandleEndOverlap);
}

bool ASUAreaTrigger::ContainsLocation(const FVector& Location) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(Location);
	const FVector E = Box->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= E.X && FMath::Abs(Local.Y) <= E.Y && FMath::Abs(Local.Z) <= E.Z;
}

bool ASUAreaTrigger::IsActorInside(const AActor* Actor) const
{
	return Occupants.ContainsByPredicate([Actor](const TWeakObjectPtr<AActor>& W) { return W.Get() == Actor; });
}

bool ASUAreaTrigger::IsRelevantActor(const AActor* Actor) const
{
	return Actor && (Actor->IsA<ASUCharacter>() || Actor->IsA<ASUVehicle>());
}

void ASUAreaTrigger::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (GetNetMode() == NM_Client || !IsRelevantActor(OtherActor) || IsActorInside(OtherActor))
	{
		return;
	}
	Occupants.Add(OtherActor);
	USUEventSubsystem::Post(this, SUEvent::AreaEntered, AreaId, OtherActor);
	OnActorEntered.Broadcast(this, OtherActor);
}

void ASUAreaTrigger::HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (GetNetMode() == NM_Client || !IsRelevantActor(OtherActor))
	{
		return;
	}
	const int32 Removed = Occupants.RemoveAll([OtherActor](const TWeakObjectPtr<AActor>& W) { return !W.IsValid() || W.Get() == OtherActor; });
	if (Removed > 0)
	{
		// a character that just sat down in a car inside the area did not really leave
		const ASUCharacter* Char = Cast<ASUCharacter>(OtherActor);
		if (Char && Char->IsSeated() && ContainsLocation(Char->GetActorLocation()))
		{
			return;
		}
		USUEventSubsystem::Post(this, SUEvent::AreaExited, AreaId, OtherActor);
		OnActorExited.Broadcast(this, OtherActor);
	}
}
