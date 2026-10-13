#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHCarBase.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UStaticMesh;
class USpotLightComponent;
class UAudioComponent;
class USoundBase;

/** What the moving machine tells everybody else about a car. Pitch/roll come from the ground locally. */
USTRUCT()
struct FHHCarNetState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	UPROPERTY()
	float Yaw = 0.f;

	/** cm/s along the car's forward axis (negative = reversing). */
	UPROPERTY()
	float Speed = 0.f;

	/** -1..1 */
	UPROPERTY()
	float Steer = 0.f;
};

/**
 * A simple kinematic car (no physics simulation, no wheel skeleton): bicycle-model steering,
 * four ground traces for height/pitch/roll, a swept box against walls and other cars. One machine
 * moves it - the server for AI cars, the driver's PC for the van - and the others smooth towards
 * the replicated state. Good enough for a small town at night, cheap, and deterministic to tune.
 *
 * Actor space: +X forward, +Y right; the root box sits CenterHeight above the wheel contact plane.
 */
UCLASS(Abstract)
class HORRORHEIST_API AHHCarBase : public AActor
{
	GENERATED_BODY()

public:
	AHHCarBase();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Signed speed along the forward axis (cm/s). */
	float GetSpeed() const { return Speed; }
	float GetSpeedKmh() const { return FMath::Abs(Speed) * 0.036f; }
	bool AreHeadlightsOn() const { return bHeadlights; }
	/** Server. */
	void SetHeadlights(bool bOn);
	/** Point on the road under the middle of the car. */
	FVector GetGroundLocation() const;
	FVector GetFlatForward() const;

protected:
	/** True on the machine that moves this car. */
	virtual bool IsLocallySimulated() const { return HasAuthority(); }
	/** Input for this frame on the simulating machine: throttle -1..1, steer -1..1. */
	virtual void GetDriveInput(float& OutThrottle, float& OutSteer, bool& bOutHandbrake) const;
	virtual bool IsEngineRunning() const { return true; }
	/** A wall / car / person stopped the car (simulating machine). */
	virtual void OnBumped(const FHitResult& Hit, float ImpactSpeed);
	/** Simulating machine, after each step (the van sends its state to the server here). */
	virtual void PostSimulate(float DeltaSeconds) {}

	void Simulate(float DeltaSeconds, float Throttle, float SteerInput, bool bHandbrake);
	void MoveSwept(const FVector& Delta);
	void FollowGround(float DeltaSeconds, bool bAllowFall);
	bool TraceGround(const FVector& Point, float TopZ, float& OutZ) const;
	void UpdateVisuals(float DeltaSeconds);
	/** Authority: copy the current transform into the replicated state. */
	void PublishState();
	void SmoothToNetState(float DeltaSeconds);
	void ApplyMeshes();

	UFUNCTION()
	void OnRep_NetState();

	UFUNCTION()
	void OnRep_Headlights();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<UStaticMeshComponent> WheelFL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<UStaticMeshComponent> WheelFR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<UStaticMeshComponent> WheelRL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<UStaticMeshComponent> WheelRR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<USpotLightComponent> HeadlightL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<USpotLightComponent> HeadlightR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<UAudioComponent> EngineAudio;

	// ---- Look (set by the level layout) ------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Look")
	TObjectPtr<UStaticMesh> BodyMesh;

	/** Optional separate wheel mesh (axle along its X, outer face +X). Empty = wheels are part of the body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Look")
	TObjectPtr<UStaticMesh> WheelMesh;

	/** Yaw that turns the mesh's front onto the actor's +X (the generated meshes face -Y). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Look")
	float MeshYaw = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Look")
	TObjectPtr<USoundBase> EngineLoop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Look")
	TObjectPtr<USoundBase> BumpSound;

	// ---- Shape -------------------------------------------------------------------------

	/** Half size of the collision box (its bottom stays above curbs and ramps). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Shape")
	FVector BoxExtent = FVector(235.f, 96.f, 72.f);

	/** Wheel contact plane -> box centre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Shape")
	float CenterHeight = 127.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Shape")
	float Wheelbase = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Shape")
	float TrackWidth = 176.f;

	/** Rear axle offset from the centre (+X forward); the van's axles are at -155 / +145. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Shape")
	float AxleOffset = -5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Shape")
	float WheelRadius = 37.5f;

	// ---- Handling ----------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Handling")
	float MaxForwardSpeed = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Handling")
	float MaxReverseSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Handling")
	float Acceleration = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Handling")
	float BrakeDeceleration = 1300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Handling")
	float CoastDeceleration = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Handling")
	float MaxSteerAngle = 34.f;

	/** How quickly the wheels follow the steering input (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car|Handling")
	float SteerResponse = 4.5f;

	UPROPERTY(ReplicatedUsing = OnRep_NetState)
	FHHCarNetState NetState;

	UPROPERTY(ReplicatedUsing = OnRep_Headlights)
	bool bHeadlights = false;

	float Speed = 0.f;
	float Steer = 0.f;
	float VerticalSpeed = 0.f;
	float WheelSpin = 0.f;
	float PublishTimer = 0.f;
	double LastNetStateTime = 0.0;
	bool bHasNetState = false;
	bool bAirborne = false;
};
