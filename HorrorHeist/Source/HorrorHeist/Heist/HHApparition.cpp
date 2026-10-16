#include "Heist/HHApparition.h"
#include "Heist/HHHauntDirector.h"
#include "Heist/HHSaltLine.h"
#include "Heist/HHHeistGameMode.h"
#include "Player/HHPlayerState.h"
#include "Player/HHCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

AHHApparition::AHHApparition()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(true);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Presence = CreateDefaultSubobject<UAudioComponent>(TEXT("Presence"));
	Presence->SetupAttachment(Root);
	Presence->bAutoActivate = false;
}

void AHHApparition::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHApparition, Mode);
}

void AHHApparition::BeginPlay()
{
	Super::BeginPlay();
	if (GhostMaterial)
	{
		for (int32 Index = 0; Index < Body->GetNumMaterials(); ++Index)
		{
			Body->SetMaterial(Index, GhostMaterial);
		}
	}
	if (IdleAnim)
	{
		Body->PlayAnimation(IdleAnim, true);
	}
	OnRep_Mode();
}

void AHHApparition::SetDirector(AHHHauntDirector* InDirector)
{
	// Defined here, not inline: assigning to TWeakObjectPtr needs the complete AHHHauntDirector type.
	Director = InDirector;
}

void AHHApparition::OnRep_Mode()
{
	const bool bVisible = Mode != EHHApparitionMode::Hidden;
	Body->SetVisibility(bVisible, true);
	SetActorTickEnabled(bVisible && HasAuthority());
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (bVisible)
	{
		if (AppearSound && HasActorBegunPlay())
		{
			UGameplayStatics::PlaySoundAtLocation(this, AppearSound, GetActorLocation());
		}
		if (Presence->Sound)
		{
			Presence->FadeIn(0.4f, Mode == EHHApparitionMode::Hunt ? 1.f : 0.5f);
		}
	}
	else if (Presence->IsPlaying())
	{
		Presence->FadeOut(0.6f, 0.f);
	}
}

void AHHApparition::Glimpse(const FVector& Location, const FVector& FaceTowards, float Lifetime, bool bInFace)
{
	if (!HasAuthority())
	{
		return;
	}
	bInYourFace = bInFace;
	SetActorLocationAndRotation(Location, FRotator(0.f, (FaceTowards - Location).Rotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Mode = EHHApparitionMode::Glimpse;
	EndTime = GetWorld()->GetTimeSeconds() + Lifetime;
	LookedAtTime = 0.f;
	Witnesses.Reset();
	OnRep_Mode();
	ForceNetUpdate();
}

void AHHApparition::Dash(const FVector& From, const FVector& To, float Speed)
{
	if (!HasAuthority())
	{
		return;
	}
	SetActorLocationAndRotation(From, FRotator(0.f, (To - From).Rotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	DashTarget = To;
	DashSpeed = FMath::Max(100.f, Speed);
	Mode = EHHApparitionMode::Dash;
	EndTime = GetWorld()->GetTimeSeconds() + FVector::Dist(From, To) / DashSpeed + 0.5f;
	Witnesses.Reset();
	OnRep_Mode();
	ForceNetUpdate();
}

void AHHApparition::TickDash(float DeltaSeconds)
{
	FVector Location = GetActorLocation();
	const FVector ToTarget = DashTarget - Location;
	const float Step = DashSpeed * DeltaSeconds;
	if (ToTarget.Size() <= Step || GetWorld()->GetTimeSeconds() > EndTime)
	{
		Vanish();
		return;
	}
	SetActorLocation(Location + ToTarget.GetSafeNormal() * Step);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (Pawn && !Witnesses.Contains(Pawn) && CanSee(Pawn, 40.f))
		{
			Witnesses.Add(Pawn);
			if (AHHHauntDirector* HauntDirector = Director.Get())
			{
				HauntDirector->OnApparitionSeen(Pawn);
			}
		}
	}
}

void AHHApparition::Hunt(const FVector& Location, APawn* Target, float Duration)
{
	if (!HasAuthority())
	{
		return;
	}
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	HuntTarget = Target;
	Mode = EHHApparitionMode::Hunt;
	EndTime = GetWorld()->GetTimeSeconds() + Duration;
	Witnesses.Reset();
	OnRep_Mode();
	ForceNetUpdate();
}

bool AHHApparition::IsInPhoto(const FVector& Eye, const FVector& Direction) const
{
	if (Mode == EHHApparitionMode::Hidden)
	{
		return false;
	}
	const FVector Center = GetActorLocation() + FVector(0.f, 0.f, 100.f);
	const FVector ToMe = Center - Eye;
	const float Distance = ToMe.Size();
	if (Distance > 1800.f || FVector::DotProduct(Direction.GetSafeNormal(), ToMe / FMath::Max(Distance, 1.f)) < FMath::Cos(FMath::DegreesToRadians(28.f)))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHApparitionPhoto), false);
	Params.AddIgnoredActor(this);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Center, ECC_Visibility, Params)
		|| FVector::DistSquared(Hit.ImpactPoint, Center) < FMath::Square(80.f);
}

void AHHApparition::OnPhotographed()
{
	if (!HasAuthority())
	{
		return;
	}
	if (Mode == EHHApparitionMode::Glimpse)
	{
		Vanish();
	}
	else if (Mode == EHHApparitionMode::Hunt)
	{
		Stall(2.5f);
	}
}

