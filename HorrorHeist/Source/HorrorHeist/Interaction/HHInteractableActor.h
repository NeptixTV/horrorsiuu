#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/HHInteractable.h"
#include "HHInteractableActor.generated.h"

class UStaticMeshComponent;
class USoundBase;

/**
 * Base class for placed interactables: a mesh, a prompt and a highlight. Blueprint
 * subclasses can react through OnInteracted without writing C++.
 */
UCLASS(Abstract, Blueprintable)
class HORRORHEIST_API AHHInteractableActor : public AActor, public IHHInteractable
{
	GENERATED_BODY()

public:
	AHHInteractableActor();

	// IHHInteractable
	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;
	virtual void SetInteractionFocus(bool bFocused) override;

protected:
	/** Blueprint hook, called wherever Interact runs (server or local). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction", meta = (DisplayName = "On Interacted"))
	void BP_OnInteracted(AActor* Interactor);

	/** Plays a sound for everyone (server -> multicast). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlaySound(USoundBase* Sound, FVector Location, float Volume);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Prompt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bInteractionEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<USoundBase> InteractSound;
};
