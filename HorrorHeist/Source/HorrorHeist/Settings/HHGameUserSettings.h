#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "HHGameUserSettings.generated.h"

/** One rebindable action and the key assigned to it (stored in GameUserSettings.ini). */
USTRUCT()
struct HORRORHEIST_API FHHKeyBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName BindingId;

	UPROPERTY()
	FKey Key;
};

DECLARE_MULTICAST_DELEGATE(FHHOnSettingsApplied);

/**
 * All player settings in one place. Extends UGameUserSettings so resolution, window mode,
 * VSync, frame limit and scalability groups use the engine's own (working) implementation;
 * everything else is applied here or by the system that owns it (audio subsystem, input
 * subsystem, player camera) when OnSettingsApplied fires. Nothing in here is cosmetic-only.
 */
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class HORRORHEIST_API UHHGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UHHGameUserSettings(const FObjectInitializer& ObjectInitializer);

	static UHHGameUserSettings* Get();

	/** Broadcast after ApplySettings / ApplyNonResolutionSettings. */
	static FHHOnSettingsApplied& OnSettingsApplied();

	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

	/** Apply everything and write GameUserSettings.ini. */
	void ApplyAndSave();

	// ---- Graphics (beyond the engine's scalability groups) ----------------------------

	/** r.AntiAliasingMethod: 0 off, 1 FXAA, 2 TAA, 4 TSR. */
	UPROPERTY(Config) int32 AntiAliasingMethod;
	/** 0 = engine (TSR / native), 1 = NVIDIA DLSS, 2 = AMD FSR, 3 = Intel XeSS (only when the plugin is installed). */
	UPROPERTY(Config) int32 UpscalerMode;
	UPROPERTY(Config) bool bVolumetricFog;
	UPROPERTY(Config) bool bMotionBlur;
	UPROPERTY(Config) bool bFilmGrain;
	UPROPERTY(Config) bool bChromaticAberration;
	UPROPERTY(Config) float FieldOfView;
	/** -1..1, mapped to +-1.5 EV exposure bias on the player camera. */
	UPROPERTY(Config) float Brightness;
	/** Output gamma (engine default 2.2). */
	UPROPERTY(Config) float DisplayGamma;

	// ---- Audio ------------------------------------------------------------------------

	UPROPERTY(Config) float MasterVolume;
	UPROPERTY(Config) float MusicVolume;
	UPROPERTY(Config) float SfxVolume;
	UPROPERTY(Config) float AmbientVolume;
	UPROPERTY(Config) float DialogueVolume;
	UPROPERTY(Config) float UiVolume;
	UPROPERTY(Config) float VoiceChatVolume;
	UPROPERTY(Config) float MicrophoneGain;
	UPROPERTY(Config) bool bVoiceChatEnabled;
	UPROPERTY(Config) bool bPushToTalk;

	// ---- Controls ---------------------------------------------------------------------

	UPROPERTY(Config) float MouseSensitivity;
	UPROPERTY(Config) float ControllerSensitivity;
	UPROPERTY(Config) bool bInvertY;
	UPROPERTY(Config) bool bToggleCrouch;
	UPROPERTY(Config) bool bToggleSprint;
	UPROPERTY(Config) TArray<FHHKeyBinding> KeyBindings;

	// ---- Gameplay ---------------------------------------------------------------------

	UPROPERTY(Config) bool bShowInteractionPrompts;
	UPROPERTY(Config) bool bInteractionHighlight;
	UPROPERTY(Config) bool bHeadBob;
	UPROPERTY(Config) bool bCameraShake;
	UPROPERTY(Config) bool bSubtitles;
	UPROPERTY(Config) bool bClosedCaptions;
	/** 0 small .. 3 extra large */
	UPROPERTY(Config) int32 SubtitleSize;
	UPROPERTY(Config) float SubtitleBackgroundOpacity;
	UPROPERTY(Config) bool bShowCrewHUD;

	// ---- Accessibility ----------------------------------------------------------------

	/** 0 off, 1 deuteranope, 2 protanope, 3 tritanope */
	UPROPERTY(Config) int32 ColorBlindMode;
	/** 0..10 */
	UPROPERTY(Config) int32 ColorBlindStrength;
	UPROPERTY(Config) bool bReduceMotion;
	UPROPERTY(Config) bool bReduceFlicker;

	// ---- Helpers ----------------------------------------------------------------------

	FKey GetKeyBinding(FName BindingId, const FKey& DefaultKey) const;
	void SetKeyBinding(FName BindingId, const FKey& Key);
	void ResetKeyBindings();

	/** Head bob / sway allowed (gameplay toggle and motion reduction combined). */
	bool AllowsHeadBob() const { return bHeadBob && !bReduceMotion; }
	bool AllowsCameraShake() const { return bCameraShake && !bReduceMotion; }

	static bool IsUpscalerAvailable(int32 Mode);
	float GetSubtitleFontScale() const;

private:
	void ResetExtrasToDefaults();
	void ApplyRenderingExtras() const;
	void ApplyColorVision() const;
	void ApplyVoice() const;
};
