#include "Audio/HHAudioSubsystem.h"
#include "Core/HHGameData.h"
#include "Core/HHGameInstance.h"
#include "Settings/HHGameUserSettings.h"
#include "HorrorHeist.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectGlobals.h"

UHHAudioSubsystem* UHHAudioSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UHHAudioSubsystem>() : nullptr;
}

void UHHAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SettingsHandle = UHHGameUserSettings::OnSettingsApplied().AddUObject(this, &UHHAudioSubsystem::ApplyVolumes);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UHHAudioSubsystem::HandlePostLoadMap);
}

void UHHAudioSubsystem::Deinitialize()
{
	UHHGameUserSettings::OnSettingsApplied().Remove(SettingsHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Deinitialize();
}

UWorld* UHHAudioSubsystem::GetAudioWorld() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

void UHHAudioSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// Map travel flushes the audio device (mix modifiers and class overrides included):
	// push the settings mix again for the new world.
	if (LoadedWorld && LoadedWorld == GetAudioWorld())
	{
		MixPushedWorld.Reset();
		bLoggedRouting = false;
		ApplyVolumes();
	}
}

namespace HHAudio
{
	static const TCHAR* ClassFolder = TEXT("/Game/HorrorHeist/Audio/Classes/");
}

USoundClass* UHHAudioSubsystem::ResolveClass(const TSoftObjectPtr<USoundClass>& FromGameData, const TCHAR* AssetName)
{
	USoundClass* SoundClass = FromGameData.IsNull() ? nullptr : FromGameData.LoadSynchronous();
	if (!SoundClass)
	{
		// DA_GameData not wired (older content build): use the asset the content builder creates.
		const FString Path = FString::Printf(TEXT("%s%s.%s"), HHAudio::ClassFolder, AssetName, AssetName);
		SoundClass = LoadObject<USoundClass>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (SoundClass)
	{
		ResolvedClasses.AddUnique(SoundClass);
	}
	return SoundClass;
}

USoundMix* UHHAudioSubsystem::ResolveMix()
{
	if (SettingsMix)
	{
		return SettingsMix;
	}

	const UHHGameData* GameData = UHHGameData::Get(GetAudioWorld());
	if (GameData && !GameData->SettingsMix.IsNull())
	{
		SettingsMix = GameData->SettingsMix.LoadSynchronous();
	}
	if (!SettingsMix)
	{
		const FString Path = FString::Printf(TEXT("%sSM_HH_Settings.SM_HH_Settings"), HHAudio::ClassFolder);
		SettingsMix = LoadObject<USoundMix>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!SettingsMix)
	{
		// Last resort: a mix that only exists at runtime. Class overrides add their adjusters to it.
		UE_LOG(LogHorrorHeist, Warning, TEXT("Audio: SM_HH_Settings not found, using a transient settings mix. Run 'The Quiet Job > Build / Rebuild All Content'."));
		SettingsMix = NewObject<USoundMix>(this, TEXT("SM_HH_Settings_Runtime"), RF_Transient);
		SettingsMix->Duration = -1.f;
		SettingsMix->FadeInTime = 0.f;
	}
	return SettingsMix;
}

void UHHAudioSubsystem::ApplyVolumes()
{
	UWorld* World = GetAudioWorld();
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (!World || !Settings || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	USoundMix* Mix = ResolveMix();
	if (!Mix)
	{
		return;
	}

	const UHHGameData* GameData = UHHGameData::Get(World);
	static const TSoftObjectPtr<USoundClass> NoClass;

	// Sliders are 0..1. Master scales every category; categories are independent of each other.
	const float Master = FMath::Clamp(Settings->MasterVolume, 0.f, 1.f);
	struct FRoute { const TSoftObjectPtr<USoundClass>& Ptr; const TCHAR* Name; float Volume; };
	const FRoute Routes[] =
	{
		{ GameData ? GameData->MusicClass : NoClass,		TEXT("SC_Music"),		Settings->MusicVolume },
		{ GameData ? GameData->SfxClass : NoClass,			TEXT("SC_SFX"),			Settings->SfxVolume },
		{ GameData ? GameData->AmbientClass : NoClass,		TEXT("SC_Ambient"),		Settings->AmbientVolume },
		{ GameData ? GameData->DialogueClass : NoClass,		TEXT("SC_Dialogue"),	Settings->DialogueVolume },
		{ GameData ? GameData->UiClass : NoClass,			TEXT("SC_UI"),			Settings->UiVolume },
		{ GameData ? GameData->VoiceChatClass : NoClass,	TEXT("SC_VoiceChat"),	Settings->bVoiceChatEnabled ? Settings->VoiceChatVolume : 0.f },
	};

	TArray<FString> Missing;
	for (const FRoute& Route : Routes)
	{
		if (USoundClass* SoundClass = ResolveClass(Route.Ptr, Route.Name))
		{
			// Fade 0 = immediate, so dragging a slider is audible right away. Children follow
			// (e.g. a designer-made SC_Music_Radio under SC_Music).
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, SoundClass, Master * FMath::Clamp(Route.Volume, 0.f, 1.f), 1.f, 0.f, true);
		}
		else
		{
			Missing.Add(Route.Name);
		}
	}

	if (MixPushedWorld.Get() != World)
	{
		UGameplayStatics::PushSoundMixModifier(World, Mix);
		MixPushedWorld = World;
	}

	if (!bLoggedRouting)
	{
		bLoggedRouting = true;
		UE_LOG(LogHorrorHeist, Log, TEXT("Audio routing: %s"), *DescribeRouting());
		if (Missing.Num() > 0)
		{
			UE_LOG(LogHorrorHeist, Warning, TEXT("Audio: sound classes missing (%s). Their volume slider has no effect until the content is rebuilt."), *FString::Join(Missing, TEXT(", ")));
		}
	}
}

FString UHHAudioSubsystem::DescribeRouting() const
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	FString Out = FString::Printf(TEXT("mix=%s"), SettingsMix ? *SettingsMix->GetName() : TEXT("<none>"));
	for (const TObjectPtr<USoundClass>& SoundClass : ResolvedClasses)
	{
		Out += FString::Printf(TEXT(", %s"), SoundClass ? *SoundClass->GetName() : TEXT("<null>"));
	}
	if (Settings)
	{
		Out += FString::Printf(TEXT(" | master %.2f music %.2f sfx %.2f ambient %.2f dialogue %.2f ui %.2f voice %.2f"),
			Settings->MasterVolume, Settings->MusicVolume, Settings->SfxVolume, Settings->AmbientVolume,
			Settings->DialogueVolume, Settings->UiVolume, Settings->VoiceChatVolume);
	}
	return Out;
}

