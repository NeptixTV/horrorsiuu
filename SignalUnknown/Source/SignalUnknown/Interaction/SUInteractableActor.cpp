#include "Interaction/SUInteractableActor.h"
#include "Core/SUGameUserSettings.h"
#include "Net/UnrealNetwork.h"

ASUInteractableActor::ASUInteractableActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bStaticGeometry = false;
	SceneRoot->SetMobility(EComponentMobility::Movable);
}

void ASUInteractableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUInteractableActor, PromptText);
}

void ASUInteractableActor::BeginPlay()
{
	Super::BeginPlay();
	if (!bGeometryBuilt)
	{
		bGeometryBuilt = true;
		BuildGeometry();
	}
}

bool ASUInteractableActor::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	if (PromptText.IsEmpty())
	{
		return false;
	}
	OutPrompt.Primary = PromptText;
	OutPrompt.bPrimary = true;
	return true;
}

void ASUInteractableActor::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	OnInteracted(User, Option);
}

USUProceduralAudioComponent* ASUInteractableActor::GetOrCreateSound(ESUSoundKind Kind, float Gain, float Radius)
{
	for (USUProceduralAudioComponent* S : Sounds)
	{
		if (S && S->GetKind() == Kind)
		{
			return S;
		}
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	USUProceduralAudioComponent* Sound = NewObject<USUProceduralAudioComponent>(this);
	Sound->SetupAttachment(SceneRoot);
	Sound->ConfigureSound(Kind, SUAudioCategory::Effects, Gain, Radius);
	Sound->RegisterComponent();
	Sound->StartSound();
	Sounds.Add(Sound);
	return Sound;
}
