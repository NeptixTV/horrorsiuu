#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/HHTypes.h"
#include "Online/CoreOnline.h"
#include "HHPlayerController.generated.h"

class AHHLobbyHUD;
class AHHPlayerState;
class APostProcessVolume;
class USoundBase;

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
	/** Crew leader: where the van goes at the end of the job (AHHEscapeZone id). */
	void RequestSelectSafehouse(FName ZoneId);
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
	bool IsPlayerTalking(const APlayerState* InPlayerState) const;

	UFUNCTION(Client, Reliable)
	void ClientNotify(const FText& Message, EHHNotifyType Type);

	/** The job is over: book payout/XP in the local profile and show the summary. */
	UFUNCTION(Client, Reliable)
	void ClientMissionReward(const FHHMissionSummary& Summary);

	/** Psyche gave out / the house got you: screen goes black, you wake up outside. */
	UFUNCTION(Client, Reliable)
	void ClientBlackout(bool bCaughtByEntity);

	/** The thing in the house threw you to the floor. */
	UFUNCTION(Client, Reliable)
	void ClientKnockedDown();

	/** Something touched your shoulder: a sound only you hear and a jolt of the camera. */
	UFUNCTION(Client, Unreliable)
	void ClientHauntTouch(USoundBase* Sound);

	/** Dead on a job: watch the next crew member who is still around (local). */
	void SpectateNext();
	/** The crew member being watched (null when not spectating). */
	APawn* GetSpectated() const { return Spectated.Get(); }

	/** Crew leader: wait for nightfall (hub) / call the job off (heist). */
	void RequestWaitForNight();
	void RequestAbandonJob();

	// ---- Server RPCs ------------------------------------------------------------------

	UFUNCTION(Server, Reliable)
	void ServerSetProfile(const FString& PlayerName, int32 Level, const FHHCosmeticLoadout& Cosmetics, const FHHEquipmentLoadout& Equipment);

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bNewReady);

	UFUNCTION(Server, Reliable)
	void ServerSelectMission(FName MissionId);

	UFUNCTION(Server, Reliable)
	void ServerSelectSafehouse(FName ZoneId);

	/** A joining player adds what they scouted before to the crew's notebook. */
	UFUNCTION(Server, Reliable)
	void ServerShareIntel(const TArray<FName>& IntelIds);

	UFUNCTION(Server, Reliable)
	void ServerWaitForNight();

	UFUNCTION(Server, Reliable)
	void ServerAbandonJob();

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

	/** Set the clock, e.g. "HHSetTime 21.5" (host). */
	UFUNCTION(Exec)
	void HHSetTime(float Hours);

	/** Clock speed multiplier, e.g. "HHTimeSpeed 10" (host). */
	UFUNCTION(Exec)
	void HHTimeSpeed(float Multiplier);

	/** Make the house angrier, e.g. "HHAddAnger 30" (host, on a job). */
	UFUNCTION(Exec)
	void HHAddAnger(float Amount);

	/** Run a haunt event now (host, on a job): 0 settle .. 10 apparition, 11 hunt, 12 voice, 13 scratch,
	 *  14 steps behind, 15 touch, 16 throw, 17 mark, 18 in your face, 19 shadow run, 20 rocking chair. */
	UFUNCTION(Exec)
	void HHHauntEvent(int32 EventIndex);

	/** End the job: 1 success, 2 partial, 3 caught, 4 abandoned, 5 hot getaway, 6 boxed in (host, on a job). */
	UFUNCTION(Exec)
	void HHEndJob(int32 Outcome);

	/** Learn all intel of the pinned / current job (host). "HHLearnIntel 0" forgets the crew's notebook for this session. */
	UFUNCTION(Exec)
	void HHLearnIntel(int32 bLearn);

	/** Call the police now (host, on a job). */
	UFUNCTION(Exec)
	void HHCallPolice();

	/** Send the neighbour out to look around (host, on a job). */
	UFUNCTION(Exec)
	void HHSendNeighbour();

	/** As if the thing in the house caught you: down, and dead if you already were (host, on a job). */
	UFUNCTION(Exec)
	void HHKnockDown();
	/** The morning after at the Vance house (host, in the hideout): 0 none, 1 noticed, 2 police, 3 a death + road closed, 4 haunting. */
	UFUNCTION(Exec)
	void HHAftermath(int32 Mode);

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

	/** Dead: whose eyes we are looking through. Re-checked every tick (they may die or get in the van). */
	TWeakObjectPtr<APawn> Spectated;
	void UpdateSpectating();
	virtual void PlayerTick(float DeltaTime) override;
};
