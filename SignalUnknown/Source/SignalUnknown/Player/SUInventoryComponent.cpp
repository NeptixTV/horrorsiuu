#include "Player/SUInventoryComponent.h"
#include "Core/SUItemDatabase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

USUInventoryComponent::USUInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USUInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USUInventoryComponent, Slots);
}

FSUInventorySlot* USUInventoryComponent::FindSlot(FName ItemId)
{
	return Slots.FindByPredicate([ItemId](const FSUInventorySlot& S) { return S.ItemId == ItemId; });
}

const FSUInventorySlot* USUInventoryComponent::FindSlot(FName ItemId) const
{
	return Slots.FindByPredicate([ItemId](const FSUInventorySlot& S) { return S.ItemId == ItemId; });
}

int32 USUInventoryComponent::CountItem(FName ItemId) const
{
	const FSUInventorySlot* Slot = FindSlot(ItemId);
	return Slot ? Slot->Count : 0;
}

float USUInventoryComponent::GetCharge(FName ItemId) const
{
	const FSUInventorySlot* Slot = FindSlot(ItemId);
	return Slot ? Slot->Charge : 0.f;
}

bool USUInventoryComponent::CanAdd(FName ItemId, int32 Count) const
{
	const FSUItemDef* Def = FSUItemDatabase::Find(ItemId);
	if (!Def || Count <= 0)
	{
		return false;
	}
	if (const FSUInventorySlot* Slot = FindSlot(ItemId))
	{
		return Slot->Count + Count <= Def->MaxStack;
	}
	return Slots.Num() < MaxSlots && Count <= Def->MaxStack;
}

int32 USUInventoryComponent::AddItem(FName ItemId, int32 Count, float Charge)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Count <= 0)
	{
		return 0;
	}
	const FSUItemDef* Def = FSUItemDatabase::Find(ItemId);
	if (!Def)
	{
		return 0;
	}
	int32 Added = 0;
	if (FSUInventorySlot* Slot = FindSlot(ItemId))
	{
		Added = FMath::Clamp(Def->MaxStack - Slot->Count, 0, Count);
		Slot->Count += Added;
	}
	else if (Slots.Num() < MaxSlots)
	{
		FSUInventorySlot NewSlot;
		NewSlot.ItemId = ItemId;
		NewSlot.Count = FMath::Min(Count, Def->MaxStack);
		NewSlot.Charge = FMath::Clamp(Charge, 0.f, 1.f);
		Added = NewSlot.Count;
		Slots.Add(NewSlot);
	}
	if (Added > 0)
	{
		OnRep_Slots();
	}
	return Added;
}

int32 USUInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Count <= 0)
	{
		return 0;
	}
	const int32 Index = Slots.IndexOfByPredicate([ItemId](const FSUInventorySlot& S) { return S.ItemId == ItemId; });
	if (Index == INDEX_NONE)
	{
		return 0;
	}
	const int32 Removed = FMath::Min(Count, Slots[Index].Count);
	Slots[Index].Count -= Removed;
	if (Slots[Index].Count <= 0)
	{
		Slots.RemoveAt(Index);
	}
	OnRep_Slots();
	return Removed;
}

void USUInventoryComponent::SetCharge(FName ItemId, float Charge)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	if (FSUInventorySlot* Slot = FindSlot(ItemId))
	{
		const float NewCharge = FMath::Clamp(Charge, 0.f, 1.f);
		// avoid replicating tiny drain steps every frame
		if (FMath::Abs(NewCharge - Slot->Charge) >= 0.005f || NewCharge <= 0.f || NewCharge >= 1.f)
		{
			Slot->Charge = NewCharge;
			OnRep_Slots();
		}
	}
}

void USUInventoryComponent::SetSlots(const TArray<FSUInventorySlot>& InSlots)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	Slots.Reset();
	for (const FSUInventorySlot& S : InSlots)
	{
		// drop items that no longer exist in this game version instead of failing the load
		if (FSUItemDatabase::Find(S.ItemId) && S.Count > 0 && Slots.Num() < MaxSlots)
		{
			Slots.Add(S);
		}
	}
	OnRep_Slots();
}

void USUInventoryComponent::OnRep_Slots()
{
	OnInventoryChanged.Broadcast();
}