void AHHApparition::Stall(float Seconds)
{
	if (HasAuthority())
	{
		StallUntil = FMath::Max(StallUntil, GetWorld()->GetTimeSeconds() + Seconds);
		// The hunt does not last longer because of it.
	}
}

void AHHApparition::Vanish()
{
	if (HasAuthority() && Mode != EHHApparitionMode::Hidden)
	{
		Mode = EHHApparitionMode::Hidden;
		HuntTarget.Reset();
		OnRep_Mode();
		ForceNetUpdate();
	}
}

bool AHHApparition::CanSee(const APawn* Pawn, float MaxAngleDegrees) const
{
	if (!Pawn)
	{
		return false;
	}
	FVector Eye;
	FRotator View;
	Pawn->GetActorEyesViewPoint(Eye, View);
	const FVector Center = GetActorLocation() + FVector(0.f, 0.f, 100.f);
	const FVector ToMe = (Center - Eye).GetSafeNormal();
	if (FVector::DotProduct(View.Vector(), ToMe) < FMath::Cos(FMath::DegreesToRadians(MaxAngleDegrees)))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHApparitionSight), false, Pawn);
	Params.AddIgnoredActor(this);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Center, ECC_Visibility, Params);
}

void AHHApparition::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	if (Mode == EHHApparitionMode::Glimpse)
	{
		TickGlimpse(DeltaSeconds);
	}
	else if (Mode == EHHApparitionMode::Hunt)
	{
		TickHunt(DeltaSeconds);
	}
	else if (Mode == EHHApparitionMode::Dash)
	{
		TickDash(DeltaSeconds);
	}
}

void AHHApparition::TickGlimpse(float DeltaSeconds)
{
	bool bStaredAt = false;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation());
		if (Distance < 380.f && !bInYourFace)
		{
			// Never close enough to touch while it is only watching.
			Vanish();
			return;
		}
		if (CanSee(Pawn, 38.f))
		{
			if (!Witnesses.Contains(Pawn))
			{
				Witnesses.Add(Pawn);
				if (AHHHauntDirector* HauntDirector = Director.Get())
				{
					HauntDirector->OnApparitionSeen(Pawn);
				}
			}
			bStaredAt |= CanSee(Pawn, 10.f);
		}
	}
	LookedAtTime = bStaredAt ? LookedAtTime + DeltaSeconds : FMath::Max(0.f, LookedAtTime - DeltaSeconds);
	if ((LookedAtTime > 0.7f && !bInYourFace) || GetWorld()->GetTimeSeconds() > EndTime)
	{
		Vanish();
	}
}

APawn* AHHApparition::PickHuntTarget() const
{
	// Whoever is still standing comes first; with nobody left standing it comes back for the downed.
	APawn* Best = nullptr;
	APawn* BestDowned = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	float BestDownedDistance = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		const AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
		const AHHCharacter* Character = Cast<AHHCharacter>(Pawn);
		if (!State || !State->IsInsideHouse() || (Character && Character->IsDead()))
		{
			continue;
		}
		const float Distance = FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation());
		if (Character && Character->IsDowned())
		{
			if (Distance < BestDownedDistance)
			{
				BestDownedDistance = Distance;
				BestDowned = Pawn;
			}
			continue;
		}
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Pawn;
		}
	}
	return Best ? Best : BestDowned;
}

void AHHApparition::TickHunt(float DeltaSeconds)
{
	APawn* Target = HuntTarget.Get();
	const AHHPlayerState* TargetState = Target ? Target->GetPlayerState<AHHPlayerState>() : nullptr;
	const AHHCharacter* TargetBody = Cast<AHHCharacter>(Target);
	if (!TargetState || !TargetState->IsInsideHouse() || (TargetBody && TargetBody->IsIncapacitated()))
	{
		// The target got out: anyone else still inside?
		Target = PickHuntTarget();
		HuntTarget = Target;
	}
	if (!Target || GetWorld()->GetTimeSeconds() > EndTime)
	{
		Vanish();
		return;
	}
	// Salt on the floor or the flash in its face: it stands still for a moment.
	if (AHHSaltLine::TryCross(this, GetActorLocation(), 75.f))
	{
		Stall(3.5f);
	}
	if (GetWorld()->GetTimeSeconds() < StallUntil)
	{
		return;
	}

	// Glide straight at the target through walls; it keeps to the target's floor height.
	const FVector Goal = Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetDefaultHalfHeight());
	FVector Location = GetActorLocation();
	FVector ToGoal = Goal - Location;
	ToGoal.Z = FMath::Clamp(ToGoal.Z, -60.f * DeltaSeconds * 10.f, 60.f * DeltaSeconds * 10.f);
	const float Distance2D = ToGoal.Size2D();
	if (Distance2D < CatchDistance && FMath::Abs(Goal.Z - Location.Z) < 150.f)
	{
		if (AHHHauntDirector* HauntDirector = Director.Get())
		{
			HauntDirector->OnApparitionCaught(Target);
		}
		Vanish();
		return;
	}
	const FVector Step = ToGoal.GetSafeNormal2D() * FMath::Min(HuntSpeed * DeltaSeconds, Distance2D) + FVector(0.f, 0.f, ToGoal.Z * FMath::Min(1.f, DeltaSeconds * 3.f));
	Location += Step;
	SetActorLocationAndRotation(Location, FRotator(0.f, ToGoal.Rotation().Yaw, 0.f));
}
