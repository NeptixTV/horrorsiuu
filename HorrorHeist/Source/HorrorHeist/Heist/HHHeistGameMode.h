#pragma once

#include "CoreMinimal.h"
#include "Core/HHGameModeBase.h"
#include "Core/HHTypes.h"
#include "HHHeistGameMode.generated.h"

class AHHHeistGameState;
class AHHHauntDirector;
class AHHHouseVolume;
class AHHVanPoint;
class AHHVanVehicle;
class AHHPoliceCar;
class AHHNeighbourNPC;
class AHHEscapeZone;
class AHHDoorActor;
class AHHPlayerController;
class AHHPlayerState;
class UHHMissionDefinition;
class USoundBase;

/**
 * Rules of a job (server only). The crew arrives by van at night, finds a way in, carries loot
 * to the van and drives it to the getaway spot picked on the job board - without getting caught.
 * Noise raises the neighbourhood alert: the neighbour comes out to look, a patrol car may pass,
 * and at 100 the police are called (a car drives to the house, arrests whoever is still there and
 * chases the van). Whoever is not in the van when it reaches the getaway spot is left behind.
 *
 * Everything mission specific comes from the mission data asset (minimum haul, target,
 * aggression, intel) and from actors in the map (house volumes, the van, the police car, the
 * neighbour, escape zones, keys, haunt elements), so new houses need no new code.
 */
UCLASS()
class HORRORHEIST_API AHHHeistGameMode : public AHHGameModeBase
{
	GENERATED_BODY()

public:
	AHHHeistGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	const UHHMissionDefinition* GetMission() const;

	// ---- Hooks from world actors and player controllers --------------------------------

	void HandleProfileReceived(AHHPlayerController* Controller);
	void HandleItemPickedUp(AController* By, const FHHInventorySlot& Item, AActor* Source);
	void HandleKeyFound(AController* By, FName KeyId, const FText& KeyName);
	void HandleDoorUnlocked(AController* By, AHHDoorActor* Door, bool bForced, bool bFromInside);
	void HandleStash(AController* By, USoundBase* Sound, const FVector& Location);
	/** Old maps: a "drive away" point at the van ends the job on the spot. */
	void HandleDriveAway(AController* By, USoundBase* Sound, const FVector& Location);
	/** Psyche hit zero or the thing in the house caught them: drop the loot, wake up outside. */
	void HandlePlayerBlackout(APawn* Pawn, bool bCaughtByEntity);
	/** Crew leader from the pause menu. */
	void HandleAbandon(AHHPlayerController* Requester);

	/** Debug (console): end now with this outcome / call the police / send the neighbour. */
	void DebugEnd(EHHMissionOutcome Outcome);
	void DebugCallPolice();
	void DebugSendNeighbour();

protected:
	void ResolveMission(const FString& Options);
	void FindWorldActors();
	void ResolveEscapeZone();
	void ApplyKnownIntel();
	void SetupObjectives();
	void TickRules();
	void TickPolice(float DeltaSeconds);
	void TickEscape();
	void UpdateHaulObjective();
	void UpdateEscapeObjective();
	void HandleNoise(const FVector& Location, float Loudness, AActor* NoiseMaker, FName Tag);
	void RaiseAlert(float Amount, const FText& Reason);
	/** Alert reached 100 (or someone saw the crew): the police are on the way. */
	void CallPolice(float Seconds, const FText& Message);
	void DispatchPoliceCar();
	void HandlePoliceArrived();
	void HandleNeighbourSpotted(APawn* Who);
	void HandleNeighbourGaveUp();
	void SendNeighbour(const FVector& Location);
	void Arrest(AHHPlayerState* State);
	bool IsInVan(const AHHPlayerState* State) const;
	EHHMissionOutcome GetGetawayOutcome() const;
	void EndMission(EHHMissionOutcome Outcome);
	void ReturnToHideout();
	AActor* FindWakePoint(const FVector& Near) const;
	AHHHeistGameState* GetHeistState() const;

	/** Psyche per second inside the house (scaled by darkness, company and the haunt stage). */
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	float PsycheDrainInside = 0.32f;

	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	float PsycheRecoverOutside = 1.6f;

	/** Alert points lost per second while it is quiet. */
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	float AlertDecay = 0.35f;

	/** Bonus on the payout when the primary target was stashed. */
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	float PrimaryTargetBonus = 0.2f;

	/** Extra share the fence keeps for goods from a job the police were called to. */
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	float HotGoodsCut = 0.15f;

	/** The neighbour comes out to look when the alert passes this. */
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	float NeighbourAlert = 45.f;

	/** Seconds after the start when the patrol car may drive by (random in this range). */
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	FVector2D PatrolWindow = FVector2D(150.f, 330.f);

	/** The van must be this far from the house before the police lose interest in the street. */
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	float SceneRadius = 3500.f;

private:
	FName MissionId;
	FName SafehouseId;
	TWeakObjectPtr<const UHHMissionDefinition> Mission;
	TWeakObjectPtr<AHHHauntDirector> Haunt;
	TArray<TWeakObjectPtr<AHHHouseVolume>> HouseVolumes;
	TWeakObjectPtr<AHHVanPoint> DrivePoint;
	TWeakObjectPtr<AHHVanVehicle> Van;
	TWeakObjectPtr<AHHPoliceCar> Police;
	TWeakObjectPtr<AHHNeighbourNPC> Neighbour;
	TWeakObjectPtr<AHHEscapeZone> Escape;
	TSet<TWeakObjectPtr<AHHPlayerState>> InitializedInventories;
	TSet<TWeakObjectPtr<AHHPlayerState>> Arrested;
	TArray<FHHLootRecord> Stashed;
	FDelegateHandle NoiseHandle;
	FDelegateHandle SpottedHandle;
	FDelegateHandle GaveUpHandle;
	FTimerHandle RulesTimer;
	FTimerHandle ReturnTimer;
	FVector HouseCenter = FVector::ZeroVector;
	FVector LastNoise = FVector::ZeroVector;
	double StartTime = 0.0;
	double PoliceCallTime = 0.0;
	double LastEscapeHint = -100.0;
	float PoliceDelay = 0.f;
	float PatrolAt = 0.f;
	float BustTimer = 0.f;
	float Alert = 0.f;
	int32 MinimumHaul = 1500;
	int32 NeighbourVisits = 0;
	bool bEntered = false;
	bool bTargetStashed = false;
	bool bHasKeyObjective = false;
	bool bHasTargetObjective = false;
	bool bHasEscapeObjective = false;
	bool bPatrolDone = false;
	bool bPoliceDispatched = false;
	bool bPoliceArrived = false;
	bool bEnded = false;
};
