#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/HHTypes.h"
#include "HHInventoryComponent.generated.h"

class UHHInventoryComponent;
class UHHEquipmentDefinition;
class UHHLootDefinition;
class UTexture2D;
class UStaticMesh;

DECLARE_MULTICAST_DELEGATE_OneParam(FHHOnInventoryChanged, UHHInventoryComponent* /*Inventory*/);

/**
 * The five things a crew member can carry on a job - equipment included ("Bag" is the bag
 * itself, so the light, entry tool, utility and gadget from the loadout take slots, the rest is
 * room for loot). Lives on the player state, so it survives the pawn. Server authoritative:
 * clients only ask to select or drop; picking up happens through validated interactions.
 */
UCLASS(ClassGroup = (HorrorHeist), meta = (BlueprintSpawnableComponent))
class HORRORHEIST_API UHHInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 NumSlots = 5;

	UHHInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Queries (any machine) ---------------------------------------------------------

	/** Inventories only exist on jobs; in the hideout the loadout screen is used instead. */
	bool IsEnabled() const { return bEnabled; }
	const TArray<FHHInventorySlot>& GetSlots() const { return Slots; }
	const FHHInventorySlot& GetSlot(int32 Index) const;
	int32 GetSelectedIndex() const { return SelectedIndex; }
	const FHHInventorySlot& GetSelected() const { return GetSlot(SelectedIndex); }
	int32 FindFreeSlot() const;
	bool HasFreeSlot() const { return FindFreeSlot() != INDEX_NONE; }
	int32 CountLoot() const;
	int32 GetCarriedLootValue() const;
	bool IsCarryingHeavy() const;

	/** Equipment carried with the given tool tag ("Lockpick", "Crowbar", "Light"...), or null. */
	const UHHEquipmentDefinition* FindTool(FName ToolTag) const;
	/** Slot holding equipment with the given tool tag, or INDEX_NONE. */
	int32 FindToolSlot(FName ToolTag) const;

	/** Display helpers shared by the HUD and the hand mesh. */
	FText GetSlotName(int32 Index) const;
	UTexture2D* GetSlotIcon(int32 Index) const;
	UStaticMesh* GetSlotMesh(int32 Index) const;

	// ---- Server ------------------------------------------------------------------------

	void SetEnabled(bool bNewEnabled);
	/** Fills the slots from the loadout (light, entry tool, utility, gadget). */
	void InitializeFromLoadout(const FHHEquipmentLoadout& Loadout);
	/** Puts the item into the selected slot if it is empty, otherwise the first free one. */
	bool AddItem(const FHHInventorySlot& Item);
	/** Uses up one charge of the equipment in a slot (salt, film, first aid). False if none left. */
	bool ConsumeCharge(int32 Index);
	FHHInventorySlot RemoveSlot(int32 Index);
	/** Empties every loot slot (stashing in the van, getting caught). Equipment stays. */
	TArray<FHHInventorySlot> RemoveAllLoot();
	/** Drops the selected item in front of the owner's pawn as a pickup. */
	bool DropSelected();

	// ---- Owning client -----------------------------------------------------------------

	/** Selects immediately (responsive hotbar) and tells the server. */
	void RequestSelect(int32 Index);
	void RequestCycle(int32 Direction);
	void RequestDrop();

	FHHOnInventoryChanged OnInventoryChanged;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSelect(int32 Index);

	UFUNCTION(Server, Reliable)
	void ServerDrop();

	UFUNCTION()
	void OnRep_Inventory();

	void Broadcast();

private:
	UPROPERTY(ReplicatedUsing = OnRep_Inventory)
	TArray<FHHInventorySlot> Slots;

	UPROPERTY(ReplicatedUsing = OnRep_Inventory)
	int32 SelectedIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Inventory)
	bool bEnabled = false;
};
