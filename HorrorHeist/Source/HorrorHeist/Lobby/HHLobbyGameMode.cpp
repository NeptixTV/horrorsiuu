#include "Lobby/HHLobbyGameMode.h"
#include "Lobby/HHLobbyGameState.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Player/HHCharacter.h"
#include "UI/HHLobbyHUD.h"
#include "Core/HHDeveloperSettings.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHMissionDefinition.h"
#include "HorrorHeist.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "HHLobbyGameMode"

AHHLobbyGameMode::AHHLobbyGameMode()
{
	DefaultPawnClass = AHHCharacter::StaticClass();
	PlayerControllerClass = AHHPlayerController::StaticClass();
	PlayerStateClass = AHHPlayerState::StaticClass();
	GameStateClass = AHHLobbyGameState::StaticClass();
	HUDClass = AHHLobbyHUD::StaticClass();
	bUseSeamlessTravel = false;
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
				if (Mission->UnlockLevel <= 1)
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

int32 AHHLobbyGameMode::FindFreeCrewSlot() const
{
	TSet<int32> Used;
	if (const AHHLobbyGameState* LobbyState = GetLobbyState())
	{
		for (const APlayerState* PlayerState : LobbyState->PlayerArray)
		{
			if (const AHHPlayerState* HHPlayerState = Cast<AHHPlayerState>(PlayerState))
			{
				Used.Add(HHPlayerState->GetCrewSlot());
			}
		}
	}
	int32 Slot = 0;
	while (Used.Contains(Slot))
	{
		++Slot;
	}
	return Slot;
}

void AHHLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	// Assign the slot before Super spawns the pawn so colours are right from the start.
	if (AHHPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AHHPlayerState>() : nullptr)
	{
		PlayerState->SetCrewSlot(FindFreeCrewSlot());
		PlayerState->SetCrewLeader(NewPlayer->IsLocalController());
	}

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

AActor* AHHLobbyGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Prefer starts nobody is standing on.
	TArray<APlayerStart*> Free;
	TArray<APlayerStart*> All;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		All.Add(Start);

		bool bOccupied = false;
		for (TActorIterator<APawn> PawnIt(GetWorld()); PawnIt; ++PawnIt)
		{
			if (FVector::DistSquared2D(PawnIt->GetActorLocation(), Start->GetActorLocation()) < FMath::Square(80.f))
			{
				bOccupied = true;
				break;
			}
		}
		if (!bOccupied)
		{
			Free.Add(Start);
		}
	}

	if (Free.Num() > 0)
	{
		return Free[FMath::RandRange(0, Free.Num() - 1)];
	}
	if (All.Num() > 0)
	{
		return All[FMath::RandRange(0, All.Num() - 1)];
	}
	return Super::ChoosePlayerStart_Implementation(Player);
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

void AHHLobbyGameMode::Depart()
{
	AHHLobbyGameState* LobbyState = GetLobbyState();
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	const UHHMissionDefinition* Mission = Registry && LobbyState ? Registry->FindMission(LobbyState->GetSelectedMission()) : nullptr;

	if (!Mission || !Mission->IsPlayable())
	{
		// The job exists on the board but has no level yet (milestone 1). Say so honestly.
		LobbyState->SetPhase(EHHLobbyPhase::Gathering);
		LobbyState->MulticastCrewMessage(
			LOCTEXT("NotPlayable", "The van won't start. This job has no map assigned yet (set Map on the mission data asset)."),
			EHHNotifyType::Warning);
		ResetReadyStates();
		return;
	}

	LobbyState->SetPhase(EHHLobbyPhase::Departing);
	LobbyState->MulticastCrewMessage(FText::Format(LOCTEXT("Departing", "Heading out to {0}."), Mission->Address), EHHNotifyType::Info);

	const FString MapPath = Mission->Map.ToSoftObjectPath().GetLongPackageName();
	UE_LOG(LogHorrorHeist, Log, TEXT("Departing to %s"), *MapPath);
	GetWorld()->ServerTravel(MapPath + TEXT("?listen"), true);
}

#undef LOCTEXT_NAMESPACE
