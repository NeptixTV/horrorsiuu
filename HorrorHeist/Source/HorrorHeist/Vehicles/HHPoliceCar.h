#pragma once

#include "CoreMinimal.h"
#include "Vehicles/HHCarBase.h"
#include "HHPoliceCar.generated.h"

class UPointLightComponent;

UENUM()
enum class EHHPoliceCarMode : uint8
{
	/** Out of town: hidden, no collision. */
	Parked,
	/** Drives the patrol route once, looking for people outside. */
	Patrol,
	/** Lights and siren, on the way to the house. */
	Respond,
	/** Stopped at the house. */
	AtScene,
	/** On the van's tail. */
	Chase,
	/** Lost the van; driving to where it was last seen. */
	Search,
	/** Driving out of town. */
	Leave
};

/**
 * The Hollowmere patrol car (server AI on top of the kinematic car). It follows the road nodes,
 * sees what its headlights and windscreen would, and reports to the heist game mode, which
 * owns the rules (what being seen means, who is caught). Placed parked at the edge of town.
 */
UCLASS()
class HORRORHEIST_API AHHPoliceCar : public AHHCarBase
{
	GENERATED_BODY()

public:
	AHHPoliceCar();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AHHPoliceCar* Get(const UObject* WorldContextObject);

	// ---- Server orders (heist game mode) -----------------------------------------------

	/** Drive the patrol route once and leave. */
	void StartPatrol();
	/** Lights and siren to Scene; a nearer car takes over if it is not there after DeadlineSeconds. */
	void Respond(const FVector& InScene, float DeadlineSeconds);
	void Chase(AActor* Target);
	/** Stop where it is (the patrol saw something). */
	void HoldPosition();
	void Leave();

	EHHPoliceCarMode GetMode() const { return Mode; }
	bool HasArrived() const { return Mode == EHHPoliceCarMode::AtScene; }
	/** Seconds since the chased target was last seen (0 while in sight). */
	float GetTimeWithoutSight() const { return TimeWithoutSight; }
	const FVector& GetLastKnownLocation() const { return LastKnown; }

	/** Line of sight from the car within range and (if MaxAngle < 180) in front of it. */
	bool CanSee(const AActor* Target, float MaxDistance, float MaxAngleDegrees) const;

protected:
	virtual void GetDriveInput(float& OutThrottle, float& OutSteer, bool& bOutHandbrake) const override;
	virtual bool IsEngineRunning() const override { return Mode != EHHPoliceCarMode::Parked; }

	void TickAI(float DeltaSeconds);
	/** Steers along Path; false when there was nothing to drive to this frame. */
	bool FollowPath();
	void SetMode(EHHPoliceCarMode NewMode);
	void SetPath(const TArray<FVector>& Points);
	void PathTo(const FVector& Goal);
	void SteerTowards(const FVector& Point, float DesiredSpeed);
	void Park();

	UFUNCTION()
	void OnRep_Signals();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Police")
	TObjectPtr<UPointLightComponent> LightRed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Police")
	TObjectPtr<UPointLightComponent> LightBlue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Police")
	TObjectPtr<UAudioComponent> SirenAudio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Police")
	TObjectPtr<USoundBase> SirenLoop;

	/** Road node ids the patrol drives along, in order (ends out of town). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Police")
	TArray<FName> PatrolRoute;

	/** Node where the car enters / leaves town (it parks here, out of sight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Police")
	FName HomeNode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Police")
	float PatrolSpeed = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Police")
	float ResponseSpeed = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Police")
	float ChaseSpeed = 1650.f;

	UPROPERTY(ReplicatedUsing = OnRep_Signals)
	bool bFlashing = false;

	UPROPERTY(ReplicatedUsing = OnRep_Signals)
	bool bSiren = false;

	UPROPERTY(ReplicatedUsing = OnRep_Signals)
	EHHPoliceCarMode Mode = EHHPoliceCarMode::Parked;

private:
	TArray<FVector> Path;
	int32 PathIndex = 0;
	TWeakObjectPtr<AActor> ChaseTarget;
	FVector LastKnown = FVector::ZeroVector;
	FVector Scene = FVector::ZeroVector;
	FTransform HomeTransform;
	double Deadline = 0.0;
	float TimeWithoutSight = 0.f;
	float RepathTimer = 0.f;
	float StuckTime = 0.f;
	float ReverseTime = 0.f;
	float FlashTimer = 0.f;
	float AIThrottle = 0.f;
	float AISteer = 0.f;
	bool bAIHandbrake = true;
	bool bFlashPhase = false;
};
