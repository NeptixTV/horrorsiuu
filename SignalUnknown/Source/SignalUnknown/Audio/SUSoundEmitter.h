// SIGNAL UNKNOWN – replicated one-shot sound at a location (spawned by the server, heard by all).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Audio/SUProceduralAudioComponent.h"
#include "SUSoundEmitter.generated.h"

UCLASS()
class SIGNALUNKNOWN_API ASUSoundEmitter : public AActor
{
	GENERATED_BODY()

public:
	ASUSoundEmitter();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only. Spawns a sound that every player hears at the location. */
	static ASUSoundEmitter* PlayAt(UObject* WorldContext, const FVector& Location, ESUSoundKind Kind, int32 TriggerKind, float Strength, float Gain, float Radius, float LifeSeconds = 8.f, int32 Repeats = 1, float RepeatInterval = 0.6f);

	UPROPERTY(Replicated) ESUSoundKind Kind = ESUSoundKind::Impact;
	UPROPERTY(Replicated) int32 TriggerKind = 0;
	UPROPERTY(Replicated) float Strength = 1.f;
	UPROPERTY(Replicated) float Gain = 1.f;
	UPROPERTY(Replicated) float Radius = 3000.f;
	UPROPERTY(Replicated) int32 Repeats = 1;
	UPROPERTY(Replicated) float RepeatInterval = 0.6f;

private:
	UPROPERTY(Transient)
	TObjectPtr<USUProceduralAudioComponent> Audio;

	int32 Played = 0;
	FTimerHandle RepeatTimer;
	void PlayOnce();
};
