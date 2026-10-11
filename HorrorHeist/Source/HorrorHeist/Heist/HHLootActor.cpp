#include "Heist/HHLootActor.h"
#include "Heist/HHHeistGameMode.h"
#include "Heist/HHHeistGameState.h"
#include "Player/HHPlayerState.h"
#include "Player/HHPlayerController.h"
#include "Player/HHInventoryComponent.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHLootDefinition.h"
#include "Data/HHEquipmentDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "HHLoot"

AHHLootActor::AHHLootActor()
{
	// The mesh is decoration; the box below is what the interaction trace hits.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetMobility(EComponentMobility::Movable);

	PickupVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PickupVolume"));
	PickupVolume->SetupAttachment(Root);
	PickupVolume->SetBoxExtent(FVector(12.f));
	PickupVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PickupVolume->SetGenerateOverlapEvents(false);
}

void AHHLootActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHLootActor, ItemId);
	DOREPLIFETIME(AHHLootActor, Kind);
	DOREPLIFETIME(AHHLootActor, Value);
	DOREPLIFETIME(AHHLootActor, bRevealed);
}

const UHHItemDefinition* AHHLootActor::GetDefinition() const
{
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	return Registry && !ItemId.IsNone() ? Registry->FindItem(ItemId) : nullptr;
}

void AHHLootActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		bRevealed = ContainerId.IsNone();
		if (Kind == EHHInventoryKind::Loot && Value <= 0 && !IsKey())
		{
			if (const UHHLootDefinition* Loot = Cast<UHHLootDefinition>(GetDefinition()))
			{
				FRandomStream Stream(FMath::Rand());
				Value = Loot->RollValue(Stream);
			}
		}
	}

	ApplyDefinition();
	ApplyRevealState();
}

void AHHLootActor::OnRep_Item()
{
	ApplyDefinition();
}

void AHHLootActor::OnRep_Revealed()
{
	ApplyRevealState();
}

void AHHLootActor::ApplyDefinition()
{
	// Placed loot may carry its own mesh from the level; dropped items take the item's mesh.
	if (!Mesh->GetStaticMesh())
	{
		UStaticMesh* ItemMesh = nullptr;
		if (const UHHLootDefinition* Loot = Cast<UHHLootDefinition>(GetDefinition()))
		{
			ItemMesh = Loot->Mesh.LoadSynchronous();
		}
		else if (const UHHEquipmentDefinition* Equipment = Cast<UHHEquipmentDefinition>(GetDefinition()))
		{
			ItemMesh = Equipment->Mesh.LoadSynchronous();
		}
		if (ItemMesh)
		{
			Mesh->SetStaticMesh(ItemMesh);
		}
	}

	// Size the pick-up box to the mesh, with a little slack so tiny items are easy to aim at.
	if (const UStaticMesh* Current = Mesh->GetStaticMesh())
	{
		const FBoxSphereBounds Bounds = Current->GetBounds();
		const FVector Scale = Mesh->GetRelativeScale3D();
		const FVector Extent = (Bounds.BoxExtent * Scale.GetAbs()).ComponentMax(FVector(6.f)) + FVector(3.f);
		PickupVolume->SetBoxExtent(Extent);
		PickupVolume->SetRelativeLocation(Mesh->GetRelativeLocation() + Mesh->GetRelativeRotation().RotateVector(Bounds.Origin * Scale));
	}
}

