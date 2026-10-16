#include "World/HHTrafficLight.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

AHHTrafficLight::AHHTrafficLight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = false;

	Housing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Housing"));
	SetRootComponent(Housing);
	RedLamp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RedLamp"));
	AmberLamp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AmberLamp"));
	GreenLamp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GreenLamp"));
	for (UStaticMeshComponent* Lamp : { RedLamp.Get(), AmberLamp.Get(), GreenLamp.Get() })
	{
		Lamp->SetupAttachment(Housing);
		Lamp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Lamp->SetCastShadow(false);
	}
}

EHHSignal AHHTrafficLight::GetSignal(int32 SignalAxis, float ServerTime)
{
	// 0-14 axis 0 green, 14-17 amber, 17-18 all red, 18-31 axis 1 green, 31-34 amber (the next cycle
	// starts after the axis-1 amber with a short all-red: folded into the first second).
	const float T = FMath::Fmod(FMath::Max(0.f, ServerTime), CycleSeconds);
	const bool bAxis0 = SignalAxis == 0;
	if (T < 1.f)
	{
		return EHHSignal::Red;
	}
	if (T < 14.f)
	{
		return bAxis0 ? EHHSignal::Green : EHHSignal::Red;
	}
	if (T < 17.f)
	{
		return bAxis0 ? EHHSignal::Amber : EHHSignal::Red;
	}
	if (T < 18.f)
	{
		return EHHSignal::Red;
	}
	if (T < 31.f)
	{
		return bAxis0 ? EHHSignal::Red : EHHSignal::Green;
	}
	return bAxis0 ? EHHSignal::Red : EHHSignal::Amber;
}

void AHHTrafficLight::BeginPlay()
{
	Super::BeginPlay();
	bFirst = true;
}

void AHHTrafficLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const float Now = GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : GetWorld()->GetTimeSeconds();
	const EHHSignal Signal = GetSignal(Axis, Now);
	if (Signal == Shown && !bFirst)
	{
		return;
	}
	bFirst = false;
	Shown = Signal;
	RedLamp->SetVisibility(Signal == EHHSignal::Red);
	AmberLamp->SetVisibility(Signal == EHHSignal::Amber);
	GreenLamp->SetVisibility(Signal == EHHSignal::Green);
}
