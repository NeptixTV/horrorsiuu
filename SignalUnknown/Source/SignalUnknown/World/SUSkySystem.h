// SIGNAL UNKNOWN – sun, moon, sky, fog, clouds, rain and lightning (local on every machine).
// Driven by the replicated day time and weather of ASUGameState.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SUSkySystem.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UPostProcessComponent;
class UInstancedStaticMeshComponent;
class USUProceduralAudioComponent;

UCLASS()
class SIGNALUNKNOWN_API ASUSkySystem : public AActor
{
	GENERATED_BODY()

public:
	ASUSkySystem();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Flash now, thunder after the delay (seconds). */
	void TriggerLightning(float Strength, float ThunderDelay);

	/** Briefly dims all daylight (horror "something passed the sun"). Not used yet by events. */
	float GetCurrentDarkness() const { return CurrentDarkness; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UDirectionalLightComponent> Flash;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UPostProcessComponent> PostProcess;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> RainStreaks;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> ThunderAudio;

	float FlashTime = -1.f;
	float FlashStrength = 0.f;
	float ThunderTime = -1.f;
	float ThunderStrength = 0.f;
	float RainScroll = 0.f;
	float CurrentDarkness = 0.f;
	bool bLocalIndoors = false;
	float IndoorCheckTimer = 0.f;

	void BuildRain();
	void UpdateRain(float DeltaSeconds, float Rain, const FVector& ViewLocation);
	bool GetViewLocation(FVector& OutLocation) const;
};
