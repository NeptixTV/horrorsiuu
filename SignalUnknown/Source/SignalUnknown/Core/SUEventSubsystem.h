// SIGNAL UNKNOWN – world event bus.
//
// Gameplay systems report what happens ("Item.Picked Camera", "Area.Entered Forsthaus", ...)
// and listeners such as the mission manager react, without the systems knowing each other.
// Mission relevant events are only posted on the server.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SUEventSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSUGameEventSignature, FName, EventName, FName, Param, AActor*, Instigator);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FSUNoiseSignature, const FVector& /*Location*/, float /*Loudness*/, AActor* /*Instigator*/);

UCLASS()
class SIGNALUNKNOWN_API USUEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Fired for every posted gameplay event. */
	UPROPERTY(BlueprintAssignable, Category = "Signal Unknown|Events")
	FSUGameEventSignature OnGameEvent;

	/** Noises creatures can hear (footsteps, engine, horn...). Server only. */
	FSUNoiseSignature OnNoise;

	UFUNCTION(BlueprintCallable, Category = "Signal Unknown|Events")
	void PostEvent(FName EventName, FName Param, AActor* Instigator);

	/** Loudness 0..1: 0.1 = careful steps, 0.5 = running, 1.0 = car horn. */
	void PostNoise(const FVector& Location, float Loudness, AActor* Instigator);

	/** Static helper; does nothing without a valid world or on clients. */
	static void Post(const UObject* WorldContext, FName EventName, FName Param, AActor* Instigator);
	static void Noise(const UObject* WorldContext, const FVector& Location, float Loudness, AActor* Instigator);
};
