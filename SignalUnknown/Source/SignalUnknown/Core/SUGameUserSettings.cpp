#include "Core/SUGameUserSettings.h"
#include "Engine/Engine.h"

USUGameUserSettings::USUGameUserSettings()
{
}

USUGameUserSettings* USUGameUserSettings::Get()
{
	return GEngine ? Cast<USUGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void USUGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	FieldOfView = 90.f;
	bMotionBlur = false;
	bFilmGrain = true;
	bHeadBob = true;
	bShowCompass = true;
	bRearMirror = true;
	MouseSensitivity = 1.f;
	bInvertY = false;
	bToggleCrouch = true;
	MasterVolume = 1.f;
	AmbienceVolume = 0.8f;
	EffectsVolume = 1.f;
	RadioVolume = 0.8f;
	VoiceVolume = 1.f;
	RadioFilterStrength = 1.f;
	bVoiceChatEnabled = true;
	bPushToTalk = true;
	bSubtitles = true;
	SubtitleScale = 1.f;
	HorrorIntensity = 1.f;
}

void USUGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	FieldOfView = FMath::Clamp(FieldOfView, 70.f, 110.f);
	MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 4.f);
	HorrorIntensity = FMath::Clamp(HorrorIntensity, 0.f, 2.f);
	SubtitleScale = FMath::Clamp(SubtitleScale, 0.75f, 2.f);
	Super::ApplySettings(bCheckForCommandLineOverrides);
	// FOV, motion blur and film grain are read every frame by the character / sky system.
}

float USUGameUserSettings::GetCategoryVolume(int32 Category) const
{
	float V = 1.f;
	switch (Category)
	{
	case SUAudioCategory::Ambience: V = AmbienceVolume; break;
	case SUAudioCategory::Effects: V = EffectsVolume; break;
	case SUAudioCategory::Radio: V = RadioVolume; break;
	case SUAudioCategory::Voice: V = VoiceVolume; break;
	default: break;
	}
	return FMath::Clamp(V, 0.f, 1.f) * FMath::Clamp(MasterVolume, 0.f, 1.f);
}
