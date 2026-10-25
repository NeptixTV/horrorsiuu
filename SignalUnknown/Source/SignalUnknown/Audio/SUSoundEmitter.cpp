#include "Audio/SUSoundEmitter.h"
#include "Core/SUGameUserSettings.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASUSoundEmitter::ASUSoundEmitter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASUSoundEmitter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUSoundEmitter, Kind);
	DOREPLIFETIME(ASUSoundEmitter, TriggerKind);
	DOREPLIFETIME(ASUSoundEmitter, Strength);
	DOREPLIFETIME(ASUSoundEmitter, Gain);
	DOREPLIFETIME(ASUSoundEmitter, Radius);
	DOREPLIFETIME(ASUSoundEmitter, Repeats);
	DOREPLIFETIME(ASUSoundEmitter, RepeatInterval);
}

ASUSoundEmitter* ASUSoundEmitter::PlayAt(UObject* WorldContext, const FVector& Location, ESUSoundKind InKind, int32 InTriggerKind, float InStrength, float InGain, float InRadius, float LifeSeconds, int32 InRepeats, float InRepeatInterval)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ASUSoundEmitter* Emitter = World->SpawnActor<ASUSoundEmitter>(ASUSoundEmitter::StaticClass(), FTransform(Location), Params);
	if (!Emitter)
	{
		return nullptr;
	}
	Emitter->Kind = InKind;
	Emitter->TriggerKind = InTriggerKind;
	Emitter->Strength = InStrength;
	Emitter->Gain = InGain;
	Emitter->Radius = InRadius;
	Emitter->Repeats = FMath::Max(1, InRepeats);
	Emitter->RepeatInterval = InRepeatInterval;
	Emitter->FinishSpawning(FTransform(Location));
	Emitter->SetLifeSpan(LifeSeconds + Emitter->Repeats * InRepeatInterval);
	return Emitter;
}

void ASUSoundEmitter::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Audio = NewObject<USUProceduralAudioComponent>(this, TEXT("Audio"));
	Audio->SetupAttachment(RootComponent);
	Audio->ConfigureSound(Kind, SUAudioCategory::Effects, Gain, Radius);
	Audio->RegisterComponent();
	Audio->StartSound();
	PlayOnce();
}

void ASUSoundEmitter::PlayOnce()
{
	if (!Audio)
	{
		return;
	}
	Audio->TriggerSound(TriggerKind, Strength);
	++Played;
	if (Played < Repeats)
	{
		const float Jitter = FMath::FRandRange(0.8f, 1.25f);
		GetWorldTimerManager().SetTimer(RepeatTimer, this, &ASUSoundEmitter::PlayOnce, FMath::Max(0.05f, RepeatInterval * Jitter), false);
	}
}
