#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHAmbientEmitter.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Positional ambience: an optional loop plus randomly timed one-shots scattered around
 * the emitter (drips, pipe ticks, creaks). Local per client, timer driven, no tick.
 */
UCLASS()
class HORRORHEIST_API AHHAmbientEmitter : public AActor
{
	GENERATED_BODY()

public:
	AHHAmbientEmitter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	void ScheduleNext();
	void PlayOneShot();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambience")
	TObjectPtr<UAudioComponent> Loop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	TArray<TObjectPtr<USoundBase>> OneShots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	float MinInterval = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	float MaxInterval = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	float ScatterRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	FVector2D VolumeRange = FVector2D(0.5f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	FVector2D PitchRange = FVector2D(0.92f, 1.08f);

	/** Closed caption for the one-shots (empty = none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	FText Caption;

	FTimerHandle Timer;
	int32 LastIndex = INDEX_NONE;
};
