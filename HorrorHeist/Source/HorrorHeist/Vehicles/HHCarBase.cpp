#include "Vehicles/HHCarBase.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

AHHCarBase::AHHCarBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetBoxExtent(BoxExtent);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(ECC_Vehicle);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	// Cameras pass through (the chase camera sits behind it); interaction traces hit the body mesh.
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetCanEverAffectNavigation(false);
	SetRootComponent(Collision);

	auto MakeMesh = [this](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(Collision);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		return Component;
	};
	Body = MakeMesh(TEXT("Body"));
	// Only the body answers interaction traces ("Get in"), nothing else collides with it.
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	WheelFL = MakeMesh(TEXT("WheelFL"));
	WheelFR = MakeMesh(TEXT("WheelFR"));
	WheelRL = MakeMesh(TEXT("WheelRL"));
	WheelRR = MakeMesh(TEXT("WheelRR"));

	auto MakeHeadlight = [this](const TCHAR* Name) -> USpotLightComponent*
	{
		USpotLightComponent* Light = CreateDefaultSubobject<USpotLightComponent>(Name);
		Light->SetupAttachment(Collision);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(1800.f);
		Light->SetAttenuationRadius(3200.f);
		Light->SetInnerConeAngle(14.f);
		Light->SetOuterConeAngle(36.f);
		Light->SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
		Light->SetCastShadows(false);
		Light->SetVisibility(false);
		return Light;
	};
	HeadlightL = MakeHeadlight(TEXT("HeadlightL"));
	HeadlightR = MakeHeadlight(TEXT("HeadlightR"));

	EngineAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineAudio"));
	EngineAudio->SetupAttachment(Collision);
	EngineAudio->bAutoActivate = false;
}

void AHHCarBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshes();
}

void AHHCarBase::ApplyMeshes()
{
	Collision->SetBoxExtent(BoxExtent, false);
	const FVector Down(0.f, 0.f, -CenterHeight);
	Body->SetStaticMesh(BodyMesh);
	Body->SetRelativeLocationAndRotation(Down, FRotator(0.f, MeshYaw, 0.f));

	const float Front = AxleOffset + Wheelbase * 0.5f;
	const float Rear = AxleOffset - Wheelbase * 0.5f;
	const float Side = TrackWidth * 0.5f;
	const float Z = WheelRadius - CenterHeight;
	struct FWheelSpot { UStaticMeshComponent* Component; float X; float Y; };
	const FWheelSpot Spots[] = { { WheelFL, Front, -Side }, { WheelFR, Front, Side }, { WheelRL, Rear, -Side }, { WheelRR, Rear, Side } };
	for (const FWheelSpot& Spot : Spots)
	{
		Spot.Component->SetStaticMesh(WheelMesh);
		Spot.Component->SetVisibility(WheelMesh != nullptr);
		Spot.Component->SetRelativeLocation(FVector(Spot.X, Spot.Y, Z));
	}

	// Headlights at the front bumper, looking slightly down the road.
	const FVector LampSpot(BoxExtent.X + 10.f, TrackWidth * 0.42f, 86.f - CenterHeight);
	HeadlightL->SetRelativeLocationAndRotation(FVector(LampSpot.X, -LampSpot.Y, LampSpot.Z), FRotator(-6.f, 0.f, 0.f));
	HeadlightR->SetRelativeLocationAndRotation(LampSpot, FRotator(-6.f, 0.f, 0.f));
}

void AHHCarBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyMeshes();
	if (EngineLoop)
	{
		EngineAudio->SetSound(EngineLoop);
	}
	if (HasAuthority())
	{
		// Settle onto the road where the level placed us and tell everyone.
		FollowGround(0.f, false);
		PublishState();
	}
	OnRep_Headlights();
}

void AHHCarBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// The driver's machine moves the van itself and must not be pulled back by its own echo.
	DOREPLIFETIME_CONDITION(AHHCarBase, NetState, COND_SkipOwner);
	DOREPLIFETIME(AHHCarBase, bHeadlights);
}

FVector AHHCarBase::GetFlatForward() const
{
	const FRotator Yaw(0.f, GetActorRotation().Yaw, 0.f);
	return Yaw.Vector();
}

FVector AHHCarBase::GetGroundLocation() const
{
	return GetActorLocation() - GetActorUpVector() * CenterHeight;
}

void AHHCarBase::SetHeadlights(bool bOn)
{
	if (HasAuthority() && bHeadlights != bOn)
	{
		bHeadlights = bOn;
		OnRep_Headlights();
	}
}

