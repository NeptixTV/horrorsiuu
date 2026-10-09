#include "Lobby/HHLobbyGameState.h"
#include "Player/HHPlayerState.h"
#include "Net/UnrealNetwork.h"

AHHLobbyGameState::AHHLobbyGameState()
{
}

void AHHLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHHLobbyGameState, SelectedMission);
	DOREPLIFETIME(AHHLobbyGameState, Phase);
	DOREPLIFETIME(AHHLobbyGameState, DepartureServerTime);
}

void AHHLobbyGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	OnCrewChanged.Broadcast();
}

void AHHLobbyGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	OnCrewChanged.Broadcast();
}

float AHHLobbyGameState::GetCountdownRemaining() const
{
	if (Phase != EHHLobbyPhase::Countdown)
	{
		return 0.f;
	}
	return static_cast<float>(FMath::Max(0.0, DepartureServerTime - GetServerWorldTimeSeconds()));
}

TArray<AHHPlayerState*> AHHLobbyGameState::GetCrew() const
{
	TArray<AHHPlayerState*> Crew;
	for (APlayerState* PlayerState : PlayerArray)
	{
		if (AHHPlayerState* HHPlayerState = Cast<AHHPlayerState>(PlayerState))
		{
			if (!HHPlayerState->IsInactive() && !HHPlayerState->IsOnlyASpectator())
			{
				Crew.Add(HHPlayerState);
			}
		}
	}
	Crew.Sort([](const AHHPlayerState& A, const AHHPlayerState& B)
	{
		return A.GetCrewSlot() < B.GetCrewSlot();
	});
	return Crew;
}

bool AHHLobbyGameState::IsEveryoneReady() const
{
	const TArray<AHHPlayerState*> Crew = GetCrew();
	if (Crew.Num() == 0)
	{
		return false;
	}
	for (const AHHPlayerState* Member : Crew)
	{
		if (!Member->IsReady())
		{
			return false;
		}
	}
	return true;
}

int32 AHHLobbyGameState::NumReady() const
{
	int32 Count = 0;
	for (const AHHPlayerState* Member : GetCrew())
	{
		Count += Member->IsReady() ? 1 : 0;
	}
	return Count;
}

void AHHLobbyGameState::SetSelectedMission(FName MissionId)
{
	if (HasAuthority() && SelectedMission != MissionId)
	{
		SelectedMission = MissionId;
		OnRep_SelectedMission();
	}
}

void AHHLobbyGameState::SetPhase(EHHLobbyPhase NewPhase, float CountdownSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	DepartureServerTime = NewPhase == EHHLobbyPhase::Countdown ? GetServerWorldTimeSeconds() + CountdownSeconds : 0.0;
	if (Phase != NewPhase)
	{
		Phase = NewPhase;
		OnRep_Phase();
	}
	ForceNetUpdate();
}

void AHHLobbyGameState::MulticastCrewMessage_Implementation(const FText& Message, EHHNotifyType Type)
{
	OnCrewMessage.Broadcast(Message, Type);
}

void AHHLobbyGameState::OnRep_SelectedMission()
{
	OnMissionChanged.Broadcast();
}

void AHHLobbyGameState::OnRep_Phase()
{
	OnPhaseChanged.Broadcast();
}
