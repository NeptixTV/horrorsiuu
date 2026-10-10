#include "World/HHPracticalLight.h"
#include "Settings/HHGameUserSettings.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

AHHPracticalLight::AHHPracticalLight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Fixture = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fixture"));
	Fixture->SetupAttachment(Root);
	Fixture->SetCollisionProfileName(TEXT("BlockAll"));

	Hum = CreateDefaultSubobject<UAudioComponent>(TEXT("Hum"));
	Hum->SetupAttachment(Root);
	Hum->bAutoActivate = true;
}

AHHPracticalPointLight::AHHPracticalPointLight()
{
	UPointLightComponent* Point = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Point->SetupAttachment(Root);
	Light = Point;
}

AHHPracticalSpotLight::AHHPracticalSpotLight()
{
	USpotLightComponent* Spot = CreateDefaultSubobject<USpotLightComponent>(TEXT("Light"));
	Spot->SetupAttachment(Root);
	Light = Spot;
}

AHHPracticalRectLight::AHHPracticalRectLight()
{
	URectLightComponent* Rect = CreateDefaultSubobject<URectLightComponent>(TEXT("Light"));
	Rect->SetupAttachment(Root);
	Light = Rect;
}

void AHHPracticalLight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHPracticalLight, bOn);
}

void AHHPracticalLight::BeginPlay()
{
	Super::BeginPlay();

	if (Light)
	{
		BaseIntensity = Light->Intensity;
		BaseColor = Light->GetLightColor();
	}
	BaseHumVolume = Hum ? Hum->VolumeMultiplier : 1.f;

	for (int32 Index = 0; Index < Fixture->GetNumMaterials(); ++Index)
	{
		if (UMaterialInstanceDynamic* Dynamic = Fixture->CreateDynamicMaterialInstance(Index))
		{
			EmissiveMaterials.Add(Dynamic);
		}
	}

	if (HasAuthority())
	{
		bOn = bStartOn;
	}
	Time = FMath::FRandRange(0.f, 50.f);
	NextEventTime = Time + FMath::FRandRange(2.f, 12.f);
	ApplyLevel(bOn ? 1.f : 0.f);
	UpdateTickState();
}

void AHHPracticalLight::SetLightOn(bool bNewOn)
{
	if (HasAuthority() && bOn != bNewOn)
	{
		bOn = bNewOn;
		OnRep_On();
		ForceNetUpdate();
	}
}

void AHHPracticalLight::OnRep_On()
{
	if (Hum)
	{
		if (bOn)
		{
			Hum->Play();
		}
		else
		{
			Hum->Stop();
		}
	}
	ApplyLevel(bOn ? 1.f : 0.f);
	UpdateTickState();
}

void AHHPracticalLight::Surge(float Duration)
{
	SurgeEndTime = Time + Duration;
	UpdateTickState();
}

void AHHPracticalLight::UpdateTickState()
{
	const bool bNeedsTick = bOn && (FlickerMode != EHHFlickerMode::Steady || SurgeEndTime > Time);
	SetActorTickEnabled(bNeedsTick);
	if (!bNeedsTick)
	{
		ApplyLevel(bOn ? 1.f : 0.f);
	}
}

void AHHPracticalLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	float Target = ComputeFlicker(DeltaSeconds);

	// Accessibility: no hard strobing. Depth is reduced and changes are smoothed.
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (Settings && Settings->bReduceFlicker)
	{
		Target = FMath::Lerp(1.f, Target, 0.25f);
		SmoothedLevel = FMath::FInterpTo(SmoothedLevel, Target, DeltaSeconds, 2.5f);
	}
	else
	{
		SmoothedLevel = Target;
	}
	ApplyLevel(SmoothedLevel);

	if (SurgeEndTime > 0.f && Time > SurgeEndTime && FlickerMode == EHHFlickerMode::Steady)
	{
		SurgeEndTime = 0.f;
		UpdateTickState();
	}
}

