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
class UHHInteractionComponent;

/**
 * Owns the lobby UI (Slate) for one local player and the "physical menu": every screen is
 * tied to a station in the hideout, and switching screens blends the camera there.
 * Exploring = no screen = first-person control.
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
	void RequestQuit();

	AHHPlayerController* GetHHController() const;
	AHHLobbyGameState* GetLobbyState() const;
	AHHPreviewStation* GetPreviewStation() const;
	UHHInteractionComponent* GetInteraction() const;

	/** Title card shown once per application start. */
	void DismissTitle();
	bool IsTitleShowing() const { return bTitleShowing; }

private:
	void ApplyViewTarget(float BlendTime);
	void TryBindGameState();
	void HandleCrewMessage(const FText& Message, EHHNotifyType Type);
	void HandlePhaseChanged();
	void HandleMissionChanged();
	void HandleLevelUp(int32 NewLevel);
	void HandleCaption(const FText& Text, float Duration, bool bIsSoundCaption);
	void HandleSessionStatus(bool bSuccess, const FText& Message);

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
};
