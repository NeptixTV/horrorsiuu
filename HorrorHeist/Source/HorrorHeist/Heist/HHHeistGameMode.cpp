#include "Heist/HHHeistGameMode.h"
#include "Heist/HHHeistGameState.h"
#include "Heist/HHHauntDirector.h"
#include "Heist/HHHouseVolume.h"
#include "Heist/HHLootActor.h"
#include "Heist/HHVanPoint.h"
#include "Interaction/HHDoorActor.h"
#include "Player/HHCharacter.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Player/HHInventoryComponent.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHLootDefinition.h"
#include "Data/HHMissionDefinition.h"
#include "Core/HHDeveloperSettings.h"
#include "Core/HHGameInstance.h"
#include "Core/HHNoiseSubsystem.h"
#include "World/HHTimeOfDay.h"
#include "HorrorHeist.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "HHHeist"

namespace HHHeistIds
{
	static const FName Enter(TEXT("Enter"));
	static const FName Haul(TEXT("Haul"));
	static const FName Target(TEXT("Target"));
	static const FName Key(TEXT("Key"));
	static const FName Escape(TEXT("Escape"));
	static const FName Police(TEXT("Police"));
	static const FName WakePointTag(TEXT("HH_WakePoint"));
}

AHHHeistGameMode::AHHHeistGameMode()
{
	GameStateClass = AHHHeistGameState::StaticClass();
}

AHHHeistGameState* AHHHeistGameMode::GetHeistState() const
{
	return GetGameState<AHHHeistGameState>();
}

const UHHMissionDefinition* AHHHeistGameMode::GetMission() const
{
	return Mission.Get();
}

// ---------------------------------------------------------------------------------------
// Setup

void AHHHeistGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	ResolveMission(Options);
}

void AHHHeistGameMode::ResolveMission(const FString& Options)
{
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	if (!Registry)
	{
		return;
	}

	// 1) ?Mission= on the travel URL, 2) the job the game instance remembers, 3) the job whose
	// map this is (starting the map directly in the editor).
	const FString FromUrl = UGameplayStatics::ParseOption(Options, TEXT("Mission"));
	const UHHMissionDefinition* Found = FromUrl.IsEmpty() ? nullptr : Registry->FindMission(FName(*FromUrl));
	if (!Found)
	{
		if (const UHHGameInstance* GameInstance = GetGameInstance<UHHGameInstance>())
		{
			Found = Registry->FindMission(GameInstance->GetActiveMission());
		}
	}
	const FString ThisMap = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	if (Found && Found->Map.ToSoftObjectPath().GetLongPackageName() != ThisMap)
	{
		Found = nullptr;	// Remembered job belongs to another map (e.g. Play-In-Editor of a different job).
	}
	if (!Found)
	{
		for (const UHHMissionDefinition* Candidate : Registry->GetMissions())
		{
			if (Candidate && !Candidate->Map.IsNull() && Candidate->Map.ToSoftObjectPath().GetLongPackageName() == ThisMap)
			{
				Found = Candidate;
				break;
			}
		}
	}

	Mission = Found;
	if (Found)
	{
		MissionId = Found->GetMissionId();
		MinimumHaul = Found->MinimumHaul;
		UE_LOG(LogHorrorHeist, Log, TEXT("Heist: playing %s on %s (minimum haul %d)."), *MissionId.ToString(), *ThisMap, MinimumHaul);
	}
	else
	{
		UE_LOG(LogHorrorHeist, Warning, TEXT("Heist: no mission data points at %s; running with default rules."), *ThisMap);
	}
}

void AHHHeistGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	for (TActorIterator<AHHHouseVolume> It(World); It; ++It)
	{
		HouseVolumes.Add(*It);
	}
	for (TActorIterator<AHHVanPoint> It(World); It; ++It)
	{
		if (It->GetPointType() == EHHVanPointType::Drive)
		{
			DrivePoint = *It;
		}
	}
	Haunt = AHHHauntDirector::Get(this);
	if (AHHHauntDirector* Director = Haunt.Get())
	{
		Director->SetAggression(Mission.IsValid() ? Mission->HauntAggression : 1.f);
	}
	if (HouseVolumes.Num() == 0 || !DrivePoint.IsValid() || !Haunt.IsValid())
	{
		UE_LOG(LogHorrorHeist, Warning, TEXT("Heist map is incomplete: %d house volumes, drive point %s, haunt director %s."),
			HouseVolumes.Num(), DrivePoint.IsValid() ? TEXT("yes") : TEXT("NO"), Haunt.IsValid() ? TEXT("yes") : TEXT("NO"));
	}

	if (AHHHeistGameState* HeistState = GetHeistState())
	{
		HeistState->SetSelectedMission(MissionId);
		HeistState->StartClock();
	}
	StartTime = World->GetTimeSeconds();
	SetupObjectives();

	if (UHHNoiseSubsystem* Noise = World->GetSubsystem<UHHNoiseSubsystem>())
	{
		NoiseHandle = Noise->OnNoise.AddUObject(this, &AHHHeistGameMode::HandleNoise);
	}
	GetWorldTimerManager().SetTimer(RulesTimer, this, &AHHHeistGameMode::TickRules, 0.5f, true, 0.5f);
}

void AHHHeistGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHHNoiseSubsystem* Noise = GetWorld() ? GetWorld()->GetSubsystem<UHHNoiseSubsystem>() : nullptr)
	{
		Noise->OnNoise.Remove(NoiseHandle);
	}
	GetWorldTimerManager().ClearTimer(RulesTimer);
	GetWorldTimerManager().ClearTimer(ReturnTimer);
	Super::EndPlay(EndPlayReason);
}

void AHHHeistGameMode::SetupObjectives()
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState)
	{
		return;
	}

	TArray<FHHObjective> List;
	auto Add = [&List](FName Id, const FText& Text, bool bOptional)
	{
		FHHObjective Objective;
		Objective.Id = Id;
		Objective.Text = Text;
		Objective.bOptional = bOptional;
		List.Add(Objective);
	};

	Add(HHHeistIds::Enter, LOCTEXT("ObjEnter", "Find a way into the house"), false);
	Add(HHHeistIds::Haul, FText::GetEmpty(), false);

	const UHHMissionDefinition* Job = Mission.Get();
	if (Job && !Job->PrimaryTargetItem.IsNone())
	{
		for (TActorIterator<AHHLootActor> It(GetWorld()); It; ++It)
		{
			if (It->GetItemId() == Job->PrimaryTargetItem)
			{
				bHasTargetObjective = true;
				break;
			}
		}
		if (bHasTargetObjective)
		{
			Add(HHHeistIds::Target, FText::Format(LOCTEXT("ObjTarget", "Optional: steal the {0}"), Job->PrimaryTarget), true);
		}
	}
	for (TActorIterator<AHHLootActor> It(GetWorld()); It; ++It)
	{
		if (It->IsKey())
		{
			bHasKeyObjective = true;
			break;
		}
	}
	if (bHasKeyObjective)
	{
		Add(HHHeistIds::Key, LOCTEXT("ObjKey", "Optional: find the spare key"), true);
	}

	HeistState->SetObjectives(List);
	HeistState->SetSecured(0, MinimumHaul);
	UpdateHaulObjective();
}

void AHHHeistGameMode::UpdateHaulObjective()
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState)
	{
		return;
	}
	const FText Text = FText::Format(LOCTEXT("ObjHaul", "Stash valuables in the van: {0} / {1}"),
		HHText::Money(HeistState->GetSecuredValue()), HHText::Money(MinimumHaul));
	const bool bDone = HeistState->GetSecuredValue() >= MinimumHaul;
	HeistState->UpdateObjective(HHHeistIds::Haul, &Text, &bDone);
}

// ---------------------------------------------------------------------------------------
// Players

void AHHHeistGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	if (bEnded)
	{
		ErrorMessage = TEXT("The crew is already on its way back.");
		return;
	}
	if (GetNumPlayers() >= UHHDeveloperSettings::Get()->MaxCrewSize)
	{
		ErrorMessage = TEXT("The crew is full.");
	}
}

void AHHHeistGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (AHHPlayerState* State = NewPlayer ? NewPlayer->GetPlayerState<AHHPlayerState>() : nullptr)
	{
		State->SetPsyche(100.f);
	}
}