USoundBase* UHHAudioSubsystem::ResolveUISound(EHHUISound Sound)
{
	const UHHGameData* GameData = UHHGameData::Get(GetAudioWorld());
	if (!GameData)
	{
		return nullptr;
	}

	const TSoftObjectPtr<USoundBase>* Ptr = nullptr;
	switch (Sound)
	{
	case EHHUISound::Hover:			Ptr = &GameData->UiHover; break;
	case EHHUISound::Click:			Ptr = &GameData->UiClick; break;
	case EHHUISound::Back:			Ptr = &GameData->UiBack; break;
	case EHHUISound::Confirm:		Ptr = &GameData->UiConfirm; break;
	case EHHUISound::Error:			Ptr = &GameData->UiError; break;
	case EHHUISound::OpenPanel:		Ptr = &GameData->UiOpenPanel; break;
	case EHHUISound::Equip:			Ptr = &GameData->UiEquip; break;
	case EHHUISound::Purchase:		Ptr = &GameData->UiPurchase; break;
	case EHHUISound::Ready:			Ptr = &GameData->UiReady; break;
	case EHHUISound::Unready:		Ptr = &GameData->UiUnready; break;
	case EHHUISound::CountdownTick:	Ptr = &GameData->UiCountdownTick; break;
	case EHHUISound::Notify:		Ptr = &GameData->UiNotify; break;
	case EHHUISound::LevelUp:		Ptr = &GameData->UiLevelUp; break;
	case EHHUISound::InventorySelect:	Ptr = &GameData->UiInventorySelect; break;
	case EHHUISound::ObjectiveComplete:	Ptr = &GameData->UiObjectiveComplete; break;
	default: break;
	}

	USoundBase* Loaded = Ptr ? Ptr->LoadSynchronous() : nullptr;
	if (Loaded)
	{
		LoadedSounds.AddUnique(Loaded);
	}
	return Loaded;
}

