#pragma once

#include "CoreMinimal.h"
#include "Core/HHGameModeBase.h"
#include "HHLobbyGameMode.generated.h"

class AHHPlayerController;
class AHHPlayerState;
class AHHLobbyGameState;

/**
 * Hideout & town hub rules (server only): job selection by the crew leader, ready checks,
 * the night-time check and the departure countdown that travels the crew to the job's map.
 * Crew slots, leadership and spawning come from AHHGameModeBase.
 */
UCLASS()
class HORRORHEIST_API AHHLobbyGameMode : public AHHGameModeBase
{
	GENERATED_BODY()

public:
	AHHLobbyGameMode();

	virtual void BeginPlay() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Called by player controllers after validating the request. */
	void HandleReadyChanged(AHHPlayerState* PlayerState);
	void HandleMissionRequest(AHHPlayerController* Requester, FName MissionId);
	void HandleProfileReceived(AHHPlayerController* Controller);
	/** Crew leader: skip the clock forward to nightfall (jobs only start after dark). */
	void HandleWaitForNight(AHHPlayerController* Requester);

protected:
	void EvaluateDeparture();
	void StartCountdown();
	void CancelCountdown(const FText& Reason);
	void CountdownTick();
	void Depart();
	void ResetReadyStates();
	AHHLobbyGameState* GetLobbyState() const;
	/** Empty when the job may leave now, otherwise the reason it cannot. */
	FText GetDepartureBlocker(const class UHHMissionDefinition* Mission) const;

	FTimerHandle CountdownTimer;
	int32 LastAnnouncedSecond = -1;
};
