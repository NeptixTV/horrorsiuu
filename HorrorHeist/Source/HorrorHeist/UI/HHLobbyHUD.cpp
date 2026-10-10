#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "UI/Widgets/SHHLobbyRoot.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/Widgets/SHHModals.h"
#include "Interaction/HHStationActor.h"
#include "Interaction/HHPreviewStation.h"
#include "Lobby/HHLobbyGameState.h"
#include "Player/HHPlayerController.h"
#include "Player/HHCharacter.h"
#include "Player/HHInteractionComponent.h"
#include "Core/HHGameInstance.h"
#include "Core/HHGameData.h"
#include "Progression/HHProfileSubsystem.h"
#include "Audio/HHAudioSubsystem.h"
#include "Online/HHSessionSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "HHLobbyHUD"

AHHLobbyHUD::AHHLobbyHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowHUD = true;
}

AHHPlayerController* AHHLobbyHUD::GetHHController() const
{
	return Cast<AHHPlayerController>(PlayerOwner);
}

AHHLobbyGameState* AHHLobbyHUD::GetLobbyState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AHHLobbyGameState>() : nullptr;
}

UHHInteractionComponent* AHHLobbyHUD::GetInteraction() const
{
	const AHHCharacter* Character = PlayerOwner ? Cast<AHHCharacter>(PlayerOwner->GetPawn()) : nullptr;
	return Character ? Character->GetInteraction() : nullptr;
}

AHHPreviewStation* AHHLobbyHUD::GetPreviewStation() const
{
	return Cast<AHHPreviewStation>(ActiveStation.Get());
}

TSharedPtr<SWidget> AHHLobbyHUD::GetFocusWidget() const
{
	return Root;
}

void AHHLobbyHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerOwner || !PlayerOwner->IsLocalController() || IsRunningDedicatedServer())
	{
		return;
	}

	FHHStyle::Initialize();
	HHUI::SetContext(this);

	SAssignNew(Root, SHHLobbyRoot).HUD(this);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->AddViewportWidgetForPlayer(PlayerOwner->GetLocalPlayer(), Root.ToSharedRef(), 10);
	}

	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		LevelUpHandle = Profile->OnLevelUp.AddUObject(this, &AHHLobbyHUD::HandleLevelUp);
	}
	if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
	{
		CaptionHandle = Audio->OnCaption.AddUObject(this, &AHHLobbyHUD::HandleCaption);
	}
	if (UHHSessionSubsystem* Sessions = UHHSessionSubsystem::Get(this))
	{
		SessionHandle = Sessions->OnSessionStatus.AddUObject(this, &AHHLobbyHUD::HandleSessionStatus);
	}
	TryBindGameState();

	Root->FadeFromBlack(2.5f);

	// First launch of the application: title card. Afterwards (returning from a job,
	// hosting, joining) go straight to the menu.
	UHHGameInstance* GameInstance = GetGameInstance<UHHGameInstance>();
	if (GameInstance && !GameInstance->HasShownTitle())
	{
		GameInstance->MarkTitleShown();
		bTitleShowing = true;
		Root->SetTitleVisible(true);
		OpenScreen(EHHLobbyScreen::MainMenu);
		if (const UHHGameData* GameData = UHHGameData::Get(this))
		{
			if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
			{
				Audio->PlaySting(GameData->TitleSting.LoadSynchronous());
			}
		}
	}
	else
	{
		OpenScreen(EHHLobbyScreen::MainMenu);
	}

	FText Title;
	FText Body;
	if (GameInstance && GameInstance->ConsumePendingMessage(Title, Body))
	{
		ShowMessage(Title, Body);
	}
}

void AHHLobbyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		Profile->OnLevelUp.Remove(LevelUpHandle);
	}
	if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
	{
		Audio->OnCaption.Remove(CaptionHandle);
	}
	if (UHHSessionSubsystem* Sessions = UHHSessionSubsystem::Get(this))
	{
		Sessions->OnSessionStatus.Remove(SessionHandle);
	}
	if (AHHLobbyGameState* State = BoundGameState.Get())
	{
		State->OnCrewMessage.Remove(CrewMessageHandle);
		State->OnPhaseChanged.Remove(PhaseHandle);
		State->OnMissionChanged.Remove(MissionHandle);
	}
	if (AHHStationActor* Station = ActiveStation.Get())
	{
		Station->OnViewDeactivated(PlayerOwner);
	}

	if (Root.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			if (PlayerOwner && PlayerOwner->GetLocalPlayer())
			{
				Viewport->RemoveViewportWidgetForPlayer(PlayerOwner->GetLocalPlayer(), Root.ToSharedRef());
			}
		}
		Root.Reset();
	}
	if (HHUI::GetContext() == this)
	{
		HHUI::SetContext(nullptr);
	}
	Super::EndPlay(EndPlayReason);
}

