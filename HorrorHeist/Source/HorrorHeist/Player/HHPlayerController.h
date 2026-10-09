#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/HHTypes.h"
#include "Online/CoreOnline.h"
#include "HHPlayerController.generated.h"

class AHHLobbyHUD;
class AHHPlayerState;
class APostProcessVolume;

/**
 * Bridges the local player (profile, settings, UI) and the server (validated RPCs).
 * Owns the menu input mode and the local post-process volume driven by video settings.
 */
UCLASS()
class HORRORHEIST_API AHHPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHHPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	// ---- Requests from the local UI (validated on the server) -------------------------

	void RequestToggleReady();
	void RequestSelectMission(FName MissionId);
	/** Sends the current profile (name, level, loadouts) to the server. */
	void PushProfileToServer();

	bool IsCrewLeader() const;
	AHHPlayerState* GetHHPlayerState() const;
	AHHLobbyHUD* GetLobbyHUD() const;

	/** Called by the HUD when a menu opens/closes. */
	void SetMenuInputMode(bool bMenuOpen);

	/** True while this client is transmitting voice. */
	bool IsTransmittingVoice() const { return bTransmitting; }
	/** True while the given crew member is talking (from the voice interface). */
	bool IsPlayerTalking(const APlayerState* PlayerState) const;

	UFUNCTION(Client, Reliable)
	void ClientNotify(const FText& Message, EHHNotifyType Type);

	// ---- Server RPCs ------------------------------------------------------------------

	UFUNCTION(Server, Reliable)
	void ServerSetProfile(const FString& PlayerName, int32 Level, const FHHCosmeticLoadout& Cosmetics, const FHHEquipmentLoadout& Equipment);

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bNewReady);

	UFUNCTION(Server, Reliable)
	void ServerSelectMission(FName MissionId);

	// ---- Development commands (console: ~) -------------------------------------------

	UFUNCTION(Exec)
	void HHGiveCash(int32 Amount);

	UFUNCTION(Exec)
	void HHGiveXP(int32 Amount);

	UFUNCTION(Exec)
	void HHUnlockAll();

	UFUNCTION(Exec)
	void HHResetProfile();

	UFUNCTION(Exec)
	void HHForceAmbientEvent(int32 EventIndex);

protected:
	void HandleMenuAction();
	void HandleInventoryAction();
	void HandleMapAction();
	void HandleReadyAction();
	void HandlePushToTalkStarted();
	void HandlePushToTalkCompleted();

	void HandleProfileChanged();
	void HandleSettingsApplied();
	void ApplyVideoSettings();
	void ApplyVoiceSettings();
	void SetTransmitting(bool bNewTransmitting);

	void BindVoiceEvents();
	void HandleTalkingStateChanged(FUniqueNetIdRef TalkerId, bool bIsTalking);

	FHHCosmeticLoadout SanitizeCosmetics(const FHHCosmeticLoadout& In) const;
	FHHEquipmentLoadout SanitizeEquipment(const FHHEquipmentLoadout& In) const;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> SettingsVolume;

	/** Exposure bias of the level's main post process volume (brightness is relative to it). */
	float BaseExposureBias = 0.f;
	bool bHasBaseExposure = false;

	/** Server side: the first profile announces the player to the crew. */
	bool bProfileReceived = false;

	bool bTransmitting = false;
	TSet<FString> TalkingPlayers;

	FDelegateHandle ProfileHandle;
	FDelegateHandle SettingsHandle;
	FDelegateHandle VoiceHandle;
};
