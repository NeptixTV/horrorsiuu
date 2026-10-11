#include "Player/HHInventoryComponent.h"
#include "Data/HHEquipmentDefinition.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHLootDefinition.h"
#include "Heist/HHLootActor.h"
#include "HorrorHeist.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

UHHInventoryComponent::UHHInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Slots.SetNum(NumSlots);
}

void UHHInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHHInventoryComponent, Slots);
	DOREPLIFETIME(UHHInventoryComponent, SelectedIndex);
	DOREPLIFETIME(UHHInventoryComponent, bEnabled);
}

const FHHInventorySlot& UHHInventoryComponent::GetSlot(int32 Index) const
{
	static const FHHInventorySlot Empty;
	return Slots.IsValidIndex(Index) ? Slots[Index] : Empty;
}

int32 UHHInventoryComponent::FindFreeSlot() const
{
	if (Slots.IsValidIndex(SelectedIndex) && Slots[SelectedIndex].IsEmpty())
	{
		return SelectedIndex;
	}
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].IsEmpty())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UHHInventoryComponent::CountLoot() const
{
	int32 Count = 0;
	for (const FHHInventorySlot& Slot : Slots)
	{
		Count += Slot.IsLoot() ? 1 : 0;
	}
	return Count;
}

int32 UHHInventoryComponent::GetCarriedLootValue() const
{
	int32 Total = 0;
	for (const FHHInventorySlot& Slot : Slots)
	{
		Total += Slot.IsLoot() ? Slot.Value : 0;
	}
	return Total;
}

bool UHHInventoryComponent::IsCarryingHeavy() const
{
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	if (!Registry)
	{
		return false;
	}
	for (const FHHInventorySlot& Slot : Slots)
	{
		if (Slot.IsLoot())
		{
			const UHHLootDefinition* Loot = Registry->FindLoot(Slot.ItemId);
			if (Loot && Loot->bHeavy)
			{
				return true;
			}
		}
	}
	return false;
}

const UHHEquipmentDefinition* UHHInventoryComponent::FindTool(FName ToolTag) const
{
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	if (!Registry || ToolTag.IsNone())
	{
		return nullptr;
	}
	for (const FHHInventorySlot& Slot : Slots)
	{
		if (Slot.Kind == EHHInventoryKind::Equipment)
		{
			const UHHEquipmentDefinition* Equipment = Registry->FindEquipment(Slot.ItemId);
			if (Equipment && Equipment->ToolTag == ToolTag)
			{
				return Equipment;
			}
		}
	}
	return nullptr;
}

FText UHHInventoryComponent::GetSlotName(int32 Index) const
{
	const FHHInventorySlot& Slot = GetSlot(Index);
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	const UHHItemDefinition* Item = Registry && !Slot.IsEmpty() ? Registry->FindItem(Slot.ItemId) : nullptr;
	return Item ? Item->DisplayName : FText::GetEmpty();
}

UTexture2D* UHHInventoryComponent::GetSlotIcon(int32 Index) const
{
	const FHHInventorySlot& Slot = GetSlot(Index);
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	const UHHItemDefinition* Item = Registry && !Slot.IsEmpty() ? Registry->FindItem(Slot.ItemId) : nullptr;
	return Item ? Registry->GetIcon(Item) : nullptr;
}

UStaticMesh* UHHInventoryComponent::GetSlotMesh(int32 Index) const
{
	const FHHInventorySlot& Slot = GetSlot(Index);
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	if (!Registry || Slot.IsEmpty())
	{
		return nullptr;
	}
	if (const UHHLootDefinition* Loot = Registry->FindLoot(Slot.ItemId))
	{
		return Loot->Mesh.LoadSynchronous();
	}
	if (const UHHEquipmentDefinition* Equipment = Registry->FindEquipment(Slot.ItemId))
	{
		return Equipment->Mesh.LoadSynchronous();
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------
// Server

void UHHInventoryComponent::SetEnabled(bool bNewEnabled)
{
	if (GetOwner() && GetOwner()->HasAuthority() && bEnabled != bNewEnabled)
	{
		bEnabled = bNewEnabled;
		Broadcast();
	}
}

void UHHInventoryComponent::InitializeFromLoadout(const FHHEquipmentLoadout& Loadout)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	Slots.Reset();
	Slots.SetNum(NumSlots);
	int32 Next = 0;
	for (EHHEquipmentSlot Slot : { EHHEquipmentSlot::Light, EHHEquipmentSlot::Entry, EHHEquipmentSlot::Utility, EHHEquipmentSlot::Gadget })
	{
		const FName Id = Loadout.Get(Slot);
		if (!Id.IsNone() && Next < NumSlots)
		{
			FHHInventorySlot& Target = Slots[Next++];
			Target.Kind = EHHInventoryKind::Equipment;
			Target.ItemId = Id;
			Target.Value = 0;
		}
	}
	SelectedIndex = 0;
	Broadcast();
}

bool UHHInventoryComponent::AddItem(const FHHInventorySlot& Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Item.IsEmpty())
	{
		return false;
	}
	const int32 Index = FindFreeSlot();
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Slots[Index] = Item;
	SelectedIndex = Index;
	Broadcast();
	return true;
}

