#include "Player/SUInteractionComponent.h"
#include "Player/SUCharacter.h"
#include "Interaction/SUInteractable.h"
#include "Engine/World.h"

USUInteractionComponent::USUInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
}

void USUInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		UpdateFocus();
	}
}

void USUInteractionComponent::UpdateFocus()
{
	FocusActor.Reset();
	FocusComponent.Reset();
	bHasPrompt = false;
	Prompt = FSUInteractionPrompt();

	ASUCharacter* Char = Cast<ASUCharacter>(GetOwner());
	if (!Char || Char->IsDead() || Char->IsSeated())
	{
		return;
	}
	const FVector Start = Char->GetEyeLocation();
	const FVector End = Start + Char->GetControlRotation().Vector() * TraceLength;
	FCollisionQueryParams Params(FName(TEXT("SUInteract")), false, Char);
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(3.f), Params))
	{
		return;
	}
	// walk up the attachment chain (e.g. an item attached to a drawer)
	AActor* Candidate = Hit.GetActor();
	ISUInteractable* Interactable = nullptr;
	for (int32 Depth = 0; Candidate && Depth < 3; ++Depth)
	{
		Interactable = Cast<ISUInteractable>(Candidate);
		if (Interactable)
		{
			break;
		}
		Candidate = Candidate->GetAttachParentActor();
	}
	if (!Interactable || Hit.Distance > Interactable->GetInteractionRange())
	{
		return;
	}
	FSUInteractionPrompt NewPrompt;
	if (!Interactable->GetInteractionPrompt(Char, Hit.GetComponent(), NewPrompt))
	{
		return;
	}
	FocusActor = Candidate;
	FocusComponent = Hit.GetComponent();
	Prompt = NewPrompt;
	bHasPrompt = NewPrompt.bPrimary || NewPrompt.bSecondary || !NewPrompt.Info.IsEmpty();
}

void USUInteractionComponent::TryInteract(ESUInteractOption Option)
{
	ASUCharacter* Char = Cast<ASUCharacter>(GetOwner());
	if (!Char || !FocusActor.IsValid())
	{
		return;
	}
	const bool bAllowed = Option == ESUInteractOption::Primary ? Prompt.bPrimary : Prompt.bSecondary;
	if (!bAllowed)
	{
		return;
	}
	Char->ServerInteract(FocusActor.Get(), FocusComponent.Get(), Option);
}
