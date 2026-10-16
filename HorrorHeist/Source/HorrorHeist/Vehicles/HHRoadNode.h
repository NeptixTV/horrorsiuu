#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHRoadNode.generated.h"

/** What traffic must do before driving through a junction node. */
UENUM(BlueprintType)
enum class EHHTrafficControl : uint8
{
	None,
	/** Stop at the line, wait a moment, go. */
	StopSign,
	/** Wait for green (AHHTrafficLight phases, shared by every light of the junction). */
	Lights
};

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
	EHHTrafficControl GetControl() const { return Control; }
	/** Which signal group of the junction this node belongs to (all lights of a junction share it). */
	FName GetJunctionId() const { return JunctionId.IsNone() ? NodeId : JunctionId; }
	/** Stop sign: must a car coming from node FromId stop here? (side street yes, main road no) */
	bool MustStopFrom(FName FromId) const { return Control == EHHTrafficControl::StopSign && (StopApproaches.Num() == 0 || StopApproaches.Contains(FromId)); }
	/** Distance from the node to the painted stop line (cm). */
	float GetStopLine() const { return StopLine; }

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

	/** Junction rule for cars driving through this node (stop sign, traffic lights). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	EHHTrafficControl Control = EHHTrafficControl::None;

	/** Lights: the junction id the signals use (defaults to this node's id). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	FName JunctionId;

	/** Stop sign: the linked nodes whose traffic has the sign (empty = an all-way stop). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	TArray<FName> StopApproaches;

	/** Where cars wait: the stop line this far before the node's centre (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	float StopLine = 620.f;
};
