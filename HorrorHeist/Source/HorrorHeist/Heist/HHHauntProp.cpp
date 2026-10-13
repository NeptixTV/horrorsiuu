#include "Heist/HHHauntProp.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

AHHHauntProp::AHHHauntProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void AHHHauntProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHHauntProp, bMoved);
	DOREPLIFETIME(AHHHauntProp, RockUntil);
}

float AHHHauntProp::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : 0.f;
}

bool AHHHauntProp::IsRocking() const
{
	return RockUntil > 0.f && GetServerTime() < RockUntil;
}

void AHHHauntProp::Rock(float Seconds)
{
	if (HasAuthority() && bCanRock)
	{
		RockUntil = GetServerTime() + Seconds;
		OnRep_Rock();
		ForceNetUpdate();
	}
}

void AHHHauntProp::OnRep_Rock()
{
	if (!IsRocking())
	{
		return;
	}
	RockBase = GetActorTransform();
	SetActorTickEnabled(true);
	if (RockSound && GetNetMode() != NM_DedicatedServer)
	{
		if (RockAudio)
		{
			RockAudio->Stop();
		}
		RockAudio = UGameplayStatics::SpawnSoundAttached(RockSound, Mesh);
	}
}

void AHHHauntProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetServerTime();
	if (Now >= RockUntil)
	{
		SetActorTransform(RockBase, false, nullptr, ETeleportType::TeleportPhysics);
		SetActorTickEnabled(false);
		if (RockAudio)
		{
			RockAudio->FadeOut(0.8f, 0.f);
			RockAudio = nullptr;
		}
		return;
	}
	// Eases in and out; the runners roll about the prop's right axis.
	const float Ease = FMath::Clamp((RockUntil - Now) / 1.5f, 0.f, 1.f);
	const float Angle = RockAngle * Ease * FMath::Sin(Now * 2.f * UE_PI / FMath::Max(0.3f, RockPeriod));
	const FQuat Swing(RockBase.GetRotation().GetRightVector(), FMath::DegreesToRadians(Angle));
	SetActorRotation(Swing * RockBase.GetRotation());
}

void AHHHauntProp::BeginPlay()
{
	Super::BeginPlay();
	Original = GetActorTransform();
	ApplyTransform();
}

void AHHHauntProp::MoveToAlternate()
{
	if (HasAuthority() && !bMoved && !AltLocation.IsZero())
	{
		bMoved = true;
		ApplyTransform();
		ForceNetUpdate();
	}
}

void AHHHauntProp::OnRep_Moved()
{
	ApplyTransform();
}

void AHHHauntProp::ApplyTransform()
{
	if (bMoved)
	{
		SetActorLocationAndRotation(AltLocation, AltRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (HasActorBegunPlay())
	{
		SetActorTransform(Original, false, nullptr, ETeleportType::TeleportPhysics);
	}
}
