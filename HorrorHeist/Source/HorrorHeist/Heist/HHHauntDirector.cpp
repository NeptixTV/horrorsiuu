#include "Heist/HHHauntDirector.h"
#include "Heist/HHApparition.h"
#include "Heist/HHHauntDevice.h"
#include "Heist/HHHauntProp.h"
#include "Heist/HHHauntThrowable.h"
#include "Heist/HHHauntMark.h"
#include "Player/HHCharacter.h"
#include "Player/HHPlayerController.h"
#include "Heist/HHHeistGameMode.h"
#include "Heist/HHHeistGameState.h"
#include "Interaction/HHDoorActor.h"
#include "World/HHPracticalLight.h"
#include "Player/HHPlayerState.h"
#include "Data/HHLootDefinition.h"
#include "Audio/HHAudioSubsystem.h"
#include "HorrorHeist.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "HHHaunt"

namespace HHHaunt
{
	/**
	 * Minimum stage, cooldown (s) and weight for each event (index = EHHHauntEvent):
	 * Settle, DistantSound, LightOut, DoorOpen, ObjectMoved, DoorSlam, DeviceOn, Flicker, Whisper, GlassFall,
	 * Apparition, Hunt, Voice, Scratch, StepsBehind, Touch, Throw, Mark, Manifest, ShadowRun, Rock.
	 */
	static const int32 MinStage[] = { 0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 4, 1, 2, 2, 2, 2, 2, 3, 2, 1 };
	static const float Cooldown[] = { 20.f, 35.f, 50.f, 45.f, 60.f, 50.f, 90.f, 40.f, 45.f, 50.f, 70.f, 80.f,
		40.f, 45.f, 60.f, 70.f, 40.f, 80.f, 90.f, 60.f, 70.f };
	static const float Weight[] = { 1.f, 2.f, 1.4f, 1.3f, 1.3f, 1.1f, 0.8f, 0.9f, 1.f, 0.9f, 0.9f, 1.2f,
		1.2f, 1.f, 0.9f, 0.7f, 1.2f, 0.8f, 0.8f, 0.9f, 0.8f };
	static_assert(UE_ARRAY_COUNT(MinStage) == static_cast<int32>(EHHHauntEvent::MAX), "Haunt event table size");
	static_assert(UE_ARRAY_COUNT(Cooldown) == static_cast<int32>(EHHHauntEvent::MAX), "Haunt event table size");
	static_assert(UE_ARRAY_COUNT(Weight) == static_cast<int32>(EHHHauntEvent::MAX), "Haunt event table size");
}

AHHHauntDirector::AHHHauntDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

AHHHauntDirector* AHHHauntDirector::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (World)
	{
		for (TActorIterator<AHHHauntDirector> It(World); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void AHHHauntDirector::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	Stream.Initialize(FMath::Rand());
	GatherHouse();
	NextEventTime = GetWorld()->GetTimeSeconds() + 25.0;
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AHHHauntDirector::Think, 1.f, true, 1.f);
}

void AHHHauntDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHHauntDirector, LastEventLocation);
	DOREPLIFETIME(AHHHauntDirector, LastEventTime);
	DOREPLIFETIME(AHHHauntDirector, HuntWarningUntil);
}

float AHHHauntDirector::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : 0.f;
}

bool AHHHauntDirector::IsHuntImminent() const
{
	return HuntWarningUntil > 0.f && GetServerTime() < HuntWarningUntil;
}

void AHHHauntDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	GetWorldTimerManager().ClearTimer(HuntTimer);
	GetWorldTimerManager().ClearTimer(StepTimer);
	Super::EndPlay(EndPlayReason);
}

void AHHHauntDirector::GatherHouse()
{
	Lights.Reset();
	Doors.Reset();
	Props.Reset();
	Devices.Reset();
	Throwables.Reset();
	Marks.Reset();
	SoundPoints.Reset();
	ApparitionPoints.Reset();

	auto InHouse = [this](const AActor* Actor)
	{
		return HouseTag.IsNone() || Actor->ActorHasTag(HouseTag);
	};

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (AHHHauntProp* Prop = Cast<AHHHauntProp>(Actor))
		{
			Props.Add(Prop);
		}
		else if (AHHHauntDevice* Device = Cast<AHHHauntDevice>(Actor))
		{
			Devices.Add(Device);
		}
		else if (AHHHauntThrowable* Throwable = Cast<AHHHauntThrowable>(Actor))
		{
			Throwables.Add(Throwable);
		}
		else if (AHHHauntMark* Mark = Cast<AHHHauntMark>(Actor))
		{
			Marks.Add(Mark);
		}
		else if (AHHApparition* Ghost = Cast<AHHApparition>(Actor))
		{
			Apparition = Ghost;
			Ghost->SetDirector(this);
		}
		if (Actor->Tags.Num() == 0 || !InHouse(Actor))
		{
			continue;
		}
		if (Actor->ActorHasTag(LightTag))
		{
			if (AHHPracticalLight* Light = Cast<AHHPracticalLight>(Actor))
			{
				Lights.Add(Light);
			}
		}
		if (Actor->ActorHasTag(DoorTag))
		{
			if (AHHDoorActor* Door = Cast<AHHDoorActor>(Actor))
			{
				Doors.Add(Door);
			}
		}
		if (Actor->ActorHasTag(SoundPointTag))
		{
			SoundPoints.Add(Actor->GetActorLocation());
		}
		if (Actor->ActorHasTag(ApparitionPointTag))
		{
			ApparitionPoints.Add(Actor->GetActorLocation());
		}
	}
	UE_LOG(LogHorrorHeist, Log, TEXT("Haunt '%s': %d lights, %d doors, %d props, %d devices, %d throwables, %d marks, %d sound points, %d apparition points, apparition %s."),
		*HouseTag.ToString(), Lights.Num(), Doors.Num(), Props.Num(), Devices.Num(), Throwables.Num(), Marks.Num(), SoundPoints.Num(),
		ApparitionPoints.Num(), Apparition.IsValid() ? TEXT("yes") : TEXT("no"));
}

