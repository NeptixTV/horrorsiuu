// SIGNAL UNKNOWN – mission 3: the abandoned manor "Gutshaus Brandt".
//
// Above ground there is only an empty house. Below it: cellar, an old mine drift, natural caves,
// a great cavern where the dweller lives and - if the optional tomb model is installed - a burial
// chamber at the very bottom. All geometry comes from the model files (Manor.sumodel, Tomb.sumodel);
// the layout data is generated into Logic/SUManorData.h.
#pragma once

#include "CoreMinimal.h"
#include "World/Sites/SUSiteBase.h"
#include "Logic/SUManorData.h"
#include "SUSiteManor.generated.h"

UCLASS()
class SIGNALUNKNOWN_API ASUSiteManor : public ASUSiteBase
{
	GENERATED_BODY()

public:
	virtual void BuildSite(ASUWorldBuilder* InBuilder, bool bInServer) override;

private:
	/** Layout data is relative to the front door; the actor sits at the centre of the plateau. */
	static FVector L(const SUManor::Vec& P) { return FVector(P.X - SUManor::PadCentre.X, P.Y - SUManor::PadCentre.Y, P.Z); }
	static FVector Anchor(const char* Name);
	FBox ZoneBox(const char* Name, float Grow = 0.f) const;
	void BuildFallback();
	void BuildGarden();
};
