// SIGNAL UNKNOWN – player settings (graphics via UGameUserSettings + own gameplay/audio options).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "SUGameUserSettings.generated.h"

UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class SIGNALUNKNOWN_API USUGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	USUGameUserSettings();

	static USUGameUserSettings* Get();

	virtual void SetToDefaults() override;
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;

	// ---- view
	UPROPERTY(Config) float FieldOfView = 90.f;
	UPROPERTY(Config) bool bMotionBlur = false;
	UPROPERTY(Config) bool bFilmGrain = true;
	UPROPERTY(Config) bool bHeadBob = true;
	UPROPERTY(Config) bool bShowCompass = true;
	UPROPERTY(Config) bool bRearMirror = true;

	// ---- controls
	UPROPERTY(Config) float MouseSensitivity = 1.f;
	UPROPERTY(Config) bool bInvertY = false;
	UPROPERTY(Config) bool bToggleCrouch = true;

	// ---- audio (0..1)
	UPROPERTY(Config) float MasterVolume = 1.f;
	UPROPERTY(Config) float AmbienceVolume = 0.8f;
	UPROPERTY(Config) float EffectsVolume = 1.f;
	UPROPERTY(Config) float RadioVolume = 0.8f;
	UPROPERTY(Config) float VoiceVolume = 1.f;
	/** Strength of the radio voice filter / static (comfort setting). */
	UPROPERTY(Config) float RadioFilterStrength = 1.f;
	UPROPERTY(Config) bool bVoiceChatEnabled = true;
	UPROPERTY(Config) bool bPushToTalk = true;

	// ---- accessibility / gameplay
	UPROPERTY(Config) bool bSubtitles = true;
	UPROPERTY(Config) float SubtitleScale = 1.f;
	/** Horror director intensity 0..2 (0 = off). */
	UPROPERTY(Config) float HorrorIntensity = 1.f;

	/** Volume for a category, already multiplied with the master volume. */
	float GetCategoryVolume(int32 Category) const;
};

/** Audio categories used by USUProceduralAudioComponent. */
namespace SUAudioCategory
{
	constexpr int32 Ambience = 0;
	constexpr int32 Effects = 1;
	constexpr int32 Radio = 2;
	constexpr int32 Voice = 3;
}
