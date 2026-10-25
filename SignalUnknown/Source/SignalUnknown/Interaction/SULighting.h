// SIGNAL UNKNOWN – lamps, light switches and the tower generator.
#pragma once

#include "CoreMinimal.h"
#include "Interaction/SUInteractableActor.h"
#include "SULighting.generated.h"

class ULocalLightComponent;
class ASUGenerator;

UENUM()
enum class ESULampStyle : uint8
{
	Ceiling,
	Desk,
	Wall,
	Post,
	Hanging,
	Neon
};

/** A light source. Not interactable itself – switched by ASULightSwitch or scripts. */
UCLASS()
class SIGNALUNKNOWN_API ASULamp : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASULamp();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;

	/** Server. */
	void SetOn(bool bInOn);
	bool IsOn() const { return bOn; }
	bool IsLit() const;

	/** All machines: flicker for a while (horror director). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFlicker(float Duration);

	UPROPERTY(Replicated) ESULampStyle Style = ESULampStyle::Ceiling;
	UPROPERTY(Replicated) float Intensity = 120.f; // candela
	UPROPERTY(Replicated) float Radius = 900.f;
	UPROPERTY(Replicated) FLinearColor LightColor = FLinearColor(1.f, 0.82f, 0.6f);
	UPROPERTY(Replicated) bool bCastShadows = true;
	/** Lamps fed by the tower generator go dark when it is off. */
	UPROPERTY(Replicated) TObjectPtr<ASUGenerator> PowerSource;
	/** Lamp switches itself on at dusk and off at dawn (street / yard lights). */
	UPROPERTY(Replicated) bool bAutoNight = false;

protected:
	virtual void BuildGeometry() override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_On)
	bool bOn = true;

	UFUNCTION()
	void OnRep_On();

	UPROPERTY(Transient)
	TObjectPtr<ULocalLightComponent> Light;

	float FlickerTime = 0.f;
	float NextAutoCheck = 0.f;
	void ApplyLight(float Factor);
};

UCLASS()
class SIGNALUNKNOWN_API ASULightSwitch : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASULightSwitch();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;
	virtual float GetInteractionRange() const override { return 200.f; }

	/** Server-side list of controlled lamps. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASULamp>> Lamps;

	UPROPERTY(Replicated) bool bOn = true;

protected:
	virtual void BuildGeometry() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Lever;
};

UCLASS()
class SIGNALUNKNOWN_API ASUGenerator : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUGenerator();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;

	bool IsRunning() const { return bRunning; }

protected:
	virtual void BuildGeometry() override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Running)
	bool bRunning = true;

	UFUNCTION()
	void OnRep_Running();
};
