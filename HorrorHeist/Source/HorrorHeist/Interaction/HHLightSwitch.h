#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "HHLightSwitch.generated.h"

/** Breaker / wall switch: toggles every practical light carrying LightTag. */
UCLASS()
class HORRORHEIST_API AHHLightSwitch : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHLightSwitch();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

protected:
	UFUNCTION()
	void OnRep_SwitchedOn();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	FName LightTag = TEXT("HH_MainLights");

	/** Lever / toggle mesh rotation when off vs. on (pitch). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	float LeverOnPitch = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	float LeverOffPitch = -25.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Switch")
	TObjectPtr<UStaticMeshComponent> Lever;

private:
	UPROPERTY(ReplicatedUsing = OnRep_SwitchedOn)
	bool bSwitchedOn = true;
};
