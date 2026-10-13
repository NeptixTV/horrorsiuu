#include "Vehicles/HHPoliceCar.h"
#include "Vehicles/HHRoadNode.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

AHHPoliceCar::AHHPoliceCar()
{
	// A sedan: lower, shorter, quicker than the van. Wheels are part of the body mesh.
	BoxExtent = FVector(228.f, 88.f, 50.f);
	CenterHeight = 85.f;
	Wheelbase = 275.f;
	TrackWidth = 150.f;
	AxleOffset = 0.f;
	WheelRadius = 33.f;
	MeshYaw = -90.f;
	MaxForwardSpeed = 1700.f;
	Acceleration = 520.f;

	auto MakeBeacon = [this](const TCHAR* Name, const FLinearColor& Color, float Side) -> UPointLightComponent*
	{
		UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name);
		Light->SetupAttachment(Collision);
		Light->SetRelativeLocation(FVector(-10.f, Side, 70.f));
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(450.f);
		Light->SetAttenuationRadius(1800.f);
		Light->SetLightColor(Color);
		Light->SetCastShadows(false);
		Light->SetVisibility(false);
		return Light;
	};
	LightRed = MakeBeacon(TEXT("LightRed"), FLinearColor(1.f, 0.05f, 0.03f), -35.f);
	LightBlue = MakeBeacon(TEXT("LightBlue"), FLinearColor(0.05f, 0.2f, 1.f), 35.f);

	SirenAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SirenAudio"));
	SirenAudio->SetupAttachment(Collision);
	SirenAudio->bAutoActivate = false;
}

void AHHPoliceCar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHPoliceCar, bFlashing);
	DOREPLIFETIME(AHHPoliceCar, bSiren);
	DOREPLIFETIME(AHHPoliceCar, Mode);
}

AHHPoliceCar* AHHPoliceCar::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AHHPoliceCar> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AHHPoliceCar::BeginPlay()
{
	Super::BeginPlay();
	HomeTransform = GetActorTransform();
	if (SirenLoop)
	{
		SirenAudio->SetSound(SirenLoop);
	}
	if (HasAuthority())
	{
		Park();
	}
	OnRep_Signals();
}

// ---------------------------------------------------------------------------------------
// Orders

void AHHPoliceCar::SetMode(EHHPoliceCarMode NewMode)
{
	if (Mode == NewMode)
	{
		return;
	}
	const bool bWasParked = Mode == EHHPoliceCarMode::Parked;
	Mode = NewMode;
	if (bWasParked && Mode != EHHPoliceCarMode::Parked)
	{
		// Out of the garage at the edge of town.
		SetActorTransform(HomeTransform, false, nullptr, ETeleportType::TeleportPhysics);
		Speed = 0.f;
		SetHeadlights(true);
		FollowGround(0.f, false);
		PublishState();
	}
	OnRep_Signals();
	ForceNetUpdate();
}

