#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHEscapeZone.generated.h"

class UBoxComponent;

/**
 * A place the van can drive to at the end of a job (the hideout garage ramp, the quarry road out
 * of town, the municipal car park). The crew picks one on the job board before leaving; on the job
 * the van has to reach that zone with the crew inside to end the job. Placed in L_Town, so the hub
 * (job board) and every job map know the same list.
 */
UCLASS()
class HORRORHEIST_API AHHEscapeZone : public AActor
{
	GENERATED_BODY()

public:
	AHHEscapeZone();

	/** All zones in the world, by SortOrder. */
	static TArray<AHHEscapeZone*> GetAll(const UObject* WorldContextObject);
	static AHHEscapeZone* Find(const UObject* WorldContextObject, FName ZoneId);

	bool ContainsPoint(const FVector& Point) const;

	FName GetZoneId() const { return ZoneId; }
	const FText& GetDisplayName() const { return DisplayName; }
	const FText& GetDescription() const { return Description; }
	bool AcceptsWhileChased() const { return bAcceptWhileChased; }
	float GetFenceCut() const { return FenceCut; }
	float GetStopSeconds() const { return StopSeconds; }
	int32 GetSortOrder() const { return SortOrder; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Escape")
	TObjectPtr<UBoxComponent> Zone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape")
	FName ZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape")
	FText DisplayName;

	/** One or two lines for the job board (rules of this place). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape", meta = (MultiLine = true))
	FText Description;

	/** False: the van can't hide here while a police car is on its tail (don't lead them home). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape")
	bool bAcceptWhileChased = false;

	/** Extra share the fence keeps when the job ends here (0.1 = 10 %). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape", meta = (ClampMin = 0.0, ClampMax = 0.9))
	float FenceCut = 0.f;

	/** The van must stand still inside the zone this long (park and kill the engine). 0 = drive in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape", meta = (ClampMin = 0.0))
	float StopSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape")
	int32 SortOrder = 0;
};
