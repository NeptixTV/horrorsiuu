// SIGNAL UNKNOWN – local ambience and acoustic environment for the local player.
//
// Wind, birds (day), crickets (night), rain; indoor / vehicle detection with matching reverb.
// Reverb is real engine reverb (UReverbEffect), so all spatial sounds – including voice chat
// played through the engine – are affected by the environment.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SUAmbienceComponent.generated.h"

class USUProceduralAudioComponent;
class UReverbEffect;

UENUM()
enum class ESUAcoustic : uint8
{
	Outdoors,
	SmallRoom,
	LargeRoom,
	Vehicle
};

UCLASS()
class SIGNALUNKNOWN_API USUAmbienceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USUAmbienceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	ESUAcoustic GetAcoustic() const { return Acoustic; }
	bool IsIndoors() const { return Acoustic == ESUAcoustic::SmallRoom || Acoustic == ESUAcoustic::LargeRoom; }

private:
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> Wind;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> Birds;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> Crickets;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> Rain;
	UPROPERTY(Transient) TObjectPtr<UReverbEffect> SmallRoomReverb;
	UPROPERTY(Transient) TObjectPtr<UReverbEffect> LargeRoomReverb;
	UPROPERTY(Transient) TObjectPtr<UReverbEffect> VehicleReverb;
	UPROPERTY(Transient) TObjectPtr<UReverbEffect> ForestReverb;

	ESUAcoustic Acoustic = ESUAcoustic::Outdoors;
	ESUAcoustic AppliedAcoustic = ESUAcoustic::Outdoors;
	bool bReverbApplied = false;
	float CheckTimer = 0.f;

	USUProceduralAudioComponent* MakeLayer(const TCHAR* Name, int32 KindIndex, float Gain);
	void DetectAcoustic();
	void ApplyReverb();
	UReverbEffect* MakeReverb(float Decay, float Density, float Diffusion, float Gain, float GainHF, float Reflections, float LateGain);
};
