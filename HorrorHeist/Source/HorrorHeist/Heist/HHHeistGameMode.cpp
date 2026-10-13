#include "Heist/HHHeistGameMode.h"
#include "Heist/HHHeistGameState.h"
#include "Heist/HHHauntDirector.h"
#include "Heist/HHHouseVolume.h"
#include "Heist/HHLootActor.h"
#include "Heist/HHVanPoint.h"
#include "Heist/HHEscapeZone.h"
#include "Heist/HHApparition.h"
#include "Heist/HHSaltLine.h"
#include "Heist/HHHauntMark.h"
#include "Data/HHEquipmentDefinition.h"
#include "Core/HHGameData.h"
#include "Interaction/HHDoorActor.h"
#include "NPC/HHNeighbourNPC.h"
#include "Vehicles/HHPoliceCar.h"
#include "Vehicles/HHRoadNode.h"
#include "Vehicles/HHVanVehicle.h"
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
#include "Online/HHSessionSubsystem.h"
#include "Progression/HHProfileSubsystem.h"
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
	static const FName DefaultSafehouse(TEXT("Hideout"));
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

	// Where the van goes at the end: ?Escape= from the job board, the remembered plan, or home.
	const FString EscapeOption = UGameplayStatics::ParseOption(Options, TEXT("Escape"));
	SafehouseId = EscapeOption.IsEmpty() ? NAME_None : FName(*EscapeOption);
	if (SafehouseId.IsNone())
	{
		if (const UHHGameInstance* GameInstance = GetGameInstance<UHHGameInstance>())
		{
			SafehouseId = GameInstance->GetSafehouse();
		}
	}
	if (SafehouseId.IsNone())
	{
		SafehouseId = HHHeistIds::DefaultSafehouse;
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
	FindWorldActors();
	Haunt = AHHHauntDirector::Get(this);
	if (AHHHauntDirector* Director = Haunt.Get())
	{
		Director->SetAggression(Mission.IsValid() ? Mission->HauntAggression : 1.f);
	}
	if (HouseVolumes.Num() == 0 || (!Van.IsValid() && !DrivePoint.IsValid()) || !Haunt.IsValid())
	{
		UE_LOG(LogHorrorHeist, Warning, TEXT("Heist map is incomplete: %d house volumes, van %s, haunt director %s."),
			HouseVolumes.Num(), Van.IsValid() || DrivePoint.IsValid() ? TEXT("yes") : TEXT("NO"), Haunt.IsValid() ? TEXT("yes") : TEXT("NO"));
	}

	if (AHHHeistGameState* HeistState = GetHeistState())
	{
		HeistState->SetSelectedMission(MissionId);
		HeistState->SetSafehouse(Escape.IsValid() ? Escape->GetZoneId() : SafehouseId);
		if (const UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
		{
			HeistState->MergeIntel(Profile->GetKnownIntel());
		}
		const UHHSessionSubsystem* Sessions = UHHSessionSubsystem::Get(this);
		HeistState->SetLobbyCode(Sessions && Sessions->HasSession() ? Sessions->GetLobbyCode() : FString());
		HeistState->StartClock();
	}
	StartTime = World->GetTimeSeconds();
	PatrolAt = FMath::FRandRange(PatrolWindow.X, FMath::Max(PatrolWindow.X, PatrolWindow.Y));
	ApplyKnownIntel();
	SetupObjectives();

	if (UHHNoiseSubsystem* Noise = World->GetSubsystem<UHHNoiseSubsystem>())
	{
		NoiseHandle = Noise->OnNoise.AddUObject(this, &AHHHeistGameMode::HandleNoise);
	}
	if (AHHNeighbourNPC* Npc = Neighbour.Get())
	{
		SpottedHandle = Npc->OnSpotted.AddUObject(this, &AHHHeistGameMode::HandleNeighbourSpotted);
		GaveUpHandle = Npc->OnGaveUp.AddUObject(this, &AHHHeistGameMode::HandleNeighbourGaveUp);
	}
	GetWorldTimerManager().SetTimer(RulesTimer, this, &AHHHeistGameMode::TickRules, 0.5f, true, 0.5f);
}

void AHHHeistGameMode::FindWorldActors()
{
	UWorld* World = GetWorld();
	HouseVolumes.Reset();
	FVector Sum = FVector::ZeroVector;
	for (TActorIterator<AHHHouseVolume> It(World); It; ++It)
	{
		HouseVolumes.Add(*It);
		Sum += It->GetActorLocation();
	}
	for (TActorIterator<AHHVanPoint> It(World); It; ++It)
	{
		if (It->GetPointType() == EHHVanPointType::Drive)
		{
			DrivePoint = *It;
		}
	}
	Van = AHHVanVehicle::Get(this);
	Police = AHHPoliceCar::Get(this);
	Neighbour = AHHNeighbourNPC::Get(this);
	ResolveEscapeZone();
	HouseCenter = HouseVolumes.Num() > 0 ? Sum / HouseVolumes.Num() : (Van.IsValid() ? Van->GetActorLocation() : FVector::ZeroVector);
}

void AHHHeistGameMode::ResolveEscapeZone()
{
	if (Escape.IsValid())
	{
		return;
	}
	// The zones live in the streamed town; look again later if it is not there yet.
	Escape = AHHEscapeZone::Find(this, SafehouseId);
	if (!Escape.IsValid())
	{
		// Unknown id (older board, renamed zone): the first getaway spot of the town.
		const TArray<AHHEscapeZone*> Zones = AHHEscapeZone::GetAll(this);
		Escape = Zones.Num() > 0 ? Zones[0] : nullptr;
		if (Escape.IsValid())
		{
			UE_LOG(LogHorrorHeist, Warning, TEXT("Heist: no escape zone '%s'; using '%s'."), *SafehouseId.ToString(), *Escape->GetZoneId().ToString());
			SafehouseId = Escape->GetZoneId();
		}
	}
	if (Escape.IsValid())
	{
		if (AHHHeistGameState* HeistState = GetHeistState())
		{
			HeistState->SetSafehouse(Escape->GetZoneId());
		}
	}
}

void AHHHeistGameMode::ApplyKnownIntel()
{
	// Keys the crew picked up while scouting by day are in their pocket tonight.
	const UHHMissionDefinition* Job = Mission.Get();
	AHHHeistGameState* HeistState = GetHeistState();
	if (!Job || !HeistState)
	{
		return;
	}
	for (const FHHJobIntel& Intel : Job->Intel)
	{
		if (Intel.KeyId.IsNone() || !(Intel.bKnownFromStart || HeistState->IsIntelKnown(Intel.IntelId)))
		{
			continue;
		}
		HeistState->AddKey(Intel.KeyId);
		for (TActorIterator<AHHLootActor> It(GetWorld()); It; ++It)
		{
			if (It->IsKey() && It->GetKeyId() == Intel.KeyId)
			{
				It->Destroy();
			}
		}
		HeistState->MulticastCrewMessage(FText::Format(LOCTEXT("KeyFromScouting", "You have {0} from scouting by day."), Intel.Title), EHHNotifyType::Success);
	}
}

void AHHHeistGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHHNoiseSubsystem* Noise = GetWorld() ? GetWorld()->GetSubsystem<UHHNoiseSubsystem>() : nullptr)
	{
		Noise->OnNoise.Remove(NoiseHandle);
	}
	if (AHHNeighbourNPC* Npc = Neighbour.Get())
	{
		Npc->OnSpotted.Remove(SpottedHandle);
		Npc->OnGaveUp.Remove(GaveUpHandle);
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
		if (It->IsKey() && !HeistState->HasKey(It->GetKeyId()))
		{
			bHasKeyObjective = true;
			break;
		}
	}
	if (bHasKeyObjective)
	{
		Add(HHHeistIds::Key, LOCTEXT("ObjKey", "Optional: find the spare key"), true);
	}

	if (Van.IsValid())
	{
		bHasEscapeObjective = true;
		Add(HHHeistIds::Escape, FText::GetEmpty(), false);
	}

	HeistState->SetObjectives(List);
	HeistState->SetSecured(0, MinimumHaul);
	UpdateHaulObjective();
	UpdateEscapeObjective();
}