void AHHCarBase::OnRep_Headlights()
{
	HeadlightL->SetVisibility(bHeadlights);
	HeadlightR->SetVisibility(bHeadlights);
}

void AHHCarBase::GetDriveInput(float& OutThrottle, float& OutSteer, bool& bOutHandbrake) const
{
	OutThrottle = 0.f;
	OutSteer = 0.f;
	bOutHandbrake = true;
}

// ---------------------------------------------------------------------------------------
// Simulation (one machine)

void AHHCarBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);

	if (IsLocallySimulated())
	{
		float Throttle = 0.f;
		float SteerInput = 0.f;
		bool bHandbrake = false;
		GetDriveInput(Throttle, SteerInput, bHandbrake);
		Simulate(Dt, Throttle, SteerInput, bHandbrake);
		PostSimulate(Dt);
		if (HasAuthority())
		{
			PublishTimer -= Dt;
			if (PublishTimer <= 0.f)
			{
				PublishTimer = 1.f / 30.f;
				PublishState();
			}
		}
	}
	else
	{
		SmoothToNetState(Dt);
	}
	UpdateVisuals(Dt);
}

void AHHCarBase::Simulate(float DeltaSeconds, float Throttle, float SteerInput, bool bHandbrake)
{
	Steer = FMath::FInterpTo(Steer, FMath::Clamp(SteerInput, -1.f, 1.f), DeltaSeconds, SteerResponse);

	if (bHandbrake)
	{
		Speed = FMath::FInterpConstantTo(Speed, 0.f, DeltaSeconds, BrakeDeceleration * 1.2f);
	}
	else if (FMath::Abs(Throttle) > 0.05f && Throttle * Speed < -1.f)
	{
		// Pressing against the direction of travel brakes first.
		Speed = FMath::FInterpConstantTo(Speed, 0.f, DeltaSeconds, BrakeDeceleration);
	}
	else if (FMath::Abs(Throttle) > 0.05f)
	{
		Speed = FMath::Clamp(Speed + Throttle * Acceleration * DeltaSeconds, -MaxReverseSpeed, MaxForwardSpeed);
	}
	else
	{
		Speed = FMath::FInterpConstantTo(Speed, 0.f, DeltaSeconds, CoastDeceleration);
	}
	if (!bAirborne)
	{
		// Hills: a ramp slows the van going up and pulls it back when nobody presses the pedal.
		const float SlopeSin = FMath::Sin(FMath::DegreesToRadians(GetActorRotation().Pitch));
		Speed -= 980.f * 0.55f * SlopeSin * DeltaSeconds;
	}

	// Bicycle model around the rear axle.
	const float SteerRadians = FMath::DegreesToRadians(Steer * MaxSteerAngle);
	const float YawRate = FMath::RadiansToDegrees(Speed / FMath::Max(Wheelbase, 1.f) * FMath::Tan(SteerRadians));
	FRotator Rotation = GetActorRotation();
	if (!bAirborne)
	{
		Rotation.Yaw = FRotator::NormalizeAxis(Rotation.Yaw + YawRate * DeltaSeconds);
		SetActorRotation(Rotation);
	}

	const FVector Delta = FRotator(0.f, Rotation.Yaw, 0.f).Vector() * Speed * DeltaSeconds;
	if (!Delta.IsNearlyZero(0.01f))
	{
		MoveSwept(Delta);
	}
	FollowGround(DeltaSeconds, true);
	WheelSpin = FMath::Fmod(WheelSpin + FMath::RadiansToDegrees(Speed * DeltaSeconds / FMath::Max(WheelRadius, 1.f)), 360.f);
}

void AHHCarBase::MoveSwept(const FVector& Delta)
{
	FHitResult Hit;
	SetActorLocation(GetActorLocation() + Delta, true, &Hit);
	if (!Hit.bBlockingHit)
	{
		return;
	}
	if (Hit.bStartPenetrating)
	{
		// Turning in place can push a corner into a wall: step out of it, then try again once.
		AddActorWorldOffset(Hit.Normal * (Hit.PenetrationDepth + 0.5f));
		SetActorLocation(GetActorLocation() + Delta, true, &Hit);
		if (!Hit.bBlockingHit)
		{
			return;
		}
	}

	const FVector Forward = GetFlatForward();
	const float ImpactSpeed = FMath::Abs(Speed) * FMath::Abs(FVector::DotProduct(Forward * FMath::Sign(Speed), -Hit.ImpactNormal));
	OnBumped(Hit, ImpactSpeed);

	// Slide along what we hit with what is left of the move.
	const FVector Remaining = Delta * (1.f - Hit.Time);
	const FVector Slide = FVector::VectorPlaneProject(Remaining, Hit.ImpactNormal.GetSafeNormal2D());
	Speed *= ImpactSpeed > 300.f ? 0.1f : 0.6f;
	if (!Slide.IsNearlyZero(0.1f))
	{
		FHitResult SlideHit;
		SetActorLocation(GetActorLocation() + FVector(Slide.X, Slide.Y, 0.f), true, &SlideHit);
	}
}

