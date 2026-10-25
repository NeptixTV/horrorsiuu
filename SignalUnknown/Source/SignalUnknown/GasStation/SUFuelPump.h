// SIGNAL UNKNOWN – fuel pump with a real refuelling process.
//
//  1. take the nozzle from the pump                 (pump: E)
//  2. open the fuel cap of the car                  (car fuel flap: E)
//  3. insert the nozzle                             (car fuel flap: E)
//  4. start / stop pumping, the display counts up   (car fuel flap: E)
//     – stops automatically when the tank is full
//  5. take the nozzle out and hang it back          (car fuel flap: Q, pump: E)
//  6. pay inside at the register                    (shop counter)
//
// The pump stays locked while a bill is open. Driving away with an open bill is recorded on the
// game state as an unpaid fuel incident (hook for a later police system).
#pragma once

#include "CoreMinimal.h"
#include "Interaction/SUInteractableActor.h"
#include "SUFuelPump.generated.h"

class UBoxComponent;
class UCableComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class ASUVehicle;

UENUM()
enum class ESUNozzleState : uint8
{
	Holstered,
	Held,
	InVehicle
};

UCLASS()
class SIGNALUNKNOWN_API ASUFuelPump : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUFuelPump();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;
	virtual float GetInteractionRange() const override { return 230.f; }

	UPROPERTY(Replicated)
	int32 PumpNumber = 1;

	// ---------------------------------------------------------------- server
	void TakeNozzle(ASUCharacter* By);
	void InsertNozzle(ASUVehicle* Vehicle, ASUCharacter* By);
	void TakeNozzleFromVehicle(ASUCharacter* By);
	/** Hang the nozzle back (from hand or car), stops pumping. */
	void ReturnNozzle();
	/** The hose was pulled away (car drove off / player walked too far). */
	void ForceReturnNozzle(bool bByVehicle);
	void SetPumping(bool bOn, ASUCharacter* By);

	/** Shop counter: amount due for this pump. */
	int32 GetBillCents() const;
	float GetBillLiters() const { return BillLiters; }
	bool HasOpenBill() const { return BillLiters > 0.005f; }
	/** Called after the register booked the payment. */
	void SettleBill();

	bool IsPumping() const { return bPumping; }
	ESUNozzleState GetNozzleState() const { return NozzleState; }

	/** Litres per second while pumping (~36 l/min). */
	static constexpr float FlowLitersPerSecond = 0.6f;
	/** Usable hose length from the outlet (cm). */
	static constexpr float HoseReach = 420.f;

protected:
	virtual void BuildGeometry() override;

private:
	UPROPERTY(Replicated) ESUNozzleState NozzleState = ESUNozzleState::Holstered;
	UPROPERTY(Replicated) TObjectPtr<ASUCharacter> Holder;
	UPROPERTY(Replicated) TObjectPtr<ASUVehicle> Vehicle;
	UPROPERTY(Replicated) bool bPumping = false;
	UPROPERTY(Replicated) float BillLiters = 0.f;
	UPROPERTY(Replicated) int32 PriceCentsPerLiter = 189;

	UPROPERTY(VisibleAnywhere, Category = "Pump") TObjectPtr<UBoxComponent> HolsterZone;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> NozzleRoot;
	UPROPERTY(Transient) TObjectPtr<UCableComponent> Hose;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> DisplayLiters;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> DisplayPrice;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> DisplayStatus;

	/** Vehicle that received the open bill (drive-off detection). */
	TWeakObjectPtr<ASUVehicle> BilledVehicle;
	/** Who started the current fill (refuel event instigator). */
	TWeakObjectPtr<ASUCharacter> PumpUser;
	float RefuelEventLiters = 0.f;

	FVector GetHolsterLocation() const;
	FVector GetOutletLocation() const;
	void UpdateNozzleTransform();
	void UpdateDisplay();
	void ClearHolder();
	void StopPumping(bool bFull);
};
