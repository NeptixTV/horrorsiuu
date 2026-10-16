#include "Vehicles/HHTrafficCar.h"
#include "Vehicles/HHRoadNode.h"
#include "Heist/HHHeistGameState.h"
#include "NPC/HHTownsperson.h"
#include "World/HHAftermathScene.h"
#include "World/HHCrosswalk.h"
#include "World/HHTimeOfDay.h"
#include "World/HHTrafficLight.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

AHHTrafficCar::AHHTrafficCar()
{
	// The town's cars share the police sedan's proportions (all generated cars are sedan-sized).
	BoxExtent = FVector(228.f, 88.f, 50.f);
	CenterHeight = 85.f;
	Wheelbase = 275.f;
	TrackWidth = 150.f;
	AxleOffset = 0.f;
	WheelRadius = 33.f;
	MeshYaw = -90.f;
	MaxForwardSpeed = 1300.f;
	Acceleration = 380.f;
}

void AHHTrafficCar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHTrafficCar, bActive);
}

void AHHTrafficCar::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		// Starts parked out of sight; the first AI tick decides whether it drives (day) or not.
		bActive = false;
	}
	OnRep_Active();
}

float AHHTrafficCar::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : GetWorld()->GetTimeSeconds();
}

bool AHHTrafficCar::WantsToDrive() const
{
	if (AHHHeistGameState::IsJobWorld(this))
	{
		return false;
	}
	// A crime scene has the street closed: this car's round is off for the day.
	for (const FName& Id : Route)
	{
		if (AHHAftermathScene::IsNodeClosed(this, Id))
		{
			return false;
		}
	}
	if (bDayOnly)
	{
		if (const AHHTimeOfDay* Clock = AHHTimeOfDay::Get(this))
		{
			return !Clock->IsNight();
		}
	}
	return true;
}

bool AHHTrafficCar::BuildLane()
{
	Lane.Reset();
	RouteToLane.Reset();
	TArray<AHHRoadNode*> Nodes;
	for (const FName& Id : Route)
	{
		if (AHHRoadNode* Node = AHHRoadNode::FindById(this, Id))
		{
			Nodes.Add(Node);
		}
	}
	const int32 Count = Nodes.Num();
	if (Count < 3)
	{
		return false;
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const AHHRoadNode* Previous = Nodes[(Index + Count - 1) % Count];
		const AHHRoadNode* Next = Nodes[(Index + 1) % Count];
		const FVector Here = Nodes[Index]->GetActorLocation();
		const FVector FromPrevious = Here - Previous->GetActorLocation();
		const FVector ToNext = Next->GetActorLocation() - Here;
		const FVector In = FromPrevious.GetSafeNormal2D();
		const FVector Out = ToNext.GetSafeNormal2D();
		// Right-hand traffic: the lane runs LaneOffset to the right of the node line; at a bend the
		// two offset lines meet on the bisector.
		const FVector RightIn = FVector::CrossProduct(FVector::UpVector, In);
		const FVector RightOut = FVector::CrossProduct(FVector::UpVector, Out);
		FVector Bisector = RightIn + RightOut;
		float Scale = 1.f;
		if (Bisector.SizeSquared() < 0.01f)
		{
			Bisector = RightIn;
		}
		else
		{
			Bisector.Normalize();
			Scale = 1.f / FMath::Max(0.35f, FVector::DotProduct(Bisector, RightIn));
		}
		const FVector Corner = Here + Bisector * LaneOffset * Scale;

		FLanePoint First;
		First.Node = Nodes[Index];
		First.FromId = Previous->GetNodeId();
		First.Axis = FMath::Abs(In.X) >= FMath::Abs(In.Y) ? 0 : 1;
		RouteToLane.Add(Lane.Num());

		const float Turn = FMath::Acos(FMath::Clamp(FVector::DotProduct(In, Out), -1.f, 1.f));
		if (FMath::RadiansToDegrees(Turn) <= 30.f)
		{
			First.Location = Corner;
			First.NodeAhead = FVector::DotProduct((Here - Corner) * FVector(1.f, 1.f, 0.f), In);
			Lane.Add(First);
			continue;
		}
		// A bend: replace the corner by an arc tangent to both lanes (a car cannot turn on the spot).
		const bool bRightTurn = In.X * Out.Y - In.Y * Out.X > 0.f;
		const float HalfTan = FMath::Tan(Turn * 0.5f);
		float Tangent = (bRightTurn ? RightTurnRadius : LeftTurnRadius) * HalfTan;
		Tangent = FMath::Min3(Tangent, 0.45f * FromPrevious.Size2D(), 0.45f * ToNext.Size2D());
		const float Radius = Tangent / FMath::Max(HalfTan, 0.01f);
		const FVector Start = Corner - In * Tangent;
		const FVector Side = bRightTurn ? RightIn : -RightIn;
		const FVector Centre = Start + Side * Radius;
		const float StartAngle = FMath::Atan2(Start.Y - Centre.Y, Start.X - Centre.X);
		const float Sign = bRightTurn ? 1.f : -1.f;
		constexpr int32 Steps = 4;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const float Angle = StartAngle + Sign * Turn * Step / Steps;
			FLanePoint Point = Step == 0 ? First : FLanePoint();
			Point.Location = FVector(Centre.X + Radius * FMath::Cos(Angle), Centre.Y + Radius * FMath::Sin(Angle), Corner.Z);
			Point.Radius = Radius;
			if (Step == 0)
			{
				Point.NodeAhead = FVector::DotProduct((Here - Point.Location) * FVector(1.f, 1.f, 0.f), In);
			}
			Lane.Add(Point);
		}
	}
	return true;
}

