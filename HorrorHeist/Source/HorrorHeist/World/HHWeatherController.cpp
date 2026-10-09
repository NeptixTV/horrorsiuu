#include "World/HHWeatherController.h"
#include "Audio/HHAudioSubsystem.h"
#include "Settings/HHGameUserSettings.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "HHWeather"

AHHWeatherController::AHHWeatherController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(SceneRoot);
	Flash->IntensityUnits = ELightUnits::Candelas;
	Flash->Intensity = 0.f;
	Flash->AttenuationRadius = 3000.f;
	Flash->LightColor = FColor(200, 215, 255);
	Flash->SourceRadius = 200.f;
	Flash->CastShadows = true;
	Flash->VolumetricScatteringIntensity = 2.f;
	Flash->SetVisibility(false);

	ThunderCaption = LOCTEXT("Thunder", "[Thunder rolls]");
}

void AHHWeatherController::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		ScheduleStrike();
	}
}

void AHHWeatherController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StrikeTimer);
	GetWorldTimerManager().ClearTimer(ThunderTimer);
	Super::EndPlay(EndPlayReason);
}

void AHHWeatherController::ScheduleStrike()
{
	GetWorldTimerManager().SetTimer(StrikeTimer, this, &AHHWeatherController::Strike, FMath::FRandRange(MinInterval, MaxInterval), false);
}

void AHHWeatherController::Strike()
{
	if (HasAuthority())
	{
		MulticastLightning(FMath::Rand());
		ScheduleStrike();
	}
}

void AHHWeatherController::MulticastLightning_Implementation(int32 Seed)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Stream.Initialize(Seed);

	// 1-3 pulses within ~0.6s.
	Pulses.Reset();
	const int32 Count = Stream.RandRange(1, 3);
	float Start = 0.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Duration = Stream.FRandRange(0.05f, 0.14f);
		Pulses.Add(FVector2D(Start, Duration));
		Start += Duration + Stream.FRandRange(0.05f, 0.22f);
	}
	FlashLength = Start + 0.4f;
	FlashTime = 0.f;
	Flash->SetVisibility(true);
	SetActorTickEnabled(true);

	// Sound travels ~340 m/s: delay = "distance".
	const float Distance = Stream.FRandRange(0.3f, 4.5f);
	PendingThunderVolume = FMath::Lerp(1.f, 0.55f, FMath::Clamp((Distance - 0.3f) / 4.2f, 0.f, 1.f));
	PendingThunder = ThunderSounds.Num() > 0 ? Stream.RandRange(0, ThunderSounds.Num() - 1) : INDEX_NONE;
	GetWorldTimerManager().SetTimer(ThunderTimer, this, &AHHWeatherController::PlayThunder, Distance, false);
}

void AHHWeatherController::PlayThunder()
{
	if (ThunderSounds.IsValidIndex(PendingThunder) && ThunderSounds[PendingThunder])
	{
		UGameplayStatics::PlaySound2D(this, ThunderSounds[PendingThunder], PendingThunderVolume);
		if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
		{
			Audio->ShowCaption(ThunderCaption, 3.f, true);
		}
	}
}

void AHHWeatherController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FlashTime += DeltaSeconds;

	float Level = 0.f;
	for (const FVector2D& Pulse : Pulses)
	{
		const float Local = FlashTime - Pulse.X;
		if (Local >= 0.f && Local <= Pulse.Y + 0.25f)
		{
			// Sharp attack, quick exponential tail.
			const float Value = Local <= Pulse.Y ? 1.f : FMath::Exp(-(Local - Pulse.Y) * 14.f);
			Level = FMath::Max(Level, Value);
		}
	}

	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (Settings && Settings->bReduceFlicker)
	{
		// One soft swell instead of strobing pulses.
		const float Alpha = FMath::Clamp(FlashTime / FMath::Max(FlashLength, 0.1f), 0.f, 1.f);
		Level = FMath::Sin(Alpha * PI) * 0.35f;
	}

	Flash->SetIntensity(FlashIntensity * Level);

	if (FlashTime > FlashLength)
	{
		Flash->SetIntensity(0.f);
		Flash->SetVisibility(false);
		SetActorTickEnabled(false);
	}
}

#undef LOCTEXT_NAMESPACE
