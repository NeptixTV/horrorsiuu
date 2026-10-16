#include "Lobby/HHLobbyGameState.h"
#include "Player/HHPlayerState.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Progression/HHProfileSubsystem.h"
#include "Net/UnrealNetwork.h"

AHHLobbyGameState::AHHLobbyGameState()
{
}

void AHHLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHHLobbyGameState, SelectedMission);
	DOREPLIFETIME(AHHLobbyGameState, Phase);
	DOREPLIFETIME(AHHLobbyGameState, DepartureBlock);
	DOREPLIFETIME(AHHLobbyGameState, Safehouse);
	DOREPLIFETIME(AHHLobbyGameState, KnownIntel);
	DOREPLIFETIME(AHHLobbyGameState, LobbyCode);
	DOREPLIFETIME(AHHLobbyGameState, DepartureServerTime);
	DOREPLIFETIME(AHHLobbyGameState, Aftermath);
}

void AHHLobbyGameState::SetAftermath(const FHHAftermath& NewAftermath)
{
	Aftermath = NewAftermath;
	ForceNetUpdate();
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

void AHHLobbyGameState::SetDepartureBlock(EHHDepartureBlock NewBlock)
{
	if (HasAuthority() && DepartureBlock != NewBlock)
	{
		DepartureBlock = NewBlock;
		OnRep_Phase();
	}
}

void AHHLobbyGameState::SetSafehouse(FName ZoneId)
{
	if (HasAuthority() && Safehouse != ZoneId)
	{
		Safehouse = ZoneId;
		OnRep_SelectedMission();
	}
}

bool AHHLobbyGameState::LearnIntel(FName IntelId)
{
	if (!HasAuthority() || IntelId.IsNone() || KnownIntel.Contains(IntelId))
	{
		return false;
	}
	KnownIntel.Add(IntelId);
	ForceNetUpdate();
	OnRep_KnownIntel();
	return true;
}

void AHHLobbyGameState::MergeIntel(const TArray<FName>& IntelIds)
{
	if (!HasAuthority())
	{
		return;
	}
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	bool bChanged = false;
	for (const FName Id : IntelIds)
	{
		// Only ids the content knows (old saves, other versions of the game).
		if (!Id.IsNone() && !KnownIntel.Contains(Id) && (!Registry || Registry->FindIntel(Id)))
		{
			KnownIntel.Add(Id);
			bChanged = true;
		}
	}
	if (bChanged)
	{
		ForceNetUpdate();
		OnRep_KnownIntel();
	}
}

void AHHLobbyGameState::ClearIntel()
{
	if (HasAuthority() && KnownIntel.Num() > 0)
	{
		KnownIntel.Reset();
		ForceNetUpdate();
		OnIntelChanged.Broadcast();
	}
}

void AHHLobbyGameState::SetLobbyCode(const FString& NewCode)
{
	if (HasAuthority() && LobbyCode != NewCode)
	{
		LobbyCode = NewCode;
		OnRep_Phase();
	}
}

void AHHLobbyGameState::OnRep_KnownIntel()
{
	// Everyone keeps the crew's notebook in their own profile, so it survives hosting next time.
	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		Profile->RememberIntel(KnownIntel);
	}
	OnIntelChanged.Broadcast();
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
