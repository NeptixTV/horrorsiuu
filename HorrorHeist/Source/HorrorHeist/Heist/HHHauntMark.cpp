#include "Heist/HHHauntMark.h"
#include "Audio/HHAudioSubsystem.h"
#include "Components/DecalComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

AHHHauntMark::AHHHauntMark()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	SetRootComponent(Decal);
	Decal->DecalSize = FVector(10.f, 40.f, 40.f);
	Decal->SetVisibility(false);
}

void AHHHauntMark::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHHauntMark, bRevealed);
}

void AHHHauntMark::BeginPlay()
{
	Super::BeginPlay();
	Fade = bRevealed ? 1.f : 0.f;
	Decal->SetVisibility(bRevealed);
}

void AHHHauntMark::Reveal()
{
	if (HasAuthority() && !bRevealed)
	{
		bRevealed = true;
		OnRep_Revealed();
		ForceNetUpdate();
	}
}

void AHHHauntMark::OnRep_Revealed()
{
	if (!bRevealed)
	{
		return;
	}
	Decal->SetVisibility(true);
	Fade = 0.f;
	if (UMaterialInterface* Base = Decal->GetDecalMaterial())
	{
		if (!Base->IsA<UMaterialInstanceDynamic>())
		{
			Decal->SetDecalMaterial(UMaterialInstanceDynamic::Create(Base, this));
		}
	}
	SetActorTickEnabled(true);
	if (GetNetMode() != NM_DedicatedServer && HasActorBegunPlay())
	{
		if (RevealSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, RevealSound, GetActorLocation());
		}
		if (!Caption.IsEmpty())
		{
			if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
			{
				Audio->ShowCaption(Caption, 3.f, true);
			}
		}
	}
}

void AHHHauntMark::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Fade = FMath::Min(1.f, Fade + DeltaSeconds / FMath::Max(0.1f, FadeSeconds));
	if (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Decal->GetDecalMaterial()))
	{
		Dynamic->SetScalarParameterValue(TEXT("Opacity"), Fade);
	}
	if (Fade >= 1.f)
	{
		SetActorTickEnabled(false);
	}
}

bool AHHHauntMark::TryPhotograph(const FVector& Eye, const FVector& Direction, const AActor* Photographer)
{
	if (!HasAuthority() || !bRevealed || bPhotographed)
	{
		return false;
	}
	const FVector Center = GetActorLocation();
	const FVector ToMark = Center - Eye;
	const float Distance = ToMark.Size();
	if (Distance > 900.f || FVector::DotProduct(Direction.GetSafeNormal(), ToMark / FMath::Max(Distance, 1.f)) < FMath::Cos(FMath::DegreesToRadians(25.f)))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHMarkPhoto), false, Photographer);
	Params.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Center, ECC_Visibility, Params) && FVector::DistSquared(Hit.ImpactPoint, Center) > FMath::Square(60.f))
	{
		return false;
	}
	bPhotographed = true;
	return true;
}