void AHHHauntDirector::SetActive(bool bNewActive)
{
	bActive = bNewActive;
	if (!bActive)
	{
		if (AHHApparition* Ghost = Apparition.Get())
		{
			Ghost->Vanish();
		}
	}
}

void AHHHauntDirector::AddAnger(float Amount)
{
	if (!HasAuthority() || !bActive)
	{
		return;
	}
	Anger = FMath::Clamp(Anger + Amount * Aggression, 0.f, 100.f);
	UpdateStage();
}

void AHHHauntDirector::OnLootTaken(const UHHLootDefinition* Loot, const FVector& Where)
{
	if (!Loot)
	{
		AddAnger(2.f);
		return;
	}
	AddAnger(Loot->HauntAnger + (Loot->bCursed ? 18.f : 0.f));
	if (Loot->bCursed && bActive)
	{
		// The house wants it back: an answer right away, close to the thief.
		PlaySoundEverywhere(SlamSounds.Num() > 0 ? SlamSounds[Stream.RandRange(0, SlamSounds.Num() - 1)].Get() : nullptr,
			Where + FVector(Stream.FRandRange(-500.f, 500.f), Stream.FRandRange(-500.f, 500.f), 0.f), LOCTEXT("SlamCaption", "[A door slams]"));
		DoFlicker();
		HitPsycheNear(Where, 900.f, 8.f);
	}
}

void AHHHauntDirector::OnPlayerCaught()
{
	// Satisfied for a while: calmer and quiet for a minute.
	Anger = FMath::Max(0.f, Anger - 18.f);
	CalmUntil = GetWorld()->GetTimeSeconds() + 60.0;
	UpdateStage();
}

void AHHHauntDirector::UpdateStage()
{
	int32 NewStage = 0;
	for (int32 Index = 0; Index < StageThresholds.Num(); ++Index)
	{
		if (Anger >= StageThresholds[Index])
		{
			NewStage = Index + 1;
		}
	}
	NewStage = FMath::Clamp(NewStage, 0, 4);

	const int32 OldStage = Stage;
	Stage = NewStage;
	if (Stage > HighestStage)
	{
		HighestStage = Stage;
		// Each new stage announces itself with its signature event.
		static const EHHHauntEvent Signature[] = { EHHHauntEvent::Settle, EHHHauntEvent::LightOut, EHHHauntEvent::DoorSlam, EHHHauntEvent::Apparition, EHHHauntEvent::Hunt };
		if (Stage > OldStage && bActive && GetPawnsInside().Num() > 0)
		{
			if (StingerSounds.Num() > 0 && Stage >= 2)
			{
				MulticastStinger(StingerSounds[Stream.RandRange(0, StingerSounds.Num() - 1)]);
			}
			RunEvent(Signature[Stage]);
			NextEventTime = GetWorld()->GetTimeSeconds() + NextEventDelay();
		}
	}

	if (AHHHeistGameState* HeistState = GetWorld()->GetGameState<AHHHeistGameState>())
	{
		HeistState->SetHaunt(Anger, Stage);
	}
}

float AHHHauntDirector::NextEventDelay() const
{
	static const FVector2D Delays[] = { FVector2D(35.f, 70.f), FVector2D(26.f, 50.f), FVector2D(18.f, 36.f), FVector2D(13.f, 26.f), FVector2D(9.f, 18.f) };
	const FVector2D Range = Delays[FMath::Clamp(Stage, 0, 4)];
	return FMath::Lerp(Range.X, Range.Y, FMath::FRand());
}

void AHHHauntDirector::Think()
{
	if (!bActive)
	{
		return;
	}
	const TArray<APawn*> Inside = GetPawnsInside();
	const double Now = GetWorld()->GetTimeSeconds();

	if (Inside.Num() > 0)
	{
		// Longer inside -> angrier. Diminishing per extra person, but a full crew still counts.
		const float People = 1.f + 0.45f * (Inside.Num() - 1);
		AddAnger(AngerPerSecond * People);
	}
	else
	{
		Anger = FMath::Max(0.f, Anger - 0.05f);
		UpdateStage();
	}

	// Streaming levels can finish late; look for house elements again a couple of times.
	if (GatherPasses < 2 && Now > 4.0 * (GatherPasses + 1))
	{
		++GatherPasses;
		GatherHouse();
	}

	if (Inside.Num() == 0 || Now < NextEventTime || Now < CalmUntil)
	{
		return;
	}

	// Try a few picks; some events have no valid target right now (all lights already out...).
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		const EHHHauntEvent Event = PickEvent();
		if (Event == EHHHauntEvent::MAX)
		{
			break;
		}
		if (RunEvent(Event))
		{
			break;
		}
		LastRun.Add(Event, Now);
	}
	NextEventTime = Now + NextEventDelay();
}

