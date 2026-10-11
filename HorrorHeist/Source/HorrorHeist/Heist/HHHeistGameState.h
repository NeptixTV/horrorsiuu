#pragma once

#include "CoreMinimal.h"
#include "Lobby/HHLobbyGameState.h"
#include "Core/HHTypes.h"
#include "HHHeistGameState.generated.h"

class USoundBase;

DECLARE_MULTICAST_DELEGATE(FHHOnHeistStateChanged);

/**
 * Replicated state of a job in progress: objectives, the haul in the van, neighbourhood alert
 * and police, how angry the house is, keys the crew found and the end-of-job summary.
 * Derives from the lobby state so the crew list, crew messages and the pinned job (the job
 * being played) work the same way everywhere.
 */
UCLASS()
class HORRORHEIST_API AHHHeistGameState : public AHHLobbyGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EHHHeistPhase GetHeistPhase() const { return HeistPhase; }
	const TArray<FHHObjective>& GetObjectives() const { return Objectives; }
	int32 GetSecuredValue() const { return SecuredValue; }
	int32 GetMinimumHaul() const { return MinimumHaul; }
	/** 0..100 how suspicious the neighbourhood is. 100 = the police were called. */
	float GetAlert() const { return Alert; }
	bool IsPoliceCalled() const { return PoliceArrivalTime > 0.0; }
	/** Seconds until the police arrive (0 when not called). */
	float GetPoliceRemaining() const;
	/** 0..100 and 0..4. */
	float GetHauntLevel() const { return HauntLevel; }
	int32 GetHauntStage() const { return HauntStage; }
	bool HasKey(FName KeyId) const { return Keys.Contains(KeyId); }
	const FHHMissionSummary& GetSummary() const { return Summary; }
	/** Seconds since the crew arrived. */
	float GetElapsed() const;
	int32 GetCrewAtVan() const { return CrewAtVan; }
	int32 GetCrewTotal() const { return CrewTotal; }

	// ---- Server ------------------------------------------------------------------------

	void StartClock();
	void SetHeistPhase(EHHHeistPhase NewPhase);
	void SetObjectives(const TArray<FHHObjective>& NewObjectives);
	void UpdateObjective(FName Id, const FText* NewText, const bool* bNewDone);
	void SetSecured(int32 NewSecured, int32 NewMinimum);
	void SetAlert(float NewAlert);
	void CallPolice(float Seconds);
	void SetHaunt(float Level, int32 Stage);
	void AddKey(FName KeyId);
	void SetSummary(const FHHMissionSummary& NewSummary);
	void SetVanCount(int32 AtVan, int32 Total);

	/** One-shot sound at a location for everybody (reliable enough for feedback, cheap). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSoundAt(USoundBase* Sound, FVector Location, float Volume);

	/** Fired locally whenever any replicated job value changes (HUD refresh). */
	FHHOnHeistStateChanged OnHeistChanged;

private:
	UFUNCTION()
	void OnRep_Heist();

	void Changed();

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	EHHHeistPhase HeistPhase = EHHHeistPhase::Approach;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	TArray<FHHObjective> Objectives;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	int32 SecuredValue = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	int32 MinimumHaul = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	float Alert = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	double PoliceArrivalTime = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	float HauntLevel = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	int32 HauntStage = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	TArray<FName> Keys;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	FHHMissionSummary Summary;

	UPROPERTY(Replicated)
	double StartServerTime = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	int32 CrewAtVan = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Heist)
	int32 CrewTotal = 0;
};
