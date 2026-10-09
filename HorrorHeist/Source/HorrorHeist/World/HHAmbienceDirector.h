#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHAmbienceDirector.generated.h"

class USoundBase;

UENUM()
enum class EHHAmbientEvent : uint8
{
	FootstepsOverhead,
	LightsStutter,
	RadioInterference,
	DistantDoor,
	Knocking,
	DoorCreak,
	Whisper,
	MAX UMETA(Hidden)
};

/**
 * The hideout is not quite safe either. Rarely, and never twice the same way in a row, the
 * director stages something small and unexplained: steps crossing the closed laundromat
 * above, a stutter in the lights, a voice inside the radio static, three knocks at the stair
 * door, a locker drifting open while nobody is looking. Server decides, everyone experiences
 * the same moment. All events are subtle on purpose: foreshadowing, not jump scares.
 */
UCLASS()
class HORRORHEIST_API AHHAmbienceDirector : public AActor
{
	GENERATED_BODY()

public:
	AHHAmbienceDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Server. Index into EHHAmbientEvent, or -1 for a weighted random pick. */
	void TriggerEvent(int32 EventIndex);

protected:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastEvent(EHHAmbientEvent Event, int32 Seed);

	void ScheduleNext(float MinDelay, float MaxDelay);
	void RunScheduledEvent();
	EHHAmbientEvent PickEvent(FRandomStream& Stream) const;
	bool TryCreepDoor(FRandomStream& Stream);
	bool IsObservedByAnyone(const FVector& Location, float ConeDegrees) const;

	// Local presentation
	void PlayFootstepsOverhead(int32 Seed);
	void PlayOverheadStep();
	void PlayLightsStutter(int32 Seed);
	void PlayRadioInterference(int32 Seed);
	void PlayAtTagged(FName Tag, USoundBase* Sound, const FText& Caption, int32 Seed, bool bNearestToViewer);
	void Caption(const FText& Text, float Duration = 3.f) const;

	UPROPERTY(EditAnywhere, Category = "Timing")
	FVector2D FirstEventDelay = FVector2D(150.f, 260.f);

	UPROPERTY(EditAnywhere, Category = "Timing")
	FVector2D EventInterval = FVector2D(110.f, 280.f);

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> OverheadSteps;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TObjectPtr<USoundBase> DistantDoorSound;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TObjectPtr<USoundBase> KnockSound;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> WhisperSounds;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TObjectPtr<USoundBase> ElectricalStutterSound;

	/** Voice heard inside the radio static during interference. */
	UPROPERTY(EditAnywhere, Category = "Sounds")
	TObjectPtr<USoundBase> RadioBleedSound;

	UPROPERTY(EditAnywhere, Category = "Tags")
	FName OverheadStartTag = TEXT("HH_OverheadStart");

	UPROPERTY(EditAnywhere, Category = "Tags")
	FName OverheadEndTag = TEXT("HH_OverheadEnd");

	UPROPERTY(EditAnywhere, Category = "Tags")
	FName StairDoorTag = TEXT("HH_StairDoor");

	UPROPERTY(EditAnywhere, Category = "Tags")
	FName WhisperTag = TEXT("HH_Whisper");

	UPROPERTY(EditAnywhere, Category = "Tags")
	FName FlickerTag = TEXT("HH_Flickerable");

	UPROPERTY(EditAnywhere, Category = "Tags")
	FName CreepyDoorTag = TEXT("HH_CreepyDoor");

private:
	FTimerHandle EventTimer;
	FTimerHandle StepTimer;
	EHHAmbientEvent LastEvent = EHHAmbientEvent::MAX;

	// Overhead walk state (local)
	FVector StepStart = FVector::ZeroVector;
	FVector StepEnd = FVector::ZeroVector;
	int32 StepCount = 0;
	int32 StepIndex = 0;
	FRandomStream StepStream;
};