void AHHHeistGameMode::UpdateEscapeObjective()
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState || !bHasEscapeObjective)
	{
		return;
	}
	ResolveEscapeZone();
	const FText Where = Escape.IsValid() ? Escape->GetDisplayName() : LOCTEXT("TheSpot", "the getaway spot");
	const FText Text = HeistState->IsPoliceCalled()
		? FText::Format(LOCTEXT("ObjEscapePolice", "Police! Everyone into the van - drive to {0}"), Where)
		: FText::Format(LOCTEXT("ObjEscapeDrive", "Get away: everyone into the van, drive to {0} ({1}/{2} in the van)"),
			Where, FText::AsNumber(HeistState->GetCrewAtVan()), FText::AsNumber(HeistState->GetCrewTotal()));
	HeistState->UpdateObjective(HHHeistIds::Escape, &Text, nullptr);
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
	const AHHVanPoint* OldVan = DrivePoint.Get();

	int32 AtVan = 0;
	TArray<APawn*> Blackouts;
	for (AHHPlayerState* State : Crew)
	{
		APawn* Pawn = State->GetPawn();
		const AHHCharacter* CrewBody = Cast<AHHCharacter>(Pawn);
		if (!Pawn || Arrested.Contains(State) || (CrewBody && CrewBody->IsDead()))
		{
			continue;
		}
		const FVector Location = Pawn->GetActorLocation();
		const bool bInVan = IsInVan(State);

		bool bInside = false;
		for (const TWeakObjectPtr<AHHHouseVolume>& Volume : HouseVolumes)
		{
			if (!bInVan && Volume.IsValid() && Volume->ContainsPoint(Location))
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
		if (bInside && State->GetPsyche() <= 0.f && !(CrewBody && CrewBody->IsIncapacitated()))
		{
			Blackouts.Add(Pawn);
		}

		// Running with loot in a rustling bag (the sack clinks louder than the duffel).
		if (const UHHInventoryComponent* Bag = State->GetInventory(); Bag && Bag->CountLoot() > 0 && !bInVan && Pawn->GetVelocity().Size2D() > 300.f)
		{
			float& Rustle = RustleTime.FindOrAdd(State);
			Rustle += Dt;
			if (Rustle >= 1.5f)
			{
				Rustle = 0.f;
				const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
				const UHHEquipmentDefinition* BagItem = Registry ? Registry->FindEquipment(State->GetEquipment().Get(EHHEquipmentSlot::Bag)) : nullptr;
				const float Loudness = (BagItem ? BagItem->GetStat(TEXT("Noise"), 30.f) : 30.f) / 100.f * 0.5f;
				if (UHHNoiseSubsystem* NoiseBus = GetWorld()->GetSubsystem<UHHNoiseSubsystem>())
				{
					NoiseBus->ReportNoise(Location, Loudness, Pawn, TEXT("Rustle"));
				}
			}
		}

		// In the van counts as "at the van"; old maps without a drivable van use the drive point's radius.
		if (bInVan || (!Van.IsValid() && OldVan && FVector::DistSquared(OldVan->GetActorLocation(), Location) < FMath::Square(OldVan->GetCrewRadius())))
		{
			++AtVan;
		}
	}
	HeistState->SetVanCount(AtVan, Crew.Num());
	UpdateEscapeObjective();

	for (APawn* Pawn : Blackouts)
	{
		HandlePlayerBlackout(Pawn, false);
	}

	TickPolice(Dt);
	if (bEnded)
	{
		return;
	}
	TickEscape();

	if (!HeistState->IsPoliceCalled())
	{
		Alert = FMath::Max(0.f, Alert - AlertDecay * Dt);
		HeistState->SetAlert(Alert);
	}
}

