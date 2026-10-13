#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/HHTypes.h"
#include "HHLobbyGameState.generated.h"

class AHHPlayerState;

DECLARE_MULTICAST_DELEGATE(FHHOnLobbyStateChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHHOnCrewMessage, const FText& /*Message*/, EHHNotifyType /*Type*/);

/**
 * Replicated lobby state: chosen job, escape plan, departure countdown, the crew's intel notebook,
 * the lobby code and crew announcements. The job state derives from it, so all of this is
 * available on jobs too.
 */
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

	/** Why the van would not leave right now (None = it would). */
	EHHDepartureBlock GetDepartureBlock() const { return DepartureBlock; }

	/** Escape zone (AHHEscapeZone::ZoneId) the van drives to at the end of the job. */
	FName GetSafehouse() const { return Safehouse; }

	/** Intel the crew has learned by scouting (ids from the mission definitions). */
	const TArray<FName>& GetKnownIntel() const { return KnownIntel; }
	bool IsIntelKnown(FName IntelId) const { return KnownIntel.Contains(IntelId); }

	/** Code friends type to join this crew ("K7Q2MX"); empty while the hideout is private. */
	const FString& GetLobbyCode() const { return LobbyCode; }

	TArray<AHHPlayerState*> GetCrew() const;
	bool IsEveryoneReady() const;
	int32 NumReady() const;

	// --- Server ---
	void SetSelectedMission(FName MissionId);
	void SetPhase(EHHLobbyPhase NewPhase, float CountdownSeconds = 0.f);
	void SetDepartureBlock(EHHDepartureBlock NewBlock);
	void SetSafehouse(FName ZoneId);
	/** Adds intel to the notebook; true when it was new. Also remembered in the host's profile. */
	bool LearnIntel(FName IntelId);
	/** Merges a list (host profile at map start, a joining player's notebook). Silent. */
	void MergeIntel(const TArray<FName>& IntelIds);
	/** Development: empty notebook. */
	void ClearIntel();
	void SetLobbyCode(const FString& NewCode);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCrewMessage(const FText& Message, EHHNotifyType Type);

	FHHOnLobbyStateChanged OnMissionChanged;
	FHHOnLobbyStateChanged OnPhaseChanged;
	FHHOnLobbyStateChanged OnCrewChanged;
	FHHOnLobbyStateChanged OnIntelChanged;
	FHHOnCrewMessage OnCrewMessage;

private:
	UFUNCTION()
	void OnRep_SelectedMission();

	UFUNCTION()
	void OnRep_Phase();

	UFUNCTION()
	void OnRep_KnownIntel();

	UPROPERTY(ReplicatedUsing = OnRep_SelectedMission)
	FName SelectedMission;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EHHLobbyPhase Phase = EHHLobbyPhase::Gathering;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EHHDepartureBlock DepartureBlock = EHHDepartureBlock::NoJob;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedMission)
	FName Safehouse;

	UPROPERTY(ReplicatedUsing = OnRep_KnownIntel)
	TArray<FName> KnownIntel;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	FString LobbyCode;

	/** Server world time at which the crew departs. */
	UPROPERTY(Replicated)
	double DepartureServerTime = 0.0;
};
