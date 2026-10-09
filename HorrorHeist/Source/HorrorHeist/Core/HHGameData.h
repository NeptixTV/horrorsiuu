#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HHGameData.generated.h"

class UHHCharacterDefinition;
class USoundBase;
class USoundClass;
class USoundMix;
class UMaterialInterface;

/** Footstep variations for one physical surface type (SurfaceType1..62, 0 = default). */
USTRUCT(BlueprintType)
struct HORRORHEIST_API FHHFootstepSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footsteps")
	int32 SurfaceType = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footsteps")
	TArray<TSoftObjectPtr<USoundBase>> Sounds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footsteps")
	float VolumeMultiplier = 1.f;
};

/**
 * The single place that wires content into code: default character, starter items,
 * sound routing and UI sounds. Designers edit DA_GameData instead of touching C++.
 * Referenced from Project Settings > Game > Horror Heist.
 */
UCLASS(BlueprintType)
class HORRORHEIST_API UHHGameData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Convenience accessor (loaded and kept alive by UHHGameInstance). */
	static const UHHGameData* Get(const UObject* WorldContextObject);

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("HHGameData")), GetFName());
	}

	// ---- Characters --------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Characters")
	TSoftObjectPtr<UHHCharacterDefinition> DefaultCharacter;

	// ---- New profile -------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	int32 StartingCash = 1500;

	/** Item ids equipped on a fresh profile (any order, slot comes from the item). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	TArray<FName> DefaultCosmetics;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	TArray<FName> DefaultEquipment;

	// ---- Audio routing (volume sliders drive these classes) ----------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Classes")
	TSoftObjectPtr<USoundMix> SettingsMix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Classes")
	TSoftObjectPtr<USoundClass> MusicClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Classes")
	TSoftObjectPtr<USoundClass> SfxClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Classes")
	TSoftObjectPtr<USoundClass> AmbientClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Classes")
	TSoftObjectPtr<USoundClass> DialogueClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Classes")
	TSoftObjectPtr<USoundClass> UiClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Classes")
	TSoftObjectPtr<USoundClass> VoiceChatClass;

	// ---- Music -------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Music")
	TSoftObjectPtr<USoundBase> LobbyMusic;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Music")
	TSoftObjectPtr<USoundBase> TitleSting;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Music")
	TSoftObjectPtr<USoundBase> DepartureSting;

	// ---- UI sounds ---------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiHover;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiClick;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiBack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiConfirm;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiError;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiOpenPanel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiEquip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiPurchase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiReady;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiUnready;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiCountdownTick;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiNotify;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|UI")
	TSoftObjectPtr<USoundBase> UiLevelUp;

	// ---- Character sounds --------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Character")
	TSoftObjectPtr<USoundBase> FlashlightOn;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Character")
	TSoftObjectPtr<USoundBase> FlashlightOff;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Character")
	TArray<TSoftObjectPtr<USoundBase>> LandingSounds;

	/** Used when no set matches the surface under the feet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Character")
	TArray<TSoftObjectPtr<USoundBase>> DefaultFootsteps;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Character")
	TArray<FHHFootstepSet> FootstepSets;

	// ---- Presentation ------------------------------------------------------------------

	/** Overlay material put on interactables while they are focused (fresnel rim). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UMaterialInterface> InteractionHighlight;
};
