#include "Core/SUGameState.h"
#include "SignalUnknown.h"
#include "Core/SUDeveloperSettings.h"
#include "World/SUWorldBuilder.h"
#include "World/SUSkySystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ASUGameState::ASUGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
}

ASUGameState* ASUGameState::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetGameState<ASUGameState>() : nullptr;
}

void ASUGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUGameState, MissionManager);
	DOREPLIFETIME(ASUGameState, MainVehicle);
	DOREPLIFETIME(ASUGameState, MoneyCents);
	DOREPLIFETIME(ASUGameState, Transactions);
	DOREPLIFETIME(ASUGameState, TimeSync);
	DOREPLIFETIME(ASUGameState, Weather);
	DOREPLIFETIME(ASUGameState, StoryStage);
	DOREPLIFETIME(ASUGameState, WorldFlags);
	DOREPLIFETIME(ASUGameState, UnpaidFuelIncidents);
}

void ASUGameState::BeginPlay()
{
	Super::BeginPlay();

	const USUGameplaySettings* Settings = GetDefault<USUGameplaySettings>();
	if (HasAuthority())
	{
		MoneyCents = Settings->StartMoneyCents;
		TimeSync.BaseDay = 1;
		TimeSync.BaseHours = Settings->StartHour;
		TimeSync.BaseServerTime = ServerNow();
		TimeSync.Rate = Settings->GameMinutesPerRealMinute / 3600.f;
		Weather = FSUWeatherState();
		Weather.ChangeServerTime = ServerNow();
		NextWeatherChange = ServerNow() + FMath::FRandRange(600.0, 1100.0);
	}
}

void ASUGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	BuildLocalWorld();
}

void ASUGameState::BuildLocalWorld()
{
	// A world builder placed by hand in a level is reused, otherwise one is spawned.
	for (TActorIterator<ASUWorldBuilder> It(GetWorld()); It; ++It)
	{
		WorldBuilder = *It;
		break;
	}
	if (!WorldBuilder)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		WorldBuilder = GetWorld()->SpawnActor<ASUWorldBuilder>(ASUWorldBuilder::StaticClass(), FTransform::Identity, Params);
	}
	if (!WorldBuilder)
	{
		UE_LOG(LogSignalUnknown, Error, TEXT("Could not create the world builder."));
		return;
	}
	const double Start = FPlatformTime::Seconds();
	WorldBuilder->BuildWorld(GetDefault<USUGameplaySettings>()->WorldSeed);
	UE_LOG(LogSignalUnknown, Log, TEXT("World generated in %.2f s"), FPlatformTime::Seconds() - Start);
	if (HasAuthority())
	{
		MainVehicle = WorldBuilder->GetMainVehicle();
	}
	bWorldReady = true;
	OnWorldReady.Broadcast();
}

double ASUGameState::ServerNow() const
{
	return GetServerWorldTimeSeconds();
}

void ASUGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		UpdateWeatherServer();
	}
}

// ------------------------------------------------------------------------------------ money

void ASUGameState::AddMoney(int32 Cents, const FText& Label)
{
	if (!HasAuthority() || Cents == 0)
	{
		return;
	}
	const int32 Old = MoneyCents;
	MoneyCents += Cents;
	FSUTransaction T;
	T.Label = Label;
	T.AmountCents = Cents;
	T.DayTimeHours = GetTimeOfDay();
	Transactions.Insert(T, 0);
	if (Transactions.Num() > 20)
	{
		Transactions.SetNum(20);
	}
	OnRep_Money(Old);
}

bool ASUGameState::TrySpend(int32 Cents, const FText& Label)
{
	if (!HasAuthority() || Cents < 0 || MoneyCents < Cents)
	{
		return false;
	}
	AddMoney(-Cents, Label);
	return true;
}

void ASUGameState::OnRep_Money(int32 OldValue)
{
	OnMoneyChanged.Broadcast(MoneyCents, MoneyCents - OldValue);
}

// ------------------------------------------------------------------------------------ time

