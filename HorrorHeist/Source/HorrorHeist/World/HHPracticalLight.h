#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHPracticalLight.generated.h"

class ULocalLightComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EHHFlickerMode : uint8
{
	Steady,
	/** Mostly steady, occasional dropouts (bad contact). */
	Flicker,
	/** Fluorescent tube: hum, rare start-up stutter. */
	Buzz,
	/** Failing bulb: dim, nervous, sputters often. */
	Dying,
	/** Old CRT: restless brightness and colour. */
	Television,
	/** Warm restless glow (space heater, candle). */
	Ember
};

/**
 * A practical light: fixture mesh + light + optional hum. Keeps the bulb's emissive
 * material in sync with the light so a flicker reads on the lamp itself, honours the
 * "reduce flicker" accessibility setting, and replicates on/off (light switches) while the
 * flicker pattern itself is cosmetic and local.
 */
UCLASS(Abstract)
class HORRORHEIST_API AHHPracticalLight : public AActor
{
	GENERATED_BODY()

public:
	AHHPracticalLight();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: switch on/off (replicated). */
	void SetLightOn(bool bNewOn);
	bool IsLightOn() const { return bOn; }

	/** Local: a few seconds of stutter (ambience events). */
	void Surge(float Duration);

	ULocalLightComponent* GetLight() const { return Light; }

protected:
	UFUNCTION()
	void OnRep_On();

	float ComputeFlicker(float DeltaSeconds);
	void ApplyLevel(float Level);
	void UpdateTickState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<UStaticMeshComponent> Fixture;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<ULocalLightComponent> Light;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<UAudioComponent> Hum;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	EHHFlickerMode FlickerMode = EHHFlickerMode::Steady;

	/** 0..1 how violent the flicker is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	float FlickerStrength = 1.f;

	/** Scalar parameter on the fixture materials driven with the light level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	FName EmissiveParameter = TEXT("EmissiveStrength");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	float EmissiveStrength = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	bool bStartOn = true;

private:
	UPROPERTY(ReplicatedUsing = OnRep_On)
	bool bOn = true;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> EmissiveMaterials;

	float BaseIntensity = 0.f;
	FLinearColor BaseColor = FLinearColor::White;
	float BaseHumVolume = 1.f;
	float Level = 1.f;
	float SmoothedLevel = 1.f;
	float Time = 0.f;
	float NextEventTime = 0.f;
	float EventEndTime = 0.f;
	float SurgeEndTime = 0.f;
	float StutterPhase = 0.f;
	float TvTarget = 1.f;
	float TvNextChange = 0.f;
};

UCLASS()
class HORRORHEIST_API AHHPracticalPointLight : public AHHPracticalLight
{
	GENERATED_BODY()
public:
	AHHPracticalPointLight();
};

UCLASS()
class HORRORHEIST_API AHHPracticalSpotLight : public AHHPracticalLight
{
	GENERATED_BODY()
public:
	AHHPracticalSpotLight();
};

UCLASS()
class HORRORHEIST_API AHHPracticalRectLight : public AHHPracticalLight
{
	GENERATED_BODY()
public:
	AHHPracticalRectLight();
};