bool AHHHeistGameMode::IsInVan(const AHHPlayerState* State) const
{
	const AHHCharacter* Character = State ? Cast<AHHCharacter>(State->GetPawn()) : nullptr;
	return Character && Van.IsValid() && Character->GetVehicle() == Van.Get();
}

// ---------------------------------------------------------------------------------------
// Police: patrol, response, arrests, chase

void AHHHeistGameMode::TickPolice(float DeltaSeconds)
{
	AHHHeistGameState* HeistState = GetHeistState();
	AHHPoliceCar* Car = Police.Get();
	if (!HeistState)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();

	// No police car in this map (older content): the old countdown rule.
	if (!Car)
	{
		if (HeistState->IsPoliceCalled() && HeistState->GetPoliceRemaining() <= 0.f)
		{
			HeistState->MulticastCrewMessage(LOCTEXT("PoliceArrived", "Blue lights at the end of the street. Too late."), EHHNotifyType::Error);
			EndMission(EHHMissionOutcome::Caught);
		}
		return;
	}

	// A patrol drives through once, some time into the job.
	if (!bPatrolDone && !HeistState->IsPoliceCalled() && Now - StartTime > PatrolAt)
	{
		bPatrolDone = true;
		Car->StartPatrol();
		if (Car->GetMode() == EHHPoliceCarMode::Patrol)
		{
			HeistState->SetPoliceState(EHHPoliceState::Patrol);
			HeistState->MulticastCrewMessage(LOCTEXT("PatrolComing", "Headlights on Main Street - a patrol car. Get off the lawn and out of sight!"), EHHNotifyType::Warning);
		}
	}
	if (Car->GetMode() == EHHPoliceCarMode::Patrol)
	{
		for (AHHPlayerState* State : HeistState->GetCrew())
		{
			const APawn* Pawn = State->GetPawn();
			if (!Pawn || IsInVan(State) || State->IsInsideHouse() || Arrested.Contains(State))
			{
				continue;
			}
			if (Car->CanSee(Pawn, 2200.f, 55.f))
			{
				// They saw someone creeping around a house at night: backup is minutes away, this car is right here.
				Car->HoldPosition();
				Alert = 100.f;
				CallPolice(25.f, FText::Format(LOCTEXT("PatrolSaw", "The patrol car saw {0}! It stopped - more police in 25 seconds."), FText::FromString(State->GetPlayerName())));
				break;
			}
		}
	}
	else if (HeistState->GetPoliceState() == EHHPoliceState::Patrol && Car->GetMode() == EHHPoliceCarMode::Parked)
	{
		HeistState->SetPoliceState(EHHPoliceState::None);
		HeistState->MulticastCrewMessage(LOCTEXT("PatrolGone", "The patrol car turned the corner. Quiet again."), EHHNotifyType::Info);
	}

	if (!HeistState->IsPoliceCalled())
	{
		return;
	}

	// Called: the car leaves late enough to arrive about when the countdown ends.
	if (!bPoliceDispatched && Now >= PoliceCallTime + PoliceDelay)
	{
		DispatchPoliceCar();
	}
	if (bPoliceDispatched && !bPoliceArrived && Car->HasArrived())
	{
		HandlePoliceArrived();
		if (bEnded)
		{
			return;
		}
	}

	// The chase.
	AHHVanVehicle* TheVan = Van.Get();
	if (Car->GetMode() == EHHPoliceCarMode::Chase && TheVan)
	{
		if (Car->GetTimeWithoutSight() > 10.f)
		{
			HeistState->SetPoliceState(EHHPoliceState::Searching);
			HeistState->MulticastCrewMessage(LOCTEXT("LostThem", "You lost them. Get to the getaway spot before they find you again."), EHHNotifyType::Success);
			Car->Leave();
		}
		else
		{
			HeistState->SetPoliceState(EHHPoliceState::Chasing);
			const float Distance = FVector::Dist2D(Car->GetActorLocation(), TheVan->GetActorLocation());
			BustTimer = Distance < 750.f && TheVan->GetSpeedKmh() < 8.f ? BustTimer + DeltaSeconds : 0.f;
			if (BustTimer >= 2.5f)
			{
				HeistState->MulticastCrewMessage(LOCTEXT("Boxed", "The patrol car cut you off. Hands where they can see them."), EHHNotifyType::Error);
				EndMission(EHHMissionOutcome::ChaseBusted);
				return;
			}
		}
	}
	else if (HeistState->GetPoliceState() == EHHPoliceState::Searching && TheVan && TheVan->GetNumOccupants() > 0
		&& Car->GetMode() != EHHPoliceCarMode::Parked && Car->CanSee(TheVan, 3000.f, 70.f))
	{
		Car->Chase(TheVan);
		HeistState->SetPoliceState(EHHPoliceState::Chasing);
		HeistState->MulticastCrewMessage(LOCTEXT("FoundAgain", "They spotted the van again!"), EHHNotifyType::Error);
	}
}

void AHHHeistGameMode::CallPolice(float Seconds, const FText& Message)
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState || HeistState->IsPoliceCalled())
	{
		return;
	}
	HeistState->CallPolice(Seconds);
	HeistState->SetPoliceState(EHHPoliceState::Responding);
	PoliceCallTime = GetWorld()->GetTimeSeconds();
	// The car needs ~10-20 s through town; it leaves the station accordingly.
	PoliceDelay = FMath::Max(0.f, Seconds - 18.f);
	bPoliceDispatched = false;
	HeistState->MulticastCrewMessage(Message, EHHNotifyType::Error);

	TArray<FHHObjective> List = HeistState->GetObjectives();
	FHHObjective Objective;
	Objective.Id = HHHeistIds::Police;
	Objective.Text = Escape.IsValid()
		? FText::Format(LOCTEXT("ObjPoliceZone", "Police on the way - into the van and off to {0}!"), Escape->GetDisplayName())
		: LOCTEXT("ObjPolice", "Police on the way - get everyone to the van and drive!");
	List.Insert(Objective, 0);
	HeistState->SetObjectives(List);
	UpdateEscapeObjective();
}

