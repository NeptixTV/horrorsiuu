#include "Lobby/HHLobbyGameMode.h"
#include "Lobby/HHLobbyGameState.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Player/HHCharacter.h"
#include "UI/HHLobbyHUD.h"
#include "Core/HHDeveloperSettings.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHMissionDefinition.h"
#include "Core/HHGameInstance.h"
#include "World/HHTimeOfDay.h"
#include "HorrorHeist.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "HHLobbyGameMode"

AHHLobbyGameMode::AHHLobbyGameMode()
{
	GameStateClass = AHHLobbyGameState::StaticClass();
}

AHHLobbyGameState* AHHLobbyGameMode::GetLobbyState() const
{
	return GetGameState<AHHLobbyGameState>();
}

void AHHLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Pre-select the first unlocked job so the board is never empty.
	if (AHHLobbyGameState* LobbyState = GetLobbyState())
	{
		if (const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this))
		{
			const TArray<const UHHMissionDefinition*> Missions = Registry->GetMissions();
			for (const UHHMissionDefinition* Mission : Missions)
			{
				if (Mission->UnlockLevel <= 1 && Mission->IsPlayable())
				{
					LobbyState->SetSelectedMission(Mission->GetMissionId());
					break;
				}
			}
		}
	}
}

void AHHLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	const int32 MaxCrew = UHHDeveloperSettings::Get()->MaxCrewSize;
	if (GetNumPlayers() >= MaxCrew)
	{
		ErrorMessage = TEXT("The crew is full.");
		return;
	}
	if (const AHHLobbyGameState* LobbyState = GetLobbyState())
	{
		if (LobbyState->GetPhase() == EHHLobbyPhase::Departing)
		{
			ErrorMessage = TEXT("The crew is already leaving for a job.");
		}
	}
}

void AHHLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	// Crew slot + leader are assigned by AHHGameModeBase before the pawn spawns.
	Super::PostLogin(NewPlayer);

	// A new arrival resets the countdown.
	if (AHHLobbyGameState* LobbyState = GetLobbyState())
	{
		if (LobbyState->GetPhase() == EHHLobbyPhase::Countdown)
		{
			CancelCountdown(LOCTEXT("NewArrival", "Someone came down the stairs. Departure on hold."));
		}
	}
}

void AHHLobbyGameMode::HandleProfileReceived(AHHPlayerController* Controller)
{
	AHHPlayerState* PlayerState = Controller ? Controller->GetPlayerState<AHHPlayerState>() : nullptr;
	AHHLobbyGameState* LobbyState = GetLobbyState();
	if (!PlayerState || !LobbyState || Controller->IsLocalController())
	{
		return;
	}
	LobbyState->MulticastCrewMessage(
		FText::Format(LOCTEXT("Joined", "{0} joined the crew."), FText::FromString(PlayerState->GetPlayerName())),
		EHHNotifyType::Crew);
}

void AHHLobbyGameMode::Logout(AController* Exiting)
{
	const AHHPlayerState* PlayerState = Exiting ? Exiting->GetPlayerState<AHHPlayerState>() : nullptr;
	const FString Name = PlayerState ? PlayerState->GetPlayerName() : FString();

	Super::Logout(Exiting);

	if (AHHLobbyGameState* LobbyState = GetLobbyState())
	{
		if (!Name.IsEmpty())
		{
			LobbyState->MulticastCrewMessage(FText::Format(LOCTEXT("Left", "{0} left the crew."), FText::FromString(Name)), EHHNotifyType::Crew);
		}
	}

	// Re-evaluate on the next tick, once the player state is gone from the array.
	GetWorldTimerManager().SetTimerForNextTick(this, &AHHLobbyGameMode::EvaluateDeparture);
}

void AHHLobbyGameMode::HandleReadyChanged(AHHPlayerState* PlayerState)
{
	EvaluateDeparture();
}