float AHHPracticalLight::ComputeFlicker(float DeltaSeconds)
{
	const float Strength = FMath::Clamp(FlickerStrength, 0.f, 1.f);
	const bool bSurging = Time < SurgeEndTime;

	// Hard stutter: a square wave of random on/off slices.
	auto Stutter = [this, DeltaSeconds](float Rate) -> float
	{
		StutterPhase += DeltaSeconds * Rate;
		if (StutterPhase > 1.f)
		{
			StutterPhase = FMath::Fmod(StutterPhase, 1.f);
			Level = FMath::FRand() < 0.55f ? FMath::FRandRange(0.f, 0.15f) : FMath::FRandRange(0.7f, 1.f);
		}
		return Level;
	};

	if (bSurging)
	{
		return Stutter(18.f);
	}

	switch (FlickerMode)
	{
	case EHHFlickerMode::Flicker:
	{
		if (Time >= NextEventTime)
		{
			EventEndTime = Time + FMath::FRandRange(0.08f, 0.6f);
			NextEventTime = Time + FMath::FRandRange(3.f, 14.f) / FMath::Max(Strength, 0.2f);
		}
		if (Time < EventEndTime)
		{
			return FMath::Lerp(1.f, Stutter(22.f), Strength);
		}
		return 1.f - 0.04f * Strength * (0.5f + 0.5f * FMath::Sin(Time * 37.f));
	}
	case EHHFlickerMode::Buzz:
	{
		if (Time >= NextEventTime)
		{
			EventEndTime = Time + FMath::FRandRange(0.25f, 1.1f);
			NextEventTime = Time + FMath::FRandRange(8.f, 26.f) / FMath::Max(Strength, 0.2f);
		}
		if (Time < EventEndTime)
		{
			return FMath::Lerp(1.f, Stutter(14.f), Strength);
		}
		// Mains ripple, barely visible.
		return 0.985f + 0.015f * FMath::Sin(Time * 2.f * UE_PI * 50.f);
	}
	case EHHFlickerMode::Dying:
	{
		if (Time >= NextEventTime)
		{
			EventEndTime = Time + FMath::FRandRange(0.2f, 1.5f);
			NextEventTime = Time + FMath::FRandRange(0.6f, 4.f);
		}
		const float Base = 0.55f + 0.15f * FMath::PerlinNoise1D(Time * 1.7f);
		if (Time < EventEndTime)
		{
			return Base * FMath::Lerp(1.f, Stutter(26.f), Strength);
		}
		return Base;
	}
	case EHHFlickerMode::Television:
	{
		if (Time >= TvNextChange)
		{
			TvTarget = FMath::FRandRange(0.45f, 1.15f);
			TvNextChange = Time + FMath::FRandRange(0.05f, 0.35f);
			if (Light)
			{
				// Cuts between cooler and warmer shots.
				const FLinearColor Shot = FMath::Lerp(FLinearColor(0.65f, 0.75f, 1.f), FLinearColor(1.f, 0.9f, 0.8f), FMath::FRand());
				Light->SetLightColor(BaseColor * Shot);
			}
		}
		Level = FMath::FInterpTo(Level, TvTarget, DeltaSeconds, 18.f);
		return FMath::Lerp(1.f, Level, Strength);
	}
	case EHHFlickerMode::Ember:
	{
		const float Noise = FMath::PerlinNoise1D(Time * 3.1f) * 0.6f + FMath::PerlinNoise1D(Time * 9.7f) * 0.4f;
		return 1.f + Noise * 0.18f * Strength;
	}
	default:
		return 1.f;
	}
}

void AHHPracticalLight::ApplyLevel(float InLevel)
{
	const float Clamped = FMath::Max(0.f, InLevel);
	if (Light)
	{
		Light->SetIntensity(BaseIntensity * Clamped);
		Light->SetVisibility(Clamped > 0.001f);
	}
	for (UMaterialInstanceDynamic* Dynamic : EmissiveMaterials)
	{
		if (Dynamic)
		{
			Dynamic->SetScalarParameterValue(EmissiveParameter, EmissiveStrength * Clamped);
		}
	}
	if (Hum)
	{
		Hum->SetVolumeMultiplier(BaseHumVolume * FMath::Clamp(Clamped, 0.f, 1.2f));
	}
}