void AHHHeistGameMode::DispatchPoliceCar()
{
	AHHPoliceCar* Car = Police.Get();
	AHHHeistGameState* HeistState = GetHeistState();
	if (!Car || !HeistState)
	{
		return;
	}
	bPoliceDispatched = true;
	const AHHRoadNode* Node = AHHRoadNode::FindNearest(this, HouseCenter);
	const FVector Scene = Node ? Node->GetActorLocation() : HouseCenter;
	// From the edge of town, or - when the patrol saw them - from where it stopped.
	Car->Respond(Scene, HeistState->GetPoliceRemaining() + 20.f);
	HeistState->MulticastCrewMessage(LOCTEXT("Sirens", "Sirens. They're coming down Main Street."), EHHNotifyType::Error);
}

void AHHHeistGameMode::HandlePoliceArrived()
{
	AHHHeistGameState* HeistState = GetHeistState();
	AHHPoliceCar* Car = Police.Get();
	if (!HeistState || !Car)
	{
		return;
	}
	bPoliceArrived = true;
	HeistState->SetPoliceState(EHHPoliceState::AtHouse);

	// Everyone still at the house on foot is arrested.
	int32 Free = 0;
	for (AHHPlayerState* State : HeistState->GetCrew())
	{
		const APawn* Pawn = State->GetPawn();
		if (!Pawn || Arrested.Contains(State))
		{
			continue;
		}
		if (!IsInVan(State) && FVector::Dist2D(Pawn->GetActorLocation(), HouseCenter) < SceneRadius)
		{
			Arrest(State);
			continue;
		}
		++Free;
	}
	if (Free == 0)
	{
		HeistState->MulticastCrewMessage(LOCTEXT("AllCaught", "Blue lights in the driveway. Nobody got away."), EHHNotifyType::Error);
		EndMission(EHHMissionOutcome::Caught);
		return;
	}

	// The van: still in the street means a chase, otherwise they search the area.
	AHHVanVehicle* TheVan = Van.Get();
	if (TheVan && TheVan->GetNumOccupants() > 0 && (FVector::Dist2D(TheVan->GetActorLocation(), HouseCenter) < SceneRadius || Car->CanSee(TheVan, 4000.f, 180.f)))
	{
		Car->Chase(TheVan);
		HeistState->SetPoliceState(EHHPoliceState::Chasing);
		HeistState->MulticastCrewMessage(LOCTEXT("ChaseStart", "They saw the van - lose them!"), EHHNotifyType::Error);
	}
	else
	{
		HeistState->SetPoliceState(EHHPoliceState::Searching);
		HeistState->MulticastCrewMessage(LOCTEXT("Searching", "The police are at the house, looking around. Stay out of their sight."), EHHNotifyType::Warning);
	}
}

void AHHHeistGameMode::Arrest(AHHPlayerState* State)
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!State || Arrested.Contains(State) || !HeistState)
	{
		return;
	}
	if (const AHHCharacter* Body = Cast<AHHCharacter>(State->GetPawn()); Body && Body->IsDead())
	{
		return;
	}
	Arrested.Add(State);
	State->SetInsideHouse(false);
	if (AHHCharacter* Character = Cast<AHHCharacter>(State->GetPawn()))
	{
		if (Character->IsSeated() && Character->GetVehicle())
		{
			Character->GetVehicle()->Exit(Character);
		}
		Character->SetArrested(true);
	}
	if (AHHPlayerController* PC = Cast<AHHPlayerController>(State->GetOwningController()))
	{
		PC->ClientNotify(LOCTEXT("YouArrested", "You were arrested. Your bag is evidence now - the crew can still get away."), EHHNotifyType::Error);
	}
	HeistState->MulticastCrewMessage(FText::Format(LOCTEXT("Arrested", "{0} was arrested."), FText::FromString(State->GetPlayerName())), EHHNotifyType::Error);
}

// ---------------------------------------------------------------------------------------
// The neighbour

void AHHHeistGameMode::SendNeighbour(const FVector& Location)
{
	AHHNeighbourNPC* Npc = Neighbour.Get();
	AHHHeistGameState* HeistState = GetHeistState();
	if (!Npc || !HeistState || HeistState->IsPoliceCalled() || NeighbourVisits >= 2)
	{
		return;
	}
	if (Npc->SendOut(Location))
	{
		++NeighbourVisits;
		HeistState->MulticastCrewMessage(FText::Format(LOCTEXT("NeighbourOut", "A door opens next door: {0} is coming out with a torch. Stay out of his sight!"),
			Npc->GetDisplayName()), EHHNotifyType::Warning);
	}
}

void AHHHeistGameMode::HandleNeighbourSpotted(APawn* Who)
{
	const AHHNeighbourNPC* Npc = Neighbour.Get();
	const AHHPlayerState* State = Who ? Who->GetPlayerState<AHHPlayerState>() : nullptr;
	Alert = 100.f;
	if (AHHHeistGameState* HeistState = GetHeistState())
	{
		HeistState->SetAlert(Alert);
	}
	CallPolice(UHHDeveloperSettings::Get()->PoliceResponseSeconds * 0.6f, FText::Format(LOCTEXT("NeighbourSaw", "{0} saw {1} and is running inside to call the police!"),
		Npc ? Npc->GetDisplayName() : LOCTEXT("TheNeighbour", "The neighbour"), State ? FText::FromString(State->GetPlayerName()) : LOCTEXT("Someone", "someone")));
}

