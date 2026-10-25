// SIGNAL UNKNOWN – finds what the local player is looking at and triggers interactions.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/SUTypes.h"
#include "SUInteractionComponent.generated.h"

UCLASS(ClassGroup = (SignalUnknown), meta = (BlueprintSpawnableComponent))
class SIGNALUNKNOWN_API USUInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USUInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool HasFocus() const { return FocusActor.IsValid() && bHasPrompt; }
	const FSUInteractionPrompt& GetPrompt() const { return Prompt; }
	AActor* GetFocusActor() const { return FocusActor.Get(); }

	/** Local: send the interaction for the focused object to the server. */
	void TryInteract(ESUInteractOption Option);

	/** Trace length (cm). */
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float TraceLength = 320.f;

private:
	TWeakObjectPtr<AActor> FocusActor;
	TWeakObjectPtr<UPrimitiveComponent> FocusComponent;
	FSUInteractionPrompt Prompt;
	bool bHasPrompt = false;

	void UpdateFocus();
};