EHHHauntEvent AHHHauntDirector::PickEvent() const
{
	const double Now = GetWorld()->GetTimeSeconds();
	float Total = 0.f;
	float Weights[static_cast<int32>(EHHHauntEvent::MAX)] = {};
	for (int32 Index = 0; Index < static_cast<int32>(EHHHauntEvent::MAX); ++Index)
	{
		const EHHHauntEvent Event = static_cast<EHHHauntEvent>(Index);
		if (HHHaunt::MinStage[Index] > Stage || Event == LastEvent)
		{
			continue;
		}
		if (const double* Last = LastRun.Find(Event))
		{
			if (Now - *Last < HHHaunt::Cooldown[Index])
			{
				continue;
			}
		}
		if (Event == EHHHauntEvent::Hunt && Now - LastHuntTime < 75.0)
		{
			continue;
		}
		// Higher stages lean towards their own events.
		const float StageBias = 1.f + 0.6f * HHHaunt::MinStage[Index] * (Stage >= HHHaunt::MinStage[Index] ? 1.f : 0.f) / FMath::Max(1, Stage);
		Weights[Index] = HHHaunt::Weight[Index] * StageBias;
		Total += Weights[Index];
	}
	if (Total <= 0.f)
	{
		return EHHHauntEvent::MAX;
	}
	float Pick = FMath::FRand() * Total;
	for (int32 Index = 0; Index < static_cast<int32>(EHHHauntEvent::MAX); ++Index)
	{
		Pick -= Weights[Index];
		if (Weights[Index] > 0.f && Pick <= 0.f)
		{
			return static_cast<EHHHauntEvent>(Index);
		}
	}
	return EHHHauntEvent::Settle;
}

bool AHHHauntDirector::TriggerEvent(EHHHauntEvent Event)
{
	return HasAuthority() && RunEvent(Event);
}

bool AHHHauntDirector::RunEvent(EHHHauntEvent Event)
{
	bool bDone = false;
	EventAt = FVector::ZeroVector;
	switch (Event)
	{
	case EHHHauntEvent::Settle:
		bDone = DoSoundInOtherRoom(SettleSounds, LOCTEXT("Creak", "[The house creaks]"), 300.f);
		break;
	case EHHHauntEvent::DistantSound:
	{
		// Knocks, running steps or something heavy dragged - always from somewhere else.
		const int32 Kind = Stream.RandRange(0, Stage >= 2 ? 3 : 2);
		if (Kind == 0)
		{
			bDone = DoSoundInOtherRoom(DistantSounds, LOCTEXT("Knock", "[Knocking somewhere in the house]"), 800.f);
		}
		else if (Kind == 1)
		{
			bDone = DoSoundInOtherRoom(FootstepSounds, LOCTEXT("Steps", "[Footsteps in another room]"), 800.f);
		}
		else if (Kind == 2)
		{
			bDone = DoSoundInOtherRoom(DragSounds, LOCTEXT("Drag", "[Something is dragged across the floor]"), 800.f);
		}
		else if (MusicBoxSound)
		{
			bDone = DoSoundInOtherRoom({ MusicBoxSound }, LOCTEXT("MusicBox", "[A music box plays]"), 700.f);
		}
		break;
	}
	case EHHHauntEvent::LightOut:		bDone = DoLightOut(); break;
	case EHHHauntEvent::DoorOpen:		bDone = DoDoorOpen(); break;
	case EHHHauntEvent::ObjectMoved:	bDone = DoObjectMoved(); break;
	case EHHHauntEvent::DoorSlam:		bDone = DoDoorSlam(); break;
	case EHHHauntEvent::DeviceOn:		bDone = DoDeviceOn(); break;
	case EHHHauntEvent::Flicker:		bDone = DoFlicker(); break;
	case EHHHauntEvent::Whisper:		bDone = DoWhisper(); break;
	case EHHHauntEvent::GlassFall:		bDone = DoGlassFall(); break;
	case EHHHauntEvent::Apparition:		bDone = DoApparition(false); break;
	case EHHHauntEvent::Hunt:			bDone = DoApparition(true); break;
	case EHHHauntEvent::Voice:			bDone = DoVoice(); break;
	case EHHHauntEvent::Scratch:		bDone = DoScratch(); break;
	case EHHHauntEvent::StepsBehind:	bDone = DoStepsBehind(); break;
	case EHHHauntEvent::Touch:			bDone = DoTouch(); break;
	case EHHHauntEvent::Throw:			bDone = DoThrow(); break;
	case EHHHauntEvent::Mark:			bDone = DoMark(); break;
	case EHHHauntEvent::Manifest:		bDone = DoManifest(); break;
	case EHHHauntEvent::ShadowRun:		bDone = DoShadowRun(); break;
	case EHHHauntEvent::Rock:			bDone = DoRock(); break;
	default: break;
	}

	if (bDone)
	{
		LastEvent = Event;
		LastRun.Add(Event, GetWorld()->GetTimeSeconds());
		// The noise meter reads this: where and when the house last did something.
		LastEventLocation = EventAt;
		LastEventTime = GetServerTime();
		UE_LOG(LogHorrorHeist, Verbose, TEXT("Haunt event %s (anger %.1f, stage %d)"), *UEnum::GetValueAsString(Event), Anger, Stage);
	}
	return bDone;
}

