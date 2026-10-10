#include "World/HHAmbienceDirector.h"
#include "World/HHPracticalLight.h"
#include "Interaction/HHDoorActor.h"
#include "Interaction/HHRadioActor.h"
#include "Audio/HHAudioSubsystem.h"
#include "HorrorHeist.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "HHAmbience"

AHHAmbienceDirector::AHHAmbienceDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AHHAmbienceDirector::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		ScheduleNext(FirstEventDelay.X, FirstEventDelay.Y);
	}
}

void AHHAmbienceDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(EventTimer);
	GetWorldTimerManager().ClearTimer(StepTimer);
	Super::EndPlay(EndPlayReason);
}

void AHHAmbienceDirector::ScheduleNext(float MinDelay, float MaxDelay)
{
	GetWorldTimerManager().SetTimer(EventTimer, this, &AHHAmbienceDirector::RunScheduledEvent, FMath::FRandRange(MinDelay, FMath::Max(MinDelay, MaxDelay)), false);
}

void AHHAmbienceDirector::RunScheduledEvent()
{
	TriggerEvent(-1);
	ScheduleNext(EventInterval.X, EventInterval.Y);
}

EHHAmbientEvent AHHAmbienceDirector::PickEvent(FRandomStream& Stream) const
{
	bool bRadioPlaying = false;
	for (TActorIterator<AHHRadioActor> It(GetWorld()); It; ++It)
	{
		bRadioPlaying |= It->IsPlaying();
	}

	struct FWeighted { EHHAmbientEvent Event; float Weight; };
	const FWeighted Table[] =
	{
		{ EHHAmbientEvent::FootstepsOverhead,	OverheadSteps.Num() > 0 ? 3.f : 0.f },
		{ EHHAmbientEvent::LightsStutter,		3.f },
		{ EHHAmbientEvent::RadioInterference,	bRadioPlaying ? 2.f : 0.f },
		{ EHHAmbientEvent::DistantDoor,			DistantDoorSound ? 2.f : 0.f },
		{ EHHAmbientEvent::Knocking,			KnockSound ? 1.f : 0.f },
		{ EHHAmbientEvent::DoorCreak,			2.f },
		{ EHHAmbientEvent::Whisper,				WhisperSounds.Num() > 0 ? 1.f : 0.f },
	};

	float Total = 0.f;
	for (const FWeighted& Entry : Table)
	{
		Total += Entry.Event == LastEvent ? 0.f : Entry.Weight;
	}
	float Roll = Stream.FRandRange(0.f, FMath::Max(Total, UE_KINDA_SMALL_NUMBER));
	for (const FWeighted& Entry : Table)
	{
		const float Weight = Entry.Event == LastEvent ? 0.f : Entry.Weight;
		if (Weight > 0.f && Roll <= Weight)
		{
			return Entry.Event;
		}
		Roll -= Weight;
	}
	return EHHAmbientEvent::LightsStutter;
}

void AHHAmbienceDirector::TriggerEvent(int32 EventIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 Seed = FMath::Rand();
	FRandomStream Stream(Seed);
	EHHAmbientEvent Event = EventIndex >= 0 && EventIndex < static_cast<int32>(EHHAmbientEvent::MAX)
		? static_cast<EHHAmbientEvent>(EventIndex)
		: PickEvent(Stream);

	if (Event == EHHAmbientEvent::DoorCreak)
	{
		// Doors only move when nobody is watching. If everyone is, do something else.
		if (!TryCreepDoor(Stream))
		{
			Event = EHHAmbientEvent::LightsStutter;
		}
		else
		{
			LastEvent = Event;
			return;
		}
	}

	LastEvent = Event;
	UE_LOG(LogHorrorHeist, Verbose, TEXT("Ambience event %d"), static_cast<int32>(Event));
	MulticastEvent(Event, Seed);
}

bool AHHAmbienceDirector::IsObservedByAnyone(const FVector& Location, float ConeDegrees) const
{
	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(ConeDegrees));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		FVector ViewLocation;
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FVector ToTarget = (Location - ViewLocation).GetSafeNormal();
		if (FVector::DotProduct(ViewRotation.Vector(), ToTarget) > CosLimit)
		{
			return true;
		}
	}
	return false;
}

bool AHHAmbienceDirector::TryCreepDoor(FRandomStream& Stream)
{
	TArray<AHHDoorActor*> Candidates;
	for (TActorIterator<AHHDoorActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(CreepyDoorTag) && !It->IsOpen() && !IsObservedByAnyone(It->GetActorLocation(), 70.f))
		{
			Candidates.Add(*It);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	AHHDoorActor* Door = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	Door->SetTargetAngle(Stream.FRandRange(12.f, 24.f), Stream.FRandRange(4.f, 9.f), true);
	return true;
}

void AHHAmbienceDirector::MulticastEvent_Implementation(EHHAmbientEvent Event, int32 Seed)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	switch (Event)
	{
	case EHHAmbientEvent::FootstepsOverhead:
		PlayFootstepsOverhead(Seed);
		break;
	case EHHAmbientEvent::LightsStutter:
		PlayLightsStutter(Seed);
		break;
	case EHHAmbientEvent::RadioInterference:
		PlayRadioInterference(Seed);
		break;
	case EHHAmbientEvent::DistantDoor:
		PlayAtTagged(OverheadEndTag, DistantDoorSound, LOCTEXT("DoorSlam", "[A door slams somewhere above]"), Seed, false);
		break;
	case EHHAmbientEvent::Knocking:
		PlayAtTagged(StairDoorTag, KnockSound, LOCTEXT("Knock", "[Three slow knocks at the stair door]"), Seed, false);
		break;
	case EHHAmbientEvent::Whisper:
	{
		FRandomStream Stream(Seed);
		USoundBase* Whisper = WhisperSounds.Num() > 0 ? WhisperSounds[Stream.RandRange(0, WhisperSounds.Num() - 1)].Get() : nullptr;
		PlayAtTagged(WhisperTag, Whisper, LOCTEXT("Whisper", "[Whispering, close by]"), Seed, true);
		break;
	}
	default:
		break;
	}
}

