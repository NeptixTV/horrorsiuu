// SIGNAL UNKNOWN – the team's old off-road vehicle.
//
// Driving model: raycast suspension + friction-circle tyres on a Chaos rigid body
// (SULogic::ComputeWheelForces). Chaos Vehicles needs a rigged skeletal mesh which the project
// does not have yet; the input interface (SetDriverInput / HandleCommand) stays the same if the
// physics is swapped for UChaosWheeledVehicleMovementComponent later.
//
// Players stay possessed by their character while seated; the driver's character forwards input.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/SUInteractable.h"
#include "Player/SUCharacter.h"
#include "Core/SUSaveGame.h"
#include "Logic/SUVehicleLogic.h"
#include "SUVehicle.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USpotLightComponent;
class UPointLightComponent;
class UTextRenderComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class USUInventoryComponent;
class USUProceduralAudioComponent;
class ASUFuelPump;

UCLASS()
class SIGNALUNKNOWN_API ASUVehicle : public AActor, public ISUInteractable
{
	GENERATED_BODY()

public:
	ASUVehicle();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ISUInteractable
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;
	virtual float GetInteractionRange() const override { return 300.f; }

	static constexpr int32 NumSeats = 4;

	// ---------------------------------------------------------------- seats (server)
	bool TryEnter(ASUCharacter* Char, int32 PreferredSeat);
	void RequestExit(ASUCharacter* Char);
	/** bForce: ignore speed (death, disconnect). */
	void RemoveOccupant(ASUCharacter* Char, bool bForce);
	USceneComponent* GetSeatComponent(int32 Seat) const;
	ASUCharacter* GetOccupant(int32 Seat) const;
	bool HasOccupants() const;
	bool IsLocalPlayerInside() const;

	// ---------------------------------------------------------------- driving
	/** Server: from the driver's RPC. Locally: prediction on the driving client. */
	void SetDriverInput(float InThrottle, float InBrake, float InSteer, bool bInHandbrake);
	void HandleCommand(ASUCharacter* By, ESUVehicleCommand Command, float Value);

	bool IsEngineRunning() const { return bEngineRunning; }
	float GetSpeedKmh() const;
	float GetForwardSpeedKmh() const;
	float GetFuelLiters() const { return FuelLiters; }
	float GetFuelCapacity() const;
	float GetEngineHealth() const { return EngineHealth; }
	float GetRpm() const { return Rpm; }
	FString GetGearText() const;
	int32 GetLightsMode() const { return LightsMode; }
	int32 GetBlinkerMode() const { return BlinkerMode; }
	bool AreWipersOn() const { return bWipers; }
	bool IsRadioOn() const { return bRadioOn; }
	int32 GetRadioStation() const { return RadioStation; }
	float GetRadioVolume() const { return RadioVolume; }
	FText GetRadioStationName() const;
	/** Current subtitle of the car radio (news / strange transmission). */
	bool GetRadioSubtitle(FSUSpeechLine& OutLine) const;
	bool IsRadioIntrusionActive() const;

	// ---------------------------------------------------------------- fuel (server)
	bool IsFuelCapOpen() const { return bFuelCapOpen; }
	void SetFuelCapOpen(bool bOpen);
	/** Adds fuel, returns the amount actually added. */
	float AddFuel(float Liters);
	ASUFuelPump* GetInsertedNozzlePump() const { return InsertedNozzlePump; }
	void SetInsertedNozzlePump(ASUFuelPump* Pump) { InsertedNozzlePump = Pump; }
	FVector GetFuelCapLocation() const;

	// ---------------------------------------------------------------- horror / damage (server)
	/** The next engine start attempts fail (CarWontStart event). */
	void BlockNextStarts(int32 Count) { StartBlockCount = FMath::Max(StartBlockCount, Count); }
	/** A strange voice interrupts the radio (switches it on if needed). */
	void TriggerRadioIntrusion(int32 LineIndex);
	void ApplyAttack(float Damage, const FVector& FromLocation);
	void RepairBy(float Amount);

	// ---------------------------------------------------------------- trunk / save
	USUInventoryComponent* GetTrunk() const { return Trunk; }
	bool TransferTrunkItem(ASUCharacter* Char, FName ItemId, bool bToTrunk);
	FSUVehicleSave GetSaveData() const;
	void ApplySaveData(const FSUVehicleSave& Data);

