#include "Player/HHInteractionComponent.h"
#include "Player/HHCharacter.h"
#include "Interaction/HHInteractable.h"
#include "Audio/HHAudioSubsystem.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"

UHHInteractionComponent::UHHInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.06f;
}

void UHHInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetFocus(nullptr);
	Super::EndPlay(EndPlayReason);
}

void UHHInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		// Only the local player needs focus; remote copies stop ticking.
		if (Pawn && Pawn->GetController() == nullptr && Pawn->GetLocalRole() == ROLE_SimulatedProxy)
		{
			SetComponentTickEnabled(false);
		}
		return;
	}
	UpdateFocus();
}

void UHHInteractionComponent::SetSuspended(bool bInSuspended)
{
	bSuspended = bInSuspended;
	if (bSuspended)
	{
		SetFocus(nullptr);
	}
}

void UHHInteractionComponent::UpdateFocus()
{
	const AHHCharacter* Character = Cast<AHHCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World || bSuspended)
	{
		SetFocus(nullptr);
		return;
	}

	FVector Start;
	FRotator Rotation;
	Character->GetViewPoint(Start, Rotation);
	const FVector End = Start + Rotation.Vector() * InteractDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHInteractionFocus), false, Character);
	FHitResult Hit;
	AActor* Candidate = nullptr;
	if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(SweepRadius), Params))
	{
		AActor* HitActor = Hit.GetActor();
		// Allow child actors / attached props to forward to their interactable parent.
		while (HitActor && !Cast<IHHInteractable>(HitActor))
		{
			HitActor = HitActor->GetAttachParentActor();
		}
		Candidate = HitActor;
	}

	SetFocus(Candidate);

	if (const IHHInteractable* Interactable = Cast<IHHInteractable>(Candidate))
	{
		AActor* Interactor = const_cast<AHHCharacter*>(Character);
		FocusedPrompt = Interactable->GetInteractionPrompt(Interactor);
		bFocusUsable = Interactable->CanInteract(Interactor);
	}
	else
	{
		FocusedPrompt = FText::GetEmpty();
		bFocusUsable = false;
	}
}

void UHHInteractionComponent::SetFocus(AActor* NewFocus)
{
	AActor* Previous = Focused.Get();
	if (Previous == NewFocus)
	{
		return;
	}
	if (IHHInteractable* Old = Cast<IHHInteractable>(Previous))
	{
		Old->SetInteractionFocus(false);
	}
	Focused = NewFocus;
	if (IHHInteractable* New = Cast<IHHInteractable>(NewFocus))
	{
		New->SetInteractionFocus(true);
	}
	if (!NewFocus)
	{
		FocusedPrompt = FText::GetEmpty();
		bFocusUsable = false;
	}
}

void UHHInteractionComponent::TryInteract()
{
	AHHCharacter* Character = Cast<AHHCharacter>(GetOwner());
	AActor* Target = Focused.Get();
	IHHInteractable* Interactable = Cast<IHHInteractable>(Target);
	if (!Character || !Interactable)
	{
		return;
	}

	if (!Interactable->CanInteract(Character))
	{
		if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
		{
			Audio->PlayUI(EHHUISound::Error);
		}
		return;
	}

	if (Interactable->IsLocalInteraction())
	{
		Interactable->Interact(Character);
		return;
	}

	Character->ServerInteract(Target);
}

void UHHInteractionComponent::ExecuteInteraction(AActor* Target)
{
	AActor* Owner = GetOwner();
	IHHInteractable* Interactable = Cast<IHHInteractable>(Target);
	if (!Owner || !Interactable || !Owner->HasAuthority() || Interactable->IsLocalInteraction())
	{
		return;
	}

	// Generous tolerance for latency; this is a co-op game, not an anti-cheat boundary.
	const float MaxDistance = InteractDistance + 200.f;
	const FVector Closest = Target->GetActorLocation();
	if (FVector::DistSquared(Owner->GetActorLocation(), Closest) > FMath::Square(MaxDistance + Target->GetSimpleCollisionRadius()))
	{
		return;
	}
	if (!Interactable->CanInteract(Owner))
	{
		return;
	}
	Interactable->Interact(Owner);
}
