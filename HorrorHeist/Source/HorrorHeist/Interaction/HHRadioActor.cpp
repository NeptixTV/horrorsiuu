#include "Interaction/HHRadioActor.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "HHRadio"

AHHRadioActor::AHHRadioActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Speaker = CreateDefaultSubobject<UAudioComponent>(TEXT("Speaker"));
	Speaker->SetupAttachment(Mesh);
	Speaker->bAutoActivate = false;

	Static = CreateDefaultSubobject<UAudioComponent>(TEXT("Static"));
	Static->SetupAttachment(Mesh);
	Static->bAutoActivate = false;
}

void AHHRadioActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHRadioActor, Station);
}

void AHHRadioActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Station = Stations.IsValidIndex(StartStation) ? StartStation : -1;
	}
	OnRep_Station();
}

FText AHHRadioActor::GetInteractionPrompt(AActor* Interactor) const
{
	if (Station < 0)
	{
		return LOCTEXT("TurnOn", "Turn on the radio");
	}
	return Station + 1 < Stations.Num() ? LOCTEXT("Tune", "Change station") : LOCTEXT("TurnOff", "Turn off the radio");
}

void AHHRadioActor::Interact(AActor* Interactor)
{
	BP_OnInteracted(Interactor);
	if (!HasAuthority())
	{
		return;
	}
	Station = Station + 1 < Stations.Num() ? Station + 1 : -1;
	OnRep_Station();
	ForceNetUpdate();
}

void AHHRadioActor::OnRep_Station()
{
	if (TuneSound && HasActorBegunPlay())
	{
		UGameplayStatics::PlaySoundAtLocation(this, TuneSound, GetActorLocation(), 0.7f);
	}

	if (Stations.IsValidIndex(Station) && Stations[Station])
	{
		Speaker->SetSound(Stations[Station]);
		Speaker->Play(FMath::FRandRange(0.f, 30.f));
		if (StaticLoop)
		{
			Static->SetSound(StaticLoop);
			Static->SetVolumeMultiplier(StaticBedVolume);
			Static->Play();
		}
	}
	else
	{
		Speaker->Stop();
		Static->Stop();
	}
}

void AHHRadioActor::Interfere(float Duration, USoundBase* BleedThrough)
{
	if (!IsPlaying())
	{
		return;
	}
	InterferenceDuration = FMath::Max(Duration, 0.5f);
	InterferenceEnd = GetWorld()->GetTimeSeconds() + InterferenceDuration;
	SetActorTickEnabled(true);
	if (BleedThrough)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BleedThrough, GetActorLocation(), 0.9f);
	}
}

void AHHRadioActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Remaining = InterferenceEnd - GetWorld()->GetTimeSeconds();
	if (Remaining <= 0.f)
	{
		Speaker->SetVolumeMultiplier(1.f);
		Static->SetVolumeMultiplier(StaticBedVolume);
		SetActorTickEnabled(false);
		return;
	}
	// Fade in fast, hold, fade out over the last second.
	const float Elapsed = InterferenceDuration - Remaining;
	const float Envelope = FMath::Min(FMath::Clamp(Elapsed / 0.3f, 0.f, 1.f), FMath::Clamp(Remaining, 0.f, 1.f));
	Speaker->SetVolumeMultiplier(FMath::Lerp(1.f, 0.08f, Envelope));
	Static->SetVolumeMultiplier(FMath::Lerp(StaticBedVolume, 1.f, Envelope));
}

#undef LOCTEXT_NAMESPACE