void AHHHeistGameMode::HandleNeighbourGaveUp()
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState || HeistState->IsPoliceCalled())
	{
		return;
	}
	Alert = FMath::Max(0.f, Alert - 25.f);
	HeistState->SetAlert(Alert);
	const AHHNeighbourNPC* Npc = Neighbour.Get();
	HeistState->MulticastCrewMessage(FText::Format(LOCTEXT("NeighbourIn", "{0} found nothing and went back inside."),
		Npc ? Npc->GetDisplayName() : LOCTEXT("TheNeighbourCap", "The neighbour")), EHHNotifyType::Info);
}

// ---------------------------------------------------------------------------------------
// The getaway

EHHMissionOutcome AHHHeistGameMode::GetGetawayOutcome() const
{
	const AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState || HeistState->GetSecuredValue() < MinimumHaul)
	{
		return EHHMissionOutcome::Partial;
	}
	return HeistState->IsPoliceCalled() ? EHHMissionOutcome::HotGetaway : EHHMissionOutcome::Success;
}

void AHHHeistGameMode::TickEscape()
{
	AHHHeistGameState* HeistState = GetHeistState();
	const AHHVanVehicle* TheVan = Van.Get();
	const AHHEscapeZone* Zone = Escape.Get();
	if (!HeistState || !TheVan || !Zone || TheVan->GetNumOccupants() == 0 || !Zone->ContainsPoint(TheVan->GetGroundLocation() + FVector(0.f, 0.f, 50.f)))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	auto Hint = [this, HeistState, Now](const FText& Text)
	{
		if (Now - LastEscapeHint > 6.0)
		{
			LastEscapeHint = Now;
			HeistState->MulticastCrewMessage(Text, EHHNotifyType::Warning);
		}
	};
	if (HeistState->GetPoliceState() == EHHPoliceState::Chasing && !Zone->AcceptsWhileChased())
	{
		Hint(FText::Format(LOCTEXT("NotWhileChased", "Not {0} - not with the police on your tail. Lose them first."), Zone->GetDisplayName()));
		return;
	}
	if (Zone->GetStopSeconds() > 0.f && TheVan->GetStoppedSeconds() < Zone->GetStopSeconds())
	{
		Hint(FText::Format(LOCTEXT("ParkHere", "Park it here and kill the engine ({0}s standing still)."), FText::AsNumber(FMath::RoundToInt(Zone->GetStopSeconds()))));
		return;
	}
	if (TheVan->GetNumOccupants() < HeistState->GetCrewTotal() - Arrested.Num())
	{
		HeistState->MulticastCrewMessage(LOCTEXT("LeftSomeone", "The van made it - but not everyone was in it."), EHHNotifyType::Warning);
	}
	EndMission(GetGetawayOutcome());
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
	static const FName Horn(TEXT("Horn"));
	static const FName Crash(TEXT("Crash"));

	if (Loudness >= 0.3f)
	{
		LastNoise = Location;
	}
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
	if (Tag == Horn)
	{
		RaiseAlert(30.f, LOCTEXT("AlertHorn", "A horn at night. Lights go on along the street."));
		return;
	}
	if (Tag == Crash)
	{
		RaiseAlert(25.f * Loudness, LOCTEXT("AlertCrash", "That crash woke the street."));
		return;
	}
	static const FName Rustle(TEXT("Rustle"));
	if (Tag == Rustle)
	{
		// Clinking loot carries in the quiet street; inside, the house hears it too.
		if (bInside && Director)
		{
			Director->AddAnger(Loudness * 0.6f);
		}
		else if (!bInside)
		{
			RaiseAlert(Loudness * 2.f, FText::GetEmpty());
		}
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
	if (!HeistState || HeistState->IsPoliceCalled() || Amount <= 0.f || bEnded)
	{
		return;
	}
	const float Before = Alert;
	Alert = FMath::Clamp(Alert + Amount, 0.f, 100.f);
	HeistState->SetAlert(Alert);

	if (Alert >= 100.f)
	{
		const float Seconds = UHHDeveloperSettings::Get()->PoliceResponseSeconds;
		CallPolice(Seconds, FText::Format(LOCTEXT("PoliceCalled", "A neighbour called the police. You have about {0} seconds."), FText::AsNumber(FMath::RoundToInt(Seconds))));
		return;
	}
	if (Before < NeighbourAlert && Alert >= NeighbourAlert && Neighbour.IsValid() && !Neighbour->IsOut())
	{
		// Someone next door wants to know what that was.
		SendNeighbour(LastNoise.IsZero() ? HouseCenter : LastNoise);
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
	EndMission(GetGetawayOutcome());
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
	if (!State || !HeistState || bEnded || Arrested.Contains(State) || IsInVan(State))
	{
		return;
	}

	// Everything in the bag stays where they fell (the crew can still fetch it).
	DropCarriedLoot(Pawn, State->GetInventory());

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

void AHHHeistGameMode::DropCarriedLoot(APawn* Pawn, UHHInventoryComponent* Inventory)
{
	if (!Pawn || !Inventory)
	{
		return;
	}
	const TArray<FHHInventorySlot> Items = Inventory->RemoveAllLoot();
	const FVector Center = Pawn->GetActorLocation();
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const float Angle = Index * 2.f * UE_PI / FMath::Max(1, Items.Num());
		FVector Spot = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 45.f;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HHLootDrop), false, Pawn);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 30.f), Spot - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
		{
			Spot = Hit.ImpactPoint;
		}
		AHHLootActor::SpawnDropped(GetWorld(), Items[Index], FTransform(FRotator(0.f, FMath::RandRange(0.f, 360.f), 0.f), Spot));
	}
}

