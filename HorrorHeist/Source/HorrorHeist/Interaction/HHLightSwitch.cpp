#include "Interaction/HHLightSwitch.h"
#include "World/HHPracticalLight.h"
#include "Core/HHNoiseSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "HHLightSwitch"

AHHLightSwitch::AHHLightSwitch()
{
	Lever = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lever"));
	Lever->SetupAttachment(Mesh);
	Lever->SetCollisionProfileName(TEXT("BlockAll"));
	Lever->SetMobility(EComponentMobility::Movable);
}

void AHHLightSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHLightSwitch, bSwitchedOn);
}

FText AHHLightSwitch::GetInteractionPrompt(AActor* Interactor) const
{
	return bSwitchedOn ? LOCTEXT("Off", "Kill the lights") : LOCTEXT("On", "Lights on");
}

void AHHLightSwitch::Interact(AActor* Interactor)
{
	Super::Interact(Interactor);
	if (!HasAuthority())
	{
		return;
	}

	bSwitchedOn = !bSwitchedOn;
	OnRep_SwitchedOn();

	for (TActorIterator<AHHPracticalLight> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(LightTag))
		{
			It->SetLightOn(bSwitchedOn);
		}
	}

	if (UHHNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UHHNoiseSubsystem>())
	{
		Noise->ReportNoise(GetActorLocation(), 0.25f, Interactor, TEXT("Switch"));
	}
}

void AHHLightSwitch::OnRep_SwitchedOn()
{
	Lever->SetRelativeRotation(FRotator(bSwitchedOn ? LeverOnPitch : LeverOffPitch, 0.f, 0.f));
}

#undef LOCTEXT_NAMESPACE
