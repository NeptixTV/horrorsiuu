#include "Audio/SUVoice.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/StrongObjectPtr.h"

USoundAttenuation* USUVoice::GetProximityAttenuation()
{
	static TStrongObjectPtr<USoundAttenuation> Attenuation;
	if (!Attenuation.IsValid())
	{
		USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("SU_VoiceAttenuation"));
		FSoundAttenuationSettings& S = A->Attenuation;
		S.bAttenuate = true;
		S.bSpatialize = true;
		S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		S.AttenuationShape = EAttenuationShape::Sphere;
		S.AttenuationShapeExtents = FVector(250.f, 0.f, 0.f);
		S.FalloffDistance = SpeechRange;
		S.bAttenuateWithLPF = true;
		S.LPFRadiusMin = 800.f;
		S.LPFRadiusMax = SpeechRange;
		S.bEnableOcclusion = true;
		S.OcclusionTraceChannel = ECC_Visibility;
		S.OcclusionLowPassFilterFrequency = 1200.f;
		S.OcclusionVolumeAttenuation = 0.5f;
		S.OcclusionInterpolationTime = 0.2f;
		S.bEnableReverbSend = true;
		Attenuation.Reset(A);
	}
	return Attenuation.Get();
}
