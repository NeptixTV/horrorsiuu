#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HHTypes.h"
#include "HHTownsperson.generated.h"

class USkeletalMeshComponent;
class UHHCosmeticComponent;
class UAnimSequence;

/**
 * The people of Hollowmere: a teller behind the glass, a guard by the vault, a cashier, a kid on
 * the slide, someone walking a loop around the block. They wear the crew's body and cosmetics.
 *
 * Purely local and deterministic (no replication): a walker's position along its path is a
 * function of the game-state clock, so every machine sees them in the same place, and the
 * traffic (server) knows who is on a zebra crossing. Shown by day / night / always; scene members
 * (the morning after a job) are switched on by their AHHAftermathScene.
 */
UCLASS()
class HORRORHEIST_API AHHTownsperson : public AActor
{
	GENERATED_BODY()

public:
	AHHTownsperson();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsPresent() const { return bShown; }
	bool IsWalking() const { return PathPoints.Num() >= 2; }
	FName GetSceneTag() const { return SceneTag; }
	/** Called by an aftermath scene for its members (SceneTag set). */
	void SetSceneActive(bool bActive);

protected:
	void UpdatePresence();
	void GatherPath();
	void UpdateWalk();
	float GetServerTime() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Townsperson")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Townsperson")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Townsperson")
	TObjectPtr<UHHCosmeticComponent> Cosmetics;

	/** Cosmetic item ids they wear (any order; slots come from the items). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	TArray<FName> CosmeticIds;

	/** Animation: Idle, Walk, Run, or a pose of the character definition (Sit, Talk, Guard, Kneel...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	FName Pose = TEXT("Idle");

	/** 1 = adult; children around 0.62. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	float BodyScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	EHHIntelTime VisibleWhen = EHHIntelTime::DayOnly;

	/** Not on job maps (the town around a job is asleep). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	bool bHubOnly = true;

	/** Walks a closed loop through the TargetPoints tagged <PathTag>_0, _1, ... (empty: stands still). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	FName PathTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	float WalkSpeed = 125.f;

	/** 0..1: where on the loop this walker is at clock zero (spreads several walkers on one path). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	float PathOffset = 0.f;

	/** Ground speed the walk/run animation was authored for (cm/s, adult size). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	float AnimSpeed = 150.f;

	/** Member of an aftermath scene: hidden until the scene turns it on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	FName SceneTag;

	/** Slowly turns to face a crew member who comes close (the bank guard, the owner being interviewed). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	bool bWatchPlayers = false;

	/** Rides the AHHSwing with this tag (sits on its seat and swings with it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Townsperson")
	FName SwingName;

private:
	TArray<FVector> PathPoints;
	TArray<float> PathLengths;
	float PathTotal = 0.f;
	float CheckTimer = 0.f;
	float BaseYaw = 0.f;
	bool bShown = true;
	bool bSceneActive = false;
	bool bAnimating = false;
};