float ASUGameState::GetTimeOfDay() const
{
	const double Elapsed = FMath::Max(0.0, ServerNow() - TimeSync.BaseServerTime);
	const double Hours = TimeSync.BaseHours + Elapsed * TimeSync.Rate;
	return static_cast<float>(FMath::Fmod(Hours, 24.0));
}

int32 ASUGameState::GetDay() const
{
	const double Elapsed = FMath::Max(0.0, ServerNow() - TimeSync.BaseServerTime);
	const double Hours = TimeSync.BaseHours + Elapsed * TimeSync.Rate;
	return TimeSync.BaseDay + static_cast<int32>(Hours / 24.0);
}

void ASUGameState::SetTimeOfDay(int32 Day, float Hours)
{
	if (!HasAuthority())
	{
		return;
	}
	TimeSync.BaseDay = FMath::Max(1, Day);
	TimeSync.BaseHours = FMath::Fmod(FMath::Max(0.f, Hours), 24.f);
	TimeSync.BaseServerTime = ServerNow();
}

void ASUGameState::AdvanceTime(float Hours)
{
	const float Now = GetTimeOfDay();
	const int32 Day = GetDay();
	const float Target = Now + FMath::Max(0.f, Hours);
	SetTimeOfDay(Day + static_cast<int32>(Target / 24.f), FMath::Fmod(Target, 24.f));
}

float ASUGameState::SunElevationForHours(float Hours)
{
	// sunrise ~05:50, sunset ~20:40, highest point 56 degrees at 13:15 (northern summer)
	const float DayLength = 14.83f;
	const float Rise = 5.83f;
	const float T = (Hours - Rise) / DayLength;
	if (T >= 0.f && T <= 1.f)
	{
		return 56.f * FMath::Sin(T * UE_PI);
	}
	// below the horizon: smooth curve down to -30 degrees at midnight
	const float NightLength = 24.f - DayLength;
	float Tn = (Hours - (Rise + DayLength)) / NightLength;
	if (Tn < 0.f)
	{
		Tn += 24.f / NightLength;
	}
	return -30.f * FMath::Sin(FMath::Clamp(Tn, 0.f, 1.f) * UE_PI);
}

float ASUGameState::GetSunElevation() const
{
	return SunElevationForHours(GetTimeOfDay());
}

float ASUGameState::GetDarkness() const
{
	const float Elev = GetSunElevation();
	float Dark = 1.f - FMath::SmoothStep(-9.f, 7.f, Elev);
	Dark += (1.f - Dark) * (GetCloud() * 0.12f + GetRain() * 0.2f);
	return FMath::Clamp(Dark, 0.f, 1.f);
}

// ------------------------------------------------------------------------------------ weather

float ASUGameState::Smooth(float Prev, float Target, float Alpha)
{
	return FMath::Lerp(Prev, Target, FMath::SmoothStep(0.f, 1.f, Alpha));
}

float ASUGameState::WeatherAlpha() const
{
	const double Elapsed = ServerNow() - Weather.ChangeServerTime;
	return FMath::Clamp(static_cast<float>(Elapsed / FMath::Max(1.f, Weather.BlendSeconds)), 0.f, 1.f);
}

float ASUGameState::GetRain() const { return Smooth(Weather.PrevRain, Weather.Rain, WeatherAlpha()); }
float ASUGameState::GetCloud() const { return Smooth(Weather.PrevCloud, Weather.Cloud, WeatherAlpha()); }
float ASUGameState::GetFog() const { return Smooth(Weather.PrevFog, Weather.Fog, WeatherAlpha()); }
float ASUGameState::GetWind() const { return Smooth(Weather.PrevWind, Weather.Wind, WeatherAlpha()); }

