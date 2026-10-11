#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "HHDoorActor.generated.h"

class USoundBase;

/** What using a door will do for a given player right now. */
enum class EHHDoorAction : uint8
{
	Open,
	Close,
	UnlockInside,
	UnlockKey,
	Pick,
	CutGlass,
	Pry,
	Rattle,
	LockedOutsideJob
};

/**
 * Hinged door / locker door. The server owns the target angle; clients swing smoothly.
 * Opens away from whoever uses it. The ambience director can nudge doors ajar while
 * nobody is looking.
 *
 * Locks: on a job a locked door can be opened from the inside (thumb turn), with the key
 * if the crew found it, or with the right tool from the inventory - lockpicks (slow, quiet),
 * a glass cutter on glazed doors (quiet) or a crowbar (fast, loud; the door stays broken).
 * Outside a job locked doors stay locked (houses in the town hub are only cased by day).
 */
UCLASS()
class HORRORHEIST_API AHHDoorActor : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHDoorActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual FText GetInteractionDetail(AActor* Interactor) const override;
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
	virtual float GetInteractDuration(AActor* Interactor) const override;
	virtual USoundBase* GetHoldSound(AActor* Interactor) const override;

	EHHDoorAction ResolveAction(AActor* Interactor) const;
	bool IsLocked() const { return bLocked; }
	/** Server: lock state changes (haunting never locks a door - that would block the job). */
	void SetLocked(bool bNewLocked);

	/** Server: move the door to an angle at a given speed (deg/s) without user input. */
	void SetTargetAngle(float Angle, float Speed, bool bPlayCreak);

	bool IsOpen() const { return !FMath::IsNearlyZero(TargetAngle, 1.f); }
	float GetCurrentAngle() const { return CurrentAngle; }

protected:
	UFUNCTION()
	void OnRep_TargetAngle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	float OpenAngle = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	float SwingSpeed = 160.f;

	/** Only opens in one direction (lockers, cabinets). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	bool bOneWay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FText OpenPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FText ClosePrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> CloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> CreakSound;

	// ---- Lock ----------------------------------------------------------------------------

	/** Which side counts as inside (thumb turn): the door's +Y (true) or -Y side. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Lock")
	bool bInsideIsPositiveY = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Lock")
	bool bUnlockFromInside = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Lock")
	bool bCanPick = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Lock")
	bool bCanPry = false;

	/** Glazed door: a glass cutter opens a hole to reach the lock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Lock")
	bool bCanCutGlass = false;

	/** Key that opens it (found somewhere in the yard / house), None = no key exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Lock")
	FName KeyId;

	/** Base times; better tools (higher "Speed" stat) are faster. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Lock")
	float PickSeconds = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Lock")
	float PrySeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Lock")
	float CutSeconds = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> LockedSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> UnlockSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> PickLoopSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> PrySound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Audio")
	TObjectPtr<USoundBase> GlassCutSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Lock", ReplicatedUsing = OnRep_Lock)
	bool bLocked = false;

	UFUNCTION()
	void OnRep_Lock();

	void OpenAwayFrom(AActor* Interactor);
	bool IsOnJob() const;
	float ToolSeconds(AActor* Interactor, FName ToolTag, float BaseSeconds, float ReferenceSpeed) const;
	void PlayForEveryone(USoundBase* Sound) const;

private:
	UPROPERTY(ReplicatedUsing = OnRep_TargetAngle)
	float TargetAngle = 0.f;

	UPROPERTY(Replicated)
	float ReplicatedSpeed = 160.f;

	UPROPERTY(Replicated)
	bool bReplicatedCreak = false;

	/** Forced with a crowbar: the latch is gone, it can never lock again. */
	UPROPERTY(Replicated)
	bool bBroken = false;

	float CurrentAngle = 0.f;
	float PreviousTarget = 0.f;
};
