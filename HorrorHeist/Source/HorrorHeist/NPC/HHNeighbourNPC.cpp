#include "NPC/HHNeighbourNPC.h"
#include "NPC/HHNpcBody.h"
#include "Data/HHCharacterDefinition.h"
#include "Player/HHCharacter.h"
#include "Player/HHCosmeticComponent.h"
#include "Animation/AnimSequence.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

AHHNeighbourNPC::AHHNeighbourNPC(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bAlwaysRelevant = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	// Driven by the server without a controller.
	Movement->bRunPhysicsWithNoController = true;
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 280.f, 0.f);
	Movement->MaxWalkSpeed = 140.f;

	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Cosmetics = CreateDefaultSubobject<UHHCosmeticComponent>(TEXT("Cosmetics"));

	Torch = CreateDefaultSubobject<USpotLightComponent>(TEXT("Torch"));
	Torch->SetupAttachment(GetCapsuleComponent());
	Torch->SetRelativeLocationAndRotation(FVector(30.f, 18.f, 35.f), FRotator(-12.f, 0.f, 0.f));
	Torch->SetIntensityUnits(ELightUnits::Candelas);
	Torch->SetIntensity(900.f);
	Torch->SetAttenuationRadius(2200.f);
	Torch->SetInnerConeAngle(9.f);
	Torch->SetOuterConeAngle(22.f);
	Torch->SetLightColor(FLinearColor(1.f, 0.9f, 0.72f));
	Torch->SetVisibility(false);

	DisplayName = NSLOCTEXT("HHNeighbour", "Name", "The neighbour");
}

void AHHNeighbourNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHNeighbourNPC, State);
	DOREPLIFETIME(AHHNeighbourNPC, Suspicion);
}

AHHNeighbourNPC* AHHNeighbourNPC::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AHHNeighbourNPC> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AHHNeighbourNPC::BeginPlay()
{
	Super::BeginPlay();
	HomeTransform = GetActorTransform();
	Definition = HHNpcBody::Setup(GetMesh(), Cosmetics, CosmeticIds);
	GatherPath();
	OnRep_State();
}

void AHHNeighbourNPC::GatherPath()
{
	TMap<int32, FVector> Points;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			const FString Text = Tag.ToString();
			if (Text.StartsWith(PathTagPrefix))
			{
				Points.Add(FCString::Atoi(*Text.RightChop(PathTagPrefix.Len())), It->GetActorLocation());
			}
		}
	}
	Points.KeySort([](int32 A, int32 B) { return A < B; });
	PathPoints.Reset();
	for (const TPair<int32, FVector>& Point : Points)
	{
		PathPoints.Add(Point.Value);
	}
}

bool AHHNeighbourNPC::SendOut(const FVector& Location)
{
	if (!HasAuthority() || State != EHHNeighbourState::Home)
	{
		return false;
	}
	if (PathPoints.Num() == 0)
	{
		GatherPath();
	}
	Interest = Location;
	const FVector Porch = PathPoints.Num() > 0 ? PathPoints[0] : HomeTransform.GetLocation();
	SetActorLocation(Porch + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
	PathIndex = FMath::Min(1, PathPoints.Num() - 1);
	Suspicion = 0.f;
	Suspect.Reset();
	SetState(EHHNeighbourState::Investigating);
	MulticastSound(DoorSound);
	return true;
}

void AHHNeighbourNPC::SetState(EHHNeighbourState NewState)
{
	State = NewState;
	StateTime = 0.f;
	GetCharacterMovement()->MaxWalkSpeed = State == EHHNeighbourState::Fleeing ? 400.f : 140.f;
	OnRep_State();
	ForceNetUpdate();
}

void AHHNeighbourNPC::OnRep_State()
{
	const bool bHome = State == EHHNeighbourState::Home;
	SetActorHiddenInGame(bHome);
	SetActorEnableCollision(!bHome);
	Torch->SetVisibility(!bHome && State != EHHNeighbourState::Fleeing);
	// At home he is parked: no collision means no floor, so no movement either.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (bHome)
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		Movement->SetComponentTickEnabled(false);
	}
	else if (!Movement->IsComponentTickEnabled())
	{
		Movement->SetComponentTickEnabled(true);
		Movement->SetMovementMode(MOVE_Walking);
	}
}

void AHHNeighbourNPC::MulticastSound_Implementation(USoundBase* Sound)
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

void AHHNeighbourNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		TickServer(DeltaSeconds);
	}
	UpdateAnimation();
}

void AHHNeighbourNPC::Walk(const FVector& Point, float SpeedScale)
{
	const FVector Direction = (Point - GetActorLocation()).GetSafeNormal2D();
	AddMovementInput(Direction, SpeedScale);
}