void AHHCarBase::OnBumped(const FHitResult& Hit, float ImpactSpeed)
{
	if (BumpSound && ImpactSpeed > 250.f)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BumpSound, Hit.ImpactPoint, FMath::Clamp(ImpactSpeed / 1000.f, 0.3f, 1.f));
	}
}

bool AHHCarBase::TraceGround(const FVector& Point, float TopZ, float& OutZ) const
{
	const UWorld* World = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHCarGround), false, this);
	TArray<AActor*> Attached;
	GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);

	const FVector Start(Point.X, Point.Y, TopZ);
	const FVector End(Point.X, Point.Y, TopZ - 650.f);
	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			return false;
		}
		// People and other cars are not road (the van would climb onto them).
		AActor* HitActor = Hit.GetActor();
		if (HitActor && (HitActor->IsA<APawn>() || HitActor->IsA<AHHCarBase>()))
		{
			Params.AddIgnoredActor(HitActor);
			continue;
		}
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

void AHHCarBase::FollowGround(float DeltaSeconds, bool bAllowFall)
{
	const FVector Location = GetActorLocation();
	const float Yaw = GetActorRotation().Yaw;
	const FRotator YawOnly(0.f, Yaw, 0.f);
	const FVector Forward = YawOnly.Vector();
	const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
	const float CurrentGround = Location.Z - CenterHeight;
	const float TopZ = CurrentGround + 180.f;

	const float Front = AxleOffset + Wheelbase * 0.5f;
	const float Rear = AxleOffset - Wheelbase * 0.5f;
	const float Side = TrackWidth * 0.5f;
	const FVector Points[4] = {
		Location + Forward * Front - Right * Side, Location + Forward * Front + Right * Side,
		Location + Forward * Rear - Right * Side, Location + Forward * Rear + Right * Side };
	float Heights[4];
	int32 Hits = 0;
	float Sum = 0.f;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (TraceGround(Points[Index], TopZ, Heights[Index]))
		{
			++Hits;
			Sum += Heights[Index];
		}
		else
		{
			Heights[Index] = TNumericLimits<float>::Lowest();
		}
	}

	if (Hits == 0)
	{
		if (!bAllowFall)
		{
			return;
		}
		// Off the edge of the world (should not happen inside the town): fall.
		bAirborne = true;
		VerticalSpeed -= 980.f * DeltaSeconds;
		SetActorLocation(Location + FVector(0.f, 0.f, VerticalSpeed * DeltaSeconds));
		return;
	}
	// A wheel over a drop uses the average of the others.
	const float Average = Sum / Hits;
	for (float& Height : Heights)
	{
		if (Height == TNumericLimits<float>::Lowest())
		{
			Height = Average;
		}
	}

	const float FrontZ = (Heights[0] + Heights[1]) * 0.5f;
	const float RearZ = (Heights[2] + Heights[3]) * 0.5f;
	const float LeftZ = (Heights[0] + Heights[2]) * 0.5f;
	const float RightZ = (Heights[1] + Heights[3]) * 0.5f;
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(FrontZ - RearZ, FMath::Max(Wheelbase, 1.f)));
	const float Roll = FMath::RadiansToDegrees(FMath::Atan2(RightZ - LeftZ, FMath::Max(TrackWidth, 1.f)));
	const FRotator TargetRotation(Pitch, Yaw, Roll);
	const float GroundZ = (FrontZ + RearZ) * 0.5f;
	const FVector Up = FRotationMatrix(TargetRotation).GetUnitAxis(EAxis::Z);
	const float TargetZ = GroundZ + Up.Z * CenterHeight;

	float NewZ = TargetZ;
	if (bAllowFall && Location.Z > TargetZ + 40.f)
	{
		// Driving off a curb or the top of the ramp: drop instead of snapping down.
		bAirborne = true;
		VerticalSpeed -= 980.f * DeltaSeconds;
		NewZ = FMath::Max(TargetZ, Location.Z + VerticalSpeed * DeltaSeconds);
	}
	else
	{
		bAirborne = false;
		VerticalSpeed = 0.f;
		if (DeltaSeconds > 0.f && NewZ > Location.Z)
		{
			NewZ = FMath::FInterpTo(Location.Z, TargetZ, DeltaSeconds, 25.f);
		}
	}
	if (NewZ <= TargetZ + 1.f)
	{
		bAirborne = false;
		VerticalSpeed = 0.f;
	}

	const FRotator Current = GetActorRotation();
	const FRotator Smoothed = DeltaSeconds > 0.f
		? FRotator(FMath::FInterpTo(Current.Pitch, Pitch, DeltaSeconds, 10.f), Yaw, FMath::FInterpTo(Current.Roll, Roll, DeltaSeconds, 10.f))
		: TargetRotation;
	SetActorLocationAndRotation(FVector(Location.X, Location.Y, NewZ), Smoothed);
}

