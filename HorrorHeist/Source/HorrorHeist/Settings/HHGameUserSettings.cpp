#include "Settings/HHGameUserSettings.h"
#include "HorrorHeist.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	IConsoleVariable* FindCVar(const TCHAR* Name)
	{
		return IConsoleManager::Get().FindConsoleVariable(Name);
	}

	void SetCVarInt(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* CVar = FindCVar(Name))
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	}

	void SetCVarFloat(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = FindCVar(Name))
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	}

	const TCHAR* UpscalerCVar(int32 Mode)
	{
		switch (Mode)
		{
		case 1: return TEXT("r.NGX.DLSS.Enable");
		case 2: return TEXT("r.FidelityFX.FSR3.Enabled");
		case 3: return TEXT("r.XeSS.Enabled");
		default: return nullptr;
		}
	}
}

UHHGameUserSettings::UHHGameUserSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ResetExtrasToDefaults();
}

UHHGameUserSettings* UHHGameUserSettings::Get()
{
	return GEngine ? Cast<UHHGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

FHHOnSettingsApplied& UHHGameUserSettings::OnSettingsApplied()
{
	static FHHOnSettingsApplied Delegate;
	return Delegate;
}

void UHHGameUserSettings::ResetExtrasToDefaults()
{
	AntiAliasingMethod = 4;
	UpscalerMode = 0;
	bVolumetricFog = true;
	bMotionBlur = false;
	bFilmGrain = true;
	bChromaticAberration = true;
	FieldOfView = 90.f;
	Brightness = 0.f;
	DisplayGamma = 2.2f;

	MasterVolume = 1.f;
	MusicVolume = 0.65f;
	SfxVolume = 1.f;
	AmbientVolume = 1.f;
	DialogueVolume = 1.f;
	UiVolume = 0.8f;
	VoiceChatVolume = 1.f;
	MicrophoneGain = 1.f;
	bVoiceChatEnabled = true;
	bPushToTalk = true;

	MouseSensitivity = 1.f;
	ControllerSensitivity = 1.f;
	bInvertY = false;
	bToggleCrouch = true;
	bToggleSprint = false;
	KeyBindings.Reset();

	bShowInteractionPrompts = true;
	bInteractionHighlight = true;
	bHeadBob = true;
	bCameraShake = true;
	bSubtitles = true;
	bClosedCaptions = false;
	SubtitleSize = 1;
	SubtitleBackgroundOpacity = 0.55f;
	bShowCrewHUD = true;

	ColorBlindMode = 0;
	ColorBlindStrength = 6;
	bReduceMotion = false;
	bReduceFlicker = false;
}

void UHHGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	ResetExtrasToDefaults();
}

void UHHGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	ApplyRenderingExtras();
	ApplyColorVision();
	ApplyVoice();

	OnSettingsApplied().Broadcast();
}

void UHHGameUserSettings::ApplyAndSave()
{
	ApplySettings(false);
	SaveSettings();
}

void UHHGameUserSettings::ApplyRenderingExtras() const
{
	SetCVarInt(TEXT("r.AntiAliasingMethod"), FMath::Clamp(AntiAliasingMethod, 0, 4) == 3 ? 4 : FMath::Clamp(AntiAliasingMethod, 0, 4));
	SetCVarInt(TEXT("r.VolumetricFog"), bVolumetricFog ? 1 : 0);

	// Only touch upscaler plugins that are actually installed.
	for (int32 Mode = 1; Mode <= 3; ++Mode)
	{
		if (const TCHAR* Name = UpscalerCVar(Mode))
		{
			if (FindCVar(Name))
			{
				SetCVarInt(Name, UpscalerMode == Mode ? 1 : 0);
			}
		}
	}

	if (GEngine)
	{
		GEngine->DisplayGamma = FMath::Clamp(DisplayGamma, 1.6f, 2.8f);
	}
}

void UHHGameUserSettings::ApplyColorVision() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	if (FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer())
	{
		const EColorVisionDeficiency Type = static_cast<EColorVisionDeficiency>(FMath::Clamp(ColorBlindMode, 0, 3));
		Renderer->SetColorVisionDeficiencyType(Type, FMath::Clamp(ColorBlindStrength, 0, 10), true, false);
	}
}

void UHHGameUserSettings::ApplyVoice() const
{
	// Voice module cvars (only present when VoIP is compiled in).
	SetCVarFloat(TEXT("voice.MicInputGain"), FMath::Clamp(MicrophoneGain, 0.f, 4.f));
}

bool UHHGameUserSettings::IsUpscalerAvailable(int32 Mode)
{
	if (Mode == 0)
	{
		return true;
	}
	const TCHAR* Name = UpscalerCVar(Mode);
	return Name && FindCVar(Name) != nullptr;
}

float UHHGameUserSettings::GetSubtitleFontScale() const
{
	switch (FMath::Clamp(SubtitleSize, 0, 3))
	{
	case 0: return 0.85f;
	case 1: return 1.f;
	case 2: return 1.25f;
	default: return 1.55f;
	}
}

FKey UHHGameUserSettings::GetKeyBinding(FName BindingId, const FKey& DefaultKey) const
{
	for (const FHHKeyBinding& Binding : KeyBindings)
	{
		if (Binding.BindingId == BindingId && Binding.Key.IsValid())
		{
			return Binding.Key;
		}
	}
	return DefaultKey;
}

void UHHGameUserSettings::SetKeyBinding(FName BindingId, const FKey& Key)
{
	for (FHHKeyBinding& Binding : KeyBindings)
	{
		if (Binding.BindingId == BindingId)
		{
			Binding.Key = Key;
			return;
		}
	}
	FHHKeyBinding& NewBinding = KeyBindings.AddDefaulted_GetRef();
	NewBinding.BindingId = BindingId;
	NewBinding.Key = Key;
}

void UHHGameUserSettings::ResetKeyBindings()
{
	KeyBindings.Reset();
}
