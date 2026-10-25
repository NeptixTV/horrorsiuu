#include "Horror/SUCreature.h"
#include "SignalUnknown.h"
#include "Player/SUCharacter.h"
#include "Vehicle/SUVehicle.h"
#include "Evidence/SUEvidenceComponent.h"
#include "Audio/SUProceduralAudioComponent.h"
#include "Core/SUGameUserSettings.h"
#include "World/SUAssetLibrary.h"
#include "World/SUWorldBuilder.h"
#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr float ViewHalfAngleCos = 0.82f;   // ~35 degrees from the view centre
	constexpr float FlashlightHalfAngleCos = 0.966f; // ~15 degrees
}

ASUCreature::ASUCreature()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	GetCapsuleComponent()->InitCapsuleSize(30.f, 130.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->MaxWalkSpeed = 150.f;
	Move->MaxAcceleration = 1500.f;
	Move->BrakingDecelerationWalking = 1800.f;
	Move->MaxStepHeight = 60.f;
	Move->SetWalkableFloorAngle(55.f);

	Evidence = CreateDefaultSubobject<USUEvidenceComponent>(TEXT("Evidence"));
	Evidence->SetupAttachment(GetCapsuleComponent());
	Evidence->Category = TEXT("Creature");
	Evidence->bRequiresLight = true;
	Evidence->MaxDistance = 4000.f;
	Evidence->MinCoverage = 0.002f;
	Evidence->IdealCoverage = 0.035f;
}

void ASUCreature::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUCreature, Type);
	DOREPLIFETIME(ASUCreature, State);
	DOREPLIFETIME(ASUCreature, Aggression);
}

ASUCreature* ASUCreature::SpawnCreature(UWorld* World, ESUCreatureType InType, const FVector& Location, float Yaw, ESUCreatureState StartState, AActor* InTarget)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const float HalfHeight = InType == ESUCreatureType::Observer ? 130.f : 70.f;
	const FTransform T(FRotator(0.f, Yaw, 0.f), Location + FVector(0.f, 0.f, HalfHeight + 5.f));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.bDeferConstruction = true;
	ASUCreature* C = World->SpawnActor<ASUCreature>(ASUCreature::StaticClass(), T, Params);
	if (C)
	{
		C->Type = InType;
		C->State = StartState;
		C->Target = InTarget;
		C->FinishSpawning(T);
		UE_LOG(LogSignalUnknown, Log, TEXT("Creature spawned: %s at %s"), InType == ESUCreatureType::Observer ? TEXT("Observer") : TEXT("Hunter"), *Location.ToCompactString());
	}
	return C;
}

int32 ASUCreature::CountActive(const UWorld* World)
{
	int32 N = 0;
	if (World)
	{
		for (TActorIterator<ASUCreature> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				++N;
			}
		}
	}
	return N;
}

void ASUCreature::BeginPlay()
{
	Super::BeginPlay();
	const bool bObserver = Type == ESUCreatureType::Observer;
	GetCapsuleComponent()->SetCapsuleSize(bObserver ? 30.f : 42.f, bObserver ? 130.f : 70.f);
	Evidence->EvidenceId = bObserver ? FName(TEXT("EV_Observer")) : FName(TEXT("EV_Hunter"));
	Evidence->DisplayName = bObserver ? SUText(TEXT("Unbekannte Gestalt")) : SUText(TEXT("Unbekanntes Raubtier"));
	Evidence->BoxExtent = bObserver ? FVector(22.f, 30.f, 125.f) : FVector(80.f, 35.f, 55.f);
	Evidence->SetRelativeLocation(FVector::ZeroVector);
	if (!bObserver)
	{
		GetCharacterMovement()->MaxAcceleration = 4200.f;
		GetCharacterMovement()->BrakingDecelerationWalking = 2500.f;
		Lifetime = 55.f;
	}
	BuildBody();

	if (GetNetMode() != NM_DedicatedServer)
	{
		VoiceAudio = NewObject<USUProceduralAudioComponent>(this);
		VoiceAudio->SetupAttachment(GetCapsuleComponent());
		VoiceAudio->SetRelativeLocation(FVector(bObserver ? 0.f : 70.f, 0.f, bObserver ? 110.f : 20.f));
		VoiceAudio->ConfigureSound(ESUSoundKind::Creature, SUAudioCategory::Effects, 1.f, bObserver ? 2500.f : 7000.f);
		VoiceAudio->RegisterComponent();
		VoiceAudio->StartSound();
		StepAudio = NewObject<USUProceduralAudioComponent>(this);
		StepAudio->SetupAttachment(GetCapsuleComponent());
		StepAudio->SetRelativeLocation(FVector(0.f, 0.f, -60.f));
		StepAudio->ConfigureSound(ESUSoundKind::Footstep, SUAudioCategory::Effects, 0.9f, 3000.f);
		StepAudio->RegisterComponent();
		StepAudio->StartSound();
	}
	if (HasAuthority())
	{
		RetreatGoal = PickRetreatPoint();
	}
}

