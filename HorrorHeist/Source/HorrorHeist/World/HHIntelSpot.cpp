#include "World/HHIntelSpot.h"
#include "Lobby/HHLobbyGameState.h"
#include "Heist/HHHeistGameState.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHMissionDefinition.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "World/HHTimeOfDay.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "HHIntelSpot"

AHHIntelSpot::AHHIntelSpot()
{
	// The spot is the prop it sits on (mailbox, window, gnome): only an invisible use volume here.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UseVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("UseVolume"));
	UseVolume->SetupAttachment(Root);
	UseVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UseVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	UseVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	UseVolume->SetGenerateOverlapEvents(false);

	Prompt = LOCTEXT("DefaultPrompt", "Have a look");
}

void AHHIntelSpot::BeginPlay()
{
	Super::BeginPlay();
	UseVolume->SetBoxExtent(UseExtent);
	if (IsOnJob() && !bUsableOnJobs)
	{
		// Out of the way on a job: the same spot may hold a job interactable (the gnome's key).
		UseVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

bool AHHIntelSpot::IsOnJob() const
{
	return AHHHeistGameState::IsJobWorld(this);
}

bool AHHIntelSpot::IsTimeRight() const
{
	const AHHTimeOfDay* Clock = AHHTimeOfDay::Get(this);
	if (!Clock || TimeRule == EHHIntelTime::Any)
	{
		return true;
	}
	return TimeRule == EHHIntelTime::DayOnly ? !Clock->IsNight() : Clock->IsNight();
}

bool AHHIntelSpot::HasSomethingNew() const
{
	const AHHLobbyGameState* State = GetWorld() ? GetWorld()->GetGameState<AHHLobbyGameState>() : nullptr;
	if (!State)
	{
		return false;
	}
	for (const FName Id : IntelIds)
	{
		if (!State->IsIntelKnown(Id))
		{
			return true;
		}
	}
	return false;
}

FText AHHIntelSpot::GetInteractionDetail(AActor* Interactor) const
{
	if (IsOnJob() && !bUsableOnJobs)
	{
		return LOCTEXT("OnJob", "No time for that now.");
	}
	if (!IsTimeRight())
	{
		return TimeRule == EHHIntelTime::DayOnly
			? LOCTEXT("NeedDay", "Too dark to make anything out. Come back by day.")
			: LOCTEXT("NeedNight", "Nothing to see in daylight.");
	}
	if (!HasSomethingNew())
	{
		return LOCTEXT("Known", "Already in the crew's notebook.");
	}
	return HoldSeconds > 0.f
		? FText::Format(LOCTEXT("Scout", "Hold - scouting adds to the job's dossier ({0}s)"), FText::AsNumber(FMath::RoundToInt(HoldSeconds)))
		: LOCTEXT("ScoutNow", "Scouting adds to the job's dossier");
}

bool AHHIntelSpot::CanInteract(AActor* Interactor) const
{
	return bInteractionEnabled && (!IsOnJob() || bUsableOnJobs) && IsTimeRight() && HasSomethingNew();
}

void AHHIntelSpot::Interact(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}
	AHHLobbyGameState* State = GetWorld()->GetGameState<AHHLobbyGameState>();
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	const APawn* Pawn = Cast<APawn>(Interactor);
	const AHHPlayerState* PlayerState = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
	if (!State || !Registry)
	{
		return;
	}

	Super::Interact(Interactor);

	TArray<FText> Learned;
	const UHHMissionDefinition* Mission = nullptr;
	for (const FName Id : IntelIds)
	{
		const UHHMissionDefinition* Owner = nullptr;
		const FHHJobIntel* Intel = Registry->FindIntel(Id, &Owner);
		if (Intel && State->LearnIntel(Id))
		{
			Learned.Add(Intel->Title);
			Mission = Owner;
		}
	}
	if (Learned.Num() == 0)
	{
		return;
	}

	const FText Who = PlayerState ? FText::FromString(PlayerState->GetPlayerName()) : LOCTEXT("Someone", "Someone");
	State->MulticastCrewMessage(FText::Format(LOCTEXT("Scouted", "{0} scouted {1}: {2}"), Who,
		Mission ? Mission->DisplayName : FText::GetEmpty(), FText::Join(FText::FromString(TEXT(", ")), Learned)), EHHNotifyType::Success);
	if (AHHPlayerController* PC = Pawn ? Cast<AHHPlayerController>(Pawn->GetController()) : nullptr)
	{
		if (!FoundText.IsEmpty())
		{
			PC->ClientNotify(FoundText, EHHNotifyType::Info);
		}
	}
}

#undef LOCTEXT_NAMESPACE
