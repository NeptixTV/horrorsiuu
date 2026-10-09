#include "Interaction/HHInteractableActor.h"
#include "Core/HHGameData.h"
#include "Settings/HHGameUserSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

AHHInteractableActor::AHHInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
}

FText AHHInteractableActor::GetInteractionPrompt(AActor* Interactor) const
{
	return Prompt;
}

bool AHHInteractableActor::CanInteract(AActor* Interactor) const
{
	return bInteractionEnabled;
}

void AHHInteractableActor::Interact(AActor* Interactor)
{
	if (InteractSound && HasAuthority() && !IsLocalInteraction())
	{
		MulticastPlaySound(InteractSound, GetActorLocation(), 1.f);
	}
	else if (InteractSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, InteractSound, GetActorLocation());
	}
	BP_OnInteracted(Interactor);
}

void AHHInteractableActor::MulticastPlaySound_Implementation(USoundBase* Sound, FVector Location, float Volume)
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, Volume);
	}
}

void AHHInteractableActor::SetInteractionFocus(bool bFocused)
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	const bool bHighlight = bFocused && (!Settings || Settings->bInteractionHighlight);

	UMaterialInterface* Overlay = nullptr;
	if (bHighlight)
	{
		if (const UHHGameData* GameData = UHHGameData::Get(this))
		{
			Overlay = GameData->InteractionHighlight.LoadSynchronous();
		}
	}

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* MeshComponent : Meshes)
	{
		MeshComponent->SetOverlayMaterial(Overlay);
	}
}