void AHHLobbyHUD::TryBindGameState()
{
	AHHLobbyGameState* State = GetLobbyState();
	if (!State || BoundGameState.Get() == State)
	{
		return;
	}
	BoundGameState = State;
	CrewMessageHandle = State->OnCrewMessage.AddUObject(this, &AHHLobbyHUD::HandleCrewMessage);
	PhaseHandle = State->OnPhaseChanged.AddUObject(this, &AHHLobbyHUD::HandlePhaseChanged);
	MissionHandle = State->OnMissionChanged.AddUObject(this, &AHHLobbyHUD::HandleMissionChanged);
}

// ---------------------------------------------------------------------------------------
// Screens and the physical camera

void AHHLobbyHUD::OpenScreen(EHHLobbyScreen NewScreen, AHHStationActor* Station)
{
	if (!Root.IsValid() || !PlayerOwner)
	{
		return;
	}
	if (bTitleShowing && NewScreen != EHHLobbyScreen::MainMenu)
	{
		return;
	}

	// Pick the station that frames this screen. Settings keeps whatever we are looking at.
	AHHStationActor* NewStation = Station;
	if (!NewStation && NewScreen != EHHLobbyScreen::None)
	{
		NewStation = AHHStationActor::FindForScreen(GetWorld(), NewScreen);
		if (!NewStation && NewScreen == EHHLobbyScreen::Settings)
		{
			NewStation = ActiveStation.IsValid() ? ActiveStation.Get() : AHHStationActor::FindForScreen(GetWorld(), EHHLobbyScreen::MainMenu);
		}
	}

	if (ActiveStation.Get() != NewStation)
	{
		if (AHHStationActor* Old = ActiveStation.Get())
		{
			Old->OnViewDeactivated(PlayerOwner);
		}
		ActiveStation = NewStation;
		if (NewStation)
		{
			NewStation->OnViewActivated(PlayerOwner);
		}
	}

	const bool bWasOpen = IsMenuOpen();
	const EHHLobbyScreen Previous = Screen;
	Screen = NewScreen;

	if (NewScreen == EHHLobbyScreen::None)
	{
		DesiredViewTarget = PlayerOwner->GetPawn();
	}
	else
	{
		DesiredViewTarget = NewStation ? static_cast<AActor*>(NewStation) : static_cast<AActor*>(PlayerOwner->GetPawn());
	}

	if (AHHPlayerController* PC = GetHHController())
	{
		PC->SetMenuInputMode(IsMenuOpen());
	}
	if (UHHInteractionComponent* Interaction = GetInteraction())
	{
		Interaction->SetSuspended(IsMenuOpen());
	}

	Root->SetScreen(NewScreen);
	ApplyViewTarget(bWasOpen || NewScreen != EHHLobbyScreen::None ? 1.1f : 0.6f);

	if (IsMenuOpen())
	{
		FSlateApplication::Get().SetKeyboardFocus(Root, EFocusCause::SetDirectly);
	}
	if (Previous != NewScreen && NewScreen != EHHLobbyScreen::None && NewScreen != EHHLobbyScreen::MainMenu)
	{
		HHUI::PlayUISound(EHHUISound::OpenPanel);
	}
}

void AHHLobbyHUD::Back()
{
	if (bTitleShowing)
	{
		DismissTitle();
		return;
	}
	if (Screen == EHHLobbyScreen::MainMenu)
	{
		CloseMenu();
	}
	else if (Screen != EHHLobbyScreen::None)
	{
		OpenScreen(EHHLobbyScreen::MainMenu);
	}
}

void AHHLobbyHUD::CloseMenu()
{
	OpenScreen(EHHLobbyScreen::None);
}

void AHHLobbyHUD::ApplyViewTarget(float BlendTime)
{
	AActor* Target = DesiredViewTarget.Get();
	if (!PlayerOwner || !Target)
	{
		return;
	}
	const APlayerCameraManager* Camera = PlayerOwner->PlayerCameraManager;
	const bool bAlready = Camera && (Camera->GetViewTarget() == Target || Camera->PendingViewTarget.Target == Target);
	if (!bAlready)
	{
		PlayerOwner->SetViewTargetWithBlend(Target, BlendTime, VTBlend_Cubic, 0.f, false);
	}
}

void AHHLobbyHUD::DismissTitle()
{
	if (!bTitleShowing)
	{
		return;
	}
	bTitleShowing = false;
	if (Root.IsValid())
	{
		Root->SetTitleVisible(false);
	}
	HHUI::PlayUISound(EHHUISound::Confirm);

	const UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this);
	if (Profile && Profile->NeedsName())
	{
		ShowNameEntry(true);
	}
}