void AHHHeistGameMode::HandleEntityCatch(APawn* Pawn)
{
	AHHCharacter* Character = Cast<AHHCharacter>(Pawn);
	AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
	AHHHeistGameState* HeistState = GetHeistState();
	if (!Character || !State || !HeistState || bEnded || Arrested.Contains(State) || IsInVan(State) || Character->IsDead())
	{
		return;
	}
	if (Character->IsDowned())
	{
		// It came back for them.
		HandleCrewDeath(Character, true);
		return;
	}
	Character->SetDowned(DownedSeconds);
	State->SetPsyche(FMath::Min(State->GetPsyche(), 15.f));
	if (AHHPlayerController* PC = Cast<AHHPlayerController>(Character->GetController()))
	{
		PC->ClientKnockedDown();
	}
	HeistState->MulticastCrewMessage(FText::Format(
		LOCTEXT("KnockedDown", "Something threw {0} to the floor! Kneel next to them and hold the interact key - {1} seconds before it's too late."),
		FText::FromString(State->GetPlayerName()), FText::AsNumber(FMath::RoundToInt(DownedSeconds))), EHHNotifyType::Error);
	if (AHHHauntDirector* Director = Haunt.Get())
	{
		Director->OnPlayerCaught();
	}
}

void AHHHeistGameMode::HandleUseItem(AHHCharacter* Character, int32 SlotIndex, const FVector& ViewLocation, const FVector& ViewDirection)
{
	AHHPlayerState* State = Character ? Character->GetPlayerState<AHHPlayerState>() : nullptr;
	UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	AHHHeistGameState* HeistState = GetHeistState();
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	if (bEnded || !Inventory || !Inventory->IsEnabled() || !HeistState || !Registry || Character->IsIncapacitated() || Character->IsSeated()
		|| Arrested.Contains(State))
	{
		return;
	}
	const FHHInventorySlot& Slot = Inventory->GetSlot(SlotIndex);
	const UHHEquipmentDefinition* Equipment = Slot.Kind == EHHInventoryKind::Equipment ? Registry->FindEquipment(Slot.ItemId) : nullptr;
	if (!Equipment || FVector::DistSquared(ViewLocation, Character->GetActorLocation()) > FMath::Square(300.f))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	double& Last = LastGearUse.FindOrAdd(State);
	if (Now - Last < 1.2)
	{
		return;
	}
	Last = Now;

	const FVector Direction = ViewDirection.GetSafeNormal();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHUseItem), false, Character);
	UHHNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UHHNoiseSubsystem>();
	static const FName WalkieTag(TEXT("Walkie"));
	static const FName SaltTag(TEXT("Salt"));
	static const FName CameraTag(TEXT("Camera"));

	if (Equipment->ToolTag == WalkieTag)
	{
		// Mark what you look at (up to 30 m) for the whole crew.
		FHitResult Hit;
		const FVector End = ViewLocation + Direction * 3000.f;
		const FVector Spot = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params) ? FVector(Hit.ImpactPoint) : End;
		HeistState->AddPing(Spot, State->GetCrewSlot(), State->GetPlayerName());
		if (Noise)
		{
			Noise->ReportNoise(Character->GetActorLocation(), 0.06f, Character, TEXT("Walkie"));
		}
		return;
	}

	if (Equipment->ToolTag == SaltTag)
	{
		// A line across the floor where you look (within 2.5 m), at right angles to your view.
		FHitResult Hit;
		const FVector Aim = ViewLocation + Direction * 250.f;
		FVector Spot = Character->GetActorLocation() + Character->GetActorForwardVector() * 80.f;
		if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, Aim, ECC_Visibility, Params))
		{
			Spot = Hit.ImpactPoint;
		}
		else
		{
			Spot = Aim;
		}
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 30.f), Spot - FVector(0.f, 0.f, 250.f), ECC_Visibility, Params))
		{
			return;
		}
		if (!Inventory->ConsumeCharge(SlotIndex))
		{
			return;
		}
		const float Yaw = Direction.Rotation().Yaw + 90.f;
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GetWorld()->SpawnActor<AHHSaltLine>(AHHSaltLine::StaticClass(), FVector(Hit.ImpactPoint) + FVector(0.f, 0.f, 0.5f), FRotator(0.f, Yaw, 0.f), Spawn);
		if (const UHHGameData* GameData = UHHGameData::Get(this))
		{
			HeistState->MulticastSoundAt(GameData->SaltPourSound.LoadSynchronous(), Hit.ImpactPoint, 1.f);
		}
		return;
	}

	if (Equipment->ToolTag == CameraTag)
	{
		if (!Inventory->ConsumeCharge(SlotIndex))
		{
			return;
		}
		HeistState->MulticastCameraFlash(ViewLocation, Direction);
		if (Noise)
		{
			Noise->ReportNoise(Character->GetActorLocation(), 0.15f, Character, TEXT("Camera"));
		}
		AHHApparition* Ghost = nullptr;
		for (TActorIterator<AHHApparition> It(GetWorld()); It; ++It)
		{
			Ghost = *It;
			break;
		}
		bool bEvidence = false;
		if (Ghost && Ghost->IsInPhoto(ViewLocation, Direction))
		{
			Ghost->OnPhotographed();
			bEvidence = true;
		}
		else
		{
			// Handprints and writing the house left behind are evidence too (each one once).
			for (TActorIterator<AHHHauntMark> It(GetWorld()); It; ++It)
			{
				if (It->TryPhotograph(ViewLocation, Direction, Character))
				{
					bEvidence = true;
					break;
				}
			}
		}
		if (bEvidence)
		{
			HeistState->AddPhoto();
			const bool bPays = HeistState->GetPhotos() <= MaxPhotos;
			HeistState->MulticastCrewMessage(bPays
				? FText::Format(LOCTEXT("Photo", "{0} got it on film. A magazine pays {1} for that."), FText::FromString(State->GetPlayerName()), HHText::Money(PhotoValue))
				: FText::Format(LOCTEXT("PhotoMore", "{0} got it on film again."), FText::FromString(State->GetPlayerName())), EHHNotifyType::Success);
			if (AHHHauntDirector* Director = Haunt.Get())
			{
				Director->AddAnger(4.f);
			}
		}
	}
}