void AHHHeistGameMode::HandleProfileReceived(AHHPlayerController* Controller)
{
	AHHPlayerState* State = Controller ? Controller->GetPlayerState<AHHPlayerState>() : nullptr;
	if (!State || InitializedInventories.Contains(State))
	{
		return;
	}
	InitializedInventories.Add(State);
	if (UHHInventoryComponent* Inventory = State->GetInventory())
	{
		Inventory->InitializeFromLoadout(State->GetEquipment());
		Inventory->SetEnabled(true);
	}
}

void AHHHeistGameMode::Logout(AController* Exiting)
{
	const AHHPlayerState* State = Exiting ? Exiting->GetPlayerState<AHHPlayerState>() : nullptr;
	const FString Name = State ? State->GetPlayerName() : FString();
	Super::Logout(Exiting);
	if (AHHHeistGameState* HeistState = GetHeistState())
	{
		if (!Name.IsEmpty())
		{
			HeistState->MulticastCrewMessage(FText::Format(LOCTEXT("Left", "{0} left the job."), FText::FromString(Name)), EHHNotifyType::Crew);
		}
	}
}

// ---------------------------------------------------------------------------------------
// Rules tick (twice a second)

void AHHHeistGameMode::TickRules()
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState || bEnded)
	{
		return;
	}
	const float Dt = 0.5f;
	const int32 Stage = Haunt.IsValid() ? Haunt->GetStage() : 0;
	const TArray<AHHPlayerState*> Crew = HeistState->GetCrew();
	const AHHVanPoint* Van = DrivePoint.Get();

	int32 AtVan = 0;
	TArray<APawn*> Blackouts;
	for (AHHPlayerState* State : Crew)
	{
		APawn* Pawn = State->GetPawn();
		if (!Pawn)
		{
			continue;
		}
		const FVector Location = Pawn->GetActorLocation();

		bool bInside = false;
		for (const TWeakObjectPtr<AHHHouseVolume>& Volume : HouseVolumes)
		{
			if (Volume.IsValid() && Volume->ContainsPoint(Location))
			{
				bInside = true;
				break;
			}
		}
		State->SetInsideHouse(bInside);

		if (bInside && !bEntered)
		{
			bEntered = true;
			const bool bDone = true;
			HeistState->UpdateObjective(HHHeistIds::Enter, nullptr, &bDone);
			HeistState->SetHeistPhase(EHHHeistPhase::Inside);
			HeistState->MulticastCrewMessage(FText::Format(LOCTEXT("InsideMsg", "{0} is inside. Keep it quiet."), FText::FromString(State->GetPlayerName())), EHHNotifyType::Info);
		}

		// Psyche: the house wears you down, the street and the crew bring you back.
		float Psyche = State->GetPsyche();
		if (bInside)
		{
			float Drain = PsycheDrainInside * (1.f + 0.3f * Stage);
			const AHHCharacter* Character = Cast<AHHCharacter>(Pawn);
			if (Character && !Character->IsFlashlightOn())
			{
				Drain *= 1.5f;
			}
			for (const AHHPlayerState* Other : Crew)
			{
				const APawn* OtherPawn = Other != State ? Other->GetPawn() : nullptr;
				if (OtherPawn && FVector::DistSquared(OtherPawn->GetActorLocation(), Location) < FMath::Square(500.f))
				{
					Drain *= 0.6f;
					break;
				}
			}
			Psyche -= Drain * Dt;
		}
		else
		{
			Psyche += PsycheRecoverOutside * Dt;
		}
		State->SetPsyche(Psyche);
		if (bInside && State->GetPsyche() <= 0.f)
		{
			Blackouts.Add(Pawn);
		}

		if (Van && FVector::DistSquared(Van->GetActorLocation(), Location) < FMath::Square(Van->GetCrewRadius()))
		{
			++AtVan;
		}
	}
	HeistState->SetVanCount(AtVan, Crew.Num());

	for (APawn* Pawn : Blackouts)
	{
		HandlePlayerBlackout(Pawn, false);
	}

	if (HeistState->IsPoliceCalled())
	{
		if (HeistState->GetPoliceRemaining() <= 0.f)
		{
			HeistState->MulticastCrewMessage(LOCTEXT("PoliceArrived", "Blue lights at the end of the street. Too late."), EHHNotifyType::Error);
			EndMission(EHHMissionOutcome::Caught);
		}
	}
	else
	{
		Alert = FMath::Max(0.f, Alert - AlertDecay * Dt);
		HeistState->SetAlert(Alert);
	}
}

