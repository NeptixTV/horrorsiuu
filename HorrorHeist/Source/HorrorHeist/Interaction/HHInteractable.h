#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HHInteractable.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHHInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything the player can use with the interact key. Native interface; Blueprint-authored
 * interactables derive from AHHInteractableActor, which exposes Blueprint events.
 */
class HORRORHEIST_API IHHInteractable
{
	GENERATED_BODY()

public:
	/** "Open the job board", "Ready up"... */
	virtual FText GetInteractionPrompt(AActor* Interactor) const { return FText::GetEmpty(); }

	virtual bool CanInteract(AActor* Interactor) const { return true; }

	/** Runs on the server, or on the interacting client when IsLocalInteraction() is true. */
	virtual void Interact(AActor* Interactor) {}

	/** UI-only interactions (opening a screen) skip the server round trip. */
	virtual bool IsLocalInteraction() const { return false; }

	/** Local highlight while the player looks at it. */
	virtual void SetInteractionFocus(bool bFocused) {}
};
