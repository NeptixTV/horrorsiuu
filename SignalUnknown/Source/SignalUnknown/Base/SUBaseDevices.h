// SIGNAL UNKNOWN – devices of the lookout tower: dispatch radio, mission computer, wall map,
// weather radio. All are real objects in the world; screens open only when used.
#pragma once

#include "CoreMinimal.h"
#include "Interaction/SUInteractableActor.h"
#include "SUBaseDevices.generated.h"

class UTextRenderComponent;
class UPointLightComponent;

/** The dispatch radio ("Funkgerät") – receives missions from the Zentrale. */
UCLASS()
class SIGNALUNKNOWN_API ASURadioStation : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASURadioStation();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;

	/** All machines: short burst of interference (horror director). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastInterference(float Duration);

	/** Pitch multiplier for the procedural voice of a speaker. */
	static float SpeakerPitch(const FText& Speaker);

protected:
	virtual void BuildGeometry() override;

private:
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> StaticSound;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> VoiceSound;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> ClickSound;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> ClockText;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Led;

	float InterferenceTime = 0.f;
	float BeepTimer = 0.f;
	uint8 LastSerial = 255;
	float ClockTimer = 0.f;
};

/** The mission computer: mission overview, photo review, reports, account. */
UCLASS()
class SIGNALUNKNOWN_API ASUComputerTerminal : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUComputerTerminal();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;

protected:
	virtual void BuildGeometry() override;

private:
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> ScreenTitle;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> ScreenStatus;
	float RefreshTimer = 0.f;
};

/** Large map board. Roads, the lake and mission pins are built from the real world layout. */
UCLASS()
class SIGNALUNKNOWN_API ASUWallMap : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUWallMap();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;

	static constexpr float BoardWidth = 150.f;
	static constexpr float BoardHeight = 110.f;

protected:
	virtual void BuildGeometry() override;

private:
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> TargetPin;
	FVector BoardPoint(const FVector2D& UV, float Out) const;
	void TryBuildLayout();
	bool bLayoutBuilt = false;
};

/** Weather radio: reads the current conditions and the forecast. */
UCLASS()
class SIGNALUNKNOWN_API ASUWeatherRadio : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUWeatherRadio();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;

protected:
	virtual void BuildGeometry() override;

private:
	UPROPERTY(Replicated) double ReportStartTime = -100.0;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> Voice;
};
