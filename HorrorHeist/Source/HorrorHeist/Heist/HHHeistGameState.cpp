#include "Heist/HHHeistGameState.h"
#include "Heist/HHHeistGameMode.h"
#include "Core/HHGameData.h"
#include "Components/PointLightComponent.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

bool AHHHeistGameState::IsJobWorld(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return false;
	}
	if (World->GetGameState())
	{
		return World->GetGameState<AHHHeistGameState>() != nullptr;
	}
	const AWorldSettings* Settings = World->GetWorldSettings();
	return Settings && Settings->DefaultGameMode && Settings->DefaultGameMode->IsChildOf(AHHHeistGameMode::StaticClass());
}

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
	DOREPLIFETIME(AHHHeistGameState, PoliceState);
	DOREPLIFETIME(AHHHeistGameState, Pings);
	DOREPLIFETIME(AHHHeistGameState, Photos);
}

float AHHHeistGameState::GetServerTime() const
{
	return static_cast<float>(GetServerWorldTimeSeconds());
}

void AHHHeistGameState::AddPing(const FVector& Location, int32 CrewSlot, const FString& Name)
{
	if (!HasAuthority())
	{
		return;
	}
	const float Now = GetServerTime();
	Pings.RemoveAll([Now, CrewSlot](const FHHPing& Ping) { return Now - Ping.Time > PingLifetime || Ping.CrewSlot == CrewSlot; });
	FHHPing Ping;
	Ping.Location = Location;
	Ping.CrewSlot = CrewSlot;
	Ping.Name = Name;
	Ping.Time = Now;
	Pings.Add(Ping);
	OnRep_Pings();
	ForceNetUpdate();
}

void AHHHeistGameState::OnRep_Pings()
{
	// Everyone hears the click of a new mark (the crew carries walkies by default).
	const float Now = GetServerTime();
	const bool bNew = Pings.ContainsByPredicate([Now](const FHHPing& Ping) { return Now - Ping.Time < 2.f; });
	if (bNew && Pings.Num() >= LastPingCount && GetNetMode() != NM_DedicatedServer)
	{
		if (const UHHGameData* GameData = UHHGameData::Get(this))
		{
			if (USoundBase* Chirp = GameData->WalkieChirpSound.LoadSynchronous())
			{
				UGameplayStatics::PlaySound2D(this, Chirp);
			}
		}
	}
	LastPingCount = Pings.Num();
	Changed();
}

void AHHHeistGameState::AddPhoto()
{
	if (HasAuthority())
	{
		++Photos;
		Changed();
		ForceNetUpdate();
	}
}

void AHHHeistGameState::MulticastCameraFlash_Implementation(FVector Location, FVector Direction)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (const UHHGameData* GameData = UHHGameData::Get(this))
	{
		if (USoundBase* Shutter = GameData->CameraFlashSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Shutter, Location);
		}
	}
	// A brief, very bright point light a little in front of the lens.
	UPointLightComponent* Flash = NewObject<UPointLightComponent>(this);
	Flash->SetMobility(EComponentMobility::Movable);
	Flash->SetIntensityUnits(ELightUnits::Candelas);
	Flash->SetIntensity(9000.f);
	Flash->SetAttenuationRadius(1400.f);
	Flash->SetLightColor(FLinearColor(0.95f, 0.97f, 1.f));
	Flash->SetCastShadows(true);
	Flash->RegisterComponent();
	Flash->SetWorldLocation(Location + Direction.GetSafeNormal() * 40.f);
	TWeakObjectPtr<UPointLightComponent> WeakFlash(Flash);
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakFlash]()
	{
		if (UPointLightComponent* Light = WeakFlash.Get())
		{
			Light->DestroyComponent();
		}
	}), 0.12f, false);
}

void AHHHeistGameState::SetPoliceState(EHHPoliceState NewState)
{
	if (HasAuthority() && PoliceState != NewState)
	{
		PoliceState = NewState;
		Changed();
	}
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