void ASUCreature::BuildBody()
{
	USUAssetLibrary* Lib = USUAssetLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	auto Part = [this, Lib](ESUShape Shape, const FVector& Loc, const FVector& Size, ESUMat Mat, const FRotator& Rot = FRotator::ZeroRotator) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(GetCapsuleComponent());
		C->SetStaticMesh(Lib->GetMesh(Shape));
		C->SetMaterial(0, Lib->GetMaterial(Mat));
		C->SetRelativeTransform(FTransform(Rot, Loc, Size / 100.f));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(true);
		C->RegisterComponent();
		return C;
	};
	// PLACEHOLDER creature bodies – to be replaced by skeletal meshes with animations.
	if (Type == ESUCreatureType::Observer)
	{
		Part(ESUShape::Cube, FVector(0.f, 0.f, 45.f), FVector(18.f, 34.f, 80.f), ESUMat::Creature, FRotator(-4.f, 0.f, 0.f));   // torso
		Part(ESUShape::Cube, FVector(2.f, 0.f, 2.f), FVector(16.f, 28.f, 18.f), ESUMat::Creature);                            // pelvis
		Head = Part(ESUShape::Sphere, FVector(6.f, 0.f, 108.f), FVector(17.f, 15.f, 30.f), ESUMat::Creature);
		Part(ESUShape::Sphere, FVector(13.f, -4.f, 112.f), FVector(1.6f, 2.f, 1.4f), ESUMat::Lamp);                           // faint eye shine
		Part(ESUShape::Sphere, FVector(13.f, 4.f, 112.f), FVector(1.6f, 2.f, 1.4f), ESUMat::Lamp);
		Limbs.Add(Part(ESUShape::Cylinder, FVector(0.f, -9.f, -62.f), FVector(8.f, 8.f, 136.f), ESUMat::Creature));            // legs
		Limbs.Add(Part(ESUShape::Cylinder, FVector(0.f, 9.f, -62.f), FVector(8.f, 8.f, 136.f), ESUMat::Creature));
		Limbs.Add(Part(ESUShape::Cylinder, FVector(2.f, -22.f, 18.f), FVector(6.f, 6.f, 150.f), ESUMat::Creature));            // arms reach the knees
		Limbs.Add(Part(ESUShape::Cylinder, FVector(2.f, 22.f, 18.f), FVector(6.f, 6.f, 150.f), ESUMat::Creature));
	}
	else
	{
		Part(ESUShape::Cube, FVector(0.f, 0.f, 18.f), FVector(150.f, 46.f, 42.f), ESUMat::Creature, FRotator(6.f, 0.f, 0.f));  // body
		Part(ESUShape::Cube, FVector(-10.f, 0.f, 42.f), FVector(120.f, 8.f, 8.f), ESUMat::BloodDry, FRotator(6.f, 0.f, 0.f));  // spine ridge
		Head = Part(ESUShape::Sphere, FVector(92.f, 0.f, 20.f), FVector(44.f, 30.f, 26.f), ESUMat::Creature);
		Part(ESUShape::Cube, FVector(110.f, 0.f, 9.f), FVector(26.f, 20.f, 6.f), ESUMat::BloodDry);                           // jaw
		Limbs.Add(Part(ESUShape::Cylinder, FVector(55.f, -26.f, -30.f), FVector(10.f, 10.f, 84.f), ESUMat::Creature));         // front legs
		Limbs.Add(Part(ESUShape::Cylinder, FVector(55.f, 26.f, -30.f), FVector(10.f, 10.f, 84.f), ESUMat::Creature));
		Limbs.Add(Part(ESUShape::Cylinder, FVector(-55.f, -26.f, -30.f), FVector(12.f, 12.f, 84.f), ESUMat::Creature));        // hind legs
		Limbs.Add(Part(ESUShape::Cylinder, FVector(-55.f, 26.f, -30.f), FVector(12.f, 12.f, 84.f), ESUMat::Creature));
	}
}

