// SIGNAL UNKNOWN – the creatures of the forest.
//
//   Observer  tall, thin figure. Watches players from the tree line or the roadside, never attacks.
//             Withdraws when it is looked at for too long, lit up or approached. Rare evidence.
//   Hunter    fast four-limbed hunter. Chases the vehicle / players at night, hits the car,
//             hurts players on foot. Gives up when the car is too fast for a while, when it is
//             blinded by a light, or after a time limit.
//
// Server-side state machine. Movement is direct steering with sweep-based obstacle avoidance
// (the procedural map has no navmesh bounds). Creatures only despawn while no player sees them.
//
// NOT IMPLEMENTED (yet): Mimic (voice imitation) and infected humans – see README.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SUCreature.generated.h"

class UStaticMeshComponent;
class USUEvidenceComponent;
class USUProceduralAudioComponent;
class ASUCharacter;
class ASUVehicle;

UENUM()
enum class ESUCreatureType : uint8
{
	Observer,
	Hunter
};

UENUM()
enum class ESUCreatureState : uint8
{
	Idle,
	Watch,      // stands still, faces its target
	Stalk,      // follows at a distance
	Chase,      // runs at the target
	Attack,     // short wind-up + hit
	Retreat,    // runs away, then despawns once unseen
	Scripted    // driven by a scripted sequence
};

UCLASS()
class SIGNALUNKNOWN_API ASUCreature : public ACharacter
{
	GENERATED_BODY()

public:
	ASUCreature();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: spawns a creature (deferred so the type is set before BeginPlay). */
	static ASUCreature* SpawnCreature(UWorld* World, ESUCreatureType Type, const FVector& Location, float Yaw, ESUCreatureState StartState, AActor* Target);

	// ---------------------------------------------------------------- server
	void SetTarget(AActor* NewTarget) { Target = NewTarget; }
	AActor* GetTarget() const { return Target.Get(); }
	void SetState(ESUCreatureState NewState);
	ESUCreatureState GetState() const { return State; }
	ESUCreatureType GetCreatureType() const { return Type; }
	/** Scripted control: walk/run to a point (Scripted state). */
	void ScriptedMoveTo(const FVector& Location, float Speed);
	void ScriptedFace(const FVector& Location);
	bool HasReachedScriptedGoal(float Tolerance = 80.f) const;
	/** Despawn as soon as no player can see it. */
	void RequestDespawn() { bDespawnRequested = true; }
	/** Max lifetime (seconds) before the creature retreats on its own. */
	void SetLifetime(float Seconds) { Lifetime = Seconds; }

	/** Any player currently has this creature in view (unoccluded, in the view cone). */
	bool IsSeenByAnyPlayer(float MaxDistance = 8000.f, float* OutLookTime = nullptr) const;

	/** Seconds the creature has been in a flashlight beam (decays when unlit). */
	float GetLitTime() const { return LitTime; }

	static int32 CountActive(const UWorld* World);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastVoice(int32 TriggerKind, float Strength);

	UPROPERTY(Replicated) ESUCreatureType Type = ESUCreatureType::Observer;

private:
	UPROPERTY(Replicated) ESUCreatureState State = ESUCreatureState::Idle;
	UPROPERTY(Replicated) float Aggression = 0.f; // drives breathing / growl audio

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Limbs;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> VoiceAudio;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> StepAudio;
	UPROPERTY(VisibleAnywhere, Category = "Creature") TObjectPtr<USUEvidenceComponent> Evidence;

	TWeakObjectPtr<AActor> Target;
	FVector ScriptedGoal = FVector::ZeroVector;
	FVector ScriptedLook = FVector::ZeroVector;
	bool bHasScriptedGoal = false;
	bool bHasScriptedLook = false;
	float ScriptedSpeed = 300.f;

	float StateTime = 0.f;
	float Age = 0.f;
	float Lifetime = 90.f;
	float LookedAtTime = 0.f;
	float LitTime = 0.f;
	float AttackCooldown = 0.f;
	float FastVehicleTime = 0.f;
	float VoiceTimer = 3.f;
	float UnseenTime = 0.f;
	float AnimPhase = 0.f;
	float StepTimer = 0.f;
	bool bDespawnRequested = false;
	bool bAttackHit = false;
	FVector RetreatGoal = FVector::ZeroVector;

	void BuildBody();
	void UpdateObserver(float DeltaSeconds);
	void UpdateHunter(float DeltaSeconds);
	void UpdateRetreat(float DeltaSeconds);
	void UpdateScripted(float DeltaSeconds);
	void UpdatePerception(float DeltaSeconds);
	void Animate(float DeltaSeconds);
	void SteerTowards(const FVector& Goal, float Speed, float DeltaSeconds);
	void FaceTowards(const FVector& Location, float DeltaSeconds, float Rate = 6.f);
	void StartRetreat();
	FVector PickRetreatPoint() const;
	bool IsLitByFlashlight(float& OutStrength) const;
	ASUCharacter* FindNearestPlayer(float& OutDistance) const;
	float GetMaxSpeed() const;
};
