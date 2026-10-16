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
#include "Heist/HHEscapeZone.h"
#include "Online/HHSessionSubsystem.h"
#include "Progression/HHProfileSubsystem.h"
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
	AHHLobbyGameState* LobbyState = GetLobbyState();
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	if (LobbyState && Registry)
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
		if (LobbyState->GetSelectedMission().IsNone())
		{
			// Usually content built by an older version of the project: the jobs have no maps yet.
			UE_LOG(LogHorrorHeist, Warning, TEXT("No playable job found (%d jobs registered). Rebuild the content: The Quiet Job > Build / Rebuild All Content."), Missions.Num());
		}
	}
	if (LobbyState)
	{
		// The crew's notebook lives in the host's profile; the escape plan defaults to home.
		if (const UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
		{
			LobbyState->MergeIntel(Profile->GetKnownIntel());
		}
		const UHHGameInstance* GameInstance = GetGameInstance<UHHGameInstance>();
		if (GameInstance)
		{
			// The morning after: police at the house, the street closed, the owners talking.
			LobbyState->SetAftermath(GameInstance->GetAftermath());
		}
		const FName Remembered = GameInstance ? GameInstance->GetSafehouse() : NAME_None;
		LobbyState->SetSafehouse(Remembered.IsNone() ? FName(TEXT("Hideout")) : Remembered);
		const UHHSessionSubsystem* Sessions = UHHSessionSubsystem::Get(this);
		LobbyState->SetLobbyCode(Sessions && Sessions->HasSession() ? Sessions->GetLobbyCode() : FString());
	}
	GetWorldTimerManager().SetTimer(PollTimer, this, &AHHLobbyGameMode::PollDeparture, 1.f, true, 1.f);
}

void AHHLobbyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PollTimer);
	GetWorldTimerManager().ClearTimer(CountdownTimer);
	Super::EndPlay(EndPlayReason);
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
	GetWorldTimerManager().SetTimerForNextTick(this, &AHHLobbyGameMode::PollDeparture);
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
	EvaluateDeparture(false);
}

