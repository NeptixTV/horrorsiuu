#include "Heist/HHHeistGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

void AHHHeistGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHHeistGameState, HeistPhase);
	DOREPLIFETIME(AHHHeistGameState, Objectives);
	DOREPLIFETIME(AHHHeistGameState, SecuredValue);
	DOREPLIFETIME(AHHHeistGameState, MinimumHaul);
	DOREPLIFETIME(AHHHeistGameState, Alert);
	DOREPLIFETIME(AHHHeistGameState, PoliceArrivalTime);
	DOREPLIFETIME(AHHHeistGameState, HauntLevel);
	DOREPLIFETIME(AHHHeistGameState, HauntStage);
	DOREPLIFETIME(AHHHeistGameState, Keys);
	DOREPLIFETIME(AHHHeistGameState, Summary);
	DOREPLIFETIME(AHHHeistGameState, StartServerTime);
	DOREPLIFETIME(AHHHeistGameState, CrewAtVan);
	DOREPLIFETIME(AHHHeistGameState, CrewTotal);
}

float AHHHeistGameState::GetPoliceRemaining() const
{
	if (PoliceArrivalTime <= 0.0)
	{
		return 0.f;
	}
	return static_cast<float>(FMath::Max(0.0, PoliceArrivalTime - GetServerWorldTimeSeconds()));
}

float AHHHeistGameState::GetElapsed() const
{
	if (Summary.Outcome != EHHMissionOutcome::None)
	{
		return Summary.DurationSeconds;
	}
	return static_cast<float>(FMath::Max(0.0, GetServerWorldTimeSeconds() - StartServerTime));
}

void AHHHeistGameState::Changed()
{
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
	OnHeistChanged.Broadcast();
}

void AHHHeistGameState::OnRep_Heist()
{
	OnHeistChanged.Broadcast();
}

void AHHHeistGameState::StartClock()
{
	if (HasAuthority())
	{
		StartServerTime = GetServerWorldTimeSeconds();
		Changed();
	}
}

void AHHHeistGameState::SetHeistPhase(EHHHeistPhase NewPhase)
{
	if (HasAuthority() && HeistPhase != NewPhase)
	{
		HeistPhase = NewPhase;
		Changed();
	}
}

void AHHHeistGameState::SetObjectives(const TArray<FHHObjective>& NewObjectives)
{
	if (HasAuthority())
	{
		Objectives = NewObjectives;
		Changed();
	}
}

void AHHHeistGameState::UpdateObjective(FName Id, const FText* NewText, const bool* bNewDone)
{
	if (!HasAuthority())
	{
		return;
	}
	for (FHHObjective& Objective : Objectives)
	{
		if (Objective.Id != Id)
		{
			continue;
		}
		bool bChanged = false;
		if (NewText && !Objective.Text.EqualTo(*NewText))
		{
			Objective.Text = *NewText;
			bChanged = true;
		}
		if (bNewDone && Objective.bDone != *bNewDone)
		{
			Objective.bDone = *bNewDone;
			bChanged = true;
		}
		if (bChanged)
		{
			Changed();
		}
		return;
	}
}

void AHHHeistGameState::SetSecured(int32 NewSecured, int32 NewMinimum)
{
	if (HasAuthority() && (SecuredValue != NewSecured || MinimumHaul != NewMinimum))
	{
		SecuredValue = NewSecured;
		MinimumHaul = NewMinimum;
		Changed();
	}
}

void AHHHeistGameState::SetAlert(float NewAlert)
{
	const float Clamped = FMath::Clamp(NewAlert, 0.f, 100.f);
	if (HasAuthority() && FMath::Abs(Clamped - Alert) > 0.25f)
	{
		Alert = Clamped;
		Changed();
	}
}

void AHHHeistGameState::CallPolice(float Seconds)
{
	if (HasAuthority() && PoliceArrivalTime <= 0.0)
	{
		PoliceArrivalTime = GetServerWorldTimeSeconds() + Seconds;
		Alert = 100.f;
		Changed();
	}
}

void AHHHeistGameState::SetHaunt(float Level, int32 Stage)
{
	if (HasAuthority() && (FMath::Abs(Level - HauntLevel) > 0.5f || Stage != HauntStage))
	{
		HauntLevel = Level;
		HauntStage = Stage;
		Changed();
	}
}

void AHHHeistGameState::AddKey(FName KeyId)
{
	if (HasAuthority() && !KeyId.IsNone() && !Keys.Contains(KeyId))
	{
		Keys.Add(KeyId);
		Changed();
	}
}

void AHHHeistGameState::SetSummary(const FHHMissionSummary& NewSummary)
{
	if (HasAuthority())
	{
		Summary = NewSummary;
		Changed();
	}
}

void AHHHeistGameState::SetVanCount(int32 AtVan, int32 Total)
{
	if (HasAuthority() && (CrewAtVan != AtVan || CrewTotal != Total))
	{
		CrewAtVan = AtVan;
		CrewTotal = Total;
		Changed();
	}
}

void AHHHeistGameState::MulticastSoundAt_Implementation(USoundBase* Sound, FVector Location, float Volume)
{
	if (Sound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, Volume);
	}
}
