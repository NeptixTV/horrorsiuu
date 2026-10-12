#include "World/HHTimeOfDay.h"
#include "World/HHPracticalLight.h"
#include "World/HHAmbientEmitter.h"
#include "Core/HHDeveloperSettings.h"
#include "Core/HHGameInstance.h"
#include "Settings/HHGameUserSettings.h"
#include "HorrorHeist.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace HHTime
{
	static float Wrap24(float Hours)
	{
		return FMath::Fmod(FMath::Fmod(Hours, 24.f) + 24.f, 24.f);
	}

	/** True when Hours lies in the window [From, To) that may wrap past midnight. */
	static bool InWindow(float Hours, float From, float To)
	{
		Hours = Wrap24(Hours);
		From = Wrap24(From);
		To = Wrap24(To);
		return From <= To ? (Hours >= From && Hours < To) : (Hours >= From || Hours < To);
	}

	static float SmoothStep01(float Edge0, float Edge1, float X)
	{
		const float T = FMath::Clamp((X - Edge0) / FMath::Max(Edge1 - Edge0, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
}

AHHTimeOfDay::AHHTimeOfDay()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->Mobility = EComponentMobility::Movable;
	Sun->Intensity = 30.f;
	Sun->bUseTemperature = true;
	Sun->Temperature = 5600.f;
	Sun->SetAtmosphereSunLight(true);
	Sun->SetAtmosphereSunLightIndex(0);
	Sun->VolumetricScatteringIntensity = 0.6f;

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->Mobility = EComponentMobility::Movable;
	Moon->Intensity = 0.3f;
	Moon->LightColor = FColor(170, 190, 255);
	Moon->SetAtmosphereSunLight(true);
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->VolumetricScatteringIntensity = 1.2f;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->Mobility = EComponentMobility::Movable;
	SkyLight->bRealTimeCapture = true;
	SkyLight->Intensity = 1.f;
}

AHHTimeOfDay* AHHTimeOfDay::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AHHTimeOfDay> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AHHTimeOfDay::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHTimeOfDay, AnchorHours);
	DOREPLIFETIME(AHHTimeOfDay, AnchorServerSeconds);
	DOREPLIFETIME(AHHTimeOfDay, HoursPerSecond);
}

void AHHTimeOfDay::BeginPlay()
{
	Super::BeginPlay();

	// A hazy, desaturated sky fits the rainy town better than a postcard blue.
	Atmosphere->SetRayleighScatteringScale(0.019f);
	Atmosphere->SetMieScatteringScale(0.022f);

	if (HasAuthority())
	{
		const UHHDeveloperSettings* Settings = UHHDeveloperSettings::Get();
		float Hours = StartHour >= 0.f ? StartHour : Settings->StartHour;
		if (const UHHGameInstance* GameInstance = GetGameInstance<UHHGameInstance>())
		{
			if (GameInstance->GetCarriedTimeOfDay() >= 0.f)
			{
				Hours = GameInstance->GetCarriedTimeOfDay();
			}
		}
		if (bNightMap && !HHTime::InWindow(Hours, Settings->NightStartHour, Settings->NightEndHour))
		{
			Hours = StartHour >= 0.f ? StartHour : Settings->NightStartHour + 2.5f;
		}
		if (const UHHGameUserSettings* UserSettings = UHHGameUserSettings::Get())
		{
			SpeedSetting = FMath::Clamp(UserSettings->TimeSpeed, 0.f, 8.f);
		}
		Reanchor(Hours);
		SettingsHandle = UHHGameUserSettings::OnSettingsApplied().AddUObject(this, &AHHTimeOfDay::HandleSettingsApplied);
	}

	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		Fog = It->GetComponent();
		break;
	}

	GatherWorldActors();
	UpdateVisuals(true);
}