// ------------------------------------------------------------------------------------ state

void ASUCreature::SetState(ESUCreatureState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	StateTime = 0.f;
	bAttackHit = false;
	if (NewState == ESUCreatureState::Retreat)
	{
		RetreatGoal = PickRetreatPoint();
	}
}

void ASUCreature::StartRetreat()
{
	if (State != ESUCreatureState::Retreat)
	{
		SetState(ESUCreatureState::Retreat);
		if (Type == ESUCreatureType::Hunter)
		{
			MulticastVoice(2, 0.8f);
		}
	}
}

void ASUCreature::ScriptedMoveTo(const FVector& Location, float Speed)
{
	SetState(ESUCreatureState::Scripted);
	ScriptedGoal = Location;
	ScriptedSpeed = Speed;
	bHasScriptedGoal = true;
}

void ASUCreature::ScriptedFace(const FVector& Location)
{
	ScriptedLook = Location;
	bHasScriptedLook = true;
}

bool ASUCreature::HasReachedScriptedGoal(float Tolerance) const
{
	return !bHasScriptedGoal || FVector::Dist2D(GetActorLocation(), ScriptedGoal) < Tolerance;
}

void ASUCreature::MulticastVoice_Implementation(int32 TriggerKind, float Strength)
{
	if (VoiceAudio)
	{
		VoiceAudio->TriggerSound(TriggerKind, Strength);
	}
}

// ------------------------------------------------------------------------------------ perception

ASUCharacter* ASUCreature::FindNearestPlayer(float& OutDistance) const
{
	ASUCharacter* Best = nullptr;
	OutDistance = TNumericLimits<float>::Max();
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDead())
		{
			continue;
		}
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (D < OutDistance)
		{
			OutDistance = D;
			Best = *It;
		}
	}
	return Best;
}

bool ASUCreature::IsSeenByAnyPlayer(float MaxDistance, float* OutLookTime) const
{
	if (OutLookTime)
	{
		*OutLookTime = LookedAtTime;
	}
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, Type == ESUCreatureType::Observer ? 60.f : 10.f);
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		const ASUCharacter* C = *It;
		if (C->IsDead() || !C->GetFirstPersonCamera())
		{
			continue;
		}
		const FVector Eye = C->GetFirstPersonCamera()->GetComponentLocation();
		const FVector To = Chest - Eye;
		const float Dist = To.Size();
		if (Dist > MaxDistance)
		{
			continue;
		}
		const FVector ViewDir = C->GetBaseAimRotation().Vector();
		if (FVector::DotProduct(ViewDir, To / FMath::Max(Dist, 1.f)) < ViewHalfAngleCos && Dist > 250.f)
		{
			continue;
		}
		FCollisionQueryParams Params(FName(TEXT("SUCreatureSeen")), false, this);
		Params.AddIgnoredActor(C);
		if (C->GetVehicle())
		{
			Params.AddIgnoredActor(C->GetVehicle());
		}
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Chest, ECC_Visibility, Params))
		{
			return true;
		}
	}
	return false;
}

