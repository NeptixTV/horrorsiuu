#include "Core/SUGameMode.h"
#include "SignalUnknown.h"
#include "Core/SUGameState.h"
#include "Core/SUPlayerState.h"
#include "Core/SUPlayerController.h"
#include "Core/SUGameInstance.h"
#include "Core/SUSaveSubsystem.h"
#include "Core/SUDeveloperSettings.h"
#include "Player/SUCharacter.h"
#include "UI/SUHUD.h"
#include "World/SUWorldBuilder.h"
#include "Mission/SUMissionManager.h"
#include "Horror/SUHorrorDirector.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASUGameMode::ASUGameMode()
{
	DefaultPawnClass = ASUCharacter::StaticClass();
	PlayerControllerClass = ASUPlayerController::StaticClass();
	PlayerStateClass = ASUPlayerState::StaticClass();
	GameStateClass = ASUGameState::StaticClass();
	HUDClass = ASUHUD::StaticClass();
	bUseSeamlessTravel = false;
}

void ASUGameMode::StartPlay()
{
	if (ASUGameState* GS = GetGameState<ASUGameState>())
	{
		GS->OnWorldReady.AddUObject(this, &ASUGameMode::HandleWorldReady);
	}
	Super::StartPlay();

	// The world may already have been built while BeginPlay was dispatched.
	if (ASUGameState* GS = GetGameState<ASUGameState>())
	{
		if (GS->IsWorldReady() && !bServerActorsSpawned)
		{
			HandleWorldReady();
		}
	}
}

void ASUGameMode::HandleWorldReady()
{
	if (bServerActorsSpawned)
	{
		return;
	}
	SpawnServerActors();

	TArray<TObjectPtr<AController>> Pending = PendingSpawns;
	PendingSpawns.Reset();
	for (AController* C : Pending)
	{
		if (IsValid(C))
		{
			RestartPlayer(C);
		}
	}

	// Host / command line starts go straight into the game; the menu start waits for the player.
	if (USUGameInstance* GI = GetGameInstance<USUGameInstance>())
	{
		if (GI->GetStartMode() == ESUStartMode::Host || GI->GetStartMode() == ESUStartMode::Solo || GetNetMode() == NM_DedicatedServer)
		{
			BeginSession(GI->ShouldLoadSaveOnStart());
		}
	}
}

void ASUGameMode::SpawnServerActors()
{
	bServerActorsSpawned = true;
	ASUGameState* GS = GetGameState<ASUGameState>();
	if (!GS)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!GS->MissionManager)
	{
		GS->MissionManager = GetWorld()->SpawnActor<ASUMissionManager>(ASUMissionManager::StaticClass(), FTransform::Identity, Params);
	}
	if (!HorrorDirector)
	{
		HorrorDirector = GetWorld()->SpawnActor<ASUHorrorDirector>(ASUHorrorDirector::StaticClass(), FTransform::Identity, Params);
	}
}

void ASUGameMode::BeginSession(bool bLoadSave)
{
	if (bSessionStarted)
	{
		return;
	}
	bSessionStarted = true;

	bool bLoaded = false;
	if (bLoadSave)
	{
		FString Message;
		bLoaded = LoadGameNow(Message);
		UE_LOG(LogSignalUnknown, Log, TEXT("Load on start: %s"), *Message);
	}
	if (USUGameInstance* GI = GetGameInstance<USUGameInstance>())
	{
		GI->ClearLoadSaveOnStart();
	}

	if (!bLoaded)
	{
		// New game: the radio starts crackling shortly after the player woke up in the tower.
		if (ASUGameState* GS = GetGameState<ASUGameState>())
		{
			if (GS->MissionManager)
			{
				GS->MissionManager->QueueMission(TEXT("M01_LastTransmission"), 20.f);
			}
		}
	}
	if (HorrorDirector)
	{
		HorrorDirector->SetActive(true);
	}
}

void ASUGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!IsValid(NewPlayer))
	{
		return;
	}
	ASUGameState* GS = GetGameState<ASUGameState>();
	ASUWorldBuilder* Builder = GS ? GS->GetWorldBuilder() : nullptr;
	if (!GS || !GS->IsWorldReady() || !Builder)
	{
		// the procedural world is not generated yet – spawn as soon as it is
		PendingSpawns.AddUnique(NewPlayer);
		return;
	}
	const FTransform Spawn = Builder->GetPlayerSpawnTransform(SpawnCounter++);
	RestartPlayerAtTransform(NewPlayer, Spawn);
}

void ASUGameMode::Logout(AController* Exiting)
{
	PendingSpawns.Remove(Exiting);
	Super::Logout(Exiting);
}

void ASUGameMode::RequestRespawn(AController* Controller, float Delay)
{
	if (!IsValid(Controller))
	{
		return;
	}
	TWeakObjectPtr<AController> Weak(Controller);
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, [this, Weak]()
	{
		AController* C = Weak.Get();
		if (!C)
		{
			return;
		}
		if (APawn* Old = C->GetPawn())
		{
			C->UnPossess();
			Old->Destroy();
		}
		RestartPlayer(C);
	}, FMath::Max(0.1f, Delay), false);
}

bool ASUGameMode::SaveGameNow(FString& OutMessage)
{
	USUSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USUSaveSubsystem>() : nullptr;
	if (!Saves)
	{
		OutMessage = TEXT("Speichersystem nicht verfügbar.");
		return false;
	}
	return Saves->SaveWorld(GetWorld(), OutMessage);
}

bool ASUGameMode::LoadGameNow(FString& OutMessage)
{
	USUSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USUSaveSubsystem>() : nullptr;
	if (!Saves)
	{
		OutMessage = TEXT("Speichersystem nicht verfügbar.");
		return false;
	}
	return Saves->LoadWorld(GetWorld(), OutMessage);
}
