#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/HHTypes.h"
#include "HHLobbyGameState.generated.h"

class AHHPlayerState;

DECLARE_MULTICAST_DELEGATE(FHHOnLobbyStateChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHHOnCrewMessage, const FText& /*Message*/, EHHNotifyType /*Type*/);

/** Replicated lobby state: chosen job, departure countdown and crew announcements. */
UCLASS()
class HORRORHEIST_API AHHLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AHHLobbyGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	FName GetSelectedMission() const { return SelectedMission; }
	EHHLobbyPhase GetPhase() const { return Phase; }
	/** Seconds until departure (0 when no countdown is running). */
	float GetCountdownRemaining() const;

	TArray<AHHPlayerState*> GetCrew() const;
	bool IsEveryoneReady() const;
	int32 NumReady() const;

	// --- Server ---
	void SetSelectedMission(FName MissionId);
	void SetPhase(EHHLobbyPhase NewPhase, float CountdownSeconds = 0.f);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCrewMessage(const FText& Message, EHHNotifyType Type);

	FHHOnLobbyStateChanged OnMissionChanged;
	FHHOnLobbyStateChanged OnPhaseChanged;
	FHHOnLobbyStateChanged OnCrewChanged;
	FHHOnCrewMessage OnCrewMessage;

private:
	UFUNCTION()
	void OnRep_SelectedMission();

	UFUNCTION()
	void OnRep_Phase();

	UPROPERTY(ReplicatedUsing = OnRep_SelectedMission)
	FName SelectedMission;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EHHLobbyPhase Phase = EHHLobbyPhase::Gathering;

	/** Server world time at which the crew departs. */
	UPROPERTY(Replicated)
	double DepartureServerTime = 0.0;
};