void AHHLootActor::ApplyRevealState()
{
	SetActorHiddenInGame(!bRevealed);
	PickupVolume->SetCollisionEnabled(bRevealed ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AHHLootActor::Reveal()
{
	if (HasAuthority() && !bRevealed)
	{
		bRevealed = true;
		ApplyRevealState();
		ForceNetUpdate();
	}
}

// ---------------------------------------------------------------------------------------
// Interaction

FText AHHLootActor::GetInteractionPrompt(AActor* Interactor) const
{
	if (IsKey())
	{
		return FText::Format(LOCTEXT("TakeKey", "Take {0}"), KeyName.IsEmpty() ? LOCTEXT("TheKey", "the key") : KeyName);
	}
	const UHHItemDefinition* Item = GetDefinition();
	return FText::Format(LOCTEXT("PickUp", "Pick up {0}"), Item ? Item->DisplayName : FText::FromName(ItemId));
}

FText AHHLootActor::GetInteractionDetail(AActor* Interactor) const
{
	if (IsKey())
	{
		return LOCTEXT("KeyDetail", "Key  -  shared with the crew");
	}

	const APawn* Pawn = Cast<APawn>(Interactor);
	const AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
	const UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	if (Inventory && Inventory->IsEnabled() && !Inventory->HasFreeSlot())
	{
		return LOCTEXT("BagFull", "Bag full (5/5)  -  drop something first");
	}

	if (Kind == EHHInventoryKind::Equipment)
	{
		return LOCTEXT("EquipmentDetail", "Equipment");
	}
	const UHHLootDefinition* Loot = Cast<UHHLootDefinition>(GetDefinition());
	if (Loot && Loot->bHeavy)
	{
		return FText::Format(LOCTEXT("ValueHeavy", "{0}  -  Heavy (slows you down)"), HHText::Money(Value));
	}
	return HHText::Money(Value);
}

UTexture2D* AHHLootActor::GetInteractionIcon() const
{
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	const UHHItemDefinition* Item = GetDefinition();
	return Registry && Item ? Registry->GetIcon(Item) : nullptr;
}

float AHHLootActor::GetInteractDuration(AActor* Interactor) const
{
	const UHHLootDefinition* Loot = Cast<UHHLootDefinition>(GetDefinition());
	return Loot && Loot->bHeavy ? HeavyPickupSeconds : 0.f;
}

bool AHHLootActor::CanInteract(AActor* Interactor) const
{
	if (bTaken || !bRevealed || !bInteractionEnabled)
	{
		return false;
	}
	if (IsKey())
	{
		return true;
	}
	const APawn* Pawn = Cast<APawn>(Interactor);
	const AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
	const UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	return Inventory && Inventory->IsEnabled() && Inventory->HasFreeSlot();
}

void AHHLootActor::Interact(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(Interactor);
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	AHHHeistGameMode* HeistMode = GetWorld()->GetAuthGameMode<AHHHeistGameMode>();
	AHHHeistGameState* HeistState = GetWorld()->GetGameState<AHHHeistGameState>();

	USoundBase* Sound = InteractSound;
	if (IsKey())
	{
		bTaken = true;
		if (HeistMode)
		{
			HeistMode->HandleKeyFound(Controller, KeyId, KeyName);
		}
	}
	else
	{
		AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
		UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
		FHHInventorySlot Item;
		Item.Kind = Kind;
		Item.ItemId = ItemId;
		Item.Value = Kind == EHHInventoryKind::Loot ? Value : 0;
		if (!Inventory || !Inventory->AddItem(Item))
		{
			return;
		}
		bTaken = true;
		if (const UHHLootDefinition* Loot = Cast<UHHLootDefinition>(GetDefinition()))
		{
			if (!Loot->PickupSound.IsNull())
			{
				Sound = Loot->PickupSound.LoadSynchronous();
			}
		}
		if (HeistMode)
		{
			HeistMode->HandleItemPickedUp(Controller, Item, this);
		}
	}

	BP_OnInteracted(Interactor);
	if (Sound && HeistState)
	{
		HeistState->MulticastSoundAt(Sound, GetActorLocation(), 1.f);
	}
	Destroy();
}

AHHLootActor* AHHLootActor::SpawnDropped(UWorld* World, const FHHInventorySlot& Item, const FTransform& Transform)
{
	if (!World || Item.IsEmpty())
	{
		return nullptr;
	}
	AHHLootActor* Actor = World->SpawnActorDeferred<AHHLootActor>(AHHLootActor::StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->ItemId = Item.ItemId;
	Actor->Kind = Item.Kind;
	Actor->Value = Item.Value;
	Actor->FinishSpawning(Transform);
	return Actor;
}

#undef LOCTEXT_NAMESPACE