void AHHTimeOfDay::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UHHGameUserSettings::OnSettingsApplied().Remove(SettingsHandle);
	if (HasAuthority())
	{
		// Carry the clock into the next map (hub -> job -> hub).
		if (UHHGameInstance* GameInstance = GetGameInstance<UHHGameInstance>())
		{
			if (GameInstance->GetCarriedTimeOfDay() < 0.f || EndPlayReason == EEndPlayReason::LevelTransition)
			{
				GameInstance->SetCarriedTimeOfDay(GetTimeOfDay());
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AHHTimeOfDay::HandleSettingsApplied()
{
	const UHHGameUserSettings* UserSettings = UHHGameUserSettings::Get();
	if (UserSettings && HasAuthority())
	{
		SetSpeed(UserSettings->TimeSpeed);
	}
}

float AHHTimeOfDay::GetTimeOfDay() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	const double Elapsed = FMath::Max(0.0, Now - AnchorServerSeconds);
	return HHTime::Wrap24(AnchorHours + static_cast<float>(Elapsed * HoursPerSecond));
}

bool AHHTimeOfDay::IsNight() const
{
	const UHHDeveloperSettings* Settings = UHHDeveloperSettings::Get();
	return HHTime::InWindow(GetTimeOfDay(), Settings->NightStartHour, Settings->NightEndHour);
}

void AHHTimeOfDay::Reanchor(float Hours)
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	AnchorHours = HHTime::Wrap24(Hours);
	AnchorServerSeconds = GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	RefreshRate();
	ForceNetUpdate();
}

void AHHTimeOfDay::RefreshRate()
{
	const float DayMinutes = FMath::Max(UHHDeveloperSettings::Get()->DayLengthMinutes, 1.f);
	HoursPerSecond = 24.f / (DayMinutes * 60.f) * FMath::Max(MapSpeed, 0.f) * FMath::Max(SpeedSetting, 0.f);
}

void AHHTimeOfDay::SkipTo(float Hours)
{
	if (!HasAuthority())
	{
		return;
	}
	Reanchor(Hours);
	UpdateVisuals(true);
	UE_LOG(LogHorrorHeist, Log, TEXT("Time of day set to %.2f h"), AnchorHours);
}

void AHHTimeOfDay::SetSpeed(float Multiplier)
{
	if (!HasAuthority())
	{
		return;
	}
	const float NewSpeed = FMath::Clamp(Multiplier, 0.f, 8.f);
	if (!FMath::IsNearlyEqual(NewSpeed, SpeedSetting))
	{
		// Keep the current time, change only the rate from now on.
		const float Current = GetTimeOfDay();
		SpeedSetting = NewSpeed;
		Reanchor(Current);
	}
}

void AHHTimeOfDay::OnRep_Anchor()
{
	UpdateVisuals(true);
}

void AHHTimeOfDay::GatherWorldActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	StreetLights.Reset();
	Windows.Reset();
	DayEmitters.Reset();
	NightEmitters.Reset();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->Tags.Num() == 0)
		{
			continue;
		}
		if (Actor->ActorHasTag(StreetLightTag))
		{
			if (AHHPracticalLight* Lamp = Cast<AHHPracticalLight>(Actor))
			{
				StreetLights.Add(Lamp);
			}
		}
		if (Actor->ActorHasTag(WindowLightTag))
		{
			Windows.Add(Actor);
		}
		if (AHHAmbientEmitter* Emitter = Cast<AHHAmbientEmitter>(Actor))
		{
			if (Actor->ActorHasTag(DayOnlyTag))
			{
				DayEmitters.Add(Emitter);
			}
			else if (Actor->ActorHasTag(NightOnlyTag))
			{
				NightEmitters.Add(Emitter);
			}
		}
	}
}

float AHHTimeOfDay::HashUnit(const AActor* Actor, uint32 Salt)
{
	const uint32 Hash = HashCombine(GetTypeHash(Actor ? Actor->GetFName() : NAME_None), Salt);
	return static_cast<float>(Hash % 10007u) / 10007.f;
}

void AHHTimeOfDay::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Streaming sub-levels (the town) may finish loading after BeginPlay: look again a few times.
	if (GatherPasses < 3)
	{
		GatherTimer += DeltaSeconds;
		if (GatherTimer > 1.5f * (GatherPasses + 1))
		{
			++GatherPasses;
			GatherWorldActors();
			UpdateSwitches(GetTimeOfDay(), true);
		}
	}

	UpdateVisuals(false);
}