bool AHHTrafficCar::IsNearAnyPlayer(const FVector& Location, float Radius) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		FVector Spot = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		if (!Pawn && PC && PC->PlayerCameraManager)
		{
			Spot = PC->PlayerCameraManager->GetCameraLocation();
		}
		else if (!Pawn)
		{
			continue;
		}
		if (FVector::DistSquared2D(Spot, Location) < FMath::Square(Radius))
		{
			return true;
		}
	}
	return false;
}

bool AHHTrafficCar::FindSpawnSpot(int32& OutIndex) const
{
	// Spawn spots are the route's nodes (the first lane point of each), from StartIndex on.
	const int32 Count = RouteToLane.Num();
	for (int32 Step = 0; Step < Count; ++Step)
	{
		const int32 Index = RouteToLane[(FMath::Max(0, StartIndex) + Step) % Count];
		const FVector Spot = Lane[Index].Location;
		if (IsNearAnyPlayer(Spot, 3500.f))
		{
			continue;
		}
		// Not on top of another car.
		bool bFree = true;
		for (TActorIterator<AHHCarBase> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->IsHidden() && FVector::DistSquared2D(It->GetActorLocation(), Spot) < FMath::Square(900.f))
			{
				bFree = false;
				break;
			}
		}
		if (bFree)
		{
			OutIndex = Index;
			return true;
		}
	}
	return false;
}

void AHHTrafficCar::SetActive(bool bNewActive)
{
	if (bActive == bNewActive)
	{
		return;
	}
	bActive = bNewActive;
	Speed = 0.f;
	ClearedStop = INDEX_NONE;
	StopTimer = 0.f;
	BlockedByPlayerTime = 0.f;
	PublishState();
	OnRep_Active();
	ForceNetUpdate();
}

void AHHTrafficCar::OnRep_Active()
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
}

void AHHTrafficCar::MulticastHonk_Implementation()
{
	if (HornSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HornSound, GetActorLocation());
	}
}

void AHHTrafficCar::Tick(float DeltaSeconds)
{
	if (HasAuthority())
	{
		TickAI(FMath::Min(DeltaSeconds, 0.05f));
	}
	if (!bActive)
	{
		// Parked: no simulation, no ground traces, no engine.
		AActor::Tick(DeltaSeconds);
		return;
	}
	Super::Tick(DeltaSeconds);
}

void AHHTrafficCar::GetDriveInput(float& OutThrottle, float& OutSteer, bool& bOutHandbrake) const
{
	OutThrottle = AIThrottle;
	OutSteer = AISteer;
	bOutHandbrake = bAIHandbrake;
}

