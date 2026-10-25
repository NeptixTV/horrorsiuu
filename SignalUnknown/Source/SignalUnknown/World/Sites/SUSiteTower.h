// SIGNAL UNKNOWN – Lookout tower "Posten 07": the players' base.
#pragma once

#include "CoreMinimal.h"
#include "World/Sites/SUSiteBase.h"
#include "SUSiteTower.generated.h"

UCLASS()
class SIGNALUNKNOWN_API ASUSiteTower : public ASUSiteBase
{
	GENERATED_BODY()

public:
	virtual void BuildSite(ASUWorldBuilder* InBuilder, bool bInServer) override;

private:
	void BuildStructure();
	void BuildStairs();
	void BuildCabin();
	void BuildInterior();
	void BuildGrounds();
	void SpawnInteractables();
};
