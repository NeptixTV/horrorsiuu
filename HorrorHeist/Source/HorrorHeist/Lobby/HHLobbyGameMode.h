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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Called by player controllers after validating the request. */
	void HandleReadyChanged(AHHPlayerState* PlayerState);
	void HandleMissionRequest(AHHPlayerController* Requester, FName MissionId);
	void HandleSafehouseRequest(AHHPlayerController* Requester, FName ZoneId);
	void HandleProfileReceived(AHHPlayerController* Controller);
	/** Crew leader: skip the clock forward to nightfall (jobs only start after dark). */
	void HandleWaitForNight(AHHPlayerController* Requester);

protected:
	/**
	 * Starts / cancels the countdown and tells the crew why the van can't leave when everyone is
	 * ready but something is missing (no job pinned, job not playable, still daylight).
	 * bAnnounce: say it again even if the reason did not change (a player just pressed ready).
	 */
	void EvaluateDeparture(bool bAnnounce = true);
	/** Once a second: starts the countdown on its own when the crew is ready and night falls. */
	void PollDeparture();
	void StartCountdown();
	void CancelCountdown(const FText& Reason);
	void CountdownTick();
	void Depart();
	void ResetReadyStates();
	AHHLobbyGameState* GetLobbyState() const;
	/** None when the job may leave now, otherwise why not (OutReason: text for the crew). */
	EHHDepartureBlock GetDepartureBlock(const class UHHMissionDefinition* Mission, FText& OutReason) const;
	const class UHHMissionDefinition* GetPinnedMission() const;

	FTimerHandle CountdownTimer;
	FTimerHandle PollTimer;
	EHHDepartureBlock LastAnnouncedBlock = EHHDepartureBlock::None;
	int32 LastAnnouncedSecond = -1;
};
