// SIGNAL UNKNOWN – "Pine Ridge Gas & Mart": pumps, shop, clerk.
#pragma once

#include "CoreMinimal.h"
#include "World/Sites/SUSiteBase.h"
#include "SUSiteGasStation.generated.h"

UCLASS()
class SIGNALUNKNOWN_API ASUSiteGasStation : public ASUSiteBase
{
	GENERATED_BODY()

public:
	virtual void BuildSite(ASUWorldBuilder* InBuilder, bool bInServer) override;

private:
	void BuildShop();
	void BuildForecourt();
	void SpawnInteractables();
};

/** Concrete barriers where a road leaves the playable area (future expansion). */
UCLASS()
class SIGNALUNKNOWN_API ASUSiteRoadBlock : public ASUSiteBase
{
	GENERATED_BODY()

public:
	virtual void BuildSite(ASUWorldBuilder* InBuilder, bool bInServer) override;

	float RoadWidth = 700.f;
};
