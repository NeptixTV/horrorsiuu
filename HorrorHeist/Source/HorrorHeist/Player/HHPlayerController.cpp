#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Lobby/HHLobbyGameMode.h"
#include "Lobby/HHLobbyGameState.h"
#include "UI/HHLobbyHUD.h"
#include "World/HHAmbienceDirector.h"
#include "Core/HHGameInstance.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHCosmeticDefinition.h"
#include "Data/HHEquipmentDefinition.h"
#include "Progression/HHProfileSubsystem.h"
#include "Settings/HHGameUserSettings.h"
#include "Settings/HHInputSubsystem.h"
#include "Audio/HHAudioSubsystem.h"
#include "Online/HHSessionSubsystem.h"
#include "HorrorHeist.h"
#include "EnhancedInputComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interfaces/VoiceInterface.h"
#include "OnlineSubsystemUtils.h"

#define LOCTEXT_NAMESPACE "HHPlayerController"

AHHPlayerController::AHHPlayerController()
{
	bShowMouseCursor = false;
}

AHHPlayerState* AHHPlayerController::GetHHPlayerState() const
{
	return GetPlayerState<AHHPlayerState>();
}

AHHLobbyHUD* AHHPlayerController::GetLobbyHUD() const
{
	return Cast<AHHLobbyHUD>(GetHUD());
}

bool AHHPlayerController::IsCrewLeader() const
{
	const AHHPlayerState* State = GetHHPlayerState();
	return State && State->IsCrewLeader();
}

void AHHPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (UHHInputSubsystem* Input = UHHInputSubsystem::Get(this))
	{
		Input->ApplyMappings();
	}

	if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
	{
		Audio->ApplyVolumes();
		Audio->PlayLobbyMusic();
	}

	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		ProfileHandle = Profile->OnProfileChanged.AddUObject(this, &AHHPlayerController::HandleProfileChanged);
	}
	SettingsHandle = UHHGameUserSettings::OnSettingsApplied().AddUObject(this, &AHHPlayerController::HandleSettingsApplied);

	// Back in a private hideout after leaving / losing a crew: drop the stale session.
	if (GetNetMode() == NM_Standalone)
	{
		if (UHHSessionSubsystem* Sessions = UHHSessionSubsystem::Get(this))
		{
			Sessions->CleanupStaleSession();
		}
	}

	ApplyVideoSettings();
	ApplyVoiceSettings();
	BindVoiceEvents();
	PushProfileToServer();
}

void AHHPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		Profile->OnProfileChanged.Remove(ProfileHandle);
	}
	UHHGameUserSettings::OnSettingsApplied().Remove(SettingsHandle);

	if (IOnlineVoicePtr Voice = Online::GetVoiceInterface(GetWorld()))
	{
		Voice->ClearOnPlayerTalkingStateChangedDelegate_Handle(VoiceHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AHHPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	UHHInputSubsystem* Actions = UHHInputSubsystem::Get(this);
	if (!Input || !Actions)
	{
		UE_LOG(LogHorrorHeist, Warning, TEXT("Enhanced Input is not configured (DefaultInput.ini)."));
		return;
	}

	Input->BindAction(Actions->GetAction(EHHInputAction::Menu), ETriggerEvent::Started, this, &AHHPlayerController::HandleMenuAction);
	Input->BindAction(Actions->GetAction(EHHInputAction::Inventory), ETriggerEvent::Started, this, &AHHPlayerController::HandleInventoryAction);
	Input->BindAction(Actions->GetAction(EHHInputAction::Map), ETriggerEvent::Started, this, &AHHPlayerController::HandleMapAction);
	Input->BindAction(Actions->GetAction(EHHInputAction::Ready), ETriggerEvent::Started, this, &AHHPlayerController::HandleReadyAction);
	Input->BindAction(Actions->GetAction(EHHInputAction::PushToTalk), ETriggerEvent::Started, this, &AHHPlayerController::HandlePushToTalkStarted);
	Input->BindAction(Actions->GetAction(EHHInputAction::PushToTalk), ETriggerEvent::Completed, this, &AHHPlayerController::HandlePushToTalkCompleted);
}

// ---------------------------------------------------------------------------------------
// Input

void AHHPlayerController::HandleMenuAction()
{
	if (AHHLobbyHUD* LobbyHUD = GetLobbyHUD())
	{
		LobbyHUD->OpenScreen(EHHLobbyScreen::MainMenu);
	}
}

void AHHPlayerController::HandleInventoryAction()
{
	if (AHHLobbyHUD* LobbyHUD = GetLobbyHUD())
	{
		LobbyHUD->OpenScreen(EHHLobbyScreen::Loadout);
	}
}

void AHHPlayerController::HandleMapAction()
{
	if (AHHLobbyHUD* LobbyHUD = GetLobbyHUD())
	{
		LobbyHUD->OpenScreen(EHHLobbyScreen::Play);
	}
}

void AHHPlayerController::HandleReadyAction()
{
	RequestToggleReady();
}

void AHHPlayerController::HandlePushToTalkStarted()
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (Settings && Settings->bVoiceChatEnabled && Settings->bPushToTalk)
	{
		SetTransmitting(true);
	}
}