void ASUGameState::SetWeather(float Rain, float Cloud, float Fog, float Wind, float BlendSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	Weather.PrevRain = GetRain();
	Weather.PrevCloud = GetCloud();
	Weather.PrevFog = GetFog();
	Weather.PrevWind = GetWind();
	Weather.Rain = FMath::Clamp(Rain, 0.f, 1.f);
	Weather.Cloud = FMath::Clamp(Cloud, 0.f, 1.f);
	Weather.Fog = FMath::Clamp(Fog, 0.f, 1.f);
	Weather.Wind = FMath::Clamp(Wind, 0.f, 1.f);
	Weather.BlendSeconds = FMath::Max(1.f, BlendSeconds);
	Weather.ChangeServerTime = ServerNow();
}

void ASUGameState::UpdateWeatherServer()
{
	const double Now = ServerNow();
	if (Now >= NextWeatherChange)
	{
		// simple weather cycle: mostly clear/cloudy, sometimes fog, occasionally a storm
		const float Roll = FMath::FRand();
		if (Roll < 0.35f)
		{
			SetWeather(0.f, FMath::FRandRange(0.05f, 0.35f), FMath::FRandRange(0.15f, 0.35f), FMath::FRandRange(0.2f, 0.4f), 120.f);
		}
		else if (Roll < 0.6f)
		{
			SetWeather(0.f, FMath::FRandRange(0.5f, 0.85f), FMath::FRandRange(0.3f, 0.5f), FMath::FRandRange(0.3f, 0.6f), 150.f);
		}
		else if (Roll < 0.78f)
		{
			SetWeather(0.f, 0.6f, FMath::FRandRange(0.75f, 1.f), 0.15f, 180.f); // fog bank
		}
		else if (Roll < 0.92f)
		{
			SetWeather(FMath::FRandRange(0.35f, 0.6f), 0.9f, 0.5f, 0.5f, 150.f); // rain
		}
		else
		{
			SetWeather(FMath::FRandRange(0.8f, 1.f), 1.f, 0.55f, 0.85f, 150.f); // thunderstorm
		}
		NextWeatherChange = Now + FMath::FRandRange(720.0, 1500.0);
	}

	if (GetRain() > 0.7f && Now >= NextLightning)
	{
		NextLightning = Now + FMath::FRandRange(14.0, 45.0);
		MulticastLightning(FMath::FRandRange(0.5f, 1.f), FMath::FRandRange(0.6f, 4.f));
	}
}

void ASUGameState::MulticastLightning_Implementation(float Strength, float ThunderDelay)
{
	if (WorldBuilder)
	{
		if (ASUSkySystem* Sky = WorldBuilder->GetSkySystem())
		{
			Sky->TriggerLightning(Strength, ThunderDelay);
		}
	}
}

FText ASUGameState::DescribeWeather(bool bForecast) const
{
	const float Rain = bForecast ? Weather.Rain : GetRain();
	const float Cloud = bForecast ? Weather.Cloud : GetCloud();
	const float Fog = bForecast ? Weather.Fog : GetFog();
	if (Rain > 0.7f)
	{
		return SUText(TEXT("Schwere Gewitter mit Starkregen, Sturmböen im Bergland. Meiden Sie offene Höhenlagen."));
	}
	if (Rain > 0.2f)
	{
		return SUText(TEXT("Regen, Straßen nass und rutschig. Sichtweiten teilweise eingeschränkt."));
	}
	if (Fog > 0.7f)
	{
		return SUText(TEXT("Dichter Nebel in den Tälern, Sichtweite unter hundert Metern."));
	}
	if (Cloud > 0.5f)
	{
		return SUText(TEXT("Stark bewölkt, trocken. Leichter Wind aus West."));
	}
	return SUText(TEXT("Klar bis leicht bewölkt, trocken. Nachts kühl."));
}

// ------------------------------------------------------------------------------------ story

void ASUGameState::SetStoryStage(int32 Stage)
{
	if (HasAuthority())
	{
		StoryStage = FMath::Max(StoryStage, Stage);
	}
}

void ASUGameState::SetWorldFlag(FName Flag)
{
	if (HasAuthority() && !Flag.IsNone())
	{
		WorldFlags.AddUnique(Flag);
	}
}