void AHHAmbienceDirector::Caption(const FText& Text, float Duration) const
{
	if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
	{
		Audio->ShowCaption(Text, Duration, true);
	}
}

void AHHAmbienceDirector::PlayAtTagged(FName Tag, USoundBase* Sound, const FText& CaptionText, int32 Seed, bool bNearestToViewer)
{
	if (!Sound)
	{
		return;
	}

	TArray<AActor*> Points;
	UGameplayStatics::GetAllActorsWithTag(this, Tag, Points);

	FVector Location = GetActorLocation();
	if (Points.Num() > 0)
	{
		if (bNearestToViewer)
		{
			FVector ViewLocation = Location;
			if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				FRotator Unused;
				PC->GetPlayerViewPoint(ViewLocation, Unused);
			}
			float Best = TNumericLimits<float>::Max();
			for (const AActor* Point : Points)
			{
				const float Distance = FVector::DistSquared(Point->GetActorLocation(), ViewLocation);
				if (Distance < Best)
				{
					Best = Distance;
					Location = Point->GetActorLocation();
				}
			}
		}
		else
		{
			FRandomStream Stream(Seed);
			Location = Points[Stream.RandRange(0, Points.Num() - 1)]->GetActorLocation();
		}
	}

	UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	Caption(CaptionText);
}

void AHHAmbienceDirector::PlayFootstepsOverhead(int32 Seed)
{
	TArray<AActor*> Starts;
	TArray<AActor*> Ends;
	UGameplayStatics::GetAllActorsWithTag(this, OverheadStartTag, Starts);
	UGameplayStatics::GetAllActorsWithTag(this, OverheadEndTag, Ends);

	StepStream.Initialize(Seed);
	StepStart = Starts.Num() > 0 ? Starts[0]->GetActorLocation() : GetActorLocation() + FVector(-400.f, 0.f, 380.f);
	StepEnd = Ends.Num() > 0 ? Ends[0]->GetActorLocation() : GetActorLocation() + FVector(400.f, 0.f, 380.f);
	if (StepStream.FRand() < 0.5f)
	{
		Swap(StepStart, StepEnd);
	}
	StepCount = StepStream.RandRange(7, 12);
	StepIndex = 0;

	Caption(LOCTEXT("Overhead", "[Slow footsteps cross the floor above]"), 4.f);
	GetWorldTimerManager().SetTimer(StepTimer, this, &AHHAmbienceDirector::PlayOverheadStep, 0.1f, false);
}

void AHHAmbienceDirector::PlayOverheadStep()
{
	if (StepIndex >= StepCount || OverheadSteps.Num() == 0)
	{
		return;
	}

	// The walker slows down and stops before reaching the end. Right above the crew.
	const float Alpha = static_cast<float>(StepIndex) / static_cast<float>(FMath::Max(StepCount, 1));
	const FVector Lateral(0.f, StepStream.FRandRange(-30.f, 30.f), 0.f);
	const FVector Location = FMath::Lerp(StepStart, StepEnd, Alpha * 0.85f) + Lateral;

	if (USoundBase* Step = OverheadSteps[StepStream.RandRange(0, OverheadSteps.Num() - 1)])
	{
		const float Volume = FMath::Lerp(0.75f, 1.f, StepStream.FRand());
		UGameplayStatics::PlaySoundAtLocation(this, Step, Location, Volume, StepStream.FRandRange(0.92f, 1.04f));
	}

	++StepIndex;
	const float Interval = FMath::Lerp(0.78f, 1.25f, Alpha) + StepStream.FRandRange(-0.06f, 0.08f);
	GetWorldTimerManager().SetTimer(StepTimer, this, &AHHAmbienceDirector::PlayOverheadStep, Interval, false);
}

void AHHAmbienceDirector::PlayLightsStutter(int32 Seed)
{
	FRandomStream Stream(Seed);
	const float Duration = Stream.FRandRange(0.8f, 2.2f);
	FVector Center = GetActorLocation();
	int32 Count = 0;
	for (TActorIterator<AHHPracticalLight> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(FlickerTag) && It->IsLightOn())
		{
			It->Surge(Duration * Stream.FRandRange(0.7f, 1.f));
			Center = Count == 0 ? It->GetActorLocation() : Center;
			++Count;
		}
	}
	if (ElectricalStutterSound && Count > 0)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ElectricalStutterSound, Center);
	}
	Caption(LOCTEXT("Lights", "[The lights stutter]"), 2.f);
}

void AHHAmbienceDirector::PlayRadioInterference(int32 Seed)
{
	FRandomStream Stream(Seed);
	for (TActorIterator<AHHRadioActor> It(GetWorld()); It; ++It)
	{
		It->Interfere(Stream.FRandRange(2.5f, 4.5f), RadioBleedSound);
	}
	Caption(LOCTEXT("Radio", "[A voice inside the static]"), 3.f);
}

#undef LOCTEXT_NAMESPACE
