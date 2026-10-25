// SIGNAL UNKNOWN – marks something in the world as photographable evidence.
//
// The component defines an oriented box (its location/rotation and BoxExtent). When a photo is
// taken, USUEvidenceSubsystem projects this box into the frame and checks visibility, size,
// distance, light and camera shake.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SUEvidenceComponent.generated.h"

UCLASS(ClassGroup = (SignalUnknown), meta = (BlueprintSpawnableComponent))
class SIGNALUNKNOWN_API USUEvidenceComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USUEvidenceComponent();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FName EvidenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FText DisplayName;

	/** Furniture, Traces, Clue, Creature, Vehicle ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FName Category;

	/** Half size of the evidence box (cm, component space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	FVector BoxExtent = FVector(50.f, 50.f, 50.f);

	/** Beyond this distance (cm) the object can't be recognised. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	float MaxDistance = 1500.f;

	/** Share of the frame below which the object is too small. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	float MinCoverage = 0.004f;

	/** Share of the frame at which the size counts fully. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	float IdealCoverage = 0.06f;

	/** Indoors / in the dark: needs flash, flashlight or a lamp to be usable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	bool bRequiresLight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evidence")
	bool bEnabled = true;

	/** World space corners of the evidence box. */
	void GetCorners(FVector OutCorners[8]) const;
	FVector GetCenter() const { return GetComponentLocation(); }
};
