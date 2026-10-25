// SIGNAL UNKNOWN – replicated world state: team money, time of day, weather, story progress.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/SUTypes.h"
#include "SUGameState.generated.h"

class ASUWorldBuilder;
class ASUMissionManager;
class ASUVehicle;

DECLARE_MULTICAST_DELEGATE(FSUWorldReadySignature);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSUMoneyChangedSignature, int32 /*NewCents*/, int32 /*DeltaCents*/);

/** Day time is replicated as a base value plus the server time when it was set. */
USTRUCT()
struct FSUTimeSync
{
	GENERATED_BODY()

	UPROPERTY() int32 BaseDay = 1;
	UPROPERTY() float BaseHours = 17.6f;
	UPROPERTY() double BaseServerTime = 0.0;
	/** Game hours per real second. */
	UPROPERTY() float Rate = 0.1f / 60.f;
};

USTRUCT()
struct FSUWeatherState
{
	GENERATED_BODY()

	UPROPERTY() float Rain = 0.f;
	UPROPERTY() float Cloud = 0.3f;
	UPROPERTY() float Fog = 0.3f;
	UPROPERTY() float Wind = 0.35f;
	UPROPERTY() float PrevRain = 0.f;
	UPROPERTY() float PrevCloud = 0.3f;
	UPROPERTY() float PrevFog = 0.3f;
	UPROPERTY() float PrevWind = 0.35f;
	UPROPERTY() double ChangeServerTime = 0.0;
	UPROPERTY() float BlendSeconds = 90.f;
};

UCLASS()
class SIGNALUNKNOWN_API ASUGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ASUGameState();

	/** The procedural world is generated here – before any player logs in or BeginPlay runs. */
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static ASUGameState* Get(const UObject* WorldContext);

	// ---------------------------------------------------------------- world generation
	/** The local procedural world (exists on every machine). */
	ASUWorldBuilder* GetWorldBuilder() const { return WorldBuilder; }
	bool IsWorldReady() const { return bWorldReady; }
	FSUWorldReadySignature OnWorldReady;

	// ---------------------------------------------------------------- money (team account)
	int32 GetMoneyCents() const { return MoneyCents; }
	const TArray<FSUTransaction>& GetTransactions() const { return Transactions; }
	/** Server only. */
	void AddMoney(int32 Cents, const FText& Label);
	/** Server only. Returns false (and books nothing) if the account would go negative. */
	bool TrySpend(int32 Cents, const FText& Label);
	void SetMoneyDirect(int32 Cents) { MoneyCents = Cents; }
	FSUMoneyChangedSignature OnMoneyChanged;

	// ---------------------------------------------------------------- time
	float GetTimeOfDay() const;
	int32 GetDay() const;
	/** Server only. */
	void SetTimeOfDay(int32 Day, float Hours);
	/** Server only: skip forward (sleeping). */
	void AdvanceTime(float Hours);
	/** Sun elevation in degrees for the current time. */
	float GetSunElevation() const;
	/** 0 = bright day, 1 = night. Includes weather. */
	float GetDarkness() const;
	static float SunElevationForHours(float Hours);

	// ---------------------------------------------------------------- weather
	float GetRain() const;
	float GetCloud() const;
	float GetFog() const;
	float GetWind() const;
	/** Server only. */
	void SetWeather(float Rain, float Cloud, float Fog, float Wind, float BlendSeconds);
	/** Short text for the weather radio. */
	FText DescribeWeather(bool bForecast) const;

	// ---------------------------------------------------------------- story / world flags
	int32 GetStoryStage() const { return StoryStage; }
	void SetStoryStage(int32 Stage);
	bool HasWorldFlag(FName Flag) const { return WorldFlags.Contains(Flag); }
	void SetWorldFlag(FName Flag);
	const TArray<FName>& GetWorldFlags() const { return WorldFlags; }
	void SetWorldFlags(const TArray<FName>& Flags) { WorldFlags = Flags; }

	int32 GetUnpaidFuelIncidents() const { return UnpaidFuelIncidents; }
	void AddUnpaidFuelIncident() { ++UnpaidFuelIncidents; }
	void SetUnpaidFuelIncidents(int32 Count) { UnpaidFuelIncidents = Count; }

	// ---------------------------------------------------------------- references (server sets)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Signal Unknown")
	TObjectPtr<ASUMissionManager> MissionManager;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Signal Unknown")
	TObjectPtr<ASUVehicle> MainVehicle;

	/** Lightning flash + delayed thunder on all machines. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastLightning(float Strength, float ThunderDelay);

private:
	UPROPERTY(ReplicatedUsing = OnRep_Money)
	int32 MoneyCents = 0;

	UPROPERTY(Replicated)
	TArray<FSUTransaction> Transactions;

	UPROPERTY(Replicated)
	FSUTimeSync TimeSync;

	UPROPERTY(Replicated)
	FSUWeatherState Weather;

	UPROPERTY(Replicated)
	int32 StoryStage = 0;

	UPROPERTY(Replicated)
	TArray<FName> WorldFlags;

	UPROPERTY(Replicated)
	int32 UnpaidFuelIncidents = 0;

	UFUNCTION()
	void OnRep_Money(int32 OldValue);

	UPROPERTY(Transient)
	TObjectPtr<ASUWorldBuilder> WorldBuilder;

	bool bWorldReady = false;
	double NextWeatherChange = 0.0;
	double NextLightning = 0.0;

	void BuildLocalWorld();
	void UpdateWeatherServer();
	double ServerNow() const;
	static float Smooth(float Prev, float Target, float Alpha);
	float WeatherAlpha() const;
};
