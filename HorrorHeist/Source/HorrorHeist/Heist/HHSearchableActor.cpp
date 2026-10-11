#include "Heist/HHSearchableActor.h"
#include "Heist/HHLootActor.h"
#include "Heist/HHHeistGameState.h"
#include "Player/HHPlayerController.h"
#include "Core/HHNoiseSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HHSearchable"

AHHSearchableActor::AHHSearchableActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh->SetMobility(EComponentMobility::Movable);

	SearchVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SearchVolume"));
	SearchVolume->SetupAttachment(Root);
	SearchVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SearchVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	SearchVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SearchVolume->SetGenerateOverlapEvents(false);

	Prompt = LOCTEXT("Search", "Search");
}

void AHHSearchableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHSearchableActor, bSearched);
}

void AHHSearchableActor::BeginPlay()
{
	Super::BeginPlay();
	SearchVolume->SetBoxExtent(SearchVolumeExtent);
	ClosedLocation = Mesh->GetRelativeLocation();
}

FText AHHSearchableActor::GetInteractionPrompt(AActor* Interactor) const
{
	if (bSearched)
	{
		return ContainerName.IsEmpty() ? LOCTEXT("Searched", "Searched") : FText::Format(LOCTEXT("SearchedNamed", "{0} (searched)"), ContainerName);
	}
	return ContainerName.IsEmpty() ? Prompt : FText::Format(LOCTEXT("SearchNamed", "Search the {0}"), ContainerName);
}

FText AHHSearchableActor::GetInteractionDetail(AActor* Interactor) const
{
	return bSearched ? FText::GetEmpty() : LOCTEXT("HoldDetail", "Hold to search");
}

bool AHHSearchableActor::CanInteract(AActor* Interactor) const
{
	return bInteractionEnabled && !bSearched;
}

void AHHSearchableActor::Interact(AActor* Interactor)
{
	if (!HasAuthority() || bSearched)
	{
		return;
	}
	bSearched = true;
	OnRep_Searched();
	ForceNetUpdate();
	BP_OnInteracted(Interactor);

	int32 Found = 0;
	if (!ContainerId.IsNone())
	{
		for (TActorIterator<AHHLootActor> It(GetWorld()); It; ++It)
		{
			if (It->GetContainerId() == ContainerId && !It->IsRevealed())
			{
				It->Reveal();
				++Found;
			}
		}
	}

	if (AHHHeistGameState* HeistState = GetWorld()->GetGameState<AHHHeistGameState>())
	{
		if (InteractSound)
		{
			HeistState->MulticastSoundAt(InteractSound, GetActorLocation(), 1.f);
		}
	}
	if (UHHNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UHHNoiseSubsystem>())
	{
		Noise->ReportNoise(GetActorLocation(), 0.2f, Interactor, TEXT("Search"));
	}
	if (Found == 0)
	{
		const APawn* Pawn = Cast<APawn>(Interactor);
		if (AHHPlayerController* PC = Pawn ? Cast<AHHPlayerController>(Pawn->GetController()) : nullptr)
		{
			PC->ClientNotify(LOCTEXT("Nothing", "Nothing worth taking in here."), EHHNotifyType::Info);
		}
	}
}

void AHHSearchableActor::OnRep_Searched()
{
	if (bSearched && !OpenOffset.IsNearlyZero())
	{
		SetActorTickEnabled(true);
	}
}

void AHHSearchableActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	OpenAlpha = FMath::FInterpTo(OpenAlpha, 1.f, DeltaSeconds, 8.f);
	Mesh->SetRelativeLocation(ClosedLocation + OpenOffset * OpenAlpha);
	if (OpenAlpha > 0.995f)
	{
		Mesh->SetRelativeLocation(ClosedLocation + OpenOffset);
		SetActorTickEnabled(false);
	}
}

#undef LOCTEXT_NAMESPACE
