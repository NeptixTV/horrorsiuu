#pragma once

#include "CoreMinimal.h"
#include "Core/HHGameModeBase.h"
#include "Core/HHTypes.h"
#include "HHHeistGameMode.generated.h"

class AHHHeistGameState;
class AHHHauntDirector;
class AHHHouseVolume;
class AHHVanPoint;
class AHHDoorActor;
class AHHPlayerController;
class AHHPlayerState;
class UHHMissionDefinition;
class USoundBase;

/**
 * Rules of a job (server only). The crew arrives by van at night, finds a way in, carries loot
 * to the van and drives off - without getting caught by the police (noise raises the
 * neighbourhood alert) and before the house turns on them.
 *
 * Everything mission specific comes from the mission data asset (minimum haul, target,
 * aggression) and from tagged actors in the map (house volumes, van points, keys, haunt
 * elements), so new houses need no new code. World actors call the Handle* functions.
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
	void HandleDriveAway(AController* By, USoundBase* Sound, const FVector& Location);
	/** Psyche hit zero or the thing in the house caught them: drop the loot, wake up outside. */
	void HandlePlayerBlackout(APawn* Pawn, bool bCaughtByEntity);
	/** Crew leader from the pause menu. */
	void HandleAbandon(AHHPlayerController* Requester);

	/** Debug (console): end now with this outcome. */
	void DebugEnd(EHHMissionOutcome Outcome);

protected:
	void ResolveMission(const FString& Options);
	void SetupObjectives();
	void TickRules();
	void UpdateHaulObjective();
	void HandleNoise(const FVector& Location, float Loudness, AActor* NoiseMaker, FName Tag);
	void RaiseAlert(float Amount, const FText& Reason);
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

private:
	FName MissionId;
	TWeakObjectPtr<const UHHMissionDefinition> Mission;
	TWeakObjectPtr<AHHHauntDirector> Haunt;
	TArray<TWeakObjectPtr<AHHHouseVolume>> HouseVolumes;
	TWeakObjectPtr<AHHVanPoint> DrivePoint;
	TSet<TWeakObjectPtr<AHHPlayerState>> InitializedInventories;
	TArray<FHHLootRecord> Stashed;
	FDelegateHandle NoiseHandle;
	FTimerHandle RulesTimer;
	FTimerHandle ReturnTimer;
	double StartTime = 0.0;
	float Alert = 0.f;
	int32 MinimumHaul = 1500;
	bool bEntered = false;
	bool bTargetStashed = false;
	bool bHasKeyObjective = false;
	bool bHasTargetObjective = false;
	bool bEnded = false;
};