void UHHAudioSubsystem::PlayUI(EHHUISound Sound)
{
	UWorld* World = GetAudioWorld();
	if (!World)
	{
		return;
	}

	if (Sound == EHHUISound::Hover)
	{
		// Sweeping the mouse over a list should not machine-gun the hover tick.
		const double Now = FPlatformTime::Seconds();
		if (Now - LastHoverTime < 0.045)
		{
			return;
		}
		LastHoverTime = Now;
	}

	if (USoundBase* SoundAsset = ResolveUISound(Sound))
	{
		const float Pitch = Sound == EHHUISound::Hover ? FMath::FRandRange(0.97f, 1.03f) : 1.f;
		UGameplayStatics::PlaySound2D(World, SoundAsset, 1.f, Pitch);
	}
}

void UHHAudioSubsystem::PlayMusic(USoundBase* Music, float FadeInSeconds)
{
	UWorld* World = GetAudioWorld();
	if (!World || !Music)
	{
		return;
	}

	if (UAudioComponent* Existing = MusicComponent.Get())
	{
		if (Existing->Sound == Music && Existing->IsPlaying())
		{
			return;
		}
		Existing->FadeOut(1.5f, 0.f);
	}

	UAudioComponent* Component = UGameplayStatics::CreateSound2D(World, Music, 1.f, 1.f, 0.f, nullptr, false, true);
	if (Component)
	{
		Component->bIsUISound = true;
		Component->FadeIn(FadeInSeconds, 1.f);
		MusicComponent = Component;
	}
}

void UHHAudioSubsystem::StopMusic(float FadeOutSeconds)
{
	if (UAudioComponent* Existing = MusicComponent.Get())
	{
		Existing->FadeOut(FadeOutSeconds, 0.f);
	}
	MusicComponent.Reset();
}

bool UHHAudioSubsystem::IsMusicPlaying() const
{
	const UAudioComponent* Existing = MusicComponent.Get();
	return Existing && Existing->IsPlaying();
}

void UHHAudioSubsystem::PlayLobbyMusic()
{
	if (const UHHGameData* GameData = UHHGameData::Get(GetAudioWorld()))
	{
		if (USoundBase* Music = GameData->LobbyMusic.LoadSynchronous())
		{
			LoadedSounds.AddUnique(Music);
			PlayMusic(Music, 6.f);
		}
	}
}

void UHHAudioSubsystem::PlaySting(USoundBase* Sting)
{
	if (UWorld* World = GetAudioWorld())
	{
		if (Sting)
		{
			UGameplayStatics::PlaySound2D(World, Sting);
		}
	}
}

void UHHAudioSubsystem::ShowCaption(const FText& Text, float Duration, bool bIsSoundCaption)
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (Settings)
	{
		if (bIsSoundCaption && !Settings->bClosedCaptions)
		{
			return;
		}
		if (!bIsSoundCaption && !Settings->bSubtitles)
		{
			return;
		}
	}
	OnCaption.Broadcast(Text, Duration, bIsSoundCaption);
}
