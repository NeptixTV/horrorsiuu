#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "HHReadyStation.generated.h"

/**
 * The van's sliding door: interacting toggles your ready state (server-validated).
 * When the whole crew is ready the lobby game mode starts the departure countdown.
 */
UCLASS()
class HORRORHEIST_API AHHReadyStation : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHReadyStation();

	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
};