// ---------------------------------------------------------------------------------------
// Tick: keep the camera where the UI says it should be, countdown ticks.

void AHHLobbyHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Root.IsValid())
	{
		return;
	}

	TryBindGameState();

	// Possession / restarts reset the view target to the pawn; re-assert ours.
	ViewTargetRetry -= DeltaSeconds;
	if (ViewTargetRetry <= 0.f)
	{
		ViewTargetRetry = 0.25f;
		if (Screen == EHHLobbyScreen::None)
		{
			DesiredViewTarget = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
		}
		ApplyViewTarget(0.6f);
	}

	if (const AHHLobbyGameState* State = BoundGameState.Get())
	{
		if (State->GetPhase() == EHHLobbyPhase::Countdown)
		{
			const int32 Second = FMath::CeilToInt(State->GetCountdownRemaining());
			if (Second != LastCountdownSecond && Second > 0)
			{
				LastCountdownSecond = Second;
				HHUI::PlayUISound(EHHUISound::CountdownTick);
			}
		}
		else
		{
			LastCountdownSecond = -1;
		}
	}
}

// ---------------------------------------------------------------------------------------
// Feedback

void AHHLobbyHUD::ShowNotification(const FText& Message, EHHNotifyType Type)
{
	if (Root.IsValid())
	{
		Root->PushToast(Message, Type);
		HHUI::PlayUISound(Type == EHHNotifyType::Error ? EHHUISound::Error : EHHUISound::Notify);
	}
}

void AHHLobbyHUD::ShowMessage(const FText& Title, const FText& Body)
{
	if (!Root.IsValid())
	{
		return;
	}
	TWeakPtr<SHHLobbyRoot> WeakRoot = Root;
	const FSimpleDelegate Close = FSimpleDelegate::CreateLambda([WeakRoot]() { if (TSharedPtr<SHHLobbyRoot> Pinned = WeakRoot.Pin()) { Pinned->CloseModal(); } });
	Root->ShowModal(SNew(SHHMessageModal)
		.Title(Title)
		.Body(Body)
		.OnClose(Close), Close);
}

void AHHLobbyHUD::ShowConfirm(const FText& Title, const FText& Body, const FText& ConfirmLabel, TFunction<void()> OnConfirm)
{
	if (!Root.IsValid())
	{
		return;
	}
	TWeakPtr<SHHLobbyRoot> WeakRoot = Root;
	Root->ShowModal(SNew(SHHConfirmModal)
		.Title(Title)
		.Body(Body)
		.ConfirmLabel(ConfirmLabel)
		.OnConfirm_Lambda([WeakRoot, OnConfirm]()
		{
			if (TSharedPtr<SHHLobbyRoot> Pinned = WeakRoot.Pin())
			{
				Pinned->CloseModal();
			}
			if (OnConfirm)
			{
				OnConfirm();
			}
		})
		.OnCancel(FSimpleDelegate::CreateLambda([WeakRoot]() { if (TSharedPtr<SHHLobbyRoot> Pinned = WeakRoot.Pin()) { Pinned->CloseModal(); } })),
		FSimpleDelegate::CreateLambda([WeakRoot]() { if (TSharedPtr<SHHLobbyRoot> Pinned = WeakRoot.Pin()) { Pinned->CloseModal(); } }));
}

void AHHLobbyHUD::ShowNameEntry(bool bFirstTime)
{
	if (!Root.IsValid())
	{
		return;
	}
	UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this);
	const FString Current = Profile && !Profile->NeedsName() ? Profile->GetPlayerName() : UHHProfileSubsystem::SuggestName();

	TWeakPtr<SHHLobbyRoot> WeakRoot = Root;
	TWeakObjectPtr<AHHLobbyHUD> WeakThis(this);
	// The very first time an alias is required, so the dialog cannot be dismissed.
	const FSimpleDelegate Cancel = FSimpleDelegate::CreateLambda([WeakRoot]() { if (TSharedPtr<SHHLobbyRoot> Pinned = WeakRoot.Pin()) { Pinned->CloseModal(); } });
	Root->ShowModal(SNew(SHHTextEntryModal)
		.Title(bFirstTime ? LOCTEXT("NameTitleFirst", "WHAT DO THEY CALL YOU?") : LOCTEXT("NameTitle", "CHANGE ALIAS"))
		.Body(LOCTEXT("NameBody", "No real names down here. Pick an alias the crew will know you by."))
		.InitialText(FText::FromString(Current))
		.MaxLength(20)
		.ConfirmLabel(LOCTEXT("NameConfirm", "THAT'S ME"))
		.OnSubmit_Lambda([WeakRoot, WeakThis](const FText& Text)
		{
			if (AHHLobbyHUD* HUD = WeakThis.Get())
			{
				if (UHHProfileSubsystem* ProfileSubsystem = UHHProfileSubsystem::Get(HUD))
				{
					ProfileSubsystem->SetPlayerName(Text.ToString());
				}
			}
			if (TSharedPtr<SHHLobbyRoot> Pinned = WeakRoot.Pin())
			{
				Pinned->CloseModal();
			}
		})
		.CanCancel(!bFirstTime)
		.OnCancel(Cancel), bFirstTime ? FSimpleDelegate() : Cancel);
}