FHHInventorySlot UHHInventoryComponent::RemoveSlot(int32 Index)
{
	FHHInventorySlot Removed;
	if (GetOwner() && GetOwner()->HasAuthority() && Slots.IsValidIndex(Index))
	{
		Removed = Slots[Index];
		Slots[Index] = FHHInventorySlot();
		Broadcast();
	}
	return Removed;
}

TArray<FHHInventorySlot> UHHInventoryComponent::RemoveAllLoot()
{
	TArray<FHHInventorySlot> Removed;
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return Removed;
	}
	for (FHHInventorySlot& Slot : Slots)
	{
		if (Slot.IsLoot())
		{
			Removed.Add(Slot);
			Slot = FHHInventorySlot();
		}
	}
	if (Removed.Num() > 0)
	{
		Broadcast();
	}
	return Removed;
}

bool UHHInventoryComponent::DropSelected()
{
	const APlayerState* PlayerState = Cast<APlayerState>(GetOwner());
	APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
	if (!Pawn || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(SelectedIndex) || Slots[SelectedIndex].IsEmpty())
	{
		return false;
	}

	// Put it on the floor in front of the player (not inside a wall).
	const FVector Eye = Pawn->GetPawnViewLocation();
	const FVector Forward = Pawn->GetActorForwardVector();
	FVector Target = Pawn->GetActorLocation() + Forward * 55.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHDropTrace), false, Pawn);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params))
	{
		Target = Hit.ImpactPoint - Forward * 25.f;
	}
	if (GetWorld()->LineTraceSingleByChannel(Hit, Target + FVector(0.f, 0.f, 40.f), Target - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
	{
		Target = Hit.ImpactPoint;
	}

	const FHHInventorySlot Item = Slots[SelectedIndex];
	const FTransform Transform(FRotator(0.f, Pawn->GetActorRotation().Yaw + 180.f, 0.f), Target);
	if (!AHHLootActor::SpawnDropped(GetWorld(), Item, Transform))
	{
		return false;
	}
	Slots[SelectedIndex] = FHHInventorySlot();
	Broadcast();
	return true;
}

// ---------------------------------------------------------------------------------------
// Client requests

void UHHInventoryComponent::RequestSelect(int32 Index)
{
	if (!bEnabled || !Slots.IsValidIndex(Index) || Index == SelectedIndex)
	{
		return;
	}
	SelectedIndex = Index;
	Broadcast();
	if (!GetOwner()->HasAuthority())
	{
		ServerSelect(Index);
	}
}

void UHHInventoryComponent::RequestCycle(int32 Direction)
{
	if (!bEnabled || Slots.Num() == 0)
	{
		return;
	}
	const int32 Step = Direction >= 0 ? 1 : -1;
	RequestSelect((SelectedIndex + Step + Slots.Num()) % Slots.Num());
}

void UHHInventoryComponent::RequestDrop()
{
	if (!bEnabled)
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		DropSelected();
	}
	else
	{
		ServerDrop();
	}
}

void UHHInventoryComponent::ServerSelect_Implementation(int32 Index)
{
	if (Slots.IsValidIndex(Index) && SelectedIndex != Index)
	{
		SelectedIndex = Index;
		Broadcast();
	}
}

void UHHInventoryComponent::ServerDrop_Implementation()
{
	DropSelected();
}

void UHHInventoryComponent::OnRep_Inventory()
{
	OnInventoryChanged.Broadcast(this);
}

void UHHInventoryComponent::Broadcast()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	OnInventoryChanged.Broadcast(this);
}
