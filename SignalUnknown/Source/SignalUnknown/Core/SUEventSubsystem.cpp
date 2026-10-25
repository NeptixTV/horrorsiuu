#include "Core/SUEventSubsystem.h"
#include "SignalUnknown.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void USUEventSubsystem::PostEvent(FName EventName, FName Param, AActor* Instigator)
{
	UE_LOG(LogSignalUnknown, Verbose, TEXT("Event %s (%s)"), *EventName.ToString(), *Param.ToString());
	OnGameEvent.Broadcast(EventName, Param, Instigator);
}

void USUEventSubsystem::PostNoise(const FVector& Location, float Loudness, AActor* Instigator)
{
	OnNoise.Broadcast(Location, FMath::Clamp(Loudness, 0.f, 1.f), Instigator);
}

void USUEventSubsystem::Post(const UObject* WorldContext, FName EventName, FName Param, AActor* Instigator)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (USUEventSubsystem* Events = World->GetSubsystem<USUEventSubsystem>())
	{
		Events->PostEvent(EventName, Param, Instigator);
	}
}

void USUEventSubsystem::Noise(const UObject* WorldContext, const FVector& Location, float Loudness, AActor* Instigator)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (USUEventSubsystem* Events = World->GetSubsystem<USUEventSubsystem>())
	{
		Events->PostNoise(Location, Loudness, Instigator);
	}
}
