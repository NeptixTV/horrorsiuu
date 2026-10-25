// SIGNAL UNKNOWN – synth component that runs one SULogic::Dsp generator.
//
// PLACEHOLDER AUDIO: until recorded sounds/music exist, all audio is synthesised. The component
// is spatialised (attenuation, occlusion, distance low-pass) unless the radius is 0.
#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Logic/SUDsp.h"
#include "SUProceduralAudioComponent.generated.h"

UENUM(BlueprintType)
enum class ESUSoundKind : uint8
{
	Wind,
	Rain,
	Birds,
	Crickets,
	RadioStatic,
	RadioVoice,
	Engine,
	Horn,
	Click,
	Music,
	Creature,
	Impact,
	Footstep,
	Stinger,
	Hum,
	Pump,
	Thunder,
	DoorCreak
};

UCLASS(ClassGroup = (SignalUnknown), meta = (BlueprintSpawnableComponent))
class SIGNALUNKNOWN_API USUProceduralAudioComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	USUProceduralAudioComponent(const FObjectInitializer& ObjectInitializer);

	/**
	 * Call before StartSound. Category = SUAudioCategory (volume setting).
	 * AttenuationRadius in cm; 0 = not spatialised (2D).
	 */
	void ConfigureSound(ESUSoundKind InKind, int32 InCategory, float InGain, float AttenuationRadius);

	void StartSound();
	void StopSound();

	/** SULogic::Dsp::Param index. Thread safe. */
	void SetSoundParam(int32 Index, float Value);
	/** One-shot trigger (meaning depends on the generator). Thread safe. */
	void TriggerSound(int32 TriggerKind, float Strength = 1.f);

	void SetBaseGain(float InGain) { BaseGain = InGain; }
	float GetBaseGain() const { return BaseGain; }
	/** 0..1 muffling (e.g. heard from inside a car). */
	void SetInterior(float Amount) { SetSoundParam(SULogic::Dsp::Param::Interior, Amount); }

	ESUSoundKind GetKind() const { return Kind; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	ESUSoundKind Kind = ESUSoundKind::Click;
	int32 Category = 1;
	float BaseGain = 1.f;
	bool bStarted = false;

	/** Created in Init before the audio thread starts rendering; never replaced afterwards. */
	std::unique_ptr<SULogic::Dsp::Generator> Generator;
	/** Values set before Init are forwarded once the generator exists. */
	float PendingParams[SULogic::Dsp::Param::Num] = {};
	bool bPendingParam[SULogic::Dsp::Param::Num] = {};
	TArray<TPair<int32, float>> PendingTriggers;
	FCriticalSection PendingLock;

	void PushGain();
};