// ---------------------------------------------------------------------------------------
// Helpers

TArray<APawn*> AHHHauntDirector::GetPawnsInside() const
{
	TArray<APawn*> Result;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		const AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
		if (State && State->IsInsideHouse())
		{
			Result.Add(Pawn);
		}
	}
	return Result;
}

float AHHHauntDirector::DistanceToNearestPawn(const FVector& Location) const
{
	float Best = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Best = FMath::Min(Best, static_cast<float>(FVector::Dist(Pawn->GetActorLocation(), Location)));
		}
	}
	return Best;
}

bool AHHHauntDirector::IsObserved(const FVector& Location, float MaxDistance) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		FVector Eye;
		FRotator View;
		Pawn->GetActorEyesViewPoint(Eye, View);
		const FVector ToTarget = Location - Eye;
		const float Distance = ToTarget.Size();
		if (Distance > MaxDistance)
		{
			continue;
		}
		if (Distance < 250.f)
		{
			return true;	// Right next to it counts as watching.
		}
		if (FVector::DotProduct(View.Vector(), ToTarget / Distance) < FMath::Cos(FMath::DegreesToRadians(55.f)))
		{
			continue;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HHHauntObserved), false, Pawn);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Location, ECC_Visibility, Params) || FVector::DistSquared(Hit.ImpactPoint, Location) < FMath::Square(60.f))
		{
			return true;
		}
	}
	return false;
}

void AHHHauntDirector::HitPsyche(APawn* Pawn, float Amount)
{
	if (AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr)
	{
		State->SetPsyche(State->GetPsyche() - Amount);
	}
}

void AHHHauntDirector::HitPsycheNear(const FVector& Location, float Radius, float Amount)
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (Pawn && FVector::Dist(Pawn->GetActorLocation(), Location) < Radius)
		{
			HitPsyche(Pawn, Amount);
		}
	}
}

void AHHHauntDirector::PlaySoundEverywhere(USoundBase* Sound, const FVector& Location, const FText& CaptionText, float Volume)
{
	if (Sound)
	{
		MulticastSound(Sound, Location, Volume, CaptionText);
	}
}

void AHHHauntDirector::MulticastSound_Implementation(USoundBase* Sound, FVector Location, float Volume, const FText& CaptionText)
{
	if (!Sound || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, Volume);
	if (!CaptionText.IsEmpty())
	{
		if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
		{
			Audio->ShowCaption(CaptionText, 3.f, true);
		}
	}
}

void AHHHauntDirector::MulticastFlicker_Implementation(FVector Center, float Radius, float Duration)
{
	for (TActorIterator<AHHPracticalLight> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), Center) < Radius)
		{
			It->Surge(Duration);
		}
	}
}

