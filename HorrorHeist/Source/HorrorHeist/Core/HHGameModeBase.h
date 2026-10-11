#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HHGameModeBase.generated.h"

class APlayerStart;

/**
 * Crew rules shared by every map (hideout + town hub, heists): crew slots, the crew leader,
 * player starts that never leave a player without a body, and crew travel. Map specific
 * rules (ready checks, objectives) live in the subclasses.
 */
UCLASS(Abstract)
class HORRORHEIST_API AHHGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHHGameModeBase();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	/**
	 * Server: take the whole crew to another map (non-seamless travel). A listen server keeps
	 * listening so the crew follows; a solo game stays offline.
	 */
	bool TravelCrewTo(const FString& MapPackage, const FString& ExtraOptions = FString());

protected:
	int32 FindFreeCrewSlot() const;
	bool HasCrewLeader() const;

	/** True when the default pawn fits at the start and nobody is standing on it. */
	bool IsStartUsable(const AActor* Start, const AController* Player);

	/** Last resort when a map has no usable PlayerStart: a point on the ground near the origin. */
	AActor* GetFallbackStart();

	UPROPERTY(Transient)
	TObjectPtr<AActor> FallbackStart;
};
