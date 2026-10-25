// SIGNAL UNKNOWN – one house of the small settlement Pine Ridge (visible, not enterable yet).
#pragma once

#include "CoreMinimal.h"
#include "World/Sites/SUSiteBase.h"
#include "SUSiteSettlement.generated.h"

UCLASS()
class SIGNALUNKNOWN_API ASUSiteSettlement : public ASUSiteBase
{
	GENERATED_BODY()

public:
	virtual void BuildSite(ASUWorldBuilder* InBuilder, bool bInServer) override;
};
