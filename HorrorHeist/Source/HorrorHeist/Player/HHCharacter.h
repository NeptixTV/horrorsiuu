#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HHCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class USpotLightComponent;
class UHHCosmeticComponent;
class UHHInteractionComponent;
class UHHFootstepComponent;
class AHHPlayerState;
struct FInputActionValue;

/**
 * The crew member. First person for the local player (full body still casts shadows and is
 * visible to everyone else), optional third-person view in the hideout to see your outfit.
 * Appearance comes entirely from the replicated player state (cosmetics + loadout).
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

	void SetThirdPerson(bool bEnable);
	/** Hide this pawn on the local screen only (e.g. while the mannequin preview is shown). */
	void SetLocallyHidden(bool bHidden);

	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void ServerSetFlashlight(bool bNewOn);

protected:
	// Input
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_LookGamepad(const FInputActionValue& Value);
	void Input_JumpStarted();
	void Input_JumpCompleted();
	void Input_SprintStarted();
	void Input_SprintCompleted();
	void Input_CrouchStarted();
	void Input_CrouchCompleted();
	void Input_Interact();
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
	void UpdateFlashlightFromLoadout();
	void UpdateLocalVisibility();
	void UpdateCamera(float DeltaSeconds);
	void UpdateFlashlightAim();
	void HandleSettingsApplied();

	UFUNCTION()
	void OnRep_Flashlight();

	UFUNCTION()
	void OnRep_Sprinting();

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHHCosmeticComponent> Cosmetics;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHHInteractionComponent> Interaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHHFootstepComponent> Footsteps;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float WalkSpeed = 210.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeed = 410.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float CrouchSpeed = 125.f;

	/** Eye height above the capsule centre while standing / crouched. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float StandingEyeZ = 70.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CrouchedEyeZ = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float LookRateYaw = 170.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float LookRatePitch = 120.f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Flashlight)
	bool bFlashlightOn = false;

	UPROPERTY(ReplicatedUsing = OnRep_Sprinting)
	bool bSprinting = false;

	bool bThirdPerson = false;
	bool bLocallyHidden = false;
	bool bWantsCrouchHeld = false;

	float CameraZ = 70.f;
	float CameraTargetZ = 70.f;
	float BobPhase = 0.f;
	float BobBlend = 0.f;
	float LandingDip = 0.f;

	TWeakObjectPtr<AHHPlayerState> BoundPlayerState;
	FDelegateHandle CosmeticsHandle;
	FDelegateHandle EquipmentHandle;
	FDelegateHandle SettingsHandle;
};
