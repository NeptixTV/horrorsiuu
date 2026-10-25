#include "Audio/SUProceduralAudioComponent.h"
#include "Core/SUGameUserSettings.h"
#include "Engine/World.h"
#include "Misc/ScopeLock.h"

USUProceduralAudioComponent::USUProceduralAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
	bAutoActivate = false;
	bAllowSpatialization = false;
}

void USUProceduralAudioComponent::ConfigureSound(ESUSoundKind InKind, int32 InCategory, float InGain, float AttenuationRadius)
{
	Kind = InKind;
	Category = InCategory;
	BaseGain = InGain;
	if (AttenuationRadius > 0.f)
	{
		bAllowSpatialization = true;
		bOverrideAttenuation = true;
		FSoundAttenuationSettings& A = AttenuationOverrides;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(FMath::Min(150.f, AttenuationRadius * 0.05f), 0.f, 0.f);
		A.FalloffDistance = AttenuationRadius;
		A.bAttenuateWithLPF = true;
		A.LPFRadiusMin = AttenuationRadius * 0.15f;
		A.LPFRadiusMax = AttenuationRadius;
		// sounds behind walls are muffled ("noises from another room")
		A.bEnableOcclusion = true;
		A.OcclusionTraceChannel = ECC_Visibility;
		A.OcclusionLowPassFilterFrequency = 900.f;
		A.OcclusionVolumeAttenuation = 0.45f;
		A.OcclusionInterpolationTime = 0.25f;
		A.bEnableReverbSend = true;
	}
	else
	{
		bAllowSpatialization = false;
	}
}

void USUProceduralAudioComponent::StartSound()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || bStarted)
	{
		return;
	}
	bStarted = true;
	PushGain();
	Start();
}

void USUProceduralAudioComponent::StopSound()
{
	if (bStarted)
	{
		Stop();
		bStarted = false;
	}
}

bool USUProceduralAudioComponent::Init(int32& SampleRate)
{
	const SULogic::Dsp::SoundKind DspKind = static_cast<SULogic::Dsp::SoundKind>(static_cast<int32>(Kind));
	NumChannels = SULogic::Dsp::IsStereoKind(DspKind) && !bAllowSpatialization ? 2 : 1;
	if (!Generator)
	{
		Generator = SULogic::Dsp::CreateGenerator(DspKind);
		if (Generator)
		{
			Generator->Prepare(static_cast<float>(SampleRate), NumChannels, static_cast<uint32>(GetUniqueID()));
			FScopeLock Lock(&PendingLock);
			for (int32 I = 0; I < SULogic::Dsp::Param::Num; ++I)
			{
				if (bPendingParam[I])
				{
					Generator->SetParam(I, PendingParams[I]);
				}
			}
			for (const TPair<int32, float>& T : PendingTriggers)
			{
				Generator->Trigger(T.Key, T.Value);
			}
			PendingTriggers.Reset();
		}
	}
	return Generator != nullptr;
}

int32 USUProceduralAudioComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	if (!Generator || NumChannels <= 0)
	{
		FMemory::Memzero(OutAudio, sizeof(float) * NumSamples);
		return NumSamples;
	}
	Generator->Render(OutAudio, NumSamples / NumChannels);
	return NumSamples;
}

void USUProceduralAudioComponent::SetSoundParam(int32 Index, float Value)
{
	if (Index < 0 || Index >= SULogic::Dsp::Param::Num)
	{
		return;
	}
	if (Generator)
	{
		Generator->SetParam(Index, Value);
		return;
	}
	FScopeLock Lock(&PendingLock);
	PendingParams[Index] = Value;
	bPendingParam[Index] = true;
}

void USUProceduralAudioComponent::TriggerSound(int32 TriggerKind, float Strength)
{
	if (Generator)
	{
		Generator->Trigger(TriggerKind, Strength);
		return;
	}
	FScopeLock Lock(&PendingLock);
	if (PendingTriggers.Num() < 16)
	{
		PendingTriggers.Add(TPair<int32, float>(TriggerKind, Strength));
	}
}

void USUProceduralAudioComponent::PushGain()
{
	float Volume = 1.f;
	if (const USUGameUserSettings* Settings = USUGameUserSettings::Get())
	{
		Volume = Settings->GetCategoryVolume(Category);
	}
	SetSoundParam(SULogic::Dsp::Param::Gain, BaseGain * Volume);
}

void USUProceduralAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bStarted)
	{
		PushGain();
	}
}