bool ASUCreature::IsLitByFlashlight(float& OutStrength) const
{
	OutStrength = 0.f;
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, Type == ESUCreatureType::Observer ? 60.f : 10.f);
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		const ASUCharacter* C = *It;
		if (C->IsDead() || !C->IsFlashlightOn() || !C->GetFirstPersonCamera())
		{
			continue;
		}
		const FVector Eye = C->GetFirstPersonCamera()->GetComponentLocation();
		const FVector To = Chest - Eye;
		const float Dist = To.Size();
		if (Dist > 1800.f || FVector::DotProduct(C->GetFlashlightDirection(), To / FMath::Max(Dist, 1.f)) < FlashlightHalfAngleCos)
		{
			continue;
		}
		FCollisionQueryParams Params(FName(TEXT("SUCreatureLit")), false, this);
		Params.AddIgnoredActor(C);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Chest, ECC_Visibility, Params))
		{
			OutStrength = FMath::Max(OutStrength, 1.f - Dist / 1800.f);
		}
	}
	return OutStrength > 0.f;
}

void ASUCreature::UpdatePerception(float DeltaSeconds)
{
	const bool bSeen = IsSeenByAnyPlayer();
	LookedAtTime = bSeen ? LookedAtTime + DeltaSeconds : FMath::Max(0.f, LookedAtTime - DeltaSeconds * 0.5f);
	UnseenTime = bSeen ? 0.f : UnseenTime + DeltaSeconds;
	float Light = 0.f;
	const bool bLit = IsLitByFlashlight(Light);
	LitTime = bLit ? LitTime + DeltaSeconds * (0.5f + Light) : FMath::Max(0.f, LitTime - DeltaSeconds);
}

// ------------------------------------------------------------------------------------ tick

void ASUCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		Age += DeltaSeconds;
		StateTime += DeltaSeconds;
		AttackCooldown = FMath::Max(0.f, AttackCooldown - DeltaSeconds);
		UpdatePerception(DeltaSeconds);

		switch (State)
		{
		case ESUCreatureState::Scripted:
			UpdateScripted(DeltaSeconds);
			break;
		case ESUCreatureState::Retreat:
			UpdateRetreat(DeltaSeconds);
			break;
		default:
			if (Type == ESUCreatureType::Observer)
			{
				UpdateObserver(DeltaSeconds);
			}
			else
			{
				UpdateHunter(DeltaSeconds);
			}
			break;
		}

		// despawn only while nobody is looking
		const bool bOld = Age > Lifetime + 40.f;
		const bool bRetreatDone = State == ESUCreatureState::Retreat && StateTime > 1.5f && UnseenTime > 0.8f;
		if ((bDespawnRequested && UnseenTime > 0.3f) || bRetreatDone || (bOld && UnseenTime > 0.5f) || GetActorLocation().Z < -50000.f)
		{
			Destroy();
			return;
		}

		VoiceTimer -= DeltaSeconds;
		if (VoiceTimer <= 0.f && State != ESUCreatureState::Scripted)
		{
			VoiceTimer = FMath::FRandRange(4.f, 9.f);
			if (Type == ESUCreatureType::Observer && State == ESUCreatureState::Watch && FMath::FRand() < 0.35f)
			{
				MulticastVoice(2, 0.35f); // faint clicking
			}
			else if (Type == ESUCreatureType::Hunter && State == ESUCreatureState::Chase)
			{
				MulticastVoice(FMath::FRand() < 0.4f ? 0 : 2, 1.f);
			}
		}
	}
	Animate(DeltaSeconds);
}