// ---------------------------------------------------------------------------------------
// Noise and alert

void AHHHeistGameMode::HandleNoise(const FVector& Location, float Loudness, AActor* NoiseMaker, FName Tag)
{
	if (bEnded)
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(NoiseMaker);
	const AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
	const bool bInside = State && State->IsInsideHouse();
	AHHHauntDirector* Director = Haunt.Get();

	static const FName ForcedEntry(TEXT("ForcedEntry"));
	static const FName Footstep(TEXT("Footstep"));
	static const FName DoorRattle(TEXT("DoorRattle"));

	if (Tag == ForcedEntry)
	{
		RaiseAlert(42.f, LOCTEXT("AlertForced", "Somebody must have heard that."));
		if (Director)
		{
			Director->AddAnger(6.f);
		}
		return;
	}
	if (Tag == Footstep)
	{
		// Running around outside is what neighbours notice; inside, the house listens.
		if (!bInside && Loudness > 0.7f)
		{
			RaiseAlert(0.25f, FText::GetEmpty());
		}
		else if (bInside && Director)
		{
			Director->AddAnger(0.04f * Loudness);
		}
		return;
	}
	if (Tag == DoorRattle)
	{
		RaiseAlert(1.5f, FText::GetEmpty());
		return;
	}

	if (Loudness >= 0.6f)
	{
		RaiseAlert(Loudness * 10.f, FText::GetEmpty());
	}
	if (bInside && Director)
	{
		Director->AddAnger(Loudness * 0.8f);
	}
}

void AHHHeistGameMode::RaiseAlert(float Amount, const FText& Reason)
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState || HeistState->IsPoliceCalled() || Amount <= 0.f)
	{
		return;
	}
	const float Before = Alert;
	Alert = FMath::Clamp(Alert + Amount, 0.f, 100.f);
	HeistState->SetAlert(Alert);

	if (Alert >= 100.f)
	{
		const float Seconds = UHHDeveloperSettings::Get()->PoliceResponseSeconds;
		HeistState->CallPolice(Seconds);
		HeistState->MulticastCrewMessage(FText::Format(LOCTEXT("PoliceCalled", "A neighbour called the police. You have about {0} seconds."), FText::AsNumber(FMath::RoundToInt(Seconds))), EHHNotifyType::Error);
		TArray<FHHObjective> List = HeistState->GetObjectives();
		FHHObjective Objective;
		Objective.Id = HHHeistIds::Police;
		Objective.Text = LOCTEXT("ObjPolice", "Police on the way - get everyone to the van and drive!");
		List.Insert(Objective, 0);
		HeistState->SetObjectives(List);
	}
	else if (Before < 50.f && Alert >= 50.f)
	{
		HeistState->MulticastCrewMessage(LOCTEXT("Suspicious", "A light went on next door. The neighbours are getting suspicious."), EHHNotifyType::Warning);
	}
	else if (!Reason.IsEmpty() && Amount >= 20.f)
	{
		HeistState->MulticastCrewMessage(Reason, EHHNotifyType::Warning);
	}
}

// ---------------------------------------------------------------------------------------
// World hooks

void AHHHeistGameMode::HandleItemPickedUp(AController* By, const FHHInventorySlot& Item, AActor* Source)
{
	if (Item.Kind != EHHInventoryKind::Loot)
	{
		return;
	}
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	const UHHLootDefinition* Loot = Registry ? Registry->FindLoot(Item.ItemId) : nullptr;
	if (AHHHauntDirector* Director = Haunt.Get())
	{
		Director->OnLootTaken(Loot, Source ? Source->GetActorLocation() : FVector::ZeroVector);
	}
	if (Loot && Loot->bHeavy)
	{
		if (UHHNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UHHNoiseSubsystem>())
		{
			Noise->ReportNoise(Source ? Source->GetActorLocation() : FVector::ZeroVector, 0.35f, By ? By->GetPawn() : nullptr, TEXT("HeavyLift"));
		}
	}
	const UHHMissionDefinition* Job = Mission.Get();
	AHHHeistGameState* HeistState = GetHeistState();
	if (Job && HeistState && Item.ItemId == Job->PrimaryTargetItem && bHasTargetObjective && !bTargetStashed)
	{
		const FText Text = FText::Format(LOCTEXT("ObjTargetCarry", "Optional: get the {0} to the van"), Job->PrimaryTarget);
		HeistState->UpdateObjective(HHHeistIds::Target, &Text, nullptr);
	}
}

