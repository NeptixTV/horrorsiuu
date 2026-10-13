#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHRoadNode.generated.h"

/**
 * A point on the town's road network (centre of a street, a junction). AI cars (the police)
 * drive from node to node; Links name the nodes this one connects to (both ways). Placed in
 * L_Town by the world builder.
 */
UCLASS()
class HORRORHEIST_API AHHRoadNode : public AActor
{
	GENERATED_BODY()

public:
	AHHRoadNode();

	FName GetNodeId() const { return NodeId; }

	/** Nearest node to Location (null when the town has no road nodes). */
	static AHHRoadNode* FindNearest(const UObject* WorldContextObject, const FVector& Location);
	static AHHRoadNode* FindById(const UObject* WorldContextObject, FName Id);

	/**
	 * Road points from the node nearest Start to the node nearest Goal (shortest by distance).
	 * False when either end has no node or the graph does not connect them.
	 */
	static bool FindPath(const UObject* WorldContextObject, const FVector& Start, const FVector& Goal, TArray<FVector>& OutPoints);
	/** Road points along the named nodes, in order (unknown ids are skipped). */
	static void PointsForRoute(const UObject* WorldContextObject, const TArray<FName>& Route, TArray<FVector>& OutPoints);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	TArray<FName> Links;
};
