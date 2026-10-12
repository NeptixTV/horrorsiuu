#include "Heist/HHVanPoint.h"
#include "Heist/HHHeistGameMode.h"
#include "Heist/HHHeistGameState.h"
#include "Player/HHPlayerState.h"
#include "Player/HHInventoryComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "HHVan"

AHHVanPoint::AHHVanPoint()
{
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UseVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("UseVolume"));
	UseVolume->SetupAttachment(Root);
	UseVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UseVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	UseVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	UseVolume->SetGenerateOverlapEvents(false);
}

void AHHVanPoint::BeginPlay()
{
	Super::BeginPlay();
	UseVolume->SetBoxExtent(UseExtent);
}

namespace HHVan
{
	static const UHHInventoryComponent* InventoryOf(AActor* Interactor)
	{
		const APawn* Pawn = Cast<APawn>(Interactor);
		const AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
		return State ? State->GetInventory() : nullptr;
	}
}

FText AHHVanPoint::GetInteractionPrompt(AActor* Interactor) const
{
	return PointType == EHHVanPointType::Stash ? LOCTEXT("Stash", "Stash your loot in the van") : LOCTEXT("Drive", "Drive away");
}

FText AHHVanPoint::GetInteractionDetail(AActor* Interactor) const
{
	const AHHHeistGameState* State = GetWorld()->GetGameState<AHHHeistGameState>();
	if (!State)
	{
		return FText::GetEmpty();
	}
	if (PointType == EHHVanPointType::Stash)
	{
		const UHHInventoryComponent* Inventory = HHVan::InventoryOf(Interactor);
		const int32 Count = Inventory ? Inventory->CountLoot() : 0;
		if (Count == 0)
		{
			return LOCTEXT("NothingToStash", "You are not carrying any loot");
		}
		return FText::Format(LOCTEXT("StashDetail", "{0} {0}|plural(one=item,other=items)  -  {1}"), Count, HHText::Money(Inventory->GetCarriedLootValue()));
	}

	if (State->GetCrewAtVan() < State->GetCrewTotal())
	{
		return FText::Format(LOCTEXT("Waiting", "Waiting for the crew ({0}/{1} at the van)"), FText::AsNumber(State->GetCrewAtVan()), FText::AsNumber(State->GetCrewTotal()));
	}
	if (State->GetSecuredValue() < State->GetMinimumHaul())
	{
		return FText::Format(LOCTEXT("Short", "Hold  -  only {0} of {1} in the van"), HHText::Money(State->GetSecuredValue()), HHText::Money(State->GetMinimumHaul()));
	}
	return FText::Format(LOCTEXT("Ready", "Hold  -  {0} in the van"), HHText::Money(State->GetSecuredValue()));
}

bool AHHVanPoint::CanInteract(AActor* Interactor) const
{
	const AHHHeistGameState* State = GetWorld()->GetGameState<AHHHeistGameState>();
	if (!bInteractionEnabled || !State || State->GetHeistPhase() == EHHHeistPhase::Ended)
	{
		return false;
	}
	if (PointType == EHHVanPointType::Stash)
	{
		const UHHInventoryComponent* Inventory = HHVan::InventoryOf(Interactor);
		return Inventory && Inventory->CountLoot() > 0;
	}
	return State->GetCrewAtVan() >= State->GetCrewTotal();
}

float AHHVanPoint::GetInteractDuration(AActor* Interactor) const
{
	return PointType == EHHVanPointType::Drive ? DriveHoldSeconds : 0.f;
}

void AHHVanPoint::Interact(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(Interactor);
	AController* User = Pawn ? Pawn->GetController() : nullptr;
	AHHHeistGameMode* HeistMode = GetWorld()->GetAuthGameMode<AHHHeistGameMode>();
	if (!HeistMode || !User)
	{
		return;
	}
	if (PointType == EHHVanPointType::Stash)
	{
		HeistMode->HandleStash(User, InteractSound, GetActorLocation());
	}
	else
	{
		HeistMode->HandleDriveAway(User, InteractSound, GetActorLocation());
	}
}

#undef LOCTEXT_NAMESPACE
