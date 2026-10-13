#include "Heist/HHHauntThrowable.h"
#include "Core/HHNoiseSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

AHHHauntThrowable::AHHHauntThrowable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("NoCollision"));
	Mesh->SetGenerateOverlapEvents(false);
}

void AHHHauntThrowable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHHauntThrowable, bThrown);
	DOREPLIFETIME(AHHHauntThrowable, From);
	DOREPLIFETIME(AHHHauntThrowable, To);
	DOREPLIFETIME(AHHHauntThrowable, ThrowStart);
	DOREPLIFETIME(AHHHauntThrowable, ThrowDuration);
}

void AHHHauntThrowable::BeginPlay()
{
	Super::BeginPlay();
	StartRotation = GetActorRotation();
	if (bThrown)
	{
		OnRep_Throw();
	}
}

float AHHHauntThrowable::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : 0.f;
}

void AHHHauntThrowable::ThrowTo(const FVector& Target, float Duration)
{
	if (!HasAuthority() || bThrown)
	{
		return;
	}
	From = GetActorLocation();
	To = Target;
	ThrowDuration = FMath::Max(0.2f, Duration);
	ThrowStart = GetServerTime();
	bThrown = true;
	OnRep_Throw();
	ForceNetUpdate();
}

void AHHHauntThrowable::OnRep_Throw()
{
	if (!bThrown)
	{
		return;
	}
	// Lands on its side at a random angle; the same on every machine (seeded by the target).
	FRandomStream Stream(static_cast<int32>(To.X * 7.f + To.Y * 13.f));
	EndRotation = FRotator(Stream.FRandRange(-90.f, 90.f), Stream.FRandRange(0.f, 360.f), Stream.FRandRange(-90.f, 90.f));
	bLanded = false;
	SetActorTickEnabled(true);
}

void AHHHauntThrowable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bThrown)
	{
		SetActorTickEnabled(false);
		return;
	}
	const float Alpha = FMath::Clamp((GetServerTime() - ThrowStart) / ThrowDuration, 0.f, 1.f);
	const float Arc = FMath::Clamp(FVector::Dist(From, To) * 0.25f, 30.f, 120.f);
	FVector Location = FMath::Lerp(FVector(From), FVector(To), Alpha);
	Location.Z += Arc * 4.f * Alpha * (1.f - Alpha);
	const FRotator Spin = FMath::Lerp(StartRotation, EndRotation + FRotator(0.f, 540.f, 0.f), Alpha);
	SetActorLocationAndRotation(Location, Alpha >= 1.f ? EndRotation : Spin);
	if (Alpha >= 1.f && !bLanded)
	{
		bLanded = true;
		SetActorTickEnabled(false);
		if (ImpactSound && GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
		}
		if (HasAuthority())
		{
			if (UHHNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UHHNoiseSubsystem>())
			{
				Noise->ReportNoise(GetActorLocation(), 0.35f, this, TEXT("HauntThrow"));
			}
		}
	}
}
