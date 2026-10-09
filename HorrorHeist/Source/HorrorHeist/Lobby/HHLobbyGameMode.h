#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HHLobbyGameMode.generated.h"

class AHHPlayerController;
class AHHPlayerState;
class AHHLobbyGameState;

/**
 * Hideout rules (server only): crew slots, job selection by the crew leader, ready checks
 * and the departure countdown that travels the crew to the job's map.
 */
UCLASS()
class HORRORHEIST_API AHHLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHHLobbyGameMode();

	virtual void BeginPlay() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Called by player controllers after validating the request. */
	void HandleReadyChanged(AHHPlayerState* PlayerState);
	void HandleMissionRequest(AHHPlayerController* Requester, FName MissionId);
	void HandleProfileReceived(AHHPlayerController* Controller);

protected:
	void EvaluateDeparture();
	void StartCountdown();
	void CancelCountdown(const FText& Reason);
	void CountdownTick();
	void Depart();
	void ResetReadyStates();
	int32 FindFreeCrewSlot() const;
	AHHLobbyGameState* GetLobbyState() const;

	FTimerHandle CountdownTimer;
	int32 LastAnnouncedSecond = -1;
};
