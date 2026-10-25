// SIGNAL UNKNOWN – interaction interface (E = primary, Q = secondary).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/SUTypes.h"
#include "SUInteractable.generated.h"

class ASUCharacter;
class UPrimitiveComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USUInteractable : public UInterface
{
	GENERATED_BODY()
};

class SIGNALUNKNOWN_API ISUInteractable
{
	GENERATED_BODY()

public:
	/**
	 * Local (any machine): fills the prompt for the looked-at component.
	 * Return false if nothing can be done right now.
	 */
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const = 0;

	/** Server only. Called after distance validation. */
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) = 0;

	/** Maximum interaction distance from the player's eyes (cm). */
	virtual float GetInteractionRange() const { return 260.f; }
};
