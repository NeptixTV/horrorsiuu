#include "Audio/HHAudioSubsystem.h"
#include "Core/HHGameData.h"
#include "Core/HHGameInstance.h"
#include "Settings/HHGameUserSettings.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

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
}

void UHHAudioSubsystem::Deinitialize()
{
	UHHGameUserSettings::OnSettingsApplied().Remove(SettingsHandle);
	Super::Deinitialize();
}

UWorld* UHHAudioSubsystem::GetAudioWorld() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

void UHHAudioSubsystem::ApplyVolumes()
{
	UWorld* World = GetAudioWorld();
	const UHHGameData* GameData = UHHGameData::Get(World);
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (!World || !GameData || !Settings)
	{
		return;
	}

	USoundMix* Mix = GameData->SettingsMix.LoadSynchronous();
	if (!Mix)
	{
		return;
	}

	const float Master = FMath::Clamp(Settings->MasterVolume, 0.f, 1.f);
	auto Apply = [World, Mix](const TSoftObjectPtr<USoundClass>& ClassPtr, float Volume)
	{
		if (USoundClass* SoundClass = ClassPtr.LoadSynchronous())
		{
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, SoundClass, FMath::Clamp(Volume, 0.f, 1.f), 1.f, 0.f, true);
		}
	};

	Apply(GameData->MusicClass, Master * Settings->MusicVolume);
	Apply(GameData->SfxClass, Master * Settings->SfxVolume);
	Apply(GameData->AmbientClass, Master * Settings->AmbientVolume);
	Apply(GameData->DialogueClass, Master * Settings->DialogueVolume);
	Apply(GameData->UiClass, Master * Settings->UiVolume);
	Apply(GameData->VoiceChatClass, Settings->bVoiceChatEnabled ? Master * Settings->VoiceChatVolume : 0.f);

	UGameplayStatics::PushSoundMixModifier(World, Mix);
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