void AHHHeistGameMode::HandleKeyFound(AController* By, FName KeyId, const FText& KeyName)
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState)
	{
		return;
	}
	HeistState->AddKey(KeyId);
	const bool bDone = true;
	HeistState->UpdateObjective(HHHeistIds::Key, nullptr, &bDone);
	const AHHPlayerState* State = By ? By->GetPlayerState<AHHPlayerState>() : nullptr;
	HeistState->MulticastCrewMessage(FText::Format(LOCTEXT("KeyFound", "{0} found {1}."),
		State ? FText::FromString(State->GetPlayerName()) : LOCTEXT("Someone", "Someone"),
		KeyName.IsEmpty() ? LOCTEXT("AKey", "a key") : KeyName), EHHNotifyType::Success);
}

void AHHHeistGameMode::HandleDoorUnlocked(AController* By, AHHDoorActor* Door, bool bForced, bool bFromInside)
{
	if (bForced)
	{
		if (AHHHauntDirector* Director = Haunt.Get())
		{
			Director->AddAnger(2.f);
		}
	}
}

void AHHHeistGameMode::HandleStash(AController* By, USoundBase* Sound, const FVector& Location)
{
	AHHPlayerState* State = By ? By->GetPlayerState<AHHPlayerState>() : nullptr;
	UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	AHHHeistGameState* HeistState = GetHeistState();
	if (!Inventory || !HeistState || bEnded)
	{
		return;
	}

	const TArray<FHHInventorySlot> Items = Inventory->RemoveAllLoot();
	if (Items.Num() == 0)
	{
		return;
	}

	int32 Added = 0;
	const UHHMissionDefinition* Job = Mission.Get();
	for (const FHHInventorySlot& Item : Items)
	{
		FHHLootRecord Record;
		Record.ItemId = Item.ItemId;
		Record.Value = Item.Value;
		Record.StolenBy = State->GetPlayerName();
		Stashed.Add(Record);
		Added += Item.Value;
		if (Job && Item.ItemId == Job->PrimaryTargetItem && !bTargetStashed)
		{
			bTargetStashed = true;
			const bool bDone = true;
			const FText Text = FText::Format(LOCTEXT("ObjTargetDone", "Optional: {0} is in the van"), Job->PrimaryTarget);
			HeistState->UpdateObjective(HHHeistIds::Target, &Text, &bDone);
		}
	}
	State->AddStashedValue(Added);

	int32 Total = 0;
	for (const FHHLootRecord& Record : Stashed)
	{
		Total += Record.Value;
	}
	HeistState->SetSecured(Total, MinimumHaul);
	UpdateHaulObjective();
	if (Sound)
	{
		HeistState->MulticastSoundAt(Sound, Location, 1.f);
	}
	if (AHHPlayerController* PC = Cast<AHHPlayerController>(By))
	{
		PC->ClientNotify(FText::Format(LOCTEXT("Stashed", "Stashed {0} {0}|plural(one=item,other=items) worth {1}."), Items.Num(), HHText::Money(Added)), EHHNotifyType::Success);
	}

	if (Total >= MinimumHaul && HeistState->GetHeistPhase() != EHHHeistPhase::Escape)
	{
		HeistState->SetHeistPhase(EHHHeistPhase::Escape);
		TArray<FHHObjective> List = HeistState->GetObjectives();
		FHHObjective Objective;
		Objective.Id = HHHeistIds::Escape;
		Objective.Text = LOCTEXT("ObjEscape", "Get everyone to the van and drive away - or push your luck");
		List.Add(Objective);
		HeistState->SetObjectives(List);
		HeistState->MulticastCrewMessage(LOCTEXT("EnoughMsg", "That's the haul the client asked for. Leave whenever you're ready."), EHHNotifyType::Success);
	}
}

