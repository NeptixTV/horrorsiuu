#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HHInteractable.generated.h"

class UTexture2D;
class USoundBase;

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

	/** Seconds the interact key must be held (0 = instant). Validated again on the server. */
	virtual float GetInteractDuration(AActor* Interactor) const { return 0.f; }

	/** Optional second line under the prompt: "$450  -  Heavy", "Needs lockpicks". */
	virtual FText GetInteractionDetail(AActor* Interactor) const { return FText::GetEmpty(); }

	/** Optional icon for the focus card (loot, equipment). */
	virtual UTexture2D* GetInteractionIcon() const { return nullptr; }

	/** Optional loop played for the holder while a timed interaction runs (lockpicking...). */
	virtual USoundBase* GetHoldSound(AActor* Interactor) const { return nullptr; }
};
