#include "Heist/HHHauntDevice.h"
#include "Audio/HHAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "HHHauntDevice"

AHHHauntDevice::AHHHauntDevice()
{
	Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
	Screen->SetupAttachment(Root);
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Screen->SetVisibility(false);

	Audio = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio"));
	Audio->SetupAttachment(Root);
	Audio->bAutoActivate = false;

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(18.f);
	Glow->SetAttenuationRadius(450.f);
	Glow->SetLightColor(FLinearColor(0.6f, 0.75f, 1.f));
	Glow->SetCastShadows(false);
	Glow->SetVisibility(false);
	Glow->SetMobility(EComponentMobility::Movable);

	DeviceName = LOCTEXT("TV", "television");
}

void AHHHauntDevice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHHauntDevice, bOn);
}

void AHHHauntDevice::BeginPlay()
{
	Super::BeginPlay();
	OnRep_On();
}

void AHHHauntDevice::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(OffTimer);
	Super::EndPlay(EndPlayReason);
}

FText AHHHauntDevice::GetInteractionPrompt(AActor* Interactor) const
{
	return FText::Format(LOCTEXT("SwitchOff", "Switch off the {0}"), DeviceName);
}

bool AHHHauntDevice::CanInteract(AActor* Interactor) const
{
	return bOn;
}

void AHHHauntDevice::Interact(AActor* Interactor)
{
	BP_OnInteracted(Interactor);
	if (HasAuthority())
	{
		TurnOff();
	}
}

void AHHHauntDevice::TurnOn(float Duration)
{
	if (!HasAuthority())
	{
		return;
	}
	bOn = true;
	OnRep_On();
	ForceNetUpdate();
	GetWorldTimerManager().ClearTimer(OffTimer);
	if (Duration > 0.f)
	{
		GetWorldTimerManager().SetTimer(OffTimer, this, &AHHHauntDevice::TurnOff, Duration, false);
	}
}

void AHHHauntDevice::TurnOff()
{
	if (HasAuthority() && bOn)
	{
		bOn = false;
		OnRep_On();
		ForceNetUpdate();
	}
}

void AHHHauntDevice::OnRep_On()
{
	Screen->SetVisibility(bOn);
	Glow->SetVisibility(bOn);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (bOn)
	{
		if (SwitchOnSound && HasActorBegunPlay())
		{
			UGameplayStatics::PlaySoundAtLocation(this, SwitchOnSound, GetActorLocation());
		}
		if (Audio->Sound)
		{
			Audio->FadeIn(0.3f, 1.f);
		}
		if (!Caption.IsEmpty() && HasActorBegunPlay())
		{
			if (UHHAudioSubsystem* AudioSubsystem = UHHAudioSubsystem::Get(this))
			{
				AudioSubsystem->ShowCaption(Caption, 3.f, true);
			}
		}
	}
	else if (Audio->IsPlaying())
	{
		Audio->FadeOut(0.25f, 0.f);
	}
}

#undef LOCTEXT_NAMESPACE