void AHHHeistGameMode::HandleDriveAway(AController* By, USoundBase* Sound, const FVector& Location)
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState || bEnded)
	{
		return;
	}
	if (HeistState->GetCrewAtVan() < HeistState->GetCrewTotal())
	{
		if (AHHPlayerController* PC = Cast<AHHPlayerController>(By))
		{
			PC->ClientNotify(LOCTEXT("NotEveryone", "Not everyone is at the van yet."), EHHNotifyType::Warning);
		}
		return;
	}
	if (Sound)
	{
		HeistState->MulticastSoundAt(Sound, Location, 1.f);
	}
	EndMission(HeistState->GetSecuredValue() >= MinimumHaul ? EHHMissionOutcome::Success : EHHMissionOutcome::Partial);
}

AActor* AHHHeistGameMode::FindWakePoint(const FVector& Near) const
{
	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(HHHeistIds::WakePointTag))
		{
			const float Distance = FVector::DistSquared(It->GetActorLocation(), Near);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = *It;
			}
		}
	}
	if (!Best)
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			return *It;
		}
	}
	return Best;
}

void AHHHeistGameMode::HandlePlayerBlackout(APawn* Pawn, bool bCaughtByEntity)
{
	AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
	AHHHeistGameState* HeistState = GetHeistState();
	if (!State || !HeistState || bEnded)
	{
		return;
	}

	// Everything in the bag stays where they fell (the crew can still fetch it).
	if (UHHInventoryComponent* Inventory = State->GetInventory())
	{
		const TArray<FHHInventorySlot> Items = Inventory->RemoveAllLoot();
		const FVector Center = Pawn->GetActorLocation();
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			const float Angle = Index * 2.f * UE_PI / FMath::Max(1, Items.Num());
			FVector Spot = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 45.f;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(HHBlackoutDrop), false, Pawn);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 30.f), Spot - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
			{
				Spot = Hit.ImpactPoint;
			}
			AHHLootActor::SpawnDropped(GetWorld(), Items[Index], FTransform(FRotator(0.f, FMath::RandRange(0.f, 360.f), 0.f), Spot));
		}
	}

	if (AActor* Wake = FindWakePoint(Pawn->GetActorLocation()))
	{
		Pawn->TeleportTo(Wake->GetActorLocation(), Wake->GetActorRotation(), false, true);
		if (AController* Controller = Pawn->GetController())
		{
			Controller->SetControlRotation(Wake->GetActorRotation());
		}
	}
	State->SetPsyche(40.f);
	State->SetInsideHouse(false);

	if (AHHPlayerController* PC = Cast<AHHPlayerController>(Pawn->GetController()))
	{
		PC->ClientBlackout(bCaughtByEntity);
	}
	HeistState->MulticastCrewMessage(FText::Format(bCaughtByEntity
		? LOCTEXT("CaughtByIt", "Something got {0}. They woke up outside - their loot is still in the house.")
		: LOCTEXT("Fainted", "{0} panicked and blacked out. They woke up outside - their loot is still in the house."),
		FText::FromString(State->GetPlayerName())), EHHNotifyType::Error);

	if (!bCaughtByEntity)
	{
		if (AHHHauntDirector* Director = Haunt.Get())
		{
			Director->OnPlayerCaught();
		}
	}
}

void AHHHeistGameMode::HandleAbandon(AHHPlayerController* Requester)
{
	const AHHPlayerState* State = Requester ? Requester->GetPlayerState<AHHPlayerState>() : nullptr;
	if (!State)
	{
		return;
	}
	if (!State->IsCrewLeader())
	{
		Requester->ClientNotify(LOCTEXT("NotLeaderAbandon", "Only the crew leader can call the job off."), EHHNotifyType::Warning);
		return;
	}
	EndMission(EHHMissionOutcome::Abandoned);
}

void AHHHeistGameMode::DebugEnd(EHHMissionOutcome Outcome)
{
	EndMission(Outcome);
}

// ---------------------------------------------------------------------------------------
// End of the job

