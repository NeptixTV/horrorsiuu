// SIGNAL UNKNOWN – the Horror Director (server only).
//
// Every 18–38 s (DeveloperSettings) the director looks at one player (isolated players are picked
// more often), builds a SULogic::DirectorContext from the real world state and lets
// SULogic::DecideHorrorEvent decide whether anything happens at all – most of the time it decides
// "nothing". Cooldowns, tension, story stage, darkness and safety rules (base, scripted sequences,
// refuelling) are part of that decision. The chosen event is then executed with real actors:
// sounds in the world, flickering lamps, radio intrusions, creatures with AI, a blocked engine.
//
// Console (server / listen host):
//   su.HorrorDebug 1            log every evaluation
//   su.HorrorEvent <Name>       force an event (DistantSound, LightFlicker, RadioInterference,
//                               TreeWatcher, RoadsideFigure, StrangeRadioMessage, CarWontStart,
//                               BuildingSounds, HunterChase)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Logic/SUDirectorLogic.h"
#include "SUHorrorDirector.generated.h"

class ASUCharacter;
class ASUAreaTrigger;

UCLASS()
class SIGNALUNKNOWN_API ASUHorrorDirector : public AInfo
{
	GENERATED_BODY()

public:
	ASUHorrorDirector();

	virtual void Tick(float DeltaSeconds) override;

	static ASUHorrorDirector* Get(const UObject* WorldContext);

	void SetActive(bool bInActive);
	bool IsActive() const { return bActive; }

	/** Debug / tests: run an event now for the given (or a random) player. Returns false if it could not be placed. */
	bool ForceEvent(SULogic::HorrorEvent Event, ASUCharacter* Target = nullptr);

	const SULogic::DirectorState& GetDirectorState() const { return DirectorState; }

private:
	bool bActive = false;
	float NextEvaluation = 60.f;
	double Clock = 0.0;
	SULogic::DirectorState DirectorState;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASUAreaTrigger>> Areas;

	void Evaluate();
	ASUCharacter* PickTarget() const;
	SULogic::DirectorContext BuildContext(const ASUCharacter* Target) const;
	bool ExecuteEvent(SULogic::HorrorEvent Event, ASUCharacter* Target);

	bool DoDistantSound(ASUCharacter* Target);
	bool DoLightFlicker(ASUCharacter* Target);
	bool DoRadioInterference(ASUCharacter* Target);
	bool DoTreeWatcher(ASUCharacter* Target);
	bool DoRoadsideFigure(ASUCharacter* Target);
	bool DoStrangeRadioMessage(ASUCharacter* Target);
	bool DoCarWontStart(ASUCharacter* Target);
	bool DoBuildingSounds(ASUCharacter* Target);
	bool DoHunterChase(ASUCharacter* Target);

	void CacheAreas();
	bool IsInArea(const AActor* Actor, std::initializer_list<FName> Ids) const;
	ASUAreaTrigger* FindNearbyBuildingArea(const FVector& Location, float MaxDistance) const;
	/** No player within MinDistance and the point is not in any player's view cone. */
	bool IsHiddenSpawnPoint(const FVector& Location, float MinDistance, float MaxViewDot) const;
	bool FindGroundPoint(const FVector& Around, FVector& OutPoint) const;
};
