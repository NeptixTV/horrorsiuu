#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHTimeOfDay.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class AHHPracticalLight;
class AHHAmbientEmitter;

/**
 * The in-game clock and everything that follows it: sun and moon, the sky, fog, street lamps
 * (HH_StreetLight), lit windows (HH_WindowLight) and day/night ambience (HH_DayOnly /
 * HH_NightOnly emitters).
 *
 * Networking: the server owns an anchor (time + server timestamp + rate). Clients compute the
 * current time from it, so nothing replicates per frame and every machine sees the same sky.
 * Street lamps switch on the server (replicated practical lights); windows and ambience are
 * purely local presentation derived from the same clock.
 *
 * Put exactly one in a persistent map (hub: running day cycle, heists: night with bNightMap).
 */
UCLASS()
class HORRORHEIST_API AHHTimeOfDay : public AActor
{
	GENERATED_BODY()

public:
	AHHTimeOfDay();

	static AHHTimeOfDay* Get(const UObject* WorldContextObject);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Hours since midnight, 0..24 (same value on every machine). */
	float GetTimeOfDay() const;
	/** 0 = deep night, 1 = full daylight (follows the sun, smooth through dusk and dawn). */
	float GetDaylight() const { return Daylight; }
	/** Inside the night window from Project Settings > Horror Heist (jobs may start). */
	bool IsNight() const;
	/** In-game hours per real second (already includes the speed setting). */
	float GetRate() const { return HoursPerSecond; }

	// ---- Server ----------------------------------------------------------------------

	/** Jump the clock (e.g. "wait until dark"). Visuals snap, lamps re-evaluate. */
	void SkipTo(float Hours);
	/** Multiplier on top of the project's day length (host's "Time speed" setting). */
	void SetSpeed(float Multiplier);

protected:
	void Reanchor(float Hours);
	void RefreshRate();
	void HandleSettingsApplied();
	void GatherWorldActors();
	void UpdateVisuals(bool bSnap);
	void UpdateSwitches(float Hours, bool bSnap);
	static float HashUnit(const AActor* Actor, uint32 Salt);

	UFUNCTION()
	void OnRep_Anchor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky")
	TObjectPtr<UDirectionalLightComponent> Moon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky")
	TObjectPtr<USkyLightComponent> SkyLight;

	// ---- Configuration (set per map by the level layout) ------------------------------

	/** Starting hour when no clock was carried over from the previous map (<0 = project default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	float StartHour = -1.f;

	/** Heist maps: always start inside the night window, even if the carried clock says day. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	bool bNightMap = false;

	/** Map-specific speed factor (heists run slower so the night lasts). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock", meta = (ClampMin = 0.0))
	float MapSpeed = 1.f;

	/** Compass rotation of the sun path (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun")
	float NorthYaw = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun")
	float Latitude = 47.f;

	/** Sun declination (season). Slightly negative = autumn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun")
	float Declination = -6.f;

	/** Noon illuminance in lux. Kept low on purpose: overcast town, matched to the exposure range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun")
	float SunLux = 32.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sun")
	float MoonLux = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog")
	FLinearColor DayFogColor = FLinearColor(0.30f, 0.33f, 0.37f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog")
	FLinearColor DuskFogColor = FLinearColor(0.32f, 0.20f, 0.14f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog")
	FLinearColor NightFogColor = FLinearColor(0.012f, 0.016f, 0.026f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog")
	float DayFogDensity = 0.012f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog")
	float NightFogDensity = 0.02f;

	/** Street lamps switch on below this daylight value (each lamp a little earlier/later). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switches", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float LampsOnBelowDaylight = 0.45f;

	/** Windows of houses with these tags stay dark (the target house in a heist: owners away). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switches")
	TArray<FName> DarkHouseTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switches")
	FName StreetLightTag = TEXT("HH_StreetLight");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switches")
	FName WindowLightTag = TEXT("HH_WindowLight");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switches")
	FName DayOnlyTag = TEXT("HH_DayOnly");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switches")
	FName NightOnlyTag = TEXT("HH_NightOnly");

private:
	/** Clock anchor: AnchorHours at server time AnchorServerSeconds, advancing at HoursPerSecond. */
	UPROPERTY(ReplicatedUsing = OnRep_Anchor)
	float AnchorHours = 12.f;

	UPROPERTY(ReplicatedUsing = OnRep_Anchor)
	double AnchorServerSeconds = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_Anchor)
	float HoursPerSecond = 0.f;

	float SpeedSetting = 1.f;
	float Daylight = 1.f;
	float GatherTimer = 0.f;
	int32 GatherPasses = 0;

	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	TArray<TWeakObjectPtr<AHHPracticalLight>> StreetLights;
	/** Shop and bank lights (tag HH_DayOnly): on in business hours, off at night. */
	TArray<TWeakObjectPtr<AHHPracticalLight>> DayLights;
	TArray<TWeakObjectPtr<AActor>> Windows;
	TArray<TWeakObjectPtr<AHHAmbientEmitter>> DayEmitters;
	TArray<TWeakObjectPtr<AHHAmbientEmitter>> NightEmitters;

	FDelegateHandle SettingsHandle;
};