void AHHPlayerController::HandlePushToTalkCompleted()
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (Settings && Settings->bPushToTalk)
	{
		SetTransmitting(false);
	}
}

void AHHPlayerController::SetMenuInputMode(bool bMenuOpen)
{
	if (!IsLocalController())
	{
		return;
	}

	if (bMenuOpen)
	{
		FlushPressedKeys();
		FInputModeUIOnly Mode;
		if (const AHHLobbyHUD* LobbyHUD = GetLobbyHUD())
		{
			Mode.SetWidgetToFocus(LobbyHUD->GetFocusWidget());
		}
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
	}
	else
	{
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
		SetShowMouseCursor(false);
	}
}

// ---------------------------------------------------------------------------------------
// Requests

void AHHPlayerController::RequestToggleReady()
{
	if (const AHHPlayerState* State = GetHHPlayerState())
	{
		ServerSetReady(!State->IsReady());
	}
}

void AHHPlayerController::RequestSelectMission(FName MissionId)
{
	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		Profile->SetLastMission(MissionId);
	}
	ServerSelectMission(MissionId);
}

void AHHPlayerController::PushProfileToServer()
{
	const UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this);
	if (!Profile || !IsLocalController())
	{
		return;
	}
	ServerSetProfile(Profile->GetPlayerName(), Profile->GetLevel(), Profile->GetCosmetics(), Profile->GetEquipment());
}

void AHHPlayerController::HandleProfileChanged()
{
	PushProfileToServer();
}

FHHCosmeticLoadout AHHPlayerController::SanitizeCosmetics(const FHHCosmeticLoadout& In) const
{
	FHHCosmeticLoadout Out;
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	for (EHHCosmeticSlot Slot : TEnumRange<EHHCosmeticSlot>())
	{
		const FName Id = In.Get(Slot);
		const UHHCosmeticDefinition* Item = Registry ? Registry->FindCosmetic(Id) : nullptr;
		Out.Set(Slot, Item && Item->Slot == Slot ? Id : NAME_None);
	}
	return Out;
}

FHHEquipmentLoadout AHHPlayerController::SanitizeEquipment(const FHHEquipmentLoadout& In) const
{
	FHHEquipmentLoadout Out;
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	for (EHHEquipmentSlot Slot : TEnumRange<EHHEquipmentSlot>())
	{
		const FName Id = In.Get(Slot);
		const UHHEquipmentDefinition* Item = Registry ? Registry->FindEquipment(Id) : nullptr;
		Out.Set(Slot, Item && Item->Slot == Slot ? Id : NAME_None);
	}
	return Out;
}

// ---------------------------------------------------------------------------------------
// Server RPCs

void AHHPlayerController::ServerSetProfile_Implementation(const FString& InPlayerName, int32 Level, const FHHCosmeticLoadout& Cosmetics, const FHHEquipmentLoadout& Equipment)
{
	AHHPlayerState* State = GetHHPlayerState();
	if (!State)
	{
		return;
	}

	FString CleanName = InPlayerName.Left(20).TrimStartAndEnd();
	if (CleanName.IsEmpty())
	{
		CleanName = TEXT("Stranger");
	}
	const bool bFirstProfile = !bProfileReceived;
	bProfileReceived = true;
	State->SetPlayerName(CleanName);
	State->SetPlayerLevel(FMath::Clamp(Level, 1, UHHProfileSubsystem::MaxLevel));
	State->SetCosmetics(SanitizeCosmetics(Cosmetics));
	State->SetEquipment(SanitizeEquipment(Equipment));

	if (bFirstProfile)
	{
		if (AHHLobbyGameMode* GameMode = GetWorld()->GetAuthGameMode<AHHLobbyGameMode>())
		{
			GameMode->HandleProfileReceived(this);
		}
	}
}

void AHHPlayerController::ServerSetReady_Implementation(bool bNewReady)
{
	AHHPlayerState* State = GetHHPlayerState();
	const AHHLobbyGameState* LobbyState = GetWorld()->GetGameState<AHHLobbyGameState>();
	if (!State || (LobbyState && LobbyState->GetPhase() == EHHLobbyPhase::Departing))
	{
		return;
	}
	State->SetReady(bNewReady);
	if (AHHLobbyGameMode* GameMode = GetWorld()->GetAuthGameMode<AHHLobbyGameMode>())
	{
		GameMode->HandleReadyChanged(State);
	}
}

void AHHPlayerController::ServerSelectMission_Implementation(FName MissionId)
{
	if (AHHLobbyGameMode* GameMode = GetWorld()->GetAuthGameMode<AHHLobbyGameMode>())
	{
		GameMode->HandleMissionRequest(this, MissionId);
	}
}

