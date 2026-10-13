#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HHCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UHHCosmeticComponent;
class UHHInventoryComponent;
class UHHInteractionComponent;
class UHHFootstepComponent;
class AHHPlayerState;
class AHHVanVehicle;
struct FInputActionValue;

/**
 * The crew member. True first person for the local player: the whole body is rendered (you see
 * your arms, legs and what you hold), only the head is collapsed while looking through the
 * eyes. Station cameras and the optional third-person view show the complete character.
 * Appearance comes entirely from the replicated player state (cosmetics + loadout); on a job
 * the selected inventory item is held in the right hand (two-bone IK towards the item).
 */
UCLASS()
class HORRORHEIST_API AHHCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHHCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void PawnClientRestart() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void Landed(const FHitResult& Hit) override;

	UHHCosmeticComponent* GetCosmetics() const { return Cosmetics; }
	UHHInteractionComponent* GetInteraction() const { return Interaction; }
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	bool IsSprinting() const { return bSprinting; }
	bool IsFlashlightOn() const { return bFlashlightOn; }
	bool IsThirdPerson() const { return bThirdPerson; }

	/** Camera location/direction used for interaction traces. */
	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** Anim instance queries (game thread). */
	bool ShouldHideHead() const { return bHeadHidden; }
	/** World position the right hand reaches for, and how strongly (0 = arm hangs naturally). */
	bool GetHandTarget(FVector& OutWorldLocation, float& OutAlpha) const;

	void SetThirdPerson(bool bEnable);
	/** Hide this pawn on the local screen only (e.g. while the mannequin preview is shown). */
	void SetLocallyHidden(bool bInHidden);

	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerBeginHold(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void ServerSetFlashlight(bool bNewOn);

	// ---- The van ------------------------------------------------------------------------

	AHHVanVehicle* GetVehicle() const { return Vehicle; }
	int32 GetSeatIndex() const { return SeatIndex; }
	bool IsSeated() const { return Vehicle != nullptr; }
	bool IsDriving() const { return Vehicle != nullptr && SeatIndex == 0; }

	/** Server (called by the van): sit down in / get up from a seat. */
	void SetSeat(AHHVanVehicle* InVehicle, int32 InSeat);

	UFUNCTION(Server, Reliable)
	void ServerExitVehicle();

	/** Owner: put the body where the server let it out (no waiting for a movement correction). */
	UFUNCTION(Client, Reliable)
	void ClientExitedVehicle(FVector Location, FRotator Rotation);

	/** Server: taken away by the police (hidden, frozen until the job ends). */
	void SetArrested(bool bInArrested);
	bool IsArrested() const { return bArrested; }

protected:
	// Input
	void Input_Move(const FInputActionValue& Value);
	void Input_MoveCompleted(const FInputActionValue& Value);
	void Input_Horn();
	void Input_Look(const FInputActionValue& Value);
	void Input_LookGamepad(const FInputActionValue& Value);
	void Input_JumpStarted();
	void Input_JumpCompleted();
	void Input_SprintStarted();
	void Input_SprintCompleted();
	void Input_CrouchStarted();
	void Input_CrouchCompleted();
	void Input_Interact();
	void Input_InteractReleased();
	void Input_Slot1();
	void Input_Slot2();
	void Input_Slot3();
	void Input_Slot4();
	void Input_Slot5();
	void Input_SlotNext();
	void Input_SlotPrev();
	void Input_Drop();
	void SelectSlot(int32 Index);
	void Input_Flashlight();
	void Input_ToggleView();
	void ApplyLookDelta(float YawDegrees, float PitchDegrees);

	void SetSprinting(bool bNewSprinting);
	void ApplyMovementSpeed();

	void ApplyCharacterDefinition();
	void BindToPlayerState();
	void UnbindFromPlayerState();
	void HandleCosmeticsChanged(AHHPlayerState* State);
	void HandleEquipmentChanged(AHHPlayerState* State);
	void HandleInventoryChanged(UHHInventoryComponent* Inventory);
	void UpdateFlashlightFromLoadout();
	void UpdateHeldItem();
	/** Light item the player can use right now (inventory on a job, loadout in the hub). */
	FName GetUsableLightId() const;
	void ReportMissingBody(const FString& Reason);
	void UpdateLocalVisibility();
	void UpdateCamera(float DeltaSeconds);
	void UpdateFlashlightAim();
	void HandleSettingsApplied();

	UFUNCTION()
	void OnRep_Flashlight();

	UFUNCTION()
	void OnRep_Sprinting();

	UFUNCTION()
	void OnRep_Seat();

	/** Hidden, no collision, no movement while seated or arrested (every machine). */
	void ApplyBodyState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> ThirdPersonBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> FlashlightPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> Flashlight;

	/** The selected item in the hand (follows the aim with the flashlight pivot). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeldItem;

	/** Simple stand-in shown only when the body mesh could not be loaded (never invisible). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FallbackBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHHCosmeticComponent> Cosmetics;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHHInteractionComponent> Interaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHHFootstepComponent> Footsteps;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float WalkSpeed = 175.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeed = 410.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float CrouchSpeed = 125.f;

	/** Eye height above the capsule centre while standing / crouched. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float StandingEyeZ = 70.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CrouchedEyeZ = 50.f;

	/** Eyes sit in front of the capsule centre; keeps the camera out of the (hidden) head and neck. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float EyeForward = 11.f;

	/** Bulky loot slows its carrier by this factor and blocks sprinting. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float HeavyCarrySpeedFactor = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float LookRateYaw = 170.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float LookRatePitch = 120.f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Flashlight)
	bool bFlashlightOn = false;

	UPROPERTY(ReplicatedUsing = OnRep_Sprinting)
	bool bSprinting = false;

	UPROPERTY(ReplicatedUsing = OnRep_Seat)
	TObjectPtr<AHHVanVehicle> Vehicle;

	UPROPERTY(ReplicatedUsing = OnRep_Seat)
	int32 SeatIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Seat)
	bool bArrested = false;

	bool bBodyStowed = false;

	bool bThirdPerson = false;
	bool bLocallyHidden = false;
	bool bWantsCrouchHeld = false;
	bool bHeadHidden = false;
	bool bCarryingHeavy = false;
	bool bReportedMissingBody = false;
	float HandAlpha = 0.f;

	float CameraZ = 70.f;
	float CameraTargetZ = 70.f;
	float BobPhase = 0.f;
	float BobBlend = 0.f;
	float LandingDip = 0.f;

	TWeakObjectPtr<AHHPlayerState> BoundPlayerState;
	FDelegateHandle CosmeticsHandle;
	FDelegateHandle EquipmentHandle;
	FDelegateHandle InventoryHandle;
	FDelegateHandle SettingsHandle;
};
