#pragma once

#include "CoreMinimal.h"
#include "Vehicles/HHCarBase.h"
#include "Interaction/HHInteractable.h"
#include "HHVanVehicle.generated.h"

class AHHCharacter;
class UCameraComponent;
class USpringArmComponent;

/**
 * The crew's van. Everyone can get in (driver + three seats); the driver steers it with the
 * movement keys (Space = handbrake, F = headlights, H = horn, G = garage door remote, E = get out).
 * Seated crew are hidden inside and see the van from a chase camera they can look around with.
 *
 * The driver's machine simulates the van and streams its state to the server, which passes it on
 * (co-op trust, with a sanity check). Without a driver the server lets it roll to a stop.
 * Interactables placed next to the van in the level (ready station, rear-door stash point) are
 * attached to it at start, so they come along.
 */
UCLASS()
class HORRORHEIST_API AHHVanVehicle : public AHHCarBase, public IHHInteractable
{
	GENERATED_BODY()

public:
	static constexpr int32 NumSeats = 4;

	AHHVanVehicle();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The crew's van in this world (first one found). */
	static AHHVanVehicle* Get(const UObject* WorldContextObject);

	// IHHInteractable
	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual FText GetInteractionDetail(AActor* Interactor) const override;
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
	virtual void SetInteractionFocus(bool bFocused) override;
	virtual float GetInteractDuration(AActor* Interactor) const override { return 0.f; }

	// ---- Seats (server) ----------------------------------------------------------------

	bool TryEnter(AHHCharacter* Character, int32 PreferredSeat);
	void Exit(AHHCharacter* Character);

	AHHCharacter* GetOccupant(int32 Seat) const;
	AHHCharacter* GetDriver() const { return GetOccupant(0); }
	int32 GetNumOccupants() const;
	bool IsOccupant(const AActor* Actor) const;
	/** Seat position relative to the van (actor space). */
	FVector GetSeatOffset(int32 Seat) const;
	/** Seconds the van has been standing still (server). */
	float GetStoppedSeconds() const { return StoppedSeconds; }

	/** Allowed to drive at all (the heist mode locks it once the job is over). */
	void SetDrivable(bool bInDrivable) { bDrivable = bInDrivable; }

	// ---- Driver's machine --------------------------------------------------------------

	/** Axis from the movement keys: X = steer, Y = throttle. Zero when released. */
	void SetDriverInput(const FVector2D& Axis);
	void SetHandbrake(bool bOn);

	UFUNCTION(Server, Reliable)
	void ServerToggleHeadlights();

	UFUNCTION(Server, Reliable)
	void ServerHorn();

	/** Opens / closes the garage door the van is near (hideout). */
	UFUNCTION(Server, Reliable)
	void ServerGarageRemote();

protected:
	virtual bool IsLocallySimulated() const override;
	virtual void GetDriveInput(float& OutThrottle, float& OutSteer, bool& bOutHandbrake) const override;
	virtual bool IsEngineRunning() const override;
	virtual void PostSimulate(float DeltaSeconds) override;
	virtual void OnBumped(const FHitResult& Hit, float ImpactSpeed) override;

	/** Driver -> server, ~30 times a second. */
	UFUNCTION(Server, Unreliable)
	void ServerDriveState(FVector_NetQuantize10 InLocation, float InYaw, float InSpeed, float InSteer);

	/** Server -> driver: the van was somewhere else (only after a big desync). */
	UFUNCTION(Client, Reliable)
	void ClientCorrect(FVector InLocation, float InYaw);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSound(USoundBase* Sound, float Volume);

	UFUNCTION()
	void OnRep_Seats();

	void AdoptAttachments();
	void UpdateChaseCamera(float DeltaSeconds);
	bool FindExitSpot(const AHHCharacter* Character, int32 Seat, FVector& OutLocation) const;
	int32 SeatFor(const AActor* Interactor) const;
	bool IsOnJob() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UStaticMeshComponent> LiveryL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UStaticMeshComponent> LiveryR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UStaticMeshComponent> SlidingDoor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<USpringArmComponent> ChaseArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Look")
	TObjectPtr<UStaticMesh> LiveryMesh;

	/** Empty where a ready station with the door mesh is attached instead (hideout). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Look")
	TObjectPtr<UStaticMesh> SlidingDoorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Sound")
	TObjectPtr<USoundBase> DoorSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Sound")
	TObjectPtr<USoundBase> EngineStartSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Sound")
	TObjectPtr<USoundBase> HornSound;

	/** Interactables within this radius at start ride along with the van. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van")
	float AdoptRadius = 420.f;

	/** How far the garage remote reaches (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van")
	float RemoteRange = 2500.f;

	/** Index = seat (0 = driver). Null = free. */
	UPROPERTY(ReplicatedUsing = OnRep_Seats)
	TArray<TObjectPtr<AHHCharacter>> Seats;

private:
	FVector2D DriverInput = FVector2D::ZeroVector;
	bool bHandbrakeInput = false;
	bool bDrivable = true;
	float SendTimer = 0.f;
	float StoppedSeconds = 0.f;
	float LastViewYaw = 0.f;
	bool bHasLastViewYaw = false;
};