float AHHTrafficCar::GetFreeDistance(float LookAhead, bool& bOutBlockedByPlayer)
{
	bOutBlockedByPlayer = false;
	const int32 Count = Lane.Num();
	const FVector Location = GetActorLocation();
	const FVector Forward = GetFlatForward();
	const float Front = BoxExtent.X;
	float Free = TNumericLimits<float>::Max();

	// Along-the-lane distance from the car to the next few lane points.
	const FVector SegmentStart = Lane[(Target + Count - 1) % Count].Location;
	const FVector SegmentEnd = Lane[Target].Location;
	float Along = FVector::Dist2D(Location, SegmentEnd);
	const float Now = GetServerTime();
	for (int32 Step = 0; Step < 12 && Along - Front < LookAhead + StopLineDistance + 1000.f; ++Step)
	{
		const int32 Index = (Target + Step) % Count;
		const FLanePoint& Point = Lane[Index];
		const AHHRoadNode* Node = Point.Node.Get();
		const float ToLine = Along + Point.NodeAhead - Front - (Node ? Node->GetStopLine() : StopLineDistance);
		if (Node)
		{
			if (Node->GetControl() == EHHTrafficControl::Lights && ToLine > -60.f)
			{
				const EHHSignal Signal = AHHTrafficLight::GetSignal(Point.Axis, Now);
				const float BrakingNeeded = FMath::Square(Speed) / (2.f * ComfortBrake * 1.6f);
				// Amber: stop if that is still comfortable, otherwise drive on through.
				if (Signal == EHHSignal::Red || (Signal == EHHSignal::Amber && ToLine > BrakingNeeded))
				{
					Free = FMath::Min(Free, FMath::Max(0.f, ToLine));
				}
			}
			else if (Node->MustStopFrom(Point.FromId) && ClearedStop != Index && ToLine > -150.f)
			{
				Free = FMath::Min(Free, FMath::Max(0.f, ToLine));
				if (ToLine < 90.f && FMath::Abs(Speed) < 40.f)
				{
					StopTimer += GetWorld()->GetDeltaSeconds();
					if (StopTimer >= StopSignSeconds)
					{
						// Looked both ways: go (cars and people in the junction still stop us below).
						ClearedStop = Index;
						StopTimer = 0.f;
					}
				}
			}
		}
		Along += FVector::Dist2D(Point.Location, Lane[(Index + 1) % Count].Location);
	}

	// A zebra crossing ahead with someone on it.
	const FVector Bumper = Location + Forward * Front;
	const FVector PathDirection = (SegmentEnd - SegmentStart).GetSafeNormal2D();
	float CrossingAlong = 0.f;
	if (AHHCrosswalk* Crossing = AHHCrosswalk::FindAhead(this, Bumper, PathDirection.IsNearlyZero() ? Forward : PathDirection, LookAhead, &CrossingAlong))
	{
		if (CrossingAlong > 0.f && Crossing->IsBusy())
		{
			Free = FMath::Min(Free, FMath::Max(0.f, CrossingAlong - 150.f));
		}
	}

	// People and cars in the lane ahead (a corridor a bit wider than the car).
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	auto Consider = [&](const AActor* Other, float Radius, bool bPlayer)
	{
		const FVector Offset = Other->GetActorLocation() - Location;
		const float Ahead = FVector::DotProduct(Offset, Forward) - Front - Radius;
		const float Side = FMath::Abs(FVector::DotProduct(Offset, Right));
		if (Ahead > -Front && Ahead < LookAhead && Side < BoxExtent.Y + Radius + 50.f && FMath::Abs(Offset.Z) < 400.f)
		{
			const float Gap = FMath::Max(0.f, Ahead - 250.f);
			if (Gap < Free)
			{
				Free = Gap;
				bOutBlockedByPlayer = bPlayer;
			}
		}
	};
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden() && It->GetActorEnableCollision())
		{
			Consider(*It, 45.f, It->IsPlayerControlled());
		}
	}
	for (TActorIterator<AHHTownsperson> It(GetWorld()); It; ++It)
	{
		if (It->IsPresent() && It->IsWalking())
		{
			Consider(*It, 40.f, false);
		}
	}
	for (TActorIterator<AHHCarBase> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsHidden())
		{
			// Only cars going our way (or standing) hold us up; oncoming traffic is in the other lane.
			if (FVector::DotProduct(It->GetFlatForward(), Forward) > -0.3f || FMath::Abs(It->GetSpeed()) < 50.f)
			{
				Consider(*It, 200.f, false);
			}
		}
	}
	return Free;
}