void AHHLobbyGameMode::HandleMissionRequest(AHHPlayerController* Requester, FName MissionId)
{
	AHHLobbyGameState* LobbyState = GetLobbyState();
	const AHHPlayerState* PlayerState = Requester ? Requester->GetPlayerState<AHHPlayerState>() : nullptr;
	if (!LobbyState || !PlayerState)
	{
		return;
	}
	if (!PlayerState->IsCrewLeader())
	{
		Requester->ClientNotify(LOCTEXT("NotLeader", "Only the crew leader picks the job."), EHHNotifyType::Warning);
		return;
	}

	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	const UHHMissionDefinition* Mission = Registry ? Registry->FindMission(MissionId) : nullptr;
	if (!Mission || LobbyState->GetSelectedMission() == MissionId)
	{
		return;
	}
	if (LobbyState->GetPhase() == EHHLobbyPhase::Departing)
	{
		return;
	}
	if (!Mission->IsPlayable())
	{
		Requester->ClientNotify(FText::Format(LOCTEXT("NotOpen", "{0} is still being cased - pick another job."), Mission->DisplayName), EHHNotifyType::Warning);
		return;
	}

	LobbyState->SetSelectedMission(MissionId);
	LobbyState->MulticastCrewMessage(
		FText::Format(LOCTEXT("JobPicked", "New job pinned: {0}"), Mission->DisplayName), EHHNotifyType::Info);

	// Changing the plan means everyone confirms again.
	if (LobbyState->GetPhase() == EHHLobbyPhase::Countdown)
	{
		CancelCountdown(LOCTEXT("PlanChanged", "The plan changed. Departure cancelled."));
	}
	ResetReadyStates();
}

void AHHLobbyGameMode::ResetReadyStates()
{
	if (AHHLobbyGameState* LobbyState = GetLobbyState())
	{
		for (AHHPlayerState* Member : LobbyState->GetCrew())
		{
			Member->SetReady(false);
		}
	}
}

void AHHLobbyGameMode::EvaluateDeparture()
{
	AHHLobbyGameState* LobbyState = GetLobbyState();
	if (!LobbyState || LobbyState->GetPhase() == EHHLobbyPhase::Departing)
	{
		return;
	}

	const bool bAllReady = LobbyState->IsEveryoneReady() && !LobbyState->GetSelectedMission().IsNone();
	if (bAllReady && LobbyState->GetPhase() == EHHLobbyPhase::Gathering)
	{
		StartCountdown();
	}
	else if (!bAllReady && LobbyState->GetPhase() == EHHLobbyPhase::Countdown)
	{
		CancelCountdown(LOCTEXT("NotReady", "Someone isn't ready. Departure on hold."));
	}
}

void AHHLobbyGameMode::StartCountdown()
{
	AHHLobbyGameState* LobbyState = GetLobbyState();
	const float Seconds = UHHDeveloperSettings::Get()->DepartureCountdownSeconds;
	LobbyState->SetPhase(EHHLobbyPhase::Countdown, Seconds);
	LastAnnouncedSecond = -1;
	GetWorldTimerManager().SetTimer(CountdownTimer, this, &AHHLobbyGameMode::CountdownTick, 0.25f, true);
}

void AHHLobbyGameMode::CancelCountdown(const FText& Reason)
{
	GetWorldTimerManager().ClearTimer(CountdownTimer);
	if (AHHLobbyGameState* LobbyState = GetLobbyState())
	{
		LobbyState->SetPhase(EHHLobbyPhase::Gathering);
		if (!Reason.IsEmpty())
		{
			LobbyState->MulticastCrewMessage(Reason, EHHNotifyType::Warning);
		}
	}
}

void AHHLobbyGameMode::CountdownTick()
{
	AHHLobbyGameState* LobbyState = GetLobbyState();
	if (!LobbyState)
	{
		return;
	}
	if (LobbyState->GetCountdownRemaining() <= 0.f)
	{
		GetWorldTimerManager().ClearTimer(CountdownTimer);
		Depart();
	}
}

