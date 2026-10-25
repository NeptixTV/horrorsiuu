// SIGNAL UNKNOWN – simple server-authoritative inventory (players and vehicle trunks).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SUInventoryComponent.generated.h"

USTRUCT(BlueprintType)
struct FSUInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Count = 0;

	/** Battery charge 0..1 for devices. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	float Charge = 1.f;
};

DECLARE_MULTICAST_DELEGATE(FSUInventoryChangedSignature);

UCLASS(ClassGroup = (SignalUnknown), meta = (BlueprintSpawnableComponent))
class SIGNALUNKNOWN_API USUInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USUInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<FSUInventorySlot>& GetSlots() const { return Slots; }
	int32 CountItem(FName ItemId) const;
	bool HasItem(FName ItemId) const { return CountItem(ItemId) > 0; }
	float GetCharge(FName ItemId) const;

	/** Server. Returns how many were added (limited by stack sizes and slot count). */
	int32 AddItem(FName ItemId, int32 Count = 1, float Charge = 1.f);
	/** Server. Returns how many were removed. */
	int32 RemoveItem(FName ItemId, int32 Count = 1);
	/** Server. */
	void SetCharge(FName ItemId, float Charge);
	/** Server: restore from a save. */
	void SetSlots(const TArray<FSUInventorySlot>& InSlots);
	bool CanAdd(FName ItemId, int32 Count = 1) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 MaxSlots = 12;

	FSUInventoryChangedSignature OnInventoryChanged;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FSUInventorySlot> Slots;

	UFUNCTION()
	void OnRep_Slots();

	FSUInventorySlot* FindSlot(FName ItemId);
	const FSUInventorySlot* FindSlot(FName ItemId) const;
};