void AHHPoliceCar::Park()
{
	Mode = EHHPoliceCarMode::Parked;
	bFlashing = false;
	bSiren = false;
	Path.Reset();
	ChaseTarget.Reset();
	Speed = 0.f;
	SetActorTransform(HomeTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SetHeadlights(false);
	PublishState();
	OnRep_Signals();
	ForceNetUpdate();
}

void AHHPoliceCar::StartPatrol()
{
	if (!HasAuthority() || Mode != EHHPoliceCarMode::Parked)
	{
		return;
	}
	SetMode(EHHPoliceCarMode::Patrol);
	TArray<FVector> Points;
	AHHRoadNode::PointsForRoute(this, PatrolRoute, Points);
	SetPath(Points);
}

void AHHPoliceCar::Respond(const FVector& InScene, float DeadlineSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	Scene = InScene;
	Deadline = GetWorld()->GetTimeSeconds() + DeadlineSeconds;
	SetMode(EHHPoliceCarMode::Respond);
	bFlashing = true;
	bSiren = true;
	OnRep_Signals();
	PathTo(Scene);
}

void AHHPoliceCar::Chase(AActor* Target)
{
	if (!HasAuthority() || !Target)
	{
		return;
	}
	ChaseTarget = Target;
	LastKnown = Target->GetActorLocation();
	TimeWithoutSight = 0.f;
	RepathTimer = 0.f;
	SetMode(EHHPoliceCarMode::Chase);
	bFlashing = true;
	bSiren = true;
	OnRep_Signals();
}

void AHHPoliceCar::HoldPosition()
{
	if (HasAuthority() && Mode != EHHPoliceCarMode::Parked)
	{
		Path.Reset();
		SetMode(EHHPoliceCarMode::AtScene);
		bFlashing = true;
		OnRep_Signals();
	}
}

void AHHPoliceCar::Leave()
{
	if (!HasAuthority() || Mode == EHHPoliceCarMode::Parked)
	{
		return;
	}
	SetMode(EHHPoliceCarMode::Leave);
	bSiren = false;
	OnRep_Signals();
	PathTo(HomeTransform.GetLocation());
}

void AHHPoliceCar::SetPath(const TArray<FVector>& Points)
{
	Path = Points;
	PathIndex = 0;
}

void AHHPoliceCar::PathTo(const FVector& Goal)
{
	TArray<FVector> Points;
	if (!AHHRoadNode::FindPath(this, GetActorLocation(), Goal, Points))
	{
		Points.Reset();
	}
	Points.Add(Goal);
	SetPath(Points);
}

bool AHHPoliceCar::CanSee(const AActor* Target, float MaxDistance, float MaxAngleDegrees) const
{
	if (!Target || Mode == EHHPoliceCarMode::Parked)
	{
		return false;
	}
	const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector Point = Target->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const FVector ToTarget = Point - Eye;
	if (ToTarget.SizeSquared() > FMath::Square(MaxDistance))
	{
		return false;
	}
	if (MaxAngleDegrees < 180.f)
	{
		const float Cos = FVector::DotProduct(GetFlatForward(), ToTarget.GetSafeNormal2D());
		if (Cos < FMath::Cos(FMath::DegreesToRadians(MaxAngleDegrees)))
		{
			return false;
		}
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHPoliceSight), false, this);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params))
	{
		return true;
	}
	const AActor* HitActor = Hit.GetActor();
	return HitActor == Target || (HitActor && HitActor->GetAttachParentActor() == Target);
}

// ---------------------------------------------------------------------------------------
// Driving

void AHHPoliceCar::Tick(float DeltaSeconds)
{
	if (HasAuthority())
	{
		TickAI(FMath::Min(DeltaSeconds, 0.05f));
	}
	Super::Tick(DeltaSeconds);

	// Light bar (local).
	if (bFlashing)
	{
		FlashTimer += DeltaSeconds;
		if (FlashTimer > 0.22f)
		{
			FlashTimer = 0.f;
			bFlashPhase = !bFlashPhase;
			LightRed->SetVisibility(bFlashPhase);
			LightBlue->SetVisibility(!bFlashPhase);
		}
	}
}

void AHHPoliceCar::GetDriveInput(float& OutThrottle, float& OutSteer, bool& bOutHandbrake) const
{
	OutThrottle = AIThrottle;
	OutSteer = AISteer;
	bOutHandbrake = bAIHandbrake;
}

void AHHPoliceCar::SteerTowards(const FVector& Point, float DesiredSpeed)
{
	const FVector ToPoint = (Point - GetActorLocation()).GetSafeNormal2D();
	const float DesiredYaw = ToPoint.Rotation().Yaw;
	const float Delta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, DesiredYaw);
	const float Turn = FMath::Abs(Delta);
	const float Wanted = Turn > 60.f ? FMath::Min(DesiredSpeed, 450.f) : (Turn > 30.f ? FMath::Min(DesiredSpeed, 800.f) : DesiredSpeed);

	bAIHandbrake = false;
	if (ReverseTime > 0.f)
	{
		// Backing out of a corner it drove into.
		AIThrottle = -1.f;
		AISteer = -FMath::Clamp(Delta / 30.f, -1.f, 1.f);
		return;
	}
	AISteer = FMath::Clamp(Delta / 30.f, -1.f, 1.f);
	AIThrottle = FMath::Clamp((Wanted - Speed) / 300.f, -1.f, 1.f);
}

