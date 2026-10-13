#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "HHHauntDirector.generated.h"

class USoundBase;
class AHHPracticalLight;
class AHHDoorActor;
class AHHHauntProp;
class AHHHauntDevice;
class AHHApparition;
class AHHPlayerState;
class UHHLootDefinition;

UENUM()
enum class EHHHauntEvent : uint8
{
	Settle,
	DistantSound,
	LightOut,
	DoorOpen,
	ObjectMoved,
	DoorSlam,
	DeviceOn,
	Flicker,
	Whisper,
	GlassFall,
	Apparition,
	Hunt,
	MAX UMETA(Hidden)
};

/**
 * The house's temper. Anger (0..100) grows the longer the crew stays inside, with every piece
 * of loot taken (cursed pieces a lot) and with noise. Anger sets the stage:
 *   0 calm      - the house settles, creaks
 *   1 unease    - a light goes out, a sound from another room, a closed door stands open
 *   2 disturbed - objects moved, doors slam, the TV switches itself on, whispers
 *   3 hostile   - glass breaks, something stands at the end of the hall
 *   4 manifest  - it hunts. Being caught means blacking out and waking up outside.
 * Events are staged, cooldown-limited and never block the job: doors are never locked, loot
 * and exits are never touched, and leaving the house always ends a hunt.
 * Server only; presentation goes out through multicasts and replicated world actors.
 */
UCLASS()
class HORRORHEIST_API AHHHauntDirector : public AActor
{
	GENERATED_BODY()

public:
	AHHHauntDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AHHHauntDirector* Get(const UObject* WorldContextObject);

	// ---- Read by the noise meter (any machine) -----------------------------------------

	/** Where and when (game-state clock) the house last did something. */
	FVector GetLastEventLocation() const { return LastEventLocation; }
	float GetLastEventTime() const { return LastEventTime; }
	/** The lights are going: a hunt starts within seconds. */
	bool IsHuntImminent() const;
	float GetServerTime() const;

	float GetAnger() const { return Anger; }
	int32 GetStage() const { return Stage; }
	int32 GetHighestStage() const { return HighestStage; }

	// ---- Server ------------------------------------------------------------------------

	void SetActive(bool bNewActive);
	void SetAggression(float NewAggression) { Aggression = FMath::Max(0.1f, NewAggression); }
	void AddAnger(float Amount);
	void OnLootTaken(const UHHLootDefinition* Loot, const FVector& Where);
	/** A crew member blacked out: the house is satisfied for a while. */
	void OnPlayerCaught();
	/** Debug / console: run an event now (ignores stage and cooldown). */
	bool TriggerEvent(EHHHauntEvent Event);

	void OnApparitionSeen(APawn* Witness);
	void OnApparitionCaught(APawn* Victim);

	/** Lights of the target house (for the flicker effect). */
	const TArray<TWeakObjectPtr<AHHPracticalLight>>& GetHouseLights() const { return Lights; }

protected:
	void Think();
	void GatherHouse();
	void UpdateStage();
	EHHHauntEvent PickEvent() const;
	bool RunEvent(EHHHauntEvent Event);
	float NextEventDelay() const;

	bool DoSoundInOtherRoom(const TArray<TObjectPtr<USoundBase>>& Sounds, const FText& CaptionText, float MinDistance);
	bool DoLightOut();
	bool DoDoorOpen();
	bool DoObjectMoved();
	bool DoDoorSlam();
	bool DoDeviceOn();
	bool DoFlicker();
	bool DoWhisper();
	bool DoGlassFall();
	bool DoApparition(bool bHunt);

	TArray<APawn*> GetPawnsInside() const;
	bool IsObserved(const FVector& Location, float MaxDistance) const;
	float DistanceToNearestPawn(const FVector& Location) const;
	void HitPsyche(APawn* Pawn, float Amount);
	void HitPsycheNear(const FVector& Location, float Radius, float Amount);
	void PlaySoundEverywhere(USoundBase* Sound, const FVector& Location, const FText& CaptionText, float Volume = 1.f);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSound(USoundBase* Sound, FVector Location, float Volume, const FText& CaptionText);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFlicker(FVector Center, float Radius, float Duration);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStinger(USoundBase* Sound);

	// ---- Configuration ---------------------------------------------------------------

	/** Only house elements carrying this tag are used (lights, doors, sound points). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	FName HouseTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	FName LightTag = TEXT("HH_HouseLight");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	FName DoorTag = TEXT("HH_HouseDoor");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	FName SoundPointTag = TEXT("HH_HauntPoint");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	FName ApparitionPointTag = TEXT("HH_ApparitionPoint");

	/** Anger thresholds for stages 1..4. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	TArray<float> StageThresholds = { 12.f, 32.f, 58.f, 85.f };

	/** Anger per second with one person inside (more people = more, but less per head). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	float AngerPerSecond = 0.2f;

	/** Lights flicker (and the noise meter pins) this long before a hunt starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	float HuntWarningSeconds = 4.f;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> SettleSounds;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> DistantSounds;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> FootstepSounds;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> DragSounds;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TObjectPtr<USoundBase> MusicBoxSound;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> SlamSounds;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> WhisperSounds;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> GlassSounds;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TObjectPtr<USoundBase> BulbPopSound;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TObjectPtr<USoundBase> DoorCreakSound;

	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> StingerSounds;

private:
	UPROPERTY(Replicated)
	FVector_NetQuantize LastEventLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float LastEventTime = -1000.f;

	UPROPERTY(Replicated)
	float HuntWarningUntil = 0.f;

	/** Location of the event being run (set by the Do* functions). */
	FVector EventAt = FVector::ZeroVector;
	FTimerHandle HuntTimer;
	FTimerHandle ThinkTimer;
	bool bActive = true;
	float Anger = 0.f;
	float Aggression = 1.f;
	int32 Stage = 0;
	int32 HighestStage = 0;
	double NextEventTime = 0.0;
	double CalmUntil = 0.0;
	double LastHuntTime = -1000.0;
	EHHHauntEvent LastEvent = EHHHauntEvent::MAX;
	TMap<EHHHauntEvent, double> LastRun;
	FRandomStream Stream;
	int32 GatherPasses = 0;

	TArray<TWeakObjectPtr<AHHPracticalLight>> Lights;
	TArray<TWeakObjectPtr<AHHDoorActor>> Doors;
	TArray<TWeakObjectPtr<AHHHauntProp>> Props;
	TArray<TWeakObjectPtr<AHHHauntDevice>> Devices;
	TArray<FVector> SoundPoints;
	TArray<FVector> ApparitionPoints;
	TWeakObjectPtr<AHHApparition> Apparition;
};
