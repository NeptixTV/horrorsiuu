#pragma once

#include "CoreMinimal.h"
#include "Vehicles/HHCarBase.h"
#include "HHTrafficCar.generated.h"

class AHHRoadNode;

/**
 * Hollowmere's daytime traffic (server AI on the kinematic car). Drives a closed loop of road
 * nodes in the right-hand lane and keeps to the rules of the road: waits at red lights (the
 * junction's AHHTrafficLight phases), stops at stop signs, lets people across zebra crossings,
 * stays behind cars and people in its way - and honks at a crew member standing in the road.
 * At night it drives home (disappears where nobody is looking) and comes back in the morning.
 */
UCLASS()
class HORRORHEIST_API AHHTrafficCar : public AHHCarBase
{
	GENERATED_BODY()

public:
	AHHTrafficCar();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsActive() const { return bActive; }

protected:
	virtual void GetDriveInput(float& OutThrottle, float& OutSteer, bool& bOutHandbrake) const override;
	virtual bool IsEngineRunning() const override { return bActive; }

	void TickAI(float DeltaSeconds);
	/** Lane points for the route (false while the road nodes are not loaded yet). */
	bool BuildLane();
	/** Server: should the car be out on the road right now (day, hub)? */
	bool WantsToDrive() const;
	void SetActive(bool bNewActive);
	/** Server: a place on the route out of every crew member's sight to (re)appear at; false if none. */
	bool FindSpawnSpot(int32& OutIndex) const;
	bool IsNearAnyPlayer(const FVector& Location, float Radius) const;
	/** Distance the front bumper may still drive before it must stand (cm); big when the way is free. */
	float GetFreeDistance(float LookAhead, bool& bOutBlockedByPlayer);
	float GetServerTime() const;

	UFUNCTION()
	void OnRep_Active();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHonk();

	/** Road node ids of the loop, in driving order (the last links back to the first). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	TArray<FName> Route;

	/** Route index where the car starts in the morning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	int32 StartIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float CruiseSpeed = 850.f;

	/** Lane centre to the right of the road nodes (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float LaneOffset = 200.f;

	/** Stop line when the road node has none: how far before a junction node the front bumper stops (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float StopLineDistance = 620.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float StopSignSeconds = 1.6f;

	/** Bends: the lane follows an arc of this radius (cm) through right / left turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float RightTurnRadius = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float LeftTurnRadius = 650.f;

	/** Sideways acceleration allowed in a bend (cm/s^2): sets the speed through the arcs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float BendAcceleration = 300.f;

	/** Comfortable braking (cm/s^2) for planned stops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float ComfortBrake = 520.f;

	/** Off the road at night (the town is asleep) and on job maps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	bool bDayOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	TObjectPtr<USoundBase> HornSound;

	UPROPERTY(ReplicatedUsing = OnRep_Active)
	bool bActive = false;

private:
	struct FLanePoint
	{
		FVector Location = FVector::ZeroVector;
		/** The junction this point turns through (null for a plain bend). */
		TWeakObjectPtr<AHHRoadNode> Node;
		FName FromId;
		/** Signal axis of the approach (0 = along X, 1 = along Y). */
		int32 Axis = 0;
		/** How far beyond this point the junction's centre lies along the approach (cm). */
		float NodeAhead = 0.f;
		/** Radius of the arc this point is on (0 = straight). */
		float Radius = 0.f;
	};

	TArray<FLanePoint> Lane;
	/** Lane index of the first point of each route node (spawn spots). */
	TArray<int32> RouteToLane;
	int32 Target = 0;
	/** The stop sign this car already stopped at (cleared once it passes the junction). */
	int32 ClearedStop = INDEX_NONE;
	float StopTimer = 0.f;
	float BlockedByPlayerTime = 0.f;
	float HonkCooldown = 0.f;
	float StateTimer = 0.f;
	float AIThrottle = 0.f;
	float AISteer = 0.f;
	bool bAIHandbrake = true;
};
