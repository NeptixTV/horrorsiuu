// SIGNAL UNKNOWN – game mode (server only): player spawning, session start, saving.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SUGameMode.generated.h"

class ASUHorrorDirector;

UCLASS()
class SIGNALUNKNOWN_API ASUGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASUGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Starts the actual game: loads the save or begins a new game (first radio call). */
	void BeginSession(bool bLoadSave);
	bool HasSessionStarted() const { return bSessionStarted; }

	/** Respawn a dead / unconscious player at the base after a delay. */
	void RequestRespawn(AController* Controller, float Delay);

	/** Server: save the current state. */
	bool SaveGameNow(FString& OutMessage);
	/** Server: load the save slot (players are moved to the base). */
	bool LoadGameNow(FString& OutMessage);

	ASUHorrorDirector* GetHorrorDirector() const { return HorrorDirector; }

private:
	void HandleWorldReady();
	void SpawnServerActors();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AController>> PendingSpawns;

	UPROPERTY(Transient)
	TObjectPtr<ASUHorrorDirector> HorrorDirector;

	int32 SpawnCounter = 0;
	bool bServerActorsSpawned = false;
	bool bSessionStarted = false;
};
