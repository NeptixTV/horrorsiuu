#include "World/HHSwing.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

AHHSwing::AHHSwing()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	SetRootComponent(Pivot);

	Seat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Seat"));
	Seat->SetupAttachment(Pivot);
	Seat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Seat->SetMobility(EComponentMobility::Movable);

	Creak = CreateDefaultSubobject<UAudioComponent>(TEXT("Creak"));
	Creak->SetupAttachment(Seat);
	Creak->bAutoActivate = true;
}

void AHHSwing::BeginPlay()
{
	Super::BeginPlay();
	Phase = FMath::FRandRange(0.f, 2.f * UE_PI);
}

void AHHSwing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Somebody riding (a visible child attached to the seat)?
	bool bRidden = false;
	TArray<AActor*> Riders;
	GetAttachedActors(Riders, true, true);
	for (const AActor* Rider : Riders)
	{
		bRidden |= Rider && !Rider->IsHidden();
	}
	Amplitude = FMath::FInterpTo(Amplitude, bRidden ? RiddenAngle : EmptyAngle, DeltaSeconds, 0.4f);
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const float Now = GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : GetWorld()->GetTimeSeconds();
	const float Angle = Amplitude * FMath::Sin(Now * 2.f * UE_PI / FMath::Max(0.5f, Period) + Phase);
	Seat->SetRelativeRotation(FRotator(0.f, 0.f, Angle));
	// The chain creaks with the swing (louder at the turning points).
	if (Creak->Sound)
	{
		Creak->SetVolumeMultiplier(FMath::Clamp(Amplitude / 12.f, 0.15f, 1.f));
	}
}
