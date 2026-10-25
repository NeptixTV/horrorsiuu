// SIGNAL UNKNOWN – the Forsthaus of mission 1: damaged rooms, traces, the ranger's notebook
// and the scripted window figure.
#pragma once

#include "CoreMinimal.h"
#include "World/Sites/SUSiteBase.h"
#include "SUSiteForsthaus.generated.h"

class ASUDoor;
class ASUAreaTrigger;

UCLASS()
class SIGNALUNKNOWN_API ASUSiteForsthaus : public ASUSiteBase
{
	GENERATED_BODY()

public:
	virtual void BuildSite(ASUWorldBuilder* InBuilder, bool bInServer) override;

private:
	void BuildHouse();
	void BuildLivingRoom();
	void BuildHallKitchen();
	void BuildOfficeBedroom();
	void BuildOutside();
	void SpawnInteractables(ASUAreaTrigger* Interior);

	/** Blood drops / smear: flat dark patches along a line. */
	void AddBloodTrail(const FVector& From, const FVector& To, int32 Count, int32 Seed);
	/** Claw-like footprint (three long toes and a heel) on the ground. */
	void AddClawPrint(const FVector& At, float Yaw);
};