// ---------------------------------------------------------------------------------------
// Replication

void AHHCarBase::PublishState()
{
	if (!HasAuthority())
	{
		return;
	}
	FHHCarNetState NewState;
	NewState.Location = GetActorLocation();
	NewState.Yaw = GetActorRotation().Yaw;
	NewState.Speed = Speed;
	NewState.Steer = Steer;
	NetState = NewState;
}

void AHHCarBase::OnRep_NetState()
{
	LastNetStateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!bHasNetState)
	{
		// First state: be there, no interpolation from the level's spot.
		bHasNetState = true;
		SetActorLocationAndRotation(NetState.Location, FRotator(0.f, NetState.Yaw, 0.f));
	}
}

void AHHCarBase::SmoothToNetState(float DeltaSeconds)
{
	if (!bHasNetState || HasAuthority())
	{
		return;
	}
	Speed = NetState.Speed;
	Steer = NetState.Steer;

	// Extrapolate a little from the last update, then ease towards it.
	const float Age = FMath::Min(static_cast<float>(GetWorld()->GetTimeSeconds() - LastNetStateTime), 0.25f);
	const FRotator TargetYaw(0.f, NetState.Yaw, 0.f);
	const FVector Target = FVector(NetState.Location) + TargetYaw.Vector() * NetState.Speed * Age;
	const FVector Location = GetActorLocation();
	FVector NewLocation;
	if (FVector::DistSquared2D(Location, Target) > FMath::Square(600.f))
	{
		NewLocation = Target;
	}
	else
	{
		NewLocation = FMath::VInterpTo(Location, Target, DeltaSeconds, 10.f);
	}
	const float NewYaw = FMath::RInterpTo(FRotator(0.f, GetActorRotation().Yaw, 0.f), TargetYaw, DeltaSeconds, 10.f).Yaw;
	SetActorLocationAndRotation(FVector(NewLocation.X, NewLocation.Y, Location.Z + (Target.Z - Location.Z) * FMath::Min(1.f, DeltaSeconds * 4.f)),
		FRotator(GetActorRotation().Pitch, NewYaw, GetActorRotation().Roll));
	FollowGround(DeltaSeconds, false);
	WheelSpin = FMath::Fmod(WheelSpin + FMath::RadiansToDegrees(Speed * DeltaSeconds / FMath::Max(WheelRadius, 1.f)), 360.f);
}

// ---------------------------------------------------------------------------------------
// Looks and sound (every machine)

void AHHCarBase::UpdateVisuals(float DeltaSeconds)
{
	if (WheelMesh)
	{
		const float SteerYaw = Steer * MaxSteerAngle;
		// The wheel mesh's axle is its X: yaw puts the axle across the car, roll spins it.
		WheelFL->SetRelativeRotation(FRotator(0.f, MeshYaw + 180.f + SteerYaw, WheelSpin));
		WheelFR->SetRelativeRotation(FRotator(0.f, MeshYaw + SteerYaw, -WheelSpin));
		WheelRL->SetRelativeRotation(FRotator(0.f, MeshYaw + 180.f, WheelSpin));
		WheelRR->SetRelativeRotation(FRotator(0.f, MeshYaw, -WheelSpin));
	}

	if (EngineAudio->GetSound())
	{
		const bool bRunning = IsEngineRunning();
		if (bRunning && !EngineAudio->IsPlaying())
		{
			EngineAudio->Play();
		}
		else if (!bRunning && EngineAudio->IsPlaying())
		{
			EngineAudio->FadeOut(0.6f, 0.f);
		}
		if (bRunning)
		{
			const float Load = FMath::Clamp(FMath::Abs(Speed) / FMath::Max(MaxForwardSpeed, 1.f), 0.f, 1.f);
			EngineAudio->SetPitchMultiplier(0.75f + 0.75f * Load);
			EngineAudio->SetVolumeMultiplier(0.45f + 0.4f * Load);
		}
	}
}
