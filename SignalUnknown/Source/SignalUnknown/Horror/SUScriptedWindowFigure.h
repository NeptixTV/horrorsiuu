// SIGNAL UNKNOWN – the scripted window sequence of mission 1 ("Sieh dich im Haus weiter um …").
//
// Runs on the server with real triggers and the real creature AI:
//   1. objective active + a player has been inside the house for a moment
//   2. three knocks somewhere in the house, the figure (Observer) is placed outside the window
//   3. as soon as a player inside actually LOOKS through the window (view cone + line of sight)
//      the figure is revealed (stinger), stands still for a moment, then runs into the forest
//   4. the bedroom door slams, tracks appear under the window (rare evidence), the objective
//      completes via the HorrorScripted event
// Fallbacks make sure the mission can always be finished (players never look, players leave).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SUScriptedWindowFigure.generated.h"

class ASUDoor;
class ASUAreaTrigger;
class ASUCreature;
class ASUCharacter;

UENUM()
enum class ESUWindowSequenceStage : uint8
{
	Waiting,
	Knocking,
	WaitForLook,
	Revealed,
	Retreating,
	Done
};

UCLASS()
class SIGNALUNKNOWN_API ASUScriptedWindowFigure : public AActor
{
	GENERATED_BODY()

public:
	ASUScriptedWindowFigure();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	FName SequenceId;
	FName MissionId;
	FName ObjectiveId;
	FVector NoiseLocation = FVector::ZeroVector;
	FVector WindowLocation = FVector::ZeroVector;
	FVector FigureLocation = FVector::ZeroVector;
	FVector RetreatLocation = FVector::ZeroVector;
	FVector TracksLocation = FVector::ZeroVector;

	UPROPERTY() TObjectPtr<ASUDoor> SlamDoor;
	UPROPERTY() TObjectPtr<ASUAreaTrigger> InteriorArea;

	bool IsRunning() const { return Stage != ESUWindowSequenceStage::Waiting && Stage != ESUWindowSequenceStage::Done; }
	/** Horror director: don't interfere while any scripted sequence runs. */
	static bool IsAnySequenceRunning(const UWorld* World);

private:
	ESUWindowSequenceStage Stage = ESUWindowSequenceStage::Waiting;
	float StageTime = 0.f;
	float InsideTime = 0.f;
	bool bHintPlayed = false;

	UPROPERTY() TObjectPtr<ASUCreature> Figure;

	void SetStage(ESUWindowSequenceStage NewStage);
	bool IsObjectiveActive() const;
	ASUCharacter* FindPlayerLookingThroughWindow() const;
	bool AnyPlayerInside() const;
	void SpawnTracks();
	void Complete();
	FName DoneFlag() const;
};
