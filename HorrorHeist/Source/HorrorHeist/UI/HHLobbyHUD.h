#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Core/HHTypes.h"
#include "HHLobbyHUD.generated.h"

class SHHLobbyRoot;
class SWidget;
class AHHStationActor;
class AHHPreviewStation;
class AHHPlayerController;
class AHHLobbyGameState;
class AHHHeistGameState;
class UHHInteractionComponent;
class UAudioComponent;

/**
 * Owns the game UI (Slate) for one local player. In the hideout it is the "physical menu":
 * every screen is tied to a station, and switching screens blends the camera there.
 * Exploring = no screen = first-person control. On a job the same HUD shows the heist layer
 * (objectives, inventory, psyche...), the main menu becomes the pause menu and it runs the
 * local dread soundscape (drone with the haunting, heartbeat at low psyche).
 */
UCLASS()
class HORRORHEIST_API AHHLobbyHUD : public AHUD
{
	GENERATED_BODY()

public:
	AHHLobbyHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Opens a screen; optionally at a specific station (else the first matching one). */
	void OpenScreen(EHHLobbyScreen NewScreen, AHHStationActor* Station = nullptr);
	/** Panel -> main menu -> exploring. */
	void Back();
	void CloseMenu();

	EHHLobbyScreen GetScreen() const { return Screen; }
	bool IsMenuOpen() const { return Screen != EHHLobbyScreen::None; }

	TSharedPtr<SWidget> GetFocusWidget() const;

	void ShowNotification(const FText& Message, EHHNotifyType Type);
	void ShowMessage(const FText& Title, const FText& Body);
	void ShowConfirm(const FText& Title, const FText& Body, const FText& ConfirmLabel, TFunction<void()> OnConfirm);
	void ShowNameEntry(bool bFirstTime);
	void ShowAddressEntry();
	/** JOIN BY CODE: asks for a six-character lobby code and searches for it. */
	void ShowCodeEntry();
	void RequestQuit();

	AHHPlayerController* GetHHController() const;
	AHHLobbyGameState* GetLobbyState() const;
	AHHHeistGameState* GetHeistState() const;
	/** True in a job map (heist rules), false in the hideout / town hub. */
	bool IsOnJob() const;

	/** Pause menu: confirm, then ask the server to call the job off (crew leader only). */
	void RequestAbandonJob();
	/** Hub: crew leader skips ahead to nightfall. */
	void RequestWaitForNight();
	/** End of a job: the summary card. */
	void ShowMissionSummary(const FHHMissionSummary& Summary);
	/** Psyche gave out / caught: black, a beat, waking up outside. */
	void PlayBlackout(bool bCaughtByEntity);
	/** Knocked down by the thing in the house: a short black flash. */
	void PlayKnockdown();

	/** 0..5 while the local player holds a noise meter on a job (0 otherwise). */
	float GetNoiseMeterReading() const { return MeterReading; }

	virtual void DrawHUD() override;
	AHHPreviewStation* GetPreviewStation() const;
	/** Shop of the store counter the menu was opened at (None = the Fence). */
	FName GetActiveShop() const;
	UHHInteractionComponent* GetInteraction() const;

	/** Title card shown once per application start. */
	void DismissTitle();
	bool IsTitleShowing() const { return bTitleShowing; }

private:
	void ApplyViewTarget(float BlendTime);
	/** The pawn, or the van it sits in. */
	AActor* GetExploreViewTarget() const;
	void TryBindGameState();
	void HandleCrewMessage(const FText& Message, EHHNotifyType Type);
	void HandlePhaseChanged();
	void HandleMissionChanged();
	void HandleLevelUp(int32 NewLevel);
	void HandleCaption(const FText& Text, float Duration, bool bIsSoundCaption);
	void HandleSessionStatus(bool bSuccess, const FText& Message);
	void UpdateJobAudio(float DeltaSeconds);
	/** Noise meter in hand: read the house (0..5) and tick faster the higher it reads. */
	void UpdateNoiseMeter(float DeltaSeconds);
	/** Walkie marks of the crew, drawn over the world. */
	void DrawPings();
	void StopJobAudio();

	TSharedPtr<SHHLobbyRoot> Root;

	EHHLobbyScreen Screen = EHHLobbyScreen::None;
	TWeakObjectPtr<AHHStationActor> ActiveStation;
	TWeakObjectPtr<AActor> DesiredViewTarget;
	TWeakObjectPtr<AHHLobbyGameState> BoundGameState;

	bool bTitleShowing = false;
	int32 LastCountdownSecond = -1;
	float ViewTargetRetry = 0.f;

	FDelegateHandle LevelUpHandle;
	FDelegateHandle CaptionHandle;
	FDelegateHandle SessionHandle;
	FDelegateHandle CrewMessageHandle;
	FDelegateHandle PhaseHandle;
	FDelegateHandle MissionHandle;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> DroneAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HeartbeatAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> NightAudio;

	FTimerHandle BlackoutTimer;
	float DroneLevel = 0.f;
	float HeartLevel = 0.f;
	bool bShowedSummary = false;

	float MeterReading = 0.f;
	float MeterTickTimer = 0.f;
};
