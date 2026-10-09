#include "World/HHAmbientEmitter.h"
#include "Audio/HHAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Engine/World.h"

AHHAmbientEmitter::AHHAmbientEmitter()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	Loop = CreateDefaultSubobject<UAudioComponent>(TEXT("Loop"));
	Loop->SetupAttachment(SceneRoot);
	Loop->bAutoActivate = true;
}

void AHHAmbientEmitter::BeginPlay()
{
	Super::BeginPlay();
	if (OneShots.Num() > 0 && GetNetMode() != NM_DedicatedServer)
	{
		ScheduleNext();
	}
}

void AHHAmbientEmitter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(Timer);
	Super::EndPlay(EndPlayReason);
}

void AHHAmbientEmitter::ScheduleNext()
{
	const float Delay = FMath::FRandRange(FMath::Max(0.2f, MinInterval), FMath::Max(MinInterval, MaxInterval));
	GetWorldTimerManager().SetTimer(Timer, this, &AHHAmbientEmitter::PlayOneShot, Delay, false);
}

void AHHAmbientEmitter::PlayOneShot()
{
	if (OneShots.Num() > 0)
	{
		int32 Index = FMath::RandRange(0, OneShots.Num() - 1);
		if (OneShots.Num() > 1 && Index == LastIndex)
		{
			Index = (Index + 1) % OneShots.Num();
		}
		LastIndex = Index;

		if (USoundBase* Sound = OneShots[Index])
		{
			const FVector2D Offset2D = FMath::RandPointInCircle(ScatterRadius);
			const FVector Location = GetActorLocation() + FVector(Offset2D.X, Offset2D.Y, FMath::FRandRange(-20.f, 20.f));
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Location,
				FMath::FRandRange(VolumeRange.X, VolumeRange.Y), FMath::FRandRange(PitchRange.X, PitchRange.Y));

			if (!Caption.IsEmpty())
			{
				if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
				{
					Audio->ShowCaption(Caption, 2.5f, true);
				}
			}
		}
	}
	ScheduleNext();
}