void AHHHeistGameMode::HandleBleedOut(AHHCharacter* Character)
{
	HandleCrewDeath(Character, false);
}

void AHHHeistGameMode::HandleRevived(AHHCharacter* Character, AHHCharacter* Helper)
{
	AHHHeistGameState* HeistState = GetHeistState();
	const AHHPlayerState* State = Character ? Character->GetPlayerState<AHHPlayerState>() : nullptr;
	const AHHPlayerState* HelperState = Helper ? Helper->GetPlayerState<AHHPlayerState>() : nullptr;
	if (HeistState && State)
	{
		HeistState->MulticastCrewMessage(FText::Format(LOCTEXT("Revived", "{0} got {1} back on their feet."),
			FText::FromString(HelperState ? HelperState->GetPlayerName() : FString(TEXT("Someone"))), FText::FromString(State->GetPlayerName())),
			EHHNotifyType::Success);
	}
}

void AHHHeistGameMode::HandleCrewDeath(AHHCharacter* Character, bool bTakenByEntity)
{
	AHHPlayerState* State = Character ? Character->GetPlayerState<AHHPlayerState>() : nullptr;
	AHHHeistGameState* HeistState = GetHeistState();
	if (!Character || !State || !HeistState || bEnded || Character->IsDead())
	{
		return;
	}
	// The loot can still be fetched; the gear they brought stays with the body.
	DropCarriedLoot(Character, State->GetInventory());
	TArray<FName>& Gear = DeadGear.FindOrAdd(State);
	Gear.Reset();
	if (const UHHInventoryComponent* Inventory = State->GetInventory())
	{
		for (const FHHInventorySlot& Slot : Inventory->GetSlots())
		{
			if (Slot.Kind == EHHInventoryKind::Equipment && !Slot.ItemId.IsNone())
			{
				Gear.AddUnique(Slot.ItemId);
			}
		}
	}
	Character->SetDead();
	State->SetInsideHouse(false);
	HeistState->MulticastCrewMessage(FText::Format(bTakenByEntity
		? LOCTEXT("TakenDead", "It came back for {0}. They are gone.")
		: LOCTEXT("BledOut", "Nobody reached {0} in time. They are gone."), FText::FromString(State->GetPlayerName())), EHHNotifyType::Error);
	if (AHHHauntDirector* Director = Haunt.Get())
	{
		Director->OnPlayerCaught();
	}
	CheckCrewLost();
}

