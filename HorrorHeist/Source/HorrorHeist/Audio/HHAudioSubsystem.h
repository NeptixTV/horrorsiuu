#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/HHTypes.h"
#include "HHAudioSubsystem.generated.h"

class USoundBase;
class UAudioComponent;

/** Text, duration, bIsSoundCaption (closed caption for a sound vs. spoken subtitle). */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FHHOnCaption, const FText& /*Text*/, float /*Duration*/, bool /*bIsSoundCaption*/);

/**
 * Volume routing (settings -> sound mix class overrides), lobby music, UI sounds and the
 * caption/subtitle feed used by the HUD. Sound classes come from UHHGameData.
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

	TWeakObjectPtr<UAudioComponent> MusicComponent;

	/** Keeps loaded UI sounds alive. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> LoadedSounds;

	double LastHoverTime = 0.0;
	FDelegateHandle SettingsHandle;
};
