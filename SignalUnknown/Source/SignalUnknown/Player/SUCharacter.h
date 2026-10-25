// SIGNAL UNKNOWN – first-person player character.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/SUTypes.h"
#include "Evidence/SUPhotoTypes.h"
#include "SUCharacter.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class USUInventoryComponent;
class USUInteractionComponent;
class USUPhotoCameraComponent;
class USUProceduralAudioComponent;
class ASUVehicle;
class ASUFuelPump;
class UVOIPTalker;
struct FInputActionValue;

/** Commands a seated player can send to the vehicle. */
UENUM()
enum class ESUVehicleCommand : uint8
{
	ToggleEngine,
	ToggleLights,
	ToggleHighBeam,
	HornOn,
	HornOff,
	BlinkLeft,
	BlinkRight,
	Hazard,
	Wipers,
	RadioPower,
	RadioNext,
	RadioPrev,
	RadioVolume
};

UCLASS()
class SIGNALUNKNOWN_API ASUCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASUCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnRep_PlayerState() override;
	virtual void PossessedBy(AController* NewController) override;

	// ---------------------------------------------------------------- accessors
	USUInventoryComponent* GetInventory() const { return Inventory; }
	USUInteractionComponent* GetInteraction() const { return Interaction; }
	USUPhotoCameraComponent* GetPhotoCamera() const { return PhotoCamera; }
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	FVector GetEyeLocation() const;
	FRotator GetViewRotationReplicated() const;

	float GetHealth() const { return Health; }
	bool IsDead() const { return bDead; }
	bool IsFlashlightOn() const { return bFlashlightOn; }
	float GetFlashlightCharge() const;
	FVector GetFlashlightDirection() const;
	ESUHeldItem GetHeldItem() const { return HeldItem; }
	ASUFuelPump* GetHeldNozzlePump() const { return HeldNozzlePump; }
	USceneComponent* GetHandAttachPoint() const { return HandAttach; }

	bool IsSeated() const { return Vehicle != nullptr; }
	bool IsDriver() const { return Vehicle != nullptr && SeatIndex == 0; }
	ASUVehicle* GetVehicle() const { return Vehicle; }
	int32 GetSeatIndex() const { return SeatIndex; }
	bool IsSprinting() const { return bSprinting; }
	float GetLookAngularSpeed() const { return LookAngularSpeed; }

	// ---------------------------------------------------------------- server side API
	void SetSeated(ASUVehicle* InVehicle, int32 InSeat);
	void ApplyDamageSU(float Amount, AActor* Causer);
	void HealFull();
	void SetHealthDirect(float InHealth);
	void TeleportToBase(const FTransform& Where);
	void SetHeldNozzlePump(ASUFuelPump* Pump);
	/** Sends a HUD notification to the owning player. */
	void NotifyOwner(const FText& Text, FLinearColor Color = FLinearColor::White);

	/** Local: fire the camera flash light for one frame (everyone else gets MulticastCameraFlash). */
	void TriggerFlashBulb();

	// ---------------------------------------------------------------- RPCs
	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target, UPrimitiveComponent* Component, ESUInteractOption Option);

	UFUNCTION(Server, Reliable)
	void ServerReportUIEvent(FName EventName, FName Param);

	UFUNCTION(Server, Reliable)
	void ServerSetFlashlight(bool bOn);

	UFUNCTION(Server, Reliable)
	void ServerSetSprint(bool bInSprint);

	UFUNCTION(Server, Reliable)
	void ServerSetHeldItem(ESUHeldItem Item);

	UFUNCTION(Server, Reliable)
	void ServerUseItem(FName ItemId);

	UFUNCTION(Server, Reliable)
	void ServerDropItem(FName ItemId);

	UFUNCTION(Server, Reliable)
	void ServerRegisterPhoto(const FSUPhotoMeta& Meta);

	UFUNCTION(Server, Unreliable)
	void ServerCameraFlash();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCameraFlash();

	UFUNCTION(Server, Reliable)
	void ServerExitVehicle();

	UFUNCTION(Server, Unreliable)
	void ServerVehicleInput(float InThrottle, float InBrake, float InSteer, bool bInHandbrake);

	UFUNCTION(Server, Reliable)
	void ServerVehicleCommand(ESUVehicleCommand Command, float Value);

	UFUNCTION(Client, Reliable)
	void ClientFadeFromBlack(float Seconds);

	UFUNCTION(Client, Reliable)
	void ClientSetViewRotation(FRotator NewRotation);

