#include "Vehicles/HHRoadNode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AHHRoadNode::AHHRoadNode()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetHidden(true);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

namespace HHRoad
{
	static UWorld* WorldOf(const UObject* WorldContextObject)
	{
		return GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	}

	static TArray<AHHRoadNode*> AllNodes(const UObject* WorldContextObject)
	{
		TArray<AHHRoadNode*> Nodes;
		if (UWorld* World = WorldOf(WorldContextObject))
		{
			for (TActorIterator<AHHRoadNode> It(World); It; ++It)
			{
				if (!It->GetNodeId().IsNone())
				{
					Nodes.Add(*It);
				}
			}
		}
		return Nodes;
	}
}

AHHRoadNode* AHHRoadNode::FindNearest(const UObject* WorldContextObject, const FVector& Location)
{
	AHHRoadNode* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (AHHRoadNode* Node : HHRoad::AllNodes(WorldContextObject))
	{
		const float Distance = FVector::DistSquared(Node->GetActorLocation(), Location);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Node;
		}
	}
	return Best;
}

AHHRoadNode* AHHRoadNode::FindById(const UObject* WorldContextObject, FName Id)
{
	for (AHHRoadNode* Node : HHRoad::AllNodes(WorldContextObject))
	{
		if (Node->NodeId == Id)
		{
			return Node;
		}
	}
	return nullptr;
}

void AHHRoadNode::PointsForRoute(const UObject* WorldContextObject, const TArray<FName>& Route, TArray<FVector>& OutPoints)
{
	OutPoints.Reset();
	for (const FName Id : Route)
	{
		if (const AHHRoadNode* Node = FindById(WorldContextObject, Id))
		{
			OutPoints.Add(Node->GetActorLocation());
		}
	}
}

bool AHHRoadNode::FindPath(const UObject* WorldContextObject, const FVector& Start, const FVector& Goal, TArray<FVector>& OutPoints)
{
	OutPoints.Reset();
	const TArray<AHHRoadNode*> Nodes = HHRoad::AllNodes(WorldContextObject);
	if (Nodes.Num() == 0)
	{
		return false;
	}
	TMap<FName, int32> IndexOf;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		IndexOf.Add(Nodes[Index]->NodeId, Index);
	}
	// Links are listed on either end; make them symmetric.
	TArray<TArray<int32>> Adjacent;
	Adjacent.SetNum(Nodes.Num());
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		for (const FName Link : Nodes[Index]->Links)
		{
			if (const int32* Other = IndexOf.Find(Link))
			{
				Adjacent[Index].AddUnique(*Other);
				Adjacent[*Other].AddUnique(Index);
			}
		}
	}

	const AHHRoadNode* From = FindNearest(WorldContextObject, Start);
	const AHHRoadNode* To = FindNearest(WorldContextObject, Goal);
	const int32 FromIndex = From ? IndexOf.FindRef(From->NodeId) : INDEX_NONE;
	const int32 ToIndex = To ? IndexOf.FindRef(To->NodeId) : INDEX_NONE;
	if (FromIndex == INDEX_NONE || ToIndex == INDEX_NONE)
	{
		return false;
	}

	// Dijkstra: a town has a few dozen nodes.
	TArray<float> Distance;
	TArray<int32> Previous;
	TArray<bool> Done;
	Distance.Init(TNumericLimits<float>::Max(), Nodes.Num());
	Previous.Init(INDEX_NONE, Nodes.Num());
	Done.Init(false, Nodes.Num());
	Distance[FromIndex] = 0.f;
	for (int32 Step = 0; Step < Nodes.Num(); ++Step)
	{
		int32 Current = INDEX_NONE;
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (!Done[Index] && Distance[Index] < TNumericLimits<float>::Max() && (Current == INDEX_NONE || Distance[Index] < Distance[Current]))
			{
				Current = Index;
			}
		}
		if (Current == INDEX_NONE || Current == ToIndex)
		{
			break;
		}
		Done[Current] = true;
		for (const int32 Next : Adjacent[Current])
		{
			const float Candidate = Distance[Current] + FVector::Dist(Nodes[Current]->GetActorLocation(), Nodes[Next]->GetActorLocation());
			if (Candidate < Distance[Next])
			{
				Distance[Next] = Candidate;
				Previous[Next] = Current;
			}
		}
	}
	if (FromIndex != ToIndex && Previous[ToIndex] == INDEX_NONE)
	{
		return false;
	}
	for (int32 Index = ToIndex; Index != INDEX_NONE; Index = Previous[Index])
	{
		OutPoints.Insert(Nodes[Index]->GetActorLocation(), 0);
	}
	return OutPoints.Num() > 0;
}