void AHHLobbyHUD::ShowAddressEntry()
{
	if (!Root.IsValid())
	{
		return;
	}
	TWeakPtr<SHHLobbyRoot> WeakRoot = Root;
	TWeakObjectPtr<AHHLobbyHUD> WeakThis(this);
	Root->ShowModal(SNew(SHHTextEntryModal)
		.Title(LOCTEXT("IpTitle", "JOIN BY ADDRESS"))
		.Body(LOCTEXT("IpBody", "Enter the host's IP address (port 7777 must be reachable)."))
		.InitialText(FText::FromString(TEXT("127.0.0.1")))
		.MaxLength(64)
		.ConfirmLabel(LOCTEXT("IpConfirm", "CONNECT"))
		.OnSubmit_Lambda([WeakRoot, WeakThis](const FText& Text)
		{
			if (TSharedPtr<SHHLobbyRoot> Pinned = WeakRoot.Pin())
			{
				Pinned->CloseModal();
			}
			if (AHHLobbyHUD* HUD = WeakThis.Get())
			{
				if (UHHSessionSubsystem* Sessions = UHHSessionSubsystem::Get(HUD))
				{
					Sessions->JoinByAddress(Text.ToString());
				}
			}
		})
		.OnCancel(FSimpleDelegate::CreateLambda([WeakRoot]() { if (TSharedPtr<SHHLobbyRoot> Pinned = WeakRoot.Pin()) { Pinned->CloseModal(); } })),
		FSimpleDelegate::CreateLambda([WeakRoot]() { if (TSharedPtr<SHHLobbyRoot> Pinned = WeakRoot.Pin()) { Pinned->CloseModal(); } }));
}

void AHHLobbyHUD::RequestQuit()
{
	TWeakObjectPtr<AHHLobbyHUD> WeakThis(this);
	ShowConfirm(
		LOCTEXT("QuitTitle", "LEAVE HOLLOWMERE?"),
		LOCTEXT("QuitBody", "Your profile and settings are saved. The house will still be there tomorrow night."),
		LOCTEXT("QuitConfirm", "QUIT GAME"),
		[WeakThis]()
		{
			if (AHHLobbyHUD* HUD = WeakThis.Get())
			{
				UKismetSystemLibrary::QuitGame(HUD, HUD->PlayerOwner, EQuitPreference::Quit, false);
			}
		});
}

void AHHLobbyHUD::HandleCrewMessage(const FText& Message, EHHNotifyType Type)
{
	ShowNotification(Message, Type);
}

void AHHLobbyHUD::HandlePhaseChanged()
{
	const AHHLobbyGameState* State = BoundGameState.Get();
	if (!State || !Root.IsValid())
	{
		return;
	}
	if (State->GetPhase() == EHHLobbyPhase::Departing)
	{
		if (const UHHGameData* GameData = UHHGameData::Get(this))
		{
			if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
			{
				Audio->StopMusic(1.5f);
				Audio->PlaySting(GameData->DepartureSting.LoadSynchronous());
			}
		}
		Root->FadeToBlack(2.f);
	}
}

void AHHLobbyHUD::HandleMissionChanged()
{
	// Panels poll the game state; nothing to rebuild here.
}

void AHHLobbyHUD::HandleLevelUp(int32 NewLevel)
{
	if (Root.IsValid())
	{
		Root->PushToast(FText::Format(LOCTEXT("LevelUp", "Reputation grows. You are now level {0}."), FText::AsNumber(NewLevel)), EHHNotifyType::Success);
		HHUI::PlayUISound(EHHUISound::LevelUp);
	}
}

void AHHLobbyHUD::HandleCaption(const FText& Text, float Duration, bool bIsSoundCaption)
{
	if (Root.IsValid())
	{
		Root->ShowCaption(Text, Duration, bIsSoundCaption);
	}
}

void AHHLobbyHUD::HandleSessionStatus(bool bSuccess, const FText& Message)
{
	if (!Message.IsEmpty())
	{
		ShowNotification(Message, bSuccess ? EHHNotifyType::Info : EHHNotifyType::Error);
	}
	if (bSuccess && Root.IsValid())
	{
		Root->FadeToBlack(0.6f);
	}
}

#undef LOCTEXT_NAMESPACE
