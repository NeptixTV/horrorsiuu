#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "Core/HHTypes.h"
#include "HHLootActor.generated.h"

class UBoxComponent;
class UHHItemDefinition;

/**
 * A pick-up in the world: placed loot (TV, laptop, jewellery...), dropped equipment or loot,
 * or a key. Placed loot rolls its value on the server when the job starts. Loot can be hidden
 * inside a container (ContainerId) until somebody searches it.
 *
 * Picking up goes through the normal validated interaction: the server puts the item into the
 * interactor's inventory (needs a free slot), tells the heist rules (value, haunting) and
 * removes the actor for everyone.
 */
UCLASS()
class HORRORHEIST_API AHHLootActor : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHLootActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IHHInteractable
	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual FText GetInteractionDetail(AActor* Interactor) const override;
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
	virtual UTexture2D* GetInteractionIcon() const override;
	virtual float GetInteractDuration(AActor* Interactor) const override;

	/** Server: spawn a dropped item. */
	static AHHLootActor* SpawnDropped(UWorld* World, const FHHInventorySlot& Item, const FTransform& Transform);

	/** Server: make hidden loot visible and usable (container searched). */
	void Reveal();

	FName GetItemId() const { return ItemId; }
	FName GetContainerId() const { return ContainerId; }
	bool IsRevealed() const { return bRevealed; }
	int32 GetValue() const { return Value; }
	bool IsKey() const { return !KeyId.IsNone(); }
	FName GetKeyId() const { return KeyId; }

protected:
	void ApplyDefinition();
	const UHHItemDefinition* GetDefinition() const;
	void ApplyRevealState();

	UFUNCTION()
	void OnRep_Item();

	UFUNCTION()
	void OnRep_Revealed();

	/** Interaction volume sized to the mesh (the mesh itself does not need collision). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UBoxComponent> PickupVolume;

	/** Loot or equipment id (LT_*, EQ_*). Ignored when KeyId is set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Item, Category = "Loot")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Item, Category = "Loot")
	EHHInventoryKind Kind = EHHInventoryKind::Loot;

	/** 0 = roll from the loot definition when the job starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Loot")
	int32 Value = 0;

	/** Reveal together with the container that has the same id. None = always visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FName ContainerId;

	/** Makes this a key (shared by the crew, takes no inventory slot), e.g. "KEY_Vance_Front". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FName KeyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FText KeyName;

	/** Small things are pocketed instantly; bulky ones take a moment to lift. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	float HeavyPickupSeconds = 1.2f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Revealed)
	bool bRevealed = true;

	bool bTaken = false;
};
