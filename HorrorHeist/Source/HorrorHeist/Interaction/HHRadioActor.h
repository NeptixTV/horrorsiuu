#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "HHRadioActor.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Old table radio: cycles off -> station 1 -> station 2 -> ... -> off for everyone.
 * Something occasionally bleeds through the static (driven by the ambience director).
 */
UCLASS()
class HORRORHEIST_API AHHRadioActor : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHRadioActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FText GetInteractionPrompt(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

	/** Local: drown the station in static for a moment, optionally with a voice in it. */
	void Interfere(float Duration, USoundBase* BleedThrough);

	bool IsPlaying() const { return Station >= 0; }

protected:
	UFUNCTION()
	void OnRep_Station();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Radio")
	TObjectPtr<UAudioComponent> Speaker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Radio")
	TObjectPtr<UAudioComponent> Static;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	TArray<TObjectPtr<USoundBase>> Stations;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	TObjectPtr<USoundBase> StaticLoop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	TObjectPtr<USoundBase> TuneSound;

	/** Station index the radio starts on (-1 = off). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	int32 StartStation = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	float StaticBedVolume = 0.08f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Station)
	int32 Station = -1;

	float InterferenceEnd = 0.f;
	float InterferenceDuration = 0.f;
};
