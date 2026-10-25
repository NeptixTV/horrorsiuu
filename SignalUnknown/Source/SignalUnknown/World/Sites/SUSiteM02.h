// SIGNAL UNKNOWN – mission 2: abandoned car on the north road.
#pragma once

#include "CoreMinimal.h"
#include "World/Sites/SUSiteBase.h"
#include "SUSiteM02.generated.h"

UCLASS()
class SIGNALUNKNOWN_API ASUSiteM02 : public ASUSiteBase
{
	GENERATED_BODY()

public:
	virtual void BuildSite(ASUWorldBuilder* InBuilder, bool bInServer) override;
};