void AHHHauntDirector::MulticastStinger_Implementation(USoundBase* Sound)
{
	if (Sound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

// ---------------------------------------------------------------------------------------
// Events

bool AHHHauntDirector::DoSoundInOtherRoom(const TArray<TObjectPtr<USoundBase>>& Sounds, const FText& CaptionText, float MinDistance)
{
	if (Sounds.Num() == 0 || SoundPoints.Num() == 0)
	{
		return false;
	}
	// A point nobody is close to, but still within earshot of someone (inside the house).
	TArray<FVector> Candidates;
	for (const FVector& Point : SoundPoints)
	{
		const float Nearest = DistanceToNearestPawn(Point);
		if (Nearest > MinDistance && Nearest < 2600.f)
		{
			Candidates.Add(Point);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	const FVector Where = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	USoundBase* Sound = Sounds[Stream.RandRange(0, Sounds.Num() - 1)];
	PlaySoundEverywhere(Sound, Where, CaptionText);
	EventAt = Where;
	HitPsycheNear(Where, 1800.f, 1.5f);
	return Sound != nullptr;
}

bool AHHHauntDirector::DoLightOut()
{
	// A light that is on, near somebody - and it is fine if they see it happen.
	TArray<AHHPracticalLight*> Candidates;
	for (const TWeakObjectPtr<AHHPracticalLight>& LightPtr : Lights)
	{
		AHHPracticalLight* Light = LightPtr.Get();
		if (Light && Light->IsLightOn() && DistanceToNearestPawn(Light->GetActorLocation()) < 1500.f)
		{
			Candidates.Add(Light);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	AHHPracticalLight* Light = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	Light->SetLightOn(false);
	EventAt = Light->GetActorLocation();
	PlaySoundEverywhere(BulbPopSound, Light->GetActorLocation(), LOCTEXT("BulbPop", "[A light bulb pops]"));
	HitPsycheNear(Light->GetActorLocation(), 900.f, 3.f);
	return true;
}

bool AHHHauntDirector::DoDoorOpen()
{
	TArray<AHHDoorActor*> Candidates;
	for (const TWeakObjectPtr<AHHDoorActor>& DoorPtr : Doors)
	{
		AHHDoorActor* Door = DoorPtr.Get();
		if (Door && !Door->IsOpen() && !Door->IsLocked() && !IsObserved(Door->GetActorLocation(), 2500.f) && DistanceToNearestPawn(Door->GetActorLocation()) < 2200.f)
		{
			Candidates.Add(Door);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	AHHDoorActor* Door = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	EventAt = Door->GetActorLocation();
	Door->SetTargetAngle(Stream.FRandRange(55.f, 85.f) * (Stream.FRand() < 0.5f ? -1.f : 1.f), 16.f, true);
	return true;
}

bool AHHHauntDirector::DoObjectMoved()
{
	TArray<AHHHauntProp*> Candidates;
	for (const TWeakObjectPtr<AHHHauntProp>& PropPtr : Props)
	{
		AHHHauntProp* Prop = PropPtr.Get();
		if (Prop && !Prop->IsMoved() && !IsObserved(Prop->GetActorLocation(), 3000.f))
		{
			Candidates.Add(Prop);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	AHHHauntProp* Moved = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	Moved->MoveToAlternate();
	EventAt = Moved->GetActorLocation();
	return true;
}

bool AHHHauntDirector::DoDoorSlam()
{
	TArray<AHHDoorActor*> Candidates;
	for (const TWeakObjectPtr<AHHDoorActor>& DoorPtr : Doors)
	{
		AHHDoorActor* Door = DoorPtr.Get();
		if (!Door || !Door->IsOpen())
		{
			continue;
		}
		const float Nearest = DistanceToNearestPawn(Door->GetActorLocation());
		// Close enough to be heard, never so close that it hits someone.
		if (Nearest > 220.f && Nearest < 1800.f)
		{
			Candidates.Add(Door);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	AHHDoorActor* Door = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	Door->SetTargetAngle(0.f, 520.f, false);
	EventAt = Door->GetActorLocation();
	if (SlamSounds.Num() > 0)
	{
		PlaySoundEverywhere(SlamSounds[Stream.RandRange(0, SlamSounds.Num() - 1)], Door->GetActorLocation(), LOCTEXT("Slam", "[A door slams]"));
	}
	HitPsycheNear(Door->GetActorLocation(), 1500.f, 4.f);
	return true;
}

bool AHHHauntDirector::DoDeviceOn()
{
	TArray<AHHHauntDevice*> Candidates;
	for (const TWeakObjectPtr<AHHHauntDevice>& DevicePtr : Devices)
	{
		AHHHauntDevice* Device = DevicePtr.Get();
		if (Device && !Device->IsOn() && DistanceToNearestPawn(Device->GetActorLocation()) < 2200.f)
		{
			Candidates.Add(Device);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	AHHHauntDevice* Device = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	EventAt = Device->GetActorLocation();
	Device->TurnOn(Stream.FRandRange(18.f, 35.f));
	HitPsycheNear(Device->GetActorLocation(), 1200.f, 3.f);
	return true;
}

bool AHHHauntDirector::DoFlicker()
{
	const TArray<APawn*> Inside = GetPawnsInside();
	if (Inside.Num() == 0)
	{
		return false;
	}
	const APawn* Center = Inside[Stream.RandRange(0, Inside.Num() - 1)];
	MulticastFlicker(Center->GetActorLocation(), 1600.f, Stream.FRandRange(1.5f, 3.f));
	EventAt = Center->GetActorLocation();
	return true;
}

bool AHHHauntDirector::DoWhisper()
{
	const TArray<APawn*> Inside = GetPawnsInside();
	if (Inside.Num() == 0 || WhisperSounds.Num() == 0)
	{
		return false;
	}
	APawn* Victim = Inside[Stream.RandRange(0, Inside.Num() - 1)];
	// Right behind them.
	const FVector Where = Victim->GetActorLocation() - Victim->GetActorForwardVector() * 120.f + FVector(0.f, 0.f, 60.f);
	PlaySoundEverywhere(WhisperSounds[Stream.RandRange(0, WhisperSounds.Num() - 1)], Where, LOCTEXT("Whisper", "[Something whispers behind you]"), 0.9f);
	HitPsyche(Victim, 5.f);
	EventAt = Where;
	return true;
}

bool AHHHauntDirector::DoGlassFall()
{
	if (GlassSounds.Num() == 0 || SoundPoints.Num() == 0)
	{
		return false;
	}
	TArray<FVector> Candidates;
	for (const FVector& Point : SoundPoints)
	{
		const float Nearest = DistanceToNearestPawn(Point);
		if (Nearest > 350.f && Nearest < 1100.f)
		{
			Candidates.Add(Point);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	const FVector Where = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	PlaySoundEverywhere(GlassSounds[Stream.RandRange(0, GlassSounds.Num() - 1)], Where, LOCTEXT("Glass", "[Glass shatters]"));
	HitPsycheNear(Where, 1100.f, 3.f);
	EventAt = Where;
	return true;
}

bool AHHHauntDirector::DoApparition(bool bHunt)
{
	AHHApparition* Ghost = Apparition.Get();
	const TArray<APawn*> Inside = GetPawnsInside();
	if (!Ghost || Ghost->GetMode() != EHHApparitionMode::Hidden || Inside.Num() == 0 || ApparitionPoints.Num() == 0)
	{
		return false;
	}

	if (bHunt)
	{
		// Start where it cannot be seen, some distance from the deepest member of the crew.
		APawn* Target = Inside[Stream.RandRange(0, Inside.Num() - 1)];
		TArray<FVector> Candidates;
		for (const FVector& Point : ApparitionPoints)
		{
			const float Distance = FVector::Dist(Point, Target->GetActorLocation());
			if (Distance > 600.f && Distance < 2200.f && !IsObserved(Point + FVector(0.f, 0.f, 100.f), 3000.f))
			{
				Candidates.Add(Point);
			}
		}
		if (Candidates.Num() == 0)
		{
			return false;
		}
		// The lights go first: a few seconds of warning (and the noise meter pins), then it comes.
		const FVector Start = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
		LastHuntTime = GetWorld()->GetTimeSeconds();
		HuntWarningUntil = GetServerTime() + HuntWarningSeconds;
		EventAt = Start;
		MulticastFlicker(Target->GetActorLocation(), 2500.f, HuntWarningSeconds + 1.f);
		TWeakObjectPtr<AHHApparition> WeakGhost(Ghost);
		TWeakObjectPtr<APawn> WeakTarget(Target);
		GetWorldTimerManager().SetTimer(HuntTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakGhost, WeakTarget, Start]()
		{
			AHHApparition* Hunter = WeakGhost.Get();
			if (!bActive || !Hunter || Hunter->GetMode() != EHHApparitionMode::Hidden || GetPawnsInside().Num() == 0)
			{
				return;
			}
			// The chosen victim may have left; the apparition picks whoever is still inside.
			Hunter->Hunt(Start, WeakTarget.Get(), 24.f);
			if (StingerSounds.Num() > 0)
			{
				MulticastStinger(StingerSounds[Stream.RandRange(0, StingerSounds.Num() - 1)]);
			}
		}), HuntWarningSeconds, false);
		return true;
	}

	// Glimpse: a point in front of somebody, at a distance, in line of sight.
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		const APawn* Viewer = Inside[Stream.RandRange(0, Inside.Num() - 1)];
		FVector Eye;
		FRotator View;
		Viewer->GetActorEyesViewPoint(Eye, View);
		const FVector Point = ApparitionPoints[Stream.RandRange(0, ApparitionPoints.Num() - 1)];
		const FVector ToPoint = Point + FVector(0.f, 0.f, 100.f) - Eye;
		const float Distance = ToPoint.Size();
		if (Distance < 550.f || Distance > 2200.f || FVector::DotProduct(View.Vector(), ToPoint / Distance) < 0.35f)
		{
			continue;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HHHauntGlimpse), false, Viewer);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point + FVector(0.f, 0.f, 100.f), ECC_Visibility, Params))
		{
			continue;
		}
		Ghost->Glimpse(Point, Viewer->GetActorLocation(), Stream.FRandRange(2.f, 3.5f));
		EventAt = Point;
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------------------
// The new repertoire

APawn* AHHHauntDirector::PickVictim() const
{
	TArray<APawn*> Candidates;
	for (APawn* Pawn : GetPawnsInside())
	{
		const AHHCharacter* Character = Cast<AHHCharacter>(Pawn);
		if (!Character || !Character->IsIncapacitated())
		{
			Candidates.Add(Pawn);
		}
	}
	return Candidates.Num() > 0 ? Candidates[Stream.RandRange(0, Candidates.Num() - 1)] : nullptr;
}

bool AHHHauntDirector::DoVoice()
{
	if (VoiceSounds.Num() == 0)
	{
		return false;
	}
	USoundBase* Sound = VoiceSounds[Stream.RandRange(0, VoiceSounds.Num() - 1)];
	if (!Sound)
	{
		return false;
	}
	const FString Name = Sound->GetName();
	FText Caption = LOCTEXT("VoiceAny", "[A voice somewhere in the house]");
	if (Name.Contains(TEXT("Hum")))
	{
		Caption = LOCTEXT("VoiceHum", "[Someone is humming a lullaby]");
	}
	else if (Name.Contains(TEXT("Giggle")))
	{
		Caption = LOCTEXT("VoiceGiggle", "[A child giggles]");
	}
	else if (Name.Contains(TEXT("Crying")))
	{
		Caption = LOCTEXT("VoiceCry", "[Someone is crying quietly]");
	}
	else if (Name.Contains(TEXT("Piano")))
	{
		Caption = LOCTEXT("VoicePiano", "[Piano keys, in another room]");
	}
	return DoSoundInOtherRoom({ Sound }, Caption, 600.f);
}

bool AHHHauntDirector::DoScratch()
{
	APawn* Victim = PickVictim();
	if (!Victim || ScratchSounds.Num() == 0)
	{
		return false;
	}
	// Inside the wall next to them: the nearest sound point a couple of metres away.
	FVector Best = FVector::ZeroVector;
	float BestDistance = TNumericLimits<float>::Max();
	for (const FVector& Point : SoundPoints)
	{
		const float Distance = FVector::Dist(Point, Victim->GetActorLocation());
		if (Distance > 180.f && Distance < 800.f && Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Point;
		}
	}
	if (BestDistance == TNumericLimits<float>::Max())
	{
		return false;
	}
	PlaySoundEverywhere(ScratchSounds[Stream.RandRange(0, ScratchSounds.Num() - 1)], Best, LOCTEXT("Scratch", "[Scratching inside the wall]"));
	HitPsyche(Victim, 3.f);
	EventAt = Best;
	return true;
}

bool AHHHauntDirector::DoStepsBehind()
{
	APawn* Victim = PickVictim();
	if (!Victim || CloseStepSounds.Num() == 0 || GetWorldTimerManager().IsTimerActive(StepTimer))
	{
		return false;
	}
	StepVictim = Victim;
	StepIndex = 0;
	EventAt = Victim->GetActorLocation() - Victim->GetActorForwardVector() * 400.f;
	StepBehind();
	GetWorldTimerManager().SetTimer(StepTimer, this, &AHHHauntDirector::StepBehind, 0.62f, true);
	return true;
}

void AHHHauntDirector::StepBehind()
{
	APawn* Victim = StepVictim.Get();
	const AHHPlayerState* State = Victim ? Victim->GetPlayerState<AHHPlayerState>() : nullptr;
	if (!Victim || !State || !State->IsInsideHouse() || !bActive)
	{
		GetWorldTimerManager().ClearTimer(StepTimer);
		return;
	}
	const int32 Steps = 6;
	// Always behind them, wherever they turn: from seven metres to an arm's length.
	const float Back = FMath::Lerp(700.f, 110.f, static_cast<float>(StepIndex) / (Steps - 1));
	const FVector Where = Victim->GetActorLocation() - Victim->GetActorForwardVector() * Back - FVector(0.f, 0.f, 70.f);
	if (StepIndex < Steps)
	{
		PlaySoundEverywhere(CloseStepSounds[StepIndex % CloseStepSounds.Num()], Where,
			StepIndex == 0 ? LOCTEXT("StepsBehind", "[Slow footsteps behind you]") : FText::GetEmpty(), 0.9f);
		++StepIndex;
		return;
	}
	GetWorldTimerManager().ClearTimer(StepTimer);
	if (BreathSounds.Num() > 0)
	{
		PlaySoundEverywhere(BreathSounds[Stream.RandRange(0, BreathSounds.Num() - 1)], Where + FVector(0.f, 0.f, 90.f),
			LOCTEXT("BreathNeck", "[Breathing, right at your neck]"));
	}
	HitPsyche(Victim, 6.f);
}

bool AHHHauntDirector::DoTouch()
{
	APawn* Victim = PickVictim();
	AHHPlayerController* PC = Victim ? Cast<AHHPlayerController>(Victim->GetController()) : nullptr;
	if (!PC)
	{
		return false;
	}
	PC->ClientHauntTouch(TouchSound);
	HitPsyche(Victim, 6.f);
	EventAt = Victim->GetActorLocation();
	return true;
}

bool AHHHauntDirector::DoThrow()
{
	TArray<AHHHauntThrowable*> Candidates;
	for (const TWeakObjectPtr<AHHHauntThrowable>& ThrowablePtr : Throwables)
	{
		AHHHauntThrowable* Throwable = ThrowablePtr.Get();
		const float Nearest = Throwable ? DistanceToNearestPawn(Throwable->GetActorLocation()) : 0.f;
		if (Throwable && Throwable->CanBeThrown() && Nearest > 150.f && Nearest < 1000.f)
		{
			Candidates.Add(Throwable);
		}
	}
	APawn* Victim = PickVictim();
	if (Candidates.Num() == 0 || !Victim)
	{
		return false;
	}
	// Towards whoever is nearest, landing at their feet (not on them).
	AHHHauntThrowable* Throwable = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	APawn* Target = Victim;
	float Nearest = TNumericLimits<float>::Max();
	for (APawn* Pawn : GetPawnsInside())
	{
		const float Distance = FVector::Dist(Pawn->GetActorLocation(), Throwable->GetActorLocation());
		if (Distance < Nearest)
		{
			Nearest = Distance;
			Target = Pawn;
		}
	}
	const FVector Toward = (Target->GetActorLocation() - Throwable->GetActorLocation()).GetSafeNormal2D();
	FVector Landing = Target->GetActorLocation() - Toward * Stream.FRandRange(60.f, 140.f) + FVector(Stream.FRandRange(-40.f, 40.f), Stream.FRandRange(-40.f, 40.f), 0.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHHauntThrow), false, Target);
	Params.AddIgnoredActor(Throwable);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Landing + FVector(0.f, 0.f, 50.f), Landing - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
	{
		Landing = Hit.ImpactPoint + FVector(0.f, 0.f, 3.f);
	}
	Throwable->ThrowTo(Landing, FMath::Clamp(Nearest / 900.f, 0.35f, 0.8f));
	HitPsycheNear(Landing, 900.f, 4.f);
	AddAnger(1.f);
	EventAt = Throwable->GetActorLocation();
	return true;
}

bool AHHHauntDirector::DoMark()
{
	TArray<AHHHauntMark*> Candidates;
	for (const TWeakObjectPtr<AHHHauntMark>& MarkPtr : Marks)
	{
		AHHHauntMark* Mark = MarkPtr.Get();
		if (!Mark || Mark->IsRevealed() || Mark->GetMinStage() > Stage)
		{
			continue;
		}
		const float Nearest = DistanceToNearestPawn(Mark->GetActorLocation());
		if (Nearest > 250.f && Nearest < 1800.f && !IsObserved(Mark->GetActorLocation(), 2500.f))
		{
			Candidates.Add(Mark);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	AHHHauntMark* Mark = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	Mark->Reveal();
	HitPsycheNear(Mark->GetActorLocation(), 1200.f, 3.f);
	EventAt = Mark->GetActorLocation();
	return true;
}

bool AHHHauntDirector::DoManifest()
{
	AHHApparition* Ghost = Apparition.Get();
	APawn* Victim = PickVictim();
	if (!Ghost || !Victim || Ghost->GetMode() != EHHApparitionMode::Hidden)
	{
		return false;
	}
	FVector Eye;
	FRotator View;
	Victim->GetActorEyesViewPoint(Eye, View);
	const FVector Ahead = FRotator(0.f, View.Yaw, 0.f).Vector();
	const FVector Point = Eye + Ahead * 170.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHHauntManifest), false, Victim);
	Params.AddIgnoredActor(Ghost);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point + Ahead * 40.f, ECC_Visibility, Params))
	{
		return false;	// A wall right in front of them: no room to stand.
	}
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Point, Point - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
	{
		return false;
	}
	Ghost->Glimpse(Hit.ImpactPoint, Victim->GetActorLocation(), 0.9f, true);
	PlaySoundEverywhere(GrowlSound, Hit.ImpactPoint, LOCTEXT("Growl", "[A low growl, right in front of you]"));
	if (StingerSounds.Num() > 0)
	{
		MulticastStinger(StingerSounds[Stream.RandRange(0, StingerSounds.Num() - 1)]);
	}
	MulticastFlicker(Hit.ImpactPoint, 900.f, 1.2f);
	HitPsyche(Victim, 12.f);
	EventAt = Hit.ImpactPoint;
	return true;
}

bool AHHHauntDirector::DoShadowRun()
{
	AHHApparition* Ghost = Apparition.Get();
	APawn* Viewer = PickVictim();
	if (!Ghost || !Viewer || Ghost->GetMode() != EHHApparitionMode::Hidden || ApparitionPoints.Num() < 2)
	{
		return false;
	}
	FVector Eye;
	FRotator View;
	Viewer->GetActorEyesViewPoint(Eye, View);
	for (int32 Attempt = 0; Attempt < 12; ++Attempt)
	{
		const FVector A = ApparitionPoints[Stream.RandRange(0, ApparitionPoints.Num() - 1)];
		const FVector B = ApparitionPoints[Stream.RandRange(0, ApparitionPoints.Num() - 1)];
		const float Length = FVector::Dist(A, B);
		const FVector Mid = (A + B) * 0.5f + FVector(0.f, 0.f, 100.f);
		const FVector ToMid = Mid - Eye;
		const float Distance = ToMid.Size();
		if (Length < 250.f || Length > 1000.f || FMath::Abs(A.Z - B.Z) > 100.f || Distance < 400.f || Distance > 1600.f
			|| FVector::DotProduct(View.Vector(), ToMid / Distance) < 0.5f)
		{
			continue;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HHHauntShadow), false, Viewer);
		Params.AddIgnoredActor(Ghost);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Mid, ECC_Visibility, Params))
		{
			continue;
		}
		Ghost->Dash(A, B, 650.f);
		EventAt = Mid;
		return true;
	}
	return false;
}

bool AHHHauntDirector::DoRock()
{
	TArray<AHHHauntProp*> Candidates;
	for (const TWeakObjectPtr<AHHHauntProp>& PropPtr : Props)
	{
		AHHHauntProp* Prop = PropPtr.Get();
		if (Prop && Prop->CanRock() && DistanceToNearestPawn(Prop->GetActorLocation()) < 2200.f)
		{
			Candidates.Add(Prop);
		}
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}
	AHHHauntProp* Prop = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
	Prop->Rock(Stream.FRandRange(10.f, 16.f));
	HitPsycheNear(Prop->GetActorLocation(), 1200.f, 2.f);
	EventAt = Prop->GetActorLocation();
	return true;
}

void AHHHauntDirector::OnApparitionSeen(APawn* Witness)
{
	HitPsyche(Witness, 9.f);
	AddAnger(1.5f);
}

void AHHHauntDirector::OnApparitionCaught(APawn* Victim)
{
	// The game mode knocks them down (or finishes them) and calms the house.
	if (AHHHeistGameMode* HeistMode = GetWorld()->GetAuthGameMode<AHHHeistGameMode>())
	{
		HeistMode->HandleEntityCatch(Victim);
	}
	else
	{
		OnPlayerCaught();
	}
}

#undef LOCTEXT_NAMESPACE