void AHHHeistGameMode::EndMission(EHHMissionOutcome Outcome)
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (bEnded || !HeistState)
	{
		return;
	}
	bEnded = true;
	GetWorldTimerManager().ClearTimer(RulesTimer);
	if (AHHHauntDirector* Director = Haunt.Get())
	{
		Director->SetActive(false);
	}

	const bool bGotAway = Outcome == EHHMissionOutcome::Success || Outcome == EHHMissionOutcome::Partial;
	const TArray<AHHPlayerState*> Crew = HeistState->GetCrew();

	int32 Secured = 0;
	for (const FHHLootRecord& Record : Stashed)
	{
		Secured += Record.Value;
	}
	int32 Carried = 0;
	for (const AHHPlayerState* State : Crew)
	{
		if (const UHHInventoryComponent* Inventory = State->GetInventory())
		{
			Carried += Inventory->GetCarriedLootValue();
		}
	}

	FHHMissionSummary Summary;
	Summary.Outcome = Outcome;
	Summary.MissionId = MissionId;
	Summary.SecuredValue = bGotAway ? Secured : 0;
	Summary.LostValue = Carried + (bGotAway ? 0 : Secured);
	Summary.DurationSeconds = HeistState->GetElapsed();
	Summary.HauntStage = Haunt.IsValid() ? Haunt->GetHighestStage() : 0;
	Summary.Loot = Stashed;

	const float Bonus = bTargetStashed ? 1.f + PrimaryTargetBonus : 1.f;
	const float Total = bGotAway ? Secured * Bonus : 0.f;
	const int32 People = FMath::Max(1, Crew.Num());
	Summary.PayoutPerPlayer = FMath::RoundToInt(Total / People / 5.f) * 5;

	const int32 XPPerThousand = UHHDeveloperSettings::Get()->XPPerThousand;
	const int32 BaseXP = FMath::RoundToInt(Total / 1000.f * XPPerThousand) + 12 * Summary.HauntStage;
	switch (Outcome)
	{
	case EHHMissionOutcome::Success:	Summary.XPPerPlayer = 40 + BaseXP; break;
	case EHHMissionOutcome::Partial:	Summary.XPPerPlayer = 15 + BaseXP / 2; break;
	case EHHMissionOutcome::Caught:		Summary.XPPerPlayer = 10; break;
	default:							Summary.XPPerPlayer = 0; break;
	}

	HeistState->SetSummary(Summary);
	HeistState->SetHeistPhase(EHHHeistPhase::Ended);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AHHPlayerController* PC = Cast<AHHPlayerController>(It->Get()))
		{
			PC->ClientMissionReward(Summary);
		}
	}

	UE_LOG(LogHorrorHeist, Log, TEXT("Heist %s ended: %s, secured %d, payout %d each, xp %d, stage %d."),
		*MissionId.ToString(), *UEnum::GetValueAsString(Outcome), Summary.SecuredValue, Summary.PayoutPerPlayer, Summary.XPPerPlayer, Summary.HauntStage);

	GetWorldTimerManager().SetTimer(ReturnTimer, this, &AHHHeistGameMode::ReturnToHideout, UHHDeveloperSettings::Get()->SummarySeconds, false);
}

void AHHHeistGameMode::ReturnToHideout()
{
	const UHHDeveloperSettings* Settings = UHHDeveloperSettings::Get();
	if (AHHTimeOfDay* Clock = AHHTimeOfDay::Get(this))
	{
		Clock->SkipTo(Settings->ReturnHour);
	}
	if (UHHGameInstance* GameInstance = GetGameInstance<UHHGameInstance>())
	{
		GameInstance->SetCarriedTimeOfDay(Settings->ReturnHour);
		GameInstance->SetActiveMission(NAME_None);
	}
	const FString LobbyMap = Settings->LobbyMap.ToSoftObjectPath().GetLongPackageName();
	if (!TravelCrewTo(LobbyMap.IsEmpty() ? FString(TEXT("/Game/HorrorHeist/Maps/L_Hideout")) : LobbyMap))
	{
		UE_LOG(LogHorrorHeist, Error, TEXT("Could not travel back to the hideout (%s)."), *LobbyMap);
	}
}

#undef LOCTEXT_NAMESPACE