void AHHTrafficCar::TickAI(float DeltaSeconds)
{
	AIThrottle = 0.f;
	AISteer = 0.f;
	bAIHandbrake = true;
	HonkCooldown = FMath::Max(0.f, HonkCooldown - DeltaSeconds);

	StateTimer -= DeltaSeconds;
	if (StateTimer <= 0.f)
	{
		StateTimer = 1.f;
		if (Lane.Num() == 0 && !BuildLane())
		{
			return;
		}
		const bool bWant = WantsToDrive();
		if (bWant && !bActive)
		{
			int32 Spot = 0;
			if (FindSpawnSpot(Spot))
			{
				const int32 Count = Lane.Num();
				const FVector From = Lane[(Spot + Count - 1) % Count].Location;
				const FVector To = Lane[Spot].Location;
				const FVector Direction = (To - From).GetSafeNormal2D();
				// Halfway along the segment into the spot, facing down the lane.
				SetActorLocationAndRotation((From + To) * 0.5f + FVector(0.f, 0.f, CenterHeight), Direction.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
				Target = Spot;
				SetActive(true);
				FollowGround(0.f, false);
				PublishState();
			}
		}
		else if (!bWant && bActive)
		{
			// Gone home: on a job map at once, in the hub once nobody sees it go.
			if (AHHHeistGameState::IsJobWorld(this) || !IsNearAnyPlayer(GetActorLocation(), 3500.f))
			{
				SetActive(false);
			}
		}
	}
	if (!bActive || Lane.Num() < 3)
	{
		return;
	}

	// Follow the lane: advance past points the car has reached, then aim at a point ahead (pure pursuit).
	const int32 Count = Lane.Num();
	const FVector Location = GetActorLocation();
	for (int32 Guard = 0; Guard < Count; ++Guard)
	{
		const FVector A = Lane[(Target + Count - 1) % Count].Location;
		const FVector B = Lane[Target].Location;
		const FVector Segment = (B - A) * FVector(1.f, 1.f, 0.f);
		const float Length = Segment.Size();
		const float T = Length > 1.f ? FVector::DotProduct((Location - A) * FVector(1.f, 1.f, 0.f), Segment) / (Length * Length) : 1.f;
		if (T < 1.f && FVector::Dist2D(Location, B) > 150.f)
		{
			break;
		}
		if (ClearedStop == Target)
		{
			ClearedStop = INDEX_NONE;
		}
		Target = (Target + 1) % Count;
	}
	const FVector A = Lane[(Target + Count - 1) % Count].Location;
	const FVector B = Lane[Target].Location;
	const FVector SegmentDirection = (B - A).GetSafeNormal2D();
	const float LookAt = FMath::Clamp(FMath::Abs(Speed) * 0.55f, 380.f, 900.f);
	FVector Aim;
	{
		// Project onto the segment, walk LookAt along the lane.
		const float Projected = FMath::Clamp(FVector::DotProduct((Location - A) * FVector(1.f, 1.f, 0.f), SegmentDirection), 0.f, FVector::Dist2D(A, B));
		float Remaining = LookAt;
		FVector From = A + SegmentDirection * Projected;
		int32 Index = Target;
		Aim = B;
		for (int32 Step = 0; Step < Count; ++Step)
		{
			const FVector To = Lane[Index].Location;
			const float Piece = FVector::Dist2D(From, To);
			if (Piece >= Remaining)
			{
				Aim = From + (To - From).GetSafeNormal2D() * Remaining;
				break;
			}
			Remaining -= Piece;
			From = To;
			Index = (Index + 1) % Count;
			Aim = To;
		}
	}
	const float DesiredYaw = (Aim - Location).GetSafeNormal2D().Rotation().Yaw;
	const float Delta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, DesiredYaw);
	AISteer = FMath::Clamp(Delta / 25.f, -1.f, 1.f);

	// How fast: cruise, slow enough for the next arc, and able to stop where the way ahead ends.
	float Wanted = CruiseSpeed;
	{
		float ToBend = FVector::Dist2D(Location, B);
		for (int32 Step = 0; Step < 6; ++Step)
		{
			const FLanePoint& Point = Lane[(Target + Step) % Count];
			if (Point.Radius > 0.f)
			{
				const float BendSpeed = FMath::Sqrt(BendAcceleration * Point.Radius);
				Wanted = FMath::Min(Wanted, FMath::Sqrt(FMath::Square(BendSpeed) + 2.f * ComfortBrake * FMath::Max(0.f, ToBend - 100.f)));
				break;
			}
			ToBend += FVector::Dist2D(Point.Location, Lane[(Target + Step + 1) % Count].Location);
		}
	}
	if (FMath::Abs(Delta) > 35.f)
	{
		Wanted = FMath::Min(Wanted, 380.f);
	}
	bool bBlockedByPlayer = false;
	const float Free = GetFreeDistance(FMath::Max(900.f, FMath::Square(Speed) / (2.f * ComfortBrake) + 600.f), bBlockedByPlayer);
	if (Free < TNumericLimits<float>::Max())
	{
		Wanted = FMath::Min(Wanted, FMath::Sqrt(2.f * ComfortBrake * FMath::Max(0.f, Free - 30.f)));
	}

	if (Wanted < 15.f && Speed < 60.f)
	{
		// Standing (red light, stop sign, someone in the way).
		AIThrottle = 0.f;
		bAIHandbrake = true;
	}
	else
	{
		bAIHandbrake = false;
		if (Speed > Wanted + 40.f)
		{
			AIThrottle = -1.f;
		}
		else if (Speed < Wanted - 20.f)
		{
			AIThrottle = FMath::Clamp((Wanted - Speed) / 250.f, 0.2f, 1.f);
		}
	}

	// Someone from the crew standing in the road: a honk after a few seconds, then every few more.
	if (bBlockedByPlayer && FMath::Abs(Speed) < 60.f)
	{
		BlockedByPlayerTime += DeltaSeconds;
		if (BlockedByPlayerTime > 3.f && HonkCooldown <= 0.f)
		{
			HonkCooldown = 4.5f;
			MulticastHonk();
		}
	}
	else
	{
		BlockedByPlayerTime = 0.f;
	}
}