void ASUCreature::UpdateObserver(float DeltaSeconds)
{
	float NearestDist = 0.f;
	ASUCharacter* Nearest = FindNearestPlayer(NearestDist);
	AActor* Focus = Target.IsValid() ? Target.Get() : Nearest;
	if (State == ESUCreatureState::Idle)
	{
		SetState(ESUCreatureState::Watch);
	}
	Aggression = 0.15f;
	GetCharacterMovement()->MaxWalkSpeed = 150.f;
	if (Focus)
	{
		FaceTowards(Focus->GetActorLocation(), DeltaSeconds, 2.f);
	}
	// withdraws when it is noticed for long, lit up, approached, or simply after a while
	if (LookedAtTime > 3.2f || LitTime > 0.9f || (Nearest && NearestDist < 1100.f) || Age > Lifetime)
	{
		StartRetreat();
	}
}

void ASUCreature::UpdateHunter(float DeltaSeconds)
{
	AActor* T = Target.Get();
	float NearestDist = 0.f;
	ASUCharacter* Nearest = FindNearestPlayer(NearestDist);
	if (ASUCharacter* TC = Cast<ASUCharacter>(T))
	{
		if (TC->IsDead())
		{
			T = nullptr;
		}
		else if (TC->GetVehicle())
		{
			T = TC->GetVehicle();
		}
	}
	if (ASUVehicle* TV = Cast<ASUVehicle>(T))
	{
		if (!TV->HasOccupants())
		{
			// everyone got out: go for the closest person
			T = Nearest;
		}
	}
	if (!T)
	{
		T = Nearest;
	}
	if (!T || Age > Lifetime)
	{
		StartRetreat();
		return;
	}
	Target = T;

	// a strong light in the face drives it off
	if (LitTime > 1.6f)
	{
		StartRetreat();
		return;
	}

	ASUVehicle* Vehicle = Cast<ASUVehicle>(T);
	const FVector TargetLoc = T->GetActorLocation();
	const float Dist = FVector::Dist2D(TargetLoc, GetActorLocation());
	if (Vehicle)
	{
		FastVehicleTime = Vehicle->GetSpeedKmh() > 78.f ? FastVehicleTime + DeltaSeconds : FMath::Max(0.f, FastVehicleTime - DeltaSeconds);
		if (FastVehicleTime > 5.f || Dist > 9000.f)
		{
			StartRetreat();
			return;
		}
	}
	else if (Dist > 6000.f)
	{
		StartRetreat();
		return;
	}

	switch (State)
	{
	case ESUCreatureState::Idle:
	case ESUCreatureState::Watch:
		SetState(ESUCreatureState::Stalk);
		break;
	case ESUCreatureState::Stalk:
	{
		Aggression = 0.5f;
		// keep a distance behind / beside the target, then break into the chase
		const FVector Behind = TargetLoc - T->GetActorForwardVector() * 1800.f + T->GetActorRightVector() * 900.f;
		SteerTowards(Behind, Vehicle ? 1800.f : 500.f, DeltaSeconds);
		if (StateTime > 5.f || LookedAtTime > 0.6f || Dist < 1200.f)
		{
			SetState(ESUCreatureState::Chase);
			MulticastVoice(0, 1.2f);
		}
		break;
	}
	case ESUCreatureState::Chase:
	{
		Aggression = 1.f;
		const FVector Predicted = TargetLoc + T->GetVelocity() * 0.35f;
		SteerTowards(Predicted, Vehicle ? 1900.f : 820.f, DeltaSeconds);
		const float Reach = Vehicle ? 330.f : 170.f;
		if (Dist < Reach && AttackCooldown <= 0.f)
		{
			SetState(ESUCreatureState::Attack);
		}
		break;
	}
	case ESUCreatureState::Attack:
	{
		FaceTowards(TargetLoc, DeltaSeconds, 12.f);
		SteerTowards(TargetLoc, Vehicle ? 1900.f : 400.f, DeltaSeconds);
		if (!bAttackHit && StateTime > 0.3f)
		{
			bAttackHit = true;
			AttackCooldown = 2.2f;
			if (Dist < (Vehicle ? 420.f : 240.f))
			{
				MulticastVoice(1, 1.2f);
				if (Vehicle)
				{
					Vehicle->ApplyAttack(FMath::FRandRange(9.f, 15.f), GetActorLocation());
				}
				else if (ASUCharacter* Victim = Cast<ASUCharacter>(T))
				{
					Victim->ApplyDamageSU(24.f, this);
				}
			}
		}
		if (StateTime > 0.7f)
		{
			SetState(ESUCreatureState::Chase);
		}
		break;
	}
	default:
		break;
	}
}

