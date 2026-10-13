#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "HHHauntDevice.generated.h"

class UAudioComponent;
class UPointLightComponent;
class USoundBase;

/**
 * A television, radio or music box that switches itself on. The crew can switch it off again
 * (it is loud and draws attention). Server switches, everyone sees and hears it.
 */
UCLASS()
class HORRORHEIST_API AHHHauntDevice : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHHauntDevice();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IHHInteractable
	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

	bool IsOn() const { return bOn; }
	/** Server: on for Duration seconds (<= 0: until switched off). */
	void TurnOn(float Duration);
	void TurnOff();

protected:
	UFUNCTION()
	void OnRep_On();

	/** Emissive screen / glow mesh, only visible while on. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Device")
	TObjectPtr<UStaticMeshComponent> Screen;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Device")
	TObjectPtr<UAudioComponent> Audio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Device")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Device")
	TObjectPtr<USoundBase> SwitchOnSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Device")
	FText DeviceName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Device")
	FText Caption;

	/** A telephone: whoever answers it hears this (and it costs them some nerve). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Device")
	TObjectPtr<USoundBase> AnswerSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Device")
	FText AnswerCaption;

private:
	UPROPERTY(ReplicatedUsing = OnRep_On)
	bool bOn = false;

	FTimerHandle OffTimer;
};
