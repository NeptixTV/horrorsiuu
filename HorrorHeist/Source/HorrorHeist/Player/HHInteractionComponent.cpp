#include "Player/HHInteractionComponent.h"
#include "Player/HHCharacter.h"
#include "Interaction/HHInteractable.h"
#include "Audio/HHAudioSubsystem.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UHHInteractionComponent::UHHInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.06f;
}

void UHHInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelHold();
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
	UpdateHold();
}

void UHHInteractionComponent::SetSuspended(bool bInSuspended)
{
	bSuspended = bInSuspended;
	if (bSuspended)
	{
		CancelHold();
		SetFocus(nullptr);
	}
}

void UHHInteractionComponent::UpdateFocus()
{
	const AHHCharacter* Character = Cast<AHHCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World || bSuspended || Character->IsSeated() || Character->IsArrested())
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
		FocusedDetail = Interactable->GetInteractionDetail(Interactor);
		FocusedIcon = Interactable->GetInteractionIcon();
		FocusedDuration = Interactable->GetInteractDuration(Interactor);
		bFocusUsable = Interactable->CanInteract(Interactor);
	}
	else
	{
		FocusedPrompt = FText::GetEmpty();
		FocusedDetail = FText::GetEmpty();
		FocusedIcon.Reset();
		FocusedDuration = 0.f;
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
		FocusedDetail = FText::GetEmpty();
		FocusedIcon.Reset();
		FocusedDuration = 0.f;
		bFocusUsable = false;
	}
}

void UHHInteractionComponent::TryInteract()
{
	AHHCharacter* Character = Cast<AHHCharacter>(GetOwner());
	AActor* Target = Focused.Get();
	IHHInteractable* Interactable = Cast<IHHInteractable>(Target);
	if (!Character || !Interactable || IsHolding())
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

	const float Duration = Interactable->GetInteractDuration(Character);
	if (Duration > 0.f)
	{
		HoldTarget = Target;
		HoldDuration = Duration;
		HoldStartTime = GetWorld()->GetTimeSeconds();
		if (USoundBase* Loop = Interactable->GetHoldSound(Character))
		{
			HoldAudio = UGameplayStatics::SpawnSoundAttached(Loop, Character->GetRootComponent());
		}
		Character->ServerBeginHold(Target);
		return;
	}

	Character->ServerInteract(Target);
}

void UHHInteractionComponent::StopInteract()
{
	CancelHold();
}

float UHHInteractionComponent::GetHoldProgress() const
{
	if (!IsHolding() || HoldDuration <= 0.f || !GetWorld())
	{
		return 0.f;
	}
	return FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - HoldStartTime) / HoldDuration), 0.f, 1.f);
}

void UHHInteractionComponent::UpdateHold()
{
	AActor* Target = HoldTarget.Get();
	if (!Target)
	{
		if (HoldTarget.IsStale() || HoldAudio)
		{
			CancelHold();
		}
		return;
	}

	// Looking away, or the target changed its mind (someone else opened it): give up.
	const IHHInteractable* Interactable = Cast<IHHInteractable>(Target);
	AHHCharacter* Character = Cast<AHHCharacter>(GetOwner());
	if (!Character || !Interactable || Focused.Get() != Target || !Interactable->CanInteract(Character))
	{
		CancelHold();
		return;
	}

	if (GetHoldProgress() >= 1.f)
	{
		CancelHold();
		Character->ServerInteract(Target);
	}
}

void UHHInteractionComponent::CancelHold()
{
	HoldTarget.Reset();
	HoldDuration = 0.f;
	if (HoldAudio)
	{
		HoldAudio->FadeOut(0.15f, 0.f);
		HoldAudio = nullptr;
	}
}

void UHHInteractionComponent::BeginServerHold(AActor* Target)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ServerHoldTarget = Target;
		ServerHoldStart = GetWorld()->GetTimeSeconds();
	}
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

	// Timed interactions must have been held (75% of the time covers network jitter).
	const float Duration = Interactable->GetInteractDuration(Owner);
	if (Duration > 0.f)
	{
		const double Held = GetWorld()->GetTimeSeconds() - ServerHoldStart;
		if (ServerHoldTarget.Get() != Target || Held < Duration * 0.75f)
		{
			return;
		}
		ServerHoldTarget.Reset();
	}
	Interactable->Interact(Owner);
}