void AHHTimeOfDay::UpdateVisuals(bool bSnap)
{
	const float Hours = GetTimeOfDay();

	// Simple solar model: hour angle, latitude and declination -> elevation / azimuth.
	const float LatRad = FMath::DegreesToRadians(Latitude);
	const float DecRad = FMath::DegreesToRadians(Declination);
	const float HourAngle = FMath::DegreesToRadians((Hours - 12.f) * 15.f);
	const float SinElevation = FMath::Sin(LatRad) * FMath::Sin(DecRad) + FMath::Cos(LatRad) * FMath::Cos(DecRad) * FMath::Cos(HourAngle);
	const float Elevation = FMath::Asin(FMath::Clamp(SinElevation, -1.f, 1.f));
	const float CosAzimuth = (FMath::Sin(DecRad) - FMath::Sin(Elevation) * FMath::Sin(LatRad)) / FMath::Max(FMath::Cos(Elevation) * FMath::Cos(LatRad), 1e-4f);
	float Azimuth = FMath::Acos(FMath::Clamp(CosAzimuth, -1.f, 1.f));
	if (HourAngle > 0.f)
	{
		Azimuth = 2.f * UE_PI - Azimuth;
	}
	const float ElevationDeg = FMath::RadiansToDegrees(Elevation);
	const float AzimuthDeg = FMath::RadiansToDegrees(Azimuth) + NorthYaw;

	// Direction towards the sun; the light travels the opposite way.
	const FVector ToSun = FRotator(ElevationDeg, AzimuthDeg, 0.f).Vector();
	Sun->SetWorldRotation((-ToSun).Rotation());

	// The moon rides opposite the sun, never lower than 25 degrees while it is up.
	const FVector ToMoon = FRotator(FMath::Max(-ElevationDeg, 25.f), AzimuthDeg + 160.f, 0.f).Vector();
	Moon->SetWorldRotation((-ToMoon).Rotation());

	Daylight = HHTime::SmoothStep01(-7.f, 9.f, ElevationDeg);
	const float SunUp = HHTime::SmoothStep01(-1.5f, 8.f, ElevationDeg);
	const float Night = 1.f - HHTime::SmoothStep01(-10.f, 2.f, ElevationDeg);
	const float Golden = 1.f - HHTime::SmoothStep01(4.f, 22.f, ElevationDeg);

	Sun->SetIntensity(SunLux * SunUp);
	Sun->SetTemperature(FMath::Lerp(6200.f, 2600.f, Golden));
	if (Sun->IsVisible() != (SunUp > 0.001f))
	{
		Sun->SetVisibility(SunUp > 0.001f);
	}
	Moon->SetIntensity(MoonLux * Night);
	if (Moon->IsVisible() != (Night > 0.001f))
	{
		Moon->SetVisibility(Night > 0.001f);
	}

	if (UExponentialHeightFogComponent* FogComponent = Fog.Get())
	{
		const FLinearColor DayColor = FMath::Lerp(DayFogColor, DuskFogColor, Golden * SunUp);
		FogComponent->SetFogInscatteringColor(FMath::Lerp(NightFogColor, DayColor, Daylight));
		FogComponent->SetFogDensity(FMath::Lerp(NightFogDensity, DayFogDensity, Daylight));
	}

	UpdateSwitches(Hours, bSnap);
}

void AHHTimeOfDay::UpdateSwitches(float Hours, bool bSnap)
{
	const bool bServer = HasAuthority();

	// Street lamps: replicated, switched by the server.
	if (bServer)
	{
		for (const TWeakObjectPtr<AHHPracticalLight>& LampPtr : StreetLights)
		{
			if (AHHPracticalLight* Lamp = LampPtr.Get())
			{
				const float Threshold = LampsOnBelowDaylight + (HashUnit(Lamp, 17u) - 0.5f) * 0.18f;
				const bool bShouldBeOn = Daylight < Threshold;
				if (Lamp->IsLightOn() != bShouldBeOn)
				{
					Lamp->SetLightOn(bShouldBeOn);
				}
			}
		}
	}

	// Windows: each house light has its own evening; some stay on all night. Local only.
	for (const TWeakObjectPtr<AActor>& WindowPtr : Windows)
	{
		AActor* Window = WindowPtr.Get();
		if (!Window)
		{
			continue;
		}
		bool bDarkHouse = false;
		for (const FName& Tag : DarkHouseTags)
		{
			bDarkHouse |= Window->ActorHasTag(Tag);
		}
		const float On = 17.6f + HashUnit(Window, 3u) * 2.6f;
		const float Off = HashUnit(Window, 5u) < 0.2f ? 7.0f : 22.2f + HashUnit(Window, 7u) * 3.4f;
		const bool bLit = !bDarkHouse && Daylight < 0.85f && HHTime::InWindow(Hours, On, Off);
		if (Window->IsHidden() == bLit)
		{
			Window->SetActorHiddenInGame(!bLit);
		}
	}

	// Day / night ambience beds.
	const bool bDayAmbience = Daylight > 0.35f;
	for (const TWeakObjectPtr<AHHAmbientEmitter>& EmitterPtr : DayEmitters)
	{
		if (AHHAmbientEmitter* Emitter = EmitterPtr.Get())
		{
			Emitter->SetEmitterActive(bDayAmbience);
		}
	}
	for (const TWeakObjectPtr<AHHAmbientEmitter>& EmitterPtr : NightEmitters)
	{
		if (AHHAmbientEmitter* Emitter = EmitterPtr.Get())
		{
			Emitter->SetEmitterActive(!bDayAmbience);
		}
	}
}