FText AHHLobbyGameMode::GetDepartureBlocker(const UHHMissionDefinition* Mission) const
{
	if (!Mission)
	{
		return LOCTEXT("NoJob", "No job is pinned to the board.");
	}
	if (!Mission->IsPlayable())
	{
		// The job exists on the board but its house is not ready yet. Say so honestly.
		return FText::Format(LOCTEXT("NotPlayable", "{0} is still being cased - this job is not available yet."), Mission->DisplayName);
	}
	if (Mission->bRequiresNight)
	{
		const AHHTimeOfDay* Clock = AHHTimeOfDay::Get(this);
		if (Clock && !Clock->IsNight())
		{
			return FText::Format(LOCTEXT("NotNight", "It's {0}. Jobs start after dark ({1}). Wait for nightfall or use 'Wait until dark' on the job board."),
				HHText::ClockTime(Clock->GetTimeOfDay()), HHText::ClockTime(UHHDeveloperSettings::Get()->NightStartHour));
		}
	}
	return FText::GetEmpty();
}

void AHHLobbyGameMode::HandleWaitForNight(AHHPlayerController* Requester)
{
	AHHLobbyGameState* LobbyState = GetLobbyState();
	const AHHPlayerState* PlayerState = Requester ? Requester->GetPlayerState<AHHPlayerState>() : nullptr;
	AHHTimeOfDay* Clock = AHHTimeOfDay::Get(this);
	if (!LobbyState || !PlayerState || !Clock)
	{
		return;
	}
	if (!PlayerState->IsCrewLeader())
	{
		Requester->ClientNotify(LOCTEXT("NotLeaderWait", "Only the crew leader decides when to head out."), EHHNotifyType::Warning);
		return;
	}
	if (Clock->IsNight())
	{
		Requester->ClientNotify(LOCTEXT("AlreadyNight", "It is already dark outside."), EHHNotifyType::Info);
		return;
	}
	Clock->SkipTo(UHHDeveloperSettings::Get()->NightStartHour + 0.25f);
	LobbyState->MulticastCrewMessage(FText::Format(LOCTEXT("Waited", "The crew waits for nightfall. It's {0}."), HHText::ClockTime(Clock->GetTimeOfDay())), EHHNotifyType::Info);
}

void AHHLobbyGameMode::Depart()
{
	AHHLobbyGameState* LobbyState = GetLobbyState();
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	const UHHMissionDefinition* Mission = Registry && LobbyState ? Registry->FindMission(LobbyState->GetSelectedMission()) : nullptr;
	if (!LobbyState)
	{
		return;
	}

	const FText Blocker = GetDepartureBlocker(Mission);
	if (!Blocker.IsEmpty())
	{
		LobbyState->SetPhase(EHHLobbyPhase::Gathering);
		LobbyState->MulticastCrewMessage(Blocker, EHHNotifyType::Warning);
		ResetReadyStates();
		return;
	}

	LobbyState->SetPhase(EHHLobbyPhase::Departing);
	LobbyState->MulticastCrewMessage(FText::Format(LOCTEXT("Departing", "Heading out to {0}."), Mission->Address), EHHNotifyType::Info);

	if (UHHGameInstance* GameInstance = GetGameInstance<UHHGameInstance>())
	{
		GameInstance->SetActiveMission(Mission->GetMissionId());
	}

	const FString MapPath = Mission->Map.ToSoftObjectPath().GetLongPackageName();
	UE_LOG(LogHorrorHeist, Log, TEXT("Departing to %s"), *MapPath);
	if (!TravelCrewTo(MapPath, FString::Printf(TEXT("?Mission=%s"), *Mission->GetMissionId().ToString())))
	{
		LobbyState->SetPhase(EHHLobbyPhase::Gathering);
		LobbyState->MulticastCrewMessage(LOCTEXT("TravelFailed", "The van would not start (travel failed, see the log)."), EHHNotifyType::Error);
		ResetReadyStates();
	}
}

#undef LOCTEXT_NAMESPACE
