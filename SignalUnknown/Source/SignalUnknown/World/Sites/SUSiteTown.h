// SIGNAL UNKNOWN – the larger town in the eastern valley. Visible from the region, reachable
// later (the road is closed at the edge of the playable area for now).
#pragma once

#include "CoreMinimal.h"
#include "World/Sites/SUSiteBase.h"
#include "SUSiteTown.generated.h"

UCLASS()
class SIGNALUNKNOWN_API ASUSiteTown : public ASUSiteBase
{
	GENERATED_BODY()

public:
	virtual void BuildSite(ASUWorldBuilder* InBuilder, bool bInServer) override;
};