void ASUCreature::UpdateRetreat(float DeltaSeconds)
{
	Aggression = Type == ESUCreatureType::Hunter ? 0.4f : 0.f;
	SteerTowards(RetreatGoal, Type == ESUCreatureType::Hunter ? 1500.f : 700.f, DeltaSeconds);
	if (FVector::Dist2D(GetActorLocation(), RetreatGoal) < 200.f)
	{
		RetreatGoal = PickRetreatPoint();
	}
}

void ASUCreature::UpdateScripted(float DeltaSeconds)
{
	if (bHasScriptedGoal && !HasReachedScriptedGoal(40.f))
	{
		SteerTowards(ScriptedGoal, ScriptedSpeed, DeltaSeconds);
	}
	else if (bHasScriptedLook)
	{
		FaceTowards(ScriptedLook, DeltaSeconds, 3.f);
	}
}

FVector ASUCreature::PickRetreatPoint() const
{
	float D = 0.f;
	const ASUCharacter* Nearest = FindNearestPlayer(D);
	FVector Away = Nearest ? (GetActorLocation() - Nearest->GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector();
	if (Away.IsNearlyZero())
	{
		Away = FVector(1.f, 0.f, 0.f);
	}
	Away = Away.RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector);
	FVector Goal = GetActorLocation() + Away * 4000.f;
	if (const ASUWorldBuilder* B = ASUWorldBuilder::Get(this))
	{
		Goal = B->GroundPoint(Goal.X, Goal.Y, 100.f);
	}
	return Goal;
}

// ------------------------------------------------------------------------------------ movement

void ASUCreature::SteerTowards(const FVector& Goal, float Speed, float DeltaSeconds)
{
	FVector Desired = Goal - GetActorLocation();
	Desired.Z = 0.f;
	if (Desired.SizeSquared() < 30.f * 30.f)
	{
		return;
	}
	Desired.Normalize();
	GetCharacterMovement()->MaxWalkSpeed = Speed;

	// pick the first free direction around the desired one (trees, rocks, houses)
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Probe = 120.f + GetVelocity().Size2D() * 0.35f;
	FCollisionQueryParams Params(FName(TEXT("SUCreatureSteer")), false, this);
	if (AActor* T = Target.Get())
	{
		Params.AddIgnoredActor(T);
	}
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius * 0.9f, HalfHeight * 0.6f);
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, HalfHeight * 0.3f);
	static const float Angles[] = {0.f, 30.f, -30.f, 60.f, -60.f, 90.f, -90.f, 130.f, -130.f};
	FVector Chosen = Desired;
	for (float A : Angles)
	{
		const FVector Dir = Desired.RotateAngleAxis(A, FVector::UpVector);
		FHitResult Hit;
		const bool bBlocked = GetWorld()->SweepSingleByChannel(Hit, Start, Start + Dir * Probe, FQuat::Identity, ECC_Pawn, Shape, Params);
		if (!bBlocked || Hit.ImpactNormal.Z > 0.6f)
		{
			Chosen = Dir;
			break;
		}
	}
	AddMovementInput(Chosen, 1.f);
	FaceTowards(GetActorLocation() + Chosen * 100.f, DeltaSeconds, Type == ESUCreatureType::Hunter ? 10.f : 5.f);
}

void ASUCreature::FaceTowards(const FVector& Location, float DeltaSeconds, float Rate)
{
	const FVector To = Location - GetActorLocation();
	if (To.SizeSquared2D() < 1.f)
	{
		return;
	}
	const FRotator Want(0.f, To.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, Rate));
}

