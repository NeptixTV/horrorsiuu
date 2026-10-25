// SIGNAL UNKNOWN – replicated base class for interactive objects.
//
// Geometry is built locally on every machine in BuildGeometry() (called from BeginPlay), using
// replicated configuration values. Interactions run on the server.
#pragma once

#include "CoreMinimal.h"
#include "World/SUProceduralActor.h"
#include "Interaction/SUInteractable.h"
#include "Audio/SUProceduralAudioComponent.h"
#include "SUInteractableActor.generated.h"

UCLASS()
class SIGNALUNKNOWN_API ASUInteractableActor : public ASUProceduralActor, public ISUInteractable
{
	GENERATED_BODY()

public:
	ASUInteractableActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ISUInteractable
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;

	/** Simple configurable prompt for the base implementation. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText PromptText;

	/** Blueprint hook: called on the server after an interaction. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void OnInteracted(ASUCharacter* User, ESUInteractOption Option);

protected:
	/** Build the visual/collision geometry. Runs on server and clients. */
	virtual void BuildGeometry() {}

	/** Creates (once) a spatial procedural sound on this actor. */
	USUProceduralAudioComponent* GetOrCreateSound(ESUSoundKind Kind, float Gain, float Radius);

	bool bGeometryBuilt = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USUProceduralAudioComponent>> Sounds;
};
