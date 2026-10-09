#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHWeatherController.generated.h"

class UPointLightComponent;
class USoundBase;

/**
 * The storm outside: synchronized lightning (server picks the moment, every client sees the
 * same flash) followed by thunder delayed by "distance". Flashes respect the reduce-flicker
 * accessibility option.
 */
UCLASS()
class HORRORHEIST_API AHHWeatherController : public AActor
{
	GENERATED_BODY()

public:
	AHHWeatherController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: strike now. */
	void Strike();

protected:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastLightning(int32 Seed);

	void ScheduleStrike();
	void PlayThunder();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather")
	TObjectPtr<UPointLightComponent> Flash;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	float FlashIntensity = 400000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	float MinInterval = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	float MaxInterval = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	TArray<TObjectPtr<USoundBase>> ThunderSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather")
	FText ThunderCaption;

private:
	FTimerHandle StrikeTimer;
	FTimerHandle ThunderTimer;

	/** Flash envelope: a few short pulses. */
	TArray<FVector2D> Pulses;	// (start time, duration)
	float FlashTime = 0.f;
	float FlashLength = 0.f;
	FRandomStream Stream;
	int32 PendingThunder = INDEX_NONE;
	float PendingThunderVolume = 1.f;
};