float ASUCreature::GetMaxSpeed() const
{
	return GetCharacterMovement()->MaxWalkSpeed;
}

// ------------------------------------------------------------------------------------ animation

void ASUCreature::Animate(float DeltaSeconds)
{
	const float Speed = GetVelocity().Size2D();
	const bool bObserver = Type == ESUCreatureType::Observer;
	AnimPhase += DeltaSeconds * (bObserver ? Speed / 30.f : Speed / 55.f + 0.5f);
	const float Amount = FMath::Clamp(Speed / (bObserver ? 300.f : 900.f), 0.f, 1.f);
	if (Limbs.Num() == 4)
	{
		if (bObserver)
		{
			const float Swing = FMath::Sin(AnimPhase) * 28.f * Amount;
			const float Sway = FMath::Sin(GetWorld()->GetTimeSeconds() * 0.7f) * 3.f;
			auto Pivot = [](UStaticMeshComponent* C, const FVector& Top, float Pitch, float Roll, float Length)
			{
				const FRotator R(Pitch, 0.f, Roll);
				C->SetRelativeLocationAndRotation(Top + R.RotateVector(FVector(0.f, 0.f, -Length * 0.5f)), R);
			};
			Pivot(Limbs[0], FVector(0.f, -9.f, 6.f), Swing, 0.f, 136.f);
			Pivot(Limbs[1], FVector(0.f, 9.f, 6.f), -Swing, 0.f, 136.f);
			Pivot(Limbs[2], FVector(2.f, -22.f, 88.f), -Swing * 0.6f, -4.f + Sway, 150.f);
			Pivot(Limbs[3], FVector(2.f, 22.f, 88.f), Swing * 0.6f, 4.f - Sway, 150.f);
		}
		else
		{
			// gallop: front and hind pairs out of phase
			const float Front = FMath::Sin(AnimPhase) * 40.f * Amount;
			const float Hind = FMath::Sin(AnimPhase + UE_PI * 0.8f) * 40.f * Amount;
			auto Pivot = [](UStaticMeshComponent* C, const FVector& Top, float Pitch, float Length)
			{
				const FRotator R(Pitch, 0.f, 0.f);
				C->SetRelativeLocationAndRotation(Top + R.RotateVector(FVector(0.f, 0.f, -Length * 0.5f)), R);
			};
			Pivot(Limbs[0], FVector(55.f, -26.f, 12.f), Front, 84.f);
			Pivot(Limbs[1], FVector(55.f, 26.f, 12.f), Front * 0.8f, 84.f);
			Pivot(Limbs[2], FVector(-55.f, -26.f, 12.f), Hind, 84.f);
			Pivot(Limbs[3], FVector(-55.f, 26.f, 12.f), Hind * 0.8f, 84.f);
		}
	}
	if (Head)
	{
		// the observer slowly tilts its head while watching
		const float Tilt = bObserver && Speed < 20.f ? FMath::Sin(GetWorld()->GetTimeSeconds() * 0.4f) * 18.f : 0.f;
		Head->SetRelativeRotation(FRotator(0.f, 0.f, Tilt));
	}
	if (VoiceAudio)
	{
		VoiceAudio->SetSoundParam(SULogic::Dsp::Param::A, bObserver ? 0.25f : 0.4f + 0.6f * Aggression);
		VoiceAudio->SetSoundParam(SULogic::Dsp::Param::B, bObserver ? 0.f : Aggression);
	}
	if (StepAudio && Speed > 60.f)
	{
		StepTimer -= DeltaSeconds;
		if (StepTimer <= 0.f)
		{
			StepTimer = bObserver ? 0.75f : FMath::Clamp(18000.f / FMath::Max(Speed, 1.f) * 0.02f, 0.12f, 0.4f);
			StepAudio->TriggerSound(0, bObserver ? 0.6f : 1.1f);
		}
	}
}