void AHHNeighbourNPC::TickServer(float DeltaSeconds)
{
	if (State == EHHNeighbourState::Home)
	{
		return;
	}
	StateTime += DeltaSeconds;
	auto Reached = [this](const FVector& Point) { return FVector::DistSquared2D(GetActorLocation(), Point) < FMath::Square(70.f); };

	switch (State)
	{
	case EHHNeighbourState::Investigating:
		if (!PathPoints.IsValidIndex(PathIndex) || StateTime > 90.f)
		{
			LookYaw = (Interest - GetActorLocation()).Rotation().Yaw;
			SetState(EHHNeighbourState::Looking);
			break;
		}
		if (Reached(PathPoints[PathIndex]))
		{
			++PathIndex;
		}
		else
		{
			Walk(PathPoints[PathIndex], 1.f);
		}
		break;

	case EHHNeighbourState::Looking:
	{
		// Sweep the torch across the yard: towards the noise, then left and right.
		const float Swing = FMath::Sin(StateTime * 0.9f) * 55.f;
		const FRotator Facing(0.f, LookYaw + Swing, 0.f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Facing, DeltaSeconds, 2.f));
		if (StateTime > LookSeconds)
		{
			PathIndex = PathPoints.Num() - 2;
			SetState(EHHNeighbourState::Returning);
		}
		break;
	}

	case EHHNeighbourState::Returning:
	case EHHNeighbourState::Fleeing:
		if (PathIndex < 0 || !PathPoints.IsValidIndex(PathIndex) || StateTime > 90.f)
		{
			const bool bGaveUp = State == EHHNeighbourState::Returning;
			SetActorTransform(HomeTransform, false, nullptr, ETeleportType::TeleportPhysics);
			MulticastSound(DoorSound);
			SetState(EHHNeighbourState::Home);
			if (bGaveUp)
			{
				OnGaveUp.Broadcast();
			}
			break;
		}
		if (Reached(PathPoints[PathIndex]))
		{
			--PathIndex;
		}
		else
		{
			Walk(PathPoints[PathIndex], 1.f);
		}
		break;

	default:
		break;
	}

	if (State == EHHNeighbourState::Investigating || State == EHHNeighbourState::Looking || State == EHHNeighbourState::Returning)
	{
		UpdateSight(DeltaSeconds);
	}
}

bool AHHNeighbourNPC::CanSee(const APawn* Pawn, float& OutDistance) const
{
	const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 70.f);
	const FVector Point = Pawn->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	const FVector ToPawn = Point - Eye;
	OutDistance = ToPawn.Size();
	if (OutDistance > SightRange)
	{
		return false;
	}
	const float Cos = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToPawn.GetSafeNormal2D());
	if (OutDistance > 250.f && Cos < FMath::Cos(FMath::DegreesToRadians(SightHalfAngle)))
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHNeighbourSight), false, this);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params))
	{
		return true;
	}
	return Hit.GetActor() == Pawn;
}

void AHHNeighbourNPC::UpdateSight(float DeltaSeconds)
{
	SightTimer -= DeltaSeconds;
	if (SightTimer > 0.f)
	{
		return;
	}
	const float Step = 0.2f;
	SightTimer = Step;

	float BestRate = 0.f;
	APawn* Seen = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		const AHHCharacter* Character = Cast<AHHCharacter>(Pawn);
		if (!Character || Character->IsSeated() || Character->IsArrested() || Character->IsHidden())
		{
			continue;
		}
		float Distance = 0.f;
		if (!CanSee(Pawn, Distance))
		{
			continue;
		}
		// Close and in the torch beam: seconds. Far, crouched, dark: much longer.
		float Rate = 1.f / FMath::Max(RecogniseSeconds, 0.1f);
		Rate *= FMath::GetMappedRangeValueClamped(FVector2f(500.f, SightRange), FVector2f(1.f, 0.3f), Distance);
		if (Character->bIsCrouched)
		{
			Rate *= 0.5f;
		}
		if (Character->IsFlashlightOn())
		{
			Rate *= 1.8f;
		}
		const FVector ToPawn = (Pawn->GetActorLocation() - Torch->GetComponentLocation()).GetSafeNormal();
		if (FVector::DotProduct(Torch->GetForwardVector(), ToPawn) > FMath::Cos(FMath::DegreesToRadians(20.f)))
		{
			Rate *= 1.5f;
		}
		if (Rate > BestRate)
		{
			BestRate = Rate;
			Seen = Pawn;
		}
	}

	if (Seen)
	{
		Suspicion = FMath::Min(1.f, Suspicion + BestRate * Step);
		// Turn towards what he thinks he saw.
		LookYaw = (Seen->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
	}
	else
	{
		Suspicion = FMath::Max(0.f, Suspicion - 0.25f * Step);
	}

	if (Suspicion >= 1.f && Seen)
	{
		Suspect = Seen;
		MulticastSound(ShoutSound);
		PathIndex = State == EHHNeighbourState::Investigating ? PathIndex - 1 : PathPoints.Num() - 2;
		SetState(EHHNeighbourState::Fleeing);
		OnSpotted.Broadcast(Seen);
	}
}

void AHHNeighbourNPC::UpdateAnimation()
{
	const UHHCharacterDefinition* Def = Definition.Get();
	if (!Def || IsHidden())
	{
		return;
	}
	const float GroundSpeed = GetVelocity().Size2D();
	UAnimSequence* Anim = GroundSpeed > 250.f ? Def->Run.LoadSynchronous() : (GroundSpeed > 20.f ? Def->Walk.LoadSynchronous() : Def->Idle.LoadSynchronous());
	HHNpcBody::PlayLoop(GetMesh(), Anim);
}