void AHHPlayerController::ClientNotify_Implementation(const FText& Message, EHHNotifyType Type)
{
	if (AHHLobbyHUD* LobbyHUD = GetLobbyHUD())
	{
		LobbyHUD->ShowNotification(Message, Type);
	}
}

// ---------------------------------------------------------------------------------------
// Settings

void AHHPlayerController::HandleSettingsApplied()
{
	ApplyVideoSettings();
	ApplyVoiceSettings();
}

void AHHPlayerController::ApplyVideoSettings()
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	UWorld* World = GetWorld();
	if (!Settings || !World || !IsLocalController())
	{
		return;
	}

	if (!bHasBaseExposure)
	{
		// Brightness is an offset on top of whatever the level designer chose.
		float BestPriority = -FLT_MAX;
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (*It != SettingsVolume && It->bUnbound && It->Settings.bOverride_AutoExposureBias && It->Priority > BestPriority)
			{
				BestPriority = It->Priority;
				BaseExposureBias = It->Settings.AutoExposureBias;
			}
		}
		bHasBaseExposure = true;
	}

	if (!SettingsVolume)
	{
		APostProcessVolume* Volume = World->SpawnActorDeferred<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Volume)
		{
			Volume->bUnbound = true;
			Volume->Priority = 10000.f;
			Volume->BlendWeight = 1.f;
			Volume->FinishSpawning(FTransform::Identity);
			SettingsVolume = Volume;
		}
	}
	if (!SettingsVolume)
	{
		return;
	}

	FPostProcessSettings& PP = SettingsVolume->Settings;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = BaseExposureBias + FMath::Clamp(Settings->Brightness, -1.f, 1.f) * 1.5f;

	PP.bOverride_MotionBlurAmount = !Settings->bMotionBlur;
	PP.MotionBlurAmount = 0.f;

	PP.bOverride_FilmGrainIntensity = !Settings->bFilmGrain;
	PP.FilmGrainIntensity = 0.f;

	PP.bOverride_SceneFringeIntensity = !Settings->bChromaticAberration;
	PP.SceneFringeIntensity = 0.f;
}

// ---------------------------------------------------------------------------------------
// Voice

void AHHPlayerController::ApplyVoiceSettings()
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (!Settings || !IsLocalController())
	{
		return;
	}
	if (GetNetMode() == NM_Standalone || !Settings->bVoiceChatEnabled)
	{
		SetTransmitting(false);
		return;
	}
	// Open mic transmits constantly; push-to-talk waits for the key.
	SetTransmitting(!Settings->bPushToTalk);
}

void AHHPlayerController::SetTransmitting(bool bNewTransmitting)
{
	if (GetNetMode() == NM_Standalone)
	{
		bNewTransmitting = false;
	}
	if (bTransmitting == bNewTransmitting)
	{
		return;
	}
	bTransmitting = bNewTransmitting;
	ToggleSpeaking(bTransmitting);
}

void AHHPlayerController::BindVoiceEvents()
{
	if (IOnlineVoicePtr Voice = Online::GetVoiceInterface(GetWorld()))
	{
		VoiceHandle = Voice->AddOnPlayerTalkingStateChangedDelegate_Handle(
			FOnPlayerTalkingStateChangedDelegate::CreateUObject(this, &AHHPlayerController::HandleTalkingStateChanged));
	}
}

void AHHPlayerController::HandleTalkingStateChanged(FUniqueNetIdRef TalkerId, bool bIsTalking)
{
	const FString Key = TalkerId->ToString();
	if (bIsTalking)
	{
		TalkingPlayers.Add(Key);
	}
	else
	{
		TalkingPlayers.Remove(Key);
	}
}

bool AHHPlayerController::IsPlayerTalking(const APlayerState* InPlayerState) const
{
	if (!InPlayerState)
	{
		return false;
	}
	if (InPlayerState == PlayerState)
	{
		return bTransmitting;
	}
	const FUniqueNetIdRepl& Id = InPlayerState->GetUniqueId();
	return Id.IsValid() && TalkingPlayers.Contains(Id.ToString());
}

// ---------------------------------------------------------------------------------------
// Development commands

void AHHPlayerController::HHGiveCash(int32 Amount)
{
	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		Profile->AddCash(Amount);
	}
}

void AHHPlayerController::HHGiveXP(int32 Amount)
{
	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		Profile->AddXP(Amount);
	}
}

void AHHPlayerController::HHUnlockAll()
{
	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		Profile->UnlockEverything();
	}
}

void AHHPlayerController::HHResetProfile()
{
	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		Profile->ResetProfile();
	}
}

void AHHPlayerController::HHForceAmbientEvent(int32 EventIndex)
{
	if (!HasAuthority())
	{
		ClientNotify(LOCTEXT("HostOnly", "Only the host can trigger ambience events."), EHHNotifyType::Warning);
		return;
	}
	for (TActorIterator<AHHAmbienceDirector> It(GetWorld()); It; ++It)
	{
		It->TriggerEvent(EventIndex);
		return;
	}
}

#undef LOCTEXT_NAMESPACE
