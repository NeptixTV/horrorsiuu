#include "Player/HHFootstepComponent.h"
#include "Player/HHCharacter.h"
#include "Core/HHGameData.h"
#include "Core/HHNoiseSubsystem.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"

UHHFootstepComponent::UHHFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
}

void UHHFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || !Movement->IsMovingOnGround())
	{
		return;
	}

	const float Speed = Character->GetVelocity().Size2D();
	if (Speed < 20.f)
	{
		// Settle on the next step so the first one after stopping is not instant.
		DistanceAccumulator = FMath::Min(DistanceAccumulator, WalkStride * 0.5f);
		return;
	}

	const AHHCharacter* HHCharacter = Cast<AHHCharacter>(Character);
	const bool bCrouched = Character->bIsCrouched;
	const bool bSprinting = HHCharacter && HHCharacter->IsSprinting() && Speed > 260.f;
	const float Stride = bCrouched ? CrouchStride : (bSprinting ? SprintStride : WalkStride);

	DistanceAccumulator += Speed * DeltaTime;
	if (DistanceAccumulator >= Stride)
	{
		DistanceAccumulator -= Stride;
		const float Loudness = bCrouched ? 0.3f : (bSprinting ? 1.f : 0.6f);
		PlayStep(Loudness);
	}
}

int32 UHHFootstepComponent::TraceSurface(FVector& OutLocation) const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	OutLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	if (!Owner || !World)
	{
		return 0;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHFootstep), false, Owner);
	Params.bReturnPhysicalMaterial = true;
	FHitResult Hit;
	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, 160.f);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		OutLocation = Hit.ImpactPoint;
		return static_cast<int32>(UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get()));
	}
	return 0;
}

USoundBase* UHHFootstepComponent::PickSound(int32 SurfaceType)
{
	const UHHGameData* GameData = UHHGameData::Get(this);
	if (!GameData)
	{
		return nullptr;
	}

	const TArray<TSoftObjectPtr<USoundBase>>* Set = &GameData->DefaultFootsteps;
	for (const FHHFootstepSet& Candidate : GameData->FootstepSets)
	{
		if (Candidate.SurfaceType == SurfaceType && Candidate.Sounds.Num() > 0)
		{
			Set = &Candidate.Sounds;
			break;
		}
	}
	if (Set->Num() == 0)
	{
		return nullptr;
	}

	// Never the same variation twice in a row.
	int32 Index = FMath::RandRange(0, Set->Num() - 1);
	if (Set->Num() > 1 && Index == LastIndex)
	{
		Index = (Index + 1) % Set->Num();
	}
	LastIndex = Index;
	return (*Set)[Index].LoadSynchronous();
}

void UHHFootstepComponent::PlayStep(float Loudness)
{
	FVector Location;
	const int32 Surface = TraceSurface(Location);
	if (USoundBase* Sound = PickSound(Surface))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, FMath::Lerp(0.35f, 1.f, Loudness), FMath::FRandRange(0.94f, 1.06f));
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (UHHNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UHHNoiseSubsystem>())
		{
			Noise->ReportNoise(Location, Loudness, GetOwner(), TEXT("Footstep"));
		}
	}
}

void UHHFootstepComponent::PlayLanding(float ImpactSpeed)
{
	const float Loudness = FMath::Clamp(ImpactSpeed / 600.f, 0.3f, 1.f);
	PlayStep(Loudness);

	if (const UHHGameData* GameData = UHHGameData::Get(this))
	{
		if (GameData->LandingSounds.Num() > 0 && ImpactSpeed > 350.f)
		{
			if (USoundBase* Thud = GameData->LandingSounds[FMath::RandRange(0, GameData->LandingSounds.Num() - 1)].LoadSynchronous())
			{
				UGameplayStatics::PlaySoundAtLocation(this, Thud, GetOwner()->GetActorLocation(), Loudness);
			}
		}
	}
}