	/** Rear view mirror image (local, only while a local player sits inside). */
	UTextureRenderTarget2D* GetMirrorTexture() const { return MirrorTexture; }

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCrank(bool bSuccess);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpact(float Strength);

private:
	// ---- components
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USceneComponent> Visual;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USceneComponent> SeatDriver;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USceneComponent> SeatPassenger;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USceneComponent> SeatRearLeft;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USceneComponent> SeatRearRight;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TArray<TObjectPtr<UBoxComponent>> DoorZones;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UBoxComponent> TrunkZone;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UBoxComponent> FuelZone;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UBoxComponent> HoodZone;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USUInventoryComponent> Trunk;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USpotLightComponent> HeadLeft;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USpotLightComponent> HeadRight;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UPointLightComponent> TailLeft;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UPointLightComponent> TailRight;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UPointLightComponent> ReverseLight;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UPointLightComponent> BlinkFL;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UPointLightComponent> BlinkFR;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UPointLightComponent> BlinkRL;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UPointLightComponent> BlinkRR;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UPointLightComponent> DomeLight;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UPointLightComponent> DashLight;

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> WheelMeshes;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> SteeringWheel;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> SpeedNeedle;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FuelNeedle;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WiperLeft;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WiperRight;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FuelCapLid;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> DashText;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> RadioText;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> EngineAudio;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> HornAudio;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> ClickAudio;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> RadioAudio;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> DispatchAudio;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> ImpactAudio;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> MirrorCapture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> MirrorTexture;

	// ---- replicated state
	UPROPERTY(Replicated) TArray<TObjectPtr<ASUCharacter>> Occupants;
	UPROPERTY(Replicated) bool bEngineRunning = false;
	UPROPERTY(Replicated) float FuelLiters = 24.f;
	UPROPERTY(Replicated) float EngineHealth = 100.f;
	UPROPERTY(Replicated) uint8 LightsMode = 0;
	UPROPERTY(Replicated) uint8 BlinkerMode = 0;
	UPROPERTY(Replicated) bool bWipers = false;
	UPROPERTY(Replicated) bool bHorn = false;
	UPROPERTY(Replicated) bool bRadioOn = false;
	UPROPERTY(Replicated) uint8 RadioStation = 0;
	UPROPERTY(Replicated) float RadioVolume = 0.7f;
	UPROPERTY(Replicated) bool bFuelCapOpen = false;
	UPROPERTY(Replicated) TObjectPtr<ASUFuelPump> InsertedNozzlePump;
	UPROPERTY(Replicated) double RadioIntrusionStart = -100.0;
	UPROPERTY(Replicated) int32 RadioIntrusionLine = 0;
	UPROPERTY(Replicated) float RepThrottle = 0.f;
	UPROPERTY(Replicated) float RepBrake = 0.f;
	UPROPERTY(Replicated) float RepSteer = 0.f;
	UPROPERTY(Replicated) bool bRepHandbrake = false;

	// ---- simulation
	struct FWheelState
	{
		FVector LocalMount = FVector::ZeroVector;
		bool bFront = false;
		bool bLeft = false;
		float Compression = 0.f;      // m
		float LastCompression = 0.f;  // m
		bool bGrounded = false;
		float SpinAngle = 0.f;
		float VisualOffset = 0.f;     // cm below the mount
	};
	FWheelState Wheels[4];
	SULogic::EngineModel EngineSpec;
	SULogic::FuelModel FuelSpec;
	float Throttle = 0.f;
	float BrakeInput = 0.f;
	float SteerInput = 0.f;
	bool bHandbrake = true;
	float SteerAngle = 0.f;
	float Rpm = 800.f;
	float BlinkTimer = 0.f;
	bool bBlinkOn = false;
	float WiperPhase = 0.f;
	int32 StartBlockCount = 0;
	float CrankTimer = -1.f;
	bool bCrankSuccess = false;
	float ImpactCooldown = 0.f;
	float NoiseTimer = 0.f;
	float LastHealthWarning = 100.f;

	void BuildVisuals();
	UStaticMeshComponent* AddVisual(USceneComponent* Parent, int32 Shape, const FVector& Location, const FVector& Size, int32 Mat, const FRotator& Rot = FRotator::ZeroRotator);
	void SimulatePhysics(float DeltaSeconds);
	void UpdateVisuals(float DeltaSeconds);
	void UpdateLights(float DeltaSeconds);
	void UpdateAudio(float DeltaSeconds);
	void UpdateMirror();
	void ServerUpdate(float DeltaSeconds);
	void StartEngineAttempt(ASUCharacter* By);
	void StopEngine(bool bStall);
	void NotifyOccupants(const FText& Text, FLinearColor Color = FLinearColor::White) const;
	float SurfaceGrip(const FHitResult& Hit) const;
	bool FindExitLocation(int32 Seat, FVector& OutLocation) const;

	UFUNCTION()
	void HandleBodyHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
