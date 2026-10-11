#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/HHTypes.h"
#include "HHAudioSubsystem.generated.h"

class USoundBase;
class USoundClass;
class USoundMix;
class UAudioComponent;

/** Text, duration, bIsSoundCaption (closed caption for a sound vs. spoken subtitle). */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FHHOnCaption, const FText& /*Text*/, float /*Duration*/, bool /*bIsSoundCaption*/);

/**
 * Volume routing (settings -> sound mix class overrides), lobby music, UI sounds and the
 * caption/subtitle feed used by the HUD. Sound classes come from UHHGameData.
 *
 * Volume routing: every category class (Music, SFX, Ambient, Dialogue, UI, Voice) gets a class
 * override of Master x Category in one settings mix. The mix is pushed once per world and
 * re-applied after every map load (map travel flushes the audio device). If DA_GameData lacks a
 * class or the mix, the assets are loaded from their fixed content paths, and a transient mix
 * is used as the last resort, so the sliders never depend on a single asset reference.
 */
UCLASS()
class HORRORHEIST_API UHHAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UHHAudioSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Push the settings mix with the current volume sliders. Safe to call often. */
	void ApplyVolumes();

	/** Multi-line description of the current routing (classes found, volumes) for logs / debugging. */
	FString DescribeRouting() const;

	void PlayUI(EHHUISound Sound);

	void PlayMusic(USoundBase* Music, float FadeInSeconds = 3.f);
	void StopMusic(float FadeOutSeconds = 2.f);
	void PlayLobbyMusic();
	bool IsMusicPlaying() const;

	/** One-shot non-positional music cue (title, departure). */
	void PlaySting(USoundBase* Sting);

	/**
	 * Show a subtitle or a closed caption ("[Footsteps overhead]"). Filtered by the
	 * subtitle / closed caption settings before it reaches the HUD.
	 */
	void ShowCaption(const FText& Text, float Duration, bool bIsSoundCaption);

	FHHOnCaption OnCaption;

private:
	USoundBase* ResolveUISound(EHHUISound Sound);
	UWorld* GetAudioWorld() const;
	USoundMix* ResolveMix();
	USoundClass* ResolveClass(const TSoftObjectPtr<USoundClass>& FromGameData, const TCHAR* AssetName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** Mix used for the settings (asset from DA_GameData, fixed path, or a transient fallback). */
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> SettingsMix;

	/** Classes resolved for the routing, kept alive while the game runs. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundClass>> ResolvedClasses;

	/** World the mix was last pushed into (pushed once per world). */
	TWeakObjectPtr<UWorld> MixPushedWorld;
	bool bLoggedRouting = false;
	FDelegateHandle PostLoadMapHandle;

	TWeakObjectPtr<UAudioComponent> MusicComponent;

	/** Keeps loaded UI sounds alive. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> LoadedSounds;

	double LastHoverTime = 0.0;
	FDelegateHandle SettingsHandle;
};