void AHHHeistGameMode::CheckCrewLost()
{
	AHHHeistGameState* HeistState = GetHeistState();
	if (!HeistState || bEnded)
	{
		return;
	}
	int32 Dead = 0;
	for (AHHPlayerState* State : HeistState->GetCrew())
	{
		const AHHCharacter* Character = Cast<AHHCharacter>(State->GetPawn());
		if (Arrested.Contains(State))
		{
			continue;
		}
		if (Character && Character->IsDead())
		{
			++Dead;
			continue;
		}
		if (Character)
		{
			return;		// Someone can still drive (or crawl back to the van and be helped up).
		}
	}
	if (Dead > 0)
	{
		HeistState->MulticastCrewMessage(LOCTEXT("CrewLost", "Nobody is left to drive the van."), EHHNotifyType::Error);
		EndMission(EHHMissionOutcome::CrewLost);
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

void AHHHeistGameMode::DebugCallPolice()
{
	Alert = 100.f;
	if (AHHHeistGameState* HeistState = GetHeistState())
	{
		HeistState->SetAlert(Alert);
	}
	CallPolice(30.f, LOCTEXT("DebugPolice", "(debug) The police were called - 30 seconds."));
}

void AHHHeistGameMode::DebugSendNeighbour()
{
	NeighbourVisits = 0;
	SendNeighbour(HouseCenter);
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

	const bool bGotAway = HHText::IsGetaway(Outcome);
	const TArray<AHHPlayerState*> Crew = HeistState->GetCrew();
	if (AHHVanVehicle* TheVan = Van.Get())
	{
		TheVan->SetDrivable(false);
	}
	if (AHHPoliceCar* Car = Police.Get())
	{
		if (Car->GetMode() == EHHPoliceCarMode::Chase)
		{
			Car->HoldPosition();
		}
	}

	int32 Secured = 0;
	for (const FHHLootRecord& Record : Stashed)
	{
		Secured += Record.Value;
	}

	FHHMissionSummary Summary;
	Summary.Outcome = Outcome;
	Summary.MissionId = MissionId;
	Summary.SecuredValue = bGotAway ? Secured : 0;
	Summary.DurationSeconds = HeistState->GetElapsed();
	Summary.HauntStage = Haunt.IsValid() ? Haunt->GetHighestStage() : 0;
	Summary.Loot = Stashed;
	Summary.bPoliceCalled = HeistState->IsPoliceCalled();
	if (const AHHEscapeZone* Zone = Escape.Get())
	{
		Summary.Safehouse = Zone->GetZoneId();
		Summary.SafehouseName = Zone->GetDisplayName();
	}

	// Roll call: who was in the van, who was left behind, who was taken away.
	const AHHVanVehicle* TheVan = Van.Get();
	int32 LostCarried = 0;
	for (AHHPlayerState* State : Crew)
	{
		FHHCrewRecord Record;
		Record.Name = State->GetPlayerName();
		Record.CrewSlot = State->GetCrewSlot();
		Record.Stashed = State->GetStashedValue();
		const bool bInVan = IsInVan(State);
		const AHHCharacter* CrewBody = Cast<AHHCharacter>(State->GetPawn());
		Record.bDrove = TheVan && State->GetPawn() && TheVan->GetDriver() == State->GetPawn();
		Record.bRevived = CrewBody && CrewBody->WasRevived();
		if (CrewBody && CrewBody->IsDead())
		{
			Record.Fate = EHHCrewFate::Dead;
		}
		else if (Arrested.Contains(State) || Outcome == EHHMissionOutcome::Caught)
		{
			Record.Fate = EHHCrewFate::Caught;
		}
		else if (Outcome == EHHMissionOutcome::ChaseBusted)
		{
			Record.Fate = bInVan ? EHHCrewFate::Caught : EHHCrewFate::LeftBehind;
		}
		else if (bGotAway && (TheVan || DrivePoint.IsValid()))
		{
			// Old maps (drive point) end with everyone at the van.
			Record.Fate = bInVan || !TheVan ? EHHCrewFate::Escaped : EHHCrewFate::LeftBehind;
		}
		else
		{
			Record.Fate = EHHCrewFate::Escaped;
		}
		if (Record.Fate != EHHCrewFate::Escaped)
		{
			if (const UHHInventoryComponent* Inventory = State->GetInventory())
			{
				Record.Lost = Inventory->GetCarriedLootValue();
				LostCarried += Record.Lost;
			}
		}
		Summary.Crew.Add(Record);
	}
	Summary.LostValue = LostCarried + (bGotAway ? 0 : Secured);

	// The money: target bonus, the fence's cut (hot goods, the getaway spot), split across the crew.
	const float Bonus = bGotAway && bTargetStashed ? Secured * PrimaryTargetBonus : 0.f;
	// Flash photos of the thing in the house sell to a magazine (whoever made it home brings the film).
	Summary.Photos = HeistState->GetPhotos();
	Summary.PhotoBonus = bGotAway ? FMath::Min(Summary.Photos, MaxPhotos) * PhotoValue : 0;
	const float Gross = bGotAway ? Secured + Bonus + Summary.PhotoBonus : 0.f;
	float CutShare = Escape.IsValid() ? Escape->GetFenceCut() : 0.f;
	if (Outcome == EHHMissionOutcome::HotGetaway)
	{
		CutShare += HotGoodsCut;
	}
	const float Cut = Gross * FMath::Clamp(CutShare, 0.f, 0.9f);
	const float Net = Gross - Cut;
	Summary.TargetBonus = FMath::RoundToInt(Bonus);
	Summary.FenceCut = FMath::RoundToInt(Cut);
	const int32 People = FMath::Max(1, Crew.Num());
	Summary.PayoutPerPlayer = FMath::RoundToInt(Net / People / 5.f) * 5;

	const int32 XPPerThousand = UHHDeveloperSettings::Get()->XPPerThousand;
	const int32 BaseXP = FMath::RoundToInt(Net / 1000.f * XPPerThousand) + 12 * Summary.HauntStage;
	switch (Outcome)
	{
	case EHHMissionOutcome::Success:		Summary.XPPerPlayer = 40 + BaseXP; break;
	case EHHMissionOutcome::HotGetaway:		Summary.XPPerPlayer = 30 + BaseXP; break;
	case EHHMissionOutcome::Partial:		Summary.XPPerPlayer = 15 + BaseXP / 2; break;
	case EHHMissionOutcome::Caught:
	case EHHMissionOutcome::ChaseBusted:
	case EHHMissionOutcome::CrewLost:		Summary.XPPerPlayer = 10 + 6 * Summary.HauntStage; break;
	default:								Summary.XPPerPlayer = 0; break;
	}

	HeistState->SetSummary(Summary);
	HeistState->SetHeistPhase(EHHHeistPhase::Ended);
	HeistState->SetPoliceState(EHHPoliceState::None);

	// Each crew member books their own share: arrested people get nothing, those left behind
	// keep their cut of the van but learn less.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AHHPlayerController* PC = Cast<AHHPlayerController>(It->Get());
		AHHPlayerState* State = PC ? PC->GetPlayerState<AHHPlayerState>() : nullptr;
		if (!PC || !State)
		{
			continue;
		}
		FHHMissionSummary Personal = Summary;
		const FHHCrewRecord* Mine = Summary.Crew.FindByPredicate([State](const FHHCrewRecord& Record) { return Record.CrewSlot == State->GetCrewSlot() && Record.Name == State->GetPlayerName(); });
		if (Mine && Mine->Fate == EHHCrewFate::Caught)
		{
			Personal.PayoutPerPlayer = 0;
			Personal.XPPerPlayer = FMath::Min(Summary.XPPerPlayer, 10);
		}
		else if (Mine && Mine->Fate == EHHCrewFate::Dead)
		{
			Personal.PayoutPerPlayer = 0;
			Personal.XPPerPlayer = FMath::Min(Summary.XPPerPlayer, 10);
			if (const TArray<FName>* Gear = DeadGear.Find(State))
			{
				Personal.LostGear = *Gear;
			}
		}
		else if (Mine && Mine->Fate == EHHCrewFate::LeftBehind)
		{
			Personal.XPPerPlayer = FMath::RoundToInt(Summary.XPPerPlayer * 0.6f);
		}
		PC->ClientMissionReward(Personal);
	}

	UE_LOG(LogHorrorHeist, Log, TEXT("Heist %s ended: %s at %s, secured %d, cut %d, payout %d each, xp %d, stage %d."),
		*MissionId.ToString(), *UEnum::GetValueAsString(Outcome), *Summary.Safehouse.ToString(), Summary.SecuredValue, Summary.FenceCut,
		Summary.PayoutPerPlayer, Summary.XPPerPlayer, Summary.HauntStage);

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