protected:
	// ---- input
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnLookGamepad(const FInputActionValue& Value);
	void OnJump(const FInputActionValue& Value);
	void OnSprintStart(const FInputActionValue& Value);
	void OnSprintStop(const FInputActionValue& Value);
	void OnCrouchStart(const FInputActionValue& Value);
	void OnCrouchStop(const FInputActionValue& Value);
	void OnInteract(const FInputActionValue& Value);
	void OnInteractAlt(const FInputActionValue& Value);
	void OnFlashlight(const FInputActionValue& Value);
	void OnCameraToggle(const FInputActionValue& Value);
	void OnAimStart(const FInputActionValue& Value);
	void OnAimStop(const FInputActionValue& Value);
	void OnShutter(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);
	void OnCameraFlash(const FInputActionValue& Value);
	void OnThrottle(const FInputActionValue& Value);
	void OnThrottleStop(const FInputActionValue& Value);
	void OnBrake(const FInputActionValue& Value);
	void OnBrakeStop(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
	void OnSteerStop(const FInputActionValue& Value);
	void OnHandbrakeStart(const FInputActionValue& Value);
	void OnHandbrakeStop(const FInputActionValue& Value);
	void OnEngine(const FInputActionValue& Value);
	void OnLights(const FInputActionValue& Value);
	void OnHighBeam(const FInputActionValue& Value);
	void OnHornStart(const FInputActionValue& Value);
	void OnHornStop(const FInputActionValue& Value);
	void OnBlinkLeft(const FInputActionValue& Value);
	void OnBlinkRight(const FInputActionValue& Value);
	void OnHazard(const FInputActionValue& Value);
	void OnWipers(const FInputActionValue& Value);
	void OnRadioPower(const FInputActionValue& Value);
	void OnRadioNext(const FInputActionValue& Value);
	void OnRadioPrev(const FInputActionValue& Value);
	void OnRadioVolume(const FInputActionValue& Value);

private:
	// ---- components
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> ViewModel;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> HandAttach;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USpotLightComponent> Flashlight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USpotLightComponent> FlashBulb;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USUInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USUInteractionComponent> Interaction;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USUPhotoCameraComponent> PhotoCamera;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> FootstepAudio;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> LegParts;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldFlashlightModel;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldCameraModel;
	UPROPERTY(Transient) TObjectPtr<UVOIPTalker> VoiceTalker;

	// ---- replicated state
	UPROPERTY(ReplicatedUsing = OnRep_Health) float Health = 100.f;
	UPROPERTY(ReplicatedUsing = OnRep_Dead) bool bDead = false;
	UPROPERTY(ReplicatedUsing = OnRep_Flashlight) bool bFlashlightOn = false;
	UPROPERTY(ReplicatedUsing = OnRep_HeldItem) ESUHeldItem HeldItem = ESUHeldItem::None;
	UPROPERTY(ReplicatedUsing = OnRep_Seat) TObjectPtr<ASUVehicle> Vehicle;
	UPROPERTY(ReplicatedUsing = OnRep_Seat) int32 SeatIndex = INDEX_NONE;
	UPROPERTY(Replicated) TObjectPtr<ASUFuelPump> HeldNozzlePump;

	UFUNCTION() void OnRep_Health(float OldHealth);
	UFUNCTION() void OnRep_Dead();
	UFUNCTION() void OnRep_Flashlight();
	UFUNCTION() void OnRep_HeldItem();
	UFUNCTION() void OnRep_Seat();

	// ---- local state
	bool bSprinting = false;
	bool bCrouchHeld = false;
	FVector2D MoveInput = FVector2D::ZeroVector;
	float StepAccumulator = 0.f;
	float NoiseTimer = 0.f;
	float HeadBobTime = 0.f;
	float LookAngularSpeed = 0.f;
	FRotator LastViewRotation = FRotator::ZeroRotator;
	float LastVehicleYaw = 0.f;
	bool bAppliedSeated = false;
	float FlashlightDrainTimer = 0.f;
	float FlashBulbTime = 0.f;
	float LastDamageTime = -100.f;

	// vehicle input sent to the server
	float InThrottleValue = 0.f;
	float InBrakeValue = 0.f;
	float InSteerValue = 0.f;
	bool bInHandbrake = false;
	float VehicleInputSendTimer = 0.f;
	float VehicleInputResendTimer = 0.f;
	bool bVehicleInputDirty = false;

	void BuildBody();
	void ApplySeatedState();
	void UpdateFootsteps(float DeltaSeconds);
	void UpdateCameraEffects(float DeltaSeconds);
	void UpdateSeatedLook(float DeltaSeconds);
	void UpdateFlashlight(float DeltaSeconds);
	void PushVehicleInput(bool bForce);
	ESUSurface DetectSurface() const;
	void Die();
	void SetupVoice();
	void ApplyMovementSpeed();
};
