// SIGNAL UNKNOWN – the register of the gas station shop.
//
// Opens the shop screen (ESUScreen::Shop). All purchases are validated and booked on the server:
// items go into the buyer's inventory, fuel bills of the pumps are paid from the team account,
// the vehicle can be repaired when it is parked on the forecourt. The clerk has to be at the
// register; if she is busy elsewhere she is called over.
#pragma once

#include "CoreMinimal.h"
#include "Interaction/SUInteractableActor.h"
#include "SUShopCounter.generated.h"

class ASUFuelPump;
class ASUNPC;
class ASUPlayerController;
class ASUVehicle;

UCLASS()
class SIGNALUNKNOWN_API ASUShopCounter : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUShopCounter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;
	virtual float GetInteractionRange() const override { return 300.f; }

	UPROPERTY(Replicated)
	TArray<TObjectPtr<ASUFuelPump>> Pumps;

	UPROPERTY(Replicated)
	TObjectPtr<ASUNPC> Clerk;

	/** Items on sale (price from the item database). */
	static const TArray<FName>& GetShopItems();

	int32 GetOpenFuelCents() const;
	float GetOpenFuelLiters() const;
	/** Team vehicle if it is parked on the forecourt, else null. */
	ASUVehicle* GetRepairableVehicle() const;
	int32 GetRepairCostCents(const ASUVehicle* Vehicle) const;
	bool IsClerkPresent() const;

	// ---------------------------------------------------------------- server (from player controller RPCs)
	void Buy(ASUPlayerController* PC, FName ItemId);
	void PayFuel(ASUPlayerController* PC);
	void RepairVehicle(ASUPlayerController* PC);

	/** Max distance between the player and the register for a purchase (cm). */
	static constexpr float ServiceDistance = 400.f;

protected:
	virtual void BuildGeometry() override;

private:
	bool ValidateCustomer(ASUPlayerController* PC, ASUCharacter*& OutChar) const;
	void PlayRegisterSound();
};