void AHHLobbyGameMode::HandleSafehouseRequest(AHHPlayerController* Requester, FName ZoneId)
{
	AHHLobbyGameState* LobbyState = GetLobbyState();
	const AHHPlayerState* PlayerState = Requester ? Requester->GetPlayerState<AHHPlayerState>() : nullptr;
	if (!LobbyState || !PlayerState || LobbyState->GetPhase() == EHHLobbyPhase::Departing || LobbyState->GetSafehouse() == ZoneId)
	{
		return;
	}
	if (!PlayerState->IsCrewLeader())
	{
		Requester->ClientNotify(LOCTEXT("NotLeaderSafehouse", "Only the crew leader plans the getaway."), EHHNotifyType::Warning);
		return;
	}
	const AHHEscapeZone* Zone = AHHEscapeZone::Find(this, ZoneId);
	if (!Zone)
	{
		return;
	}
	LobbyState->SetSafehouse(ZoneId);
	if (UHHGameInstance* GameInstance = GetGameInstance<UHHGameInstance>())
	{
		GameInstance->SetSafehouse(ZoneId);
	}
	LobbyState->MulticastCrewMessage(FText::Format(LOCTEXT("SafehousePicked", "Getaway: {0}"), Zone->GetDisplayName()), EHHNotifyType::Info);
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

const UHHMissionDefinition* AHHLobbyGameMode::GetPinnedMission() const
{
	const AHHLobbyGameState* LobbyState = GetLobbyState();
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	return LobbyState && Registry ? Registry->FindMission(LobbyState->GetSelectedMission()) : nullptr;
}

void AHHLobbyGameMode::PollDeparture()
{
	EvaluateDeparture(false);
}

void AHHLobbyGameMode::EvaluateDeparture(bool bAnnounce)
{
	AHHLobbyGameState* LobbyState = GetLobbyState();
	if (!LobbyState || LobbyState->GetPhase() == EHHLobbyPhase::Departing)
	{
		return;
	}

	FText Reason;
	const EHHDepartureBlock Block = GetDepartureBlock(GetPinnedMission(), Reason);
	LobbyState->SetDepartureBlock(Block);
	const bool bEveryoneReady = LobbyState->IsEveryoneReady();

	if (LobbyState->GetPhase() == EHHLobbyPhase::Countdown)
	{
		if (!bEveryoneReady)
		{
			CancelCountdown(LOCTEXT("NotReady", "Someone isn't ready. Departure on hold."));
		}
		else if (Block != EHHDepartureBlock::None)
		{
			CancelCountdown(Reason);
		}
		return;
	}

	if (!bEveryoneReady)
	{
		LastAnnouncedBlock = EHHDepartureBlock::None;
		return;
	}
	if (Block == EHHDepartureBlock::None)
	{
		LastAnnouncedBlock = EHHDepartureBlock::None;
		StartCountdown();
		return;
	}
	// Everyone is ready but the van can't leave: say why instead of silently doing nothing.
	if (bAnnounce || Block != LastAnnouncedBlock)
	{
		LastAnnouncedBlock = Block;
		LobbyState->MulticastCrewMessage(Reason, EHHNotifyType::Warning);
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

EHHDepartureBlock AHHLobbyGameMode::GetDepartureBlock(const UHHMissionDefinition* Mission, FText& OutReason) const
{
	if (!Mission)
	{
		OutReason = LOCTEXT("NoJob", "Everyone's ready, but no job is pinned. The crew leader picks a house on the job board and presses PIN THIS JOB.");
		return EHHDepartureBlock::NoJob;
	}
	if (!Mission->IsPlayable())
	{
		// The job exists on the board but its house is not ready yet. Say so honestly.
		OutReason = Mission->Map.IsNull() && Mission->bAvailable
			? FText::Format(LOCTEXT("NoMap", "{0} has no map assigned - the game content is out of date. Rebuild it in the editor: The Quiet Job > Build / Rebuild All Content."), Mission->DisplayName)
			: FText::Format(LOCTEXT("NotPlayable", "{0} is still being cased - this job is not available yet."), Mission->DisplayName);
		return EHHDepartureBlock::NotPlayable;
	}
	if (Mission->bRequiresNight)
	{
		const AHHTimeOfDay* Clock = AHHTimeOfDay::Get(this);
		if (Clock && !Clock->IsNight())
		{
			OutReason = FText::Format(LOCTEXT("NotNight", "Ready - but it's {0}. The van leaves after dark ({1}), on its own once night falls. The leader can press WAIT UNTIL DARK on the job board."),
				HHText::ClockTime(Clock->GetTimeOfDay()), HHText::ClockTime(UHHDeveloperSettings::Get()->NightStartHour));
			return EHHDepartureBlock::NotNight;
		}
	}
	OutReason = FText::GetEmpty();
	return EHHDepartureBlock::None;
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
	EvaluateDeparture(false);
}

void AHHLobbyGameMode::Depart()
{
	AHHLobbyGameState* LobbyState = GetLobbyState();
	const UHHMissionDefinition* Mission = GetPinnedMission();
	if (!LobbyState)
	{
		return;
	}

	FText Blocker;
	if (GetDepartureBlock(Mission, Blocker) != EHHDepartureBlock::None)
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
		GameInstance->SetSafehouse(LobbyState->GetSafehouse());
	}

	const FString MapPath = Mission->Map.ToSoftObjectPath().GetLongPackageName();
	UE_LOG(LogHorrorHeist, Log, TEXT("Departing to %s (getaway: %s)"), *MapPath, *LobbyState->GetSafehouse().ToString());
	FString Options = FString::Printf(TEXT("?Mission=%s"), *Mission->GetMissionId().ToString());
	if (!LobbyState->GetSafehouse().IsNone())
	{
		Options += FString::Printf(TEXT("?Escape=%s"), *LobbyState->GetSafehouse().ToString());
	}
	if (!TravelCrewTo(MapPath, Options))
	{
		LobbyState->SetPhase(EHHLobbyPhase::Gathering);
		LobbyState->MulticastCrewMessage(LOCTEXT("TravelFailed", "The van would not start (travel failed, see the log)."), EHHNotifyType::Error);
		ResetReadyStates();
	}
}

#undef LOCTEXT_NAMESPACE