void AHHPoliceCar::TickAI(float DeltaSeconds)
{
	ReverseTime = FMath::Max(0.f, ReverseTime - DeltaSeconds);
	AIThrottle = 0.f;
	AISteer = 0.f;
	bAIHandbrake = true;

	if (Mode == EHHPoliceCarMode::Parked || Mode == EHHPoliceCarMode::AtScene)
	{
		return;
	}

	bool bSteered = false;
	if (Mode == EHHPoliceCarMode::Chase)
	{
		AActor* Target = ChaseTarget.Get();
		if (!Target)
		{
			SetMode(EHHPoliceCarMode::Search);
			PathTo(LastKnown);
			return;
		}
		if (CanSee(Target, 6000.f, 180.f))
		{
			LastKnown = Target->GetActorLocation();
			TimeWithoutSight = 0.f;
		}
		else
		{
			TimeWithoutSight += DeltaSeconds;
		}
		const float Distance = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
		if (TimeWithoutSight <= 0.f && Distance < 3500.f)
		{
			// Close and in sight: straight at it (aim a little ahead of a moving van).
			FVector Lead = Target->GetActorLocation();
			if (const AHHCarBase* Car = Cast<AHHCarBase>(Target))
			{
				Lead += Car->GetFlatForward() * Car->GetSpeed() * 0.4f;
			}
			SteerTowards(Lead, Distance < 900.f ? FMath::Min(ChaseSpeed, FMath::Abs(Speed) + 200.f) : ChaseSpeed);
			bSteered = true;
		}
		else
		{
			RepathTimer -= DeltaSeconds;
			if (RepathTimer <= 0.f)
			{
				RepathTimer = 1.5f;
				PathTo(LastKnown);
			}
		}
	}

	if (!bSteered && !FollowPath())
	{
		return;
	}

	// Stuck against something with the pedal down: back off for a moment.
	if (AIThrottle > 0.3f && FMath::Abs(Speed) < 50.f)
	{
		StuckTime += DeltaSeconds;
		if (StuckTime > 1.6f)
		{
			StuckTime = 0.f;
			ReverseTime = 1.2f;
		}
	}
	else
	{
		StuckTime = 0.f;
	}
}

bool AHHPoliceCar::FollowPath()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Mode == EHHPoliceCarMode::Respond && Now > Deadline && FVector::Dist2D(GetActorLocation(), Scene) > 3000.f)
	{
		// Stuck somewhere on the way: the "next car" arrives from the nearest corner instead.
		const FVector Back = (HomeTransform.GetLocation() - Scene).GetSafeNormal2D();
		SetActorLocationAndRotation(Scene + Back * 1600.f + FVector(0.f, 0.f, CenterHeight + 50.f), (-Back).Rotation());
		FollowGround(0.f, false);
		PathTo(Scene);
		Deadline = Now + 30.f;
	}
	if (!Path.IsValidIndex(PathIndex))
	{
		// End of the road.
		if (Mode == EHHPoliceCarMode::Respond)
		{
			SetMode(EHHPoliceCarMode::AtScene);
			bSiren = false;
			OnRep_Signals();
		}
		else if (Mode == EHHPoliceCarMode::Patrol || Mode == EHHPoliceCarMode::Leave)
		{
			Park();
		}
		return false;
	}
	const FVector Target = Path[PathIndex];
	const bool bLast = PathIndex == Path.Num() - 1;
	const float Distance = FVector::Dist2D(GetActorLocation(), Target);
	if (Distance < (bLast ? 600.f : 450.f))
	{
		++PathIndex;
		return false;
	}
	const float Cruise = Mode == EHHPoliceCarMode::Patrol || Mode == EHHPoliceCarMode::Leave ? PatrolSpeed
		: (Mode == EHHPoliceCarMode::Respond ? ResponseSpeed : ChaseSpeed);
	SteerTowards(Target, bLast && Distance < 1500.f ? FMath::Min(Cruise, 500.f) : Cruise);
	return true;
}

void AHHPoliceCar::OnRep_Signals()
{
	const bool bParked = Mode == EHHPoliceCarMode::Parked;
	SetActorHiddenInGame(bParked);
	SetActorEnableCollision(!bParked);
	if (!bFlashing)
	{
		LightRed->SetVisibility(false);
		LightBlue->SetVisibility(false);
	}
	if (SirenAudio->GetSound())
	{
		if (bSiren && !SirenAudio->IsPlaying())
		{
			SirenAudio->Play();
		}
		else if (!bSiren && SirenAudio->IsPlaying())
		{
			SirenAudio->FadeOut(1.f, 0.f);
		}
	}
}
