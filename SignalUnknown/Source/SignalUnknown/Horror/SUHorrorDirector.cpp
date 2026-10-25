#include "Horror/SUHorrorDirector.h"
#include "SignalUnknown.h"
#include "Horror/SUCreature.h"
#include "Horror/SUScriptedWindowFigure.h"
#include "Player/SUCharacter.h"
#include "Vehicle/SUVehicle.h"
#include "Interaction/SULighting.h"
#include "Base/SUBaseDevices.h"
#include "Mission/SUAreaTrigger.h"
#include "Mission/SUMissionManager.h"
#include "Mission/SUMissionDatabase.h"
#include "Audio/SUSoundEmitter.h"
#include "World/SUWorldBuilder.h"
#include "Core/SUGameState.h"
#include "Core/SUDeveloperSettings.h"
#include "Core/SUGameUserSettings.h"
#include "Logic/SUWorldLayout.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
	int32 GHorrorDebug = 0;
	FAutoConsoleVariableRef CVarHorrorDebug(TEXT("su.HorrorDebug"), GHorrorDebug, TEXT("1 = log every Horror Director evaluation."));

	FAutoConsoleCommandWithWorldAndArgs CmdHorrorEvent(
		TEXT("su.HorrorEvent"),
		TEXT("Force a horror event on the server: su.HorrorEvent TreeWatcher"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASUHorrorDirector* Director = ASUHorrorDirector::Get(World);
			if (!Director)
			{
				UE_LOG(LogSignalUnknown, Warning, TEXT("su.HorrorEvent: no Horror Director in this world (run it on the server / listen host)."));
				return;
			}
			const FString Name = Args.Num() > 0 ? Args[0] : FString();
			for (int32 I = 0; I < static_cast<int32>(SULogic::HorrorEvent::Count); ++I)
			{
				const SULogic::HorrorEvent E = static_cast<SULogic::HorrorEvent>(I);
				if (Name.Equals(ANSI_TO_TCHAR(SULogic::HorrorEventName(E)), ESearchCase::IgnoreCase))
				{
					const bool bOk = Director->ForceEvent(E);
					UE_LOG(LogSignalUnknown, Log, TEXT("su.HorrorEvent %s: %s"), *Name, bOk ? TEXT("executed") : TEXT("could not be placed here"));
					return;
				}
			}
			UE_LOG(LogSignalUnknown, Warning, TEXT("su.HorrorEvent: unknown event '%s'"), *Name);
		}));

	const FName SafeAreas[] = {SUArea::TowerCabin, SUArea::TowerGrounds};
	const FName IndoorAreas[] = {SUArea::TowerCabin, SUArea::GasShop, SUArea::ForsthausInterior, SUArea::Shed};
}

ASUHorrorDirector::ASUHorrorDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	bReplicates = false;
}

ASUHorrorDirector* ASUHorrorDirector::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (World)
	{
		for (TActorIterator<ASUHorrorDirector> It(World); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ASUHorrorDirector::SetActive(bool bInActive)
{
	bActive = bInActive && GetDefault<USUGameplaySettings>()->bHorrorDirectorEnabled;
	NextEvaluation = 60.f;
	UE_LOG(LogSignalUnknown, Log, TEXT("Horror Director %s"), bActive ? TEXT("active") : TEXT("inactive"));
}

void ASUHorrorDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bActive)
	{
		return;
	}
	Clock += DeltaSeconds;
	SULogic::DecayHorrorTension(DirectorState, DeltaSeconds);
	NextEvaluation -= DeltaSeconds;
	if (NextEvaluation <= 0.f)
	{
		const USUGameplaySettings* S = GetDefault<USUGameplaySettings>();
		NextEvaluation = FMath::FRandRange(S->DirectorMinIntervalSeconds, FMath::Max(S->DirectorMinIntervalSeconds, S->DirectorMaxIntervalSeconds));
		Evaluate();
	}
}

void ASUHorrorDirector::CacheAreas()
{
	if (Areas.Num() > 0)
	{
		return;
	}
	for (TActorIterator<ASUAreaTrigger> It(GetWorld()); It; ++It)
	{
		Areas.Add(*It);
	}
}

bool ASUHorrorDirector::IsInArea(const AActor* Actor, std::initializer_list<FName> Ids) const
{
	for (const TObjectPtr<ASUAreaTrigger>& A : Areas)
	{
		if (!A)
		{
			continue;
		}
		for (const FName& Id : Ids)
		{
			if (A->GetAreaId() == Id && A->IsActorInside(Actor))
			{
				return true;
			}
		}
	}
	return false;
}

ASUAreaTrigger* ASUHorrorDirector::FindNearbyBuildingArea(const FVector& Location, float MaxDistance) const
{
	ASUAreaTrigger* Best = nullptr;
	float BestDist = MaxDistance;
	for (const TObjectPtr<ASUAreaTrigger>& A : Areas)
	{
		if (!A)
		{
			continue;
		}
		const FName Id = A->GetAreaId();
		const bool bBuilding = Id == SUArea::ForsthausInterior || Id == SUArea::GasShop || Id == SUArea::Shed || Id == SUArea::TowerCabin;
		if (!bBuilding)
		{
			continue;
		}
		const float D = FVector::Dist2D(A->GetActorLocation(), Location);
		if (D < BestDist)
		{
			BestDist = D;
			Best = A;
		}
	}
	return Best;
}

// ------------------------------------------------------------------------------------ decision

ASUCharacter* ASUHorrorDirector::PickTarget() const
{
	TArray<ASUCharacter*> Players;
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && It->GetController())
		{
			Players.Add(*It);
		}
	}
	if (Players.Num() == 0)
	{
		return nullptr;
	}
	// isolated players are more likely to be the focus
	TArray<float> Weights;
	float Total = 0.f;
	for (const ASUCharacter* P : Players)
	{
		float Nearest = 5000.f;
		for (const ASUCharacter* O : Players)
		{
			if (O != P)
			{
				Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist(O->GetActorLocation(), P->GetActorLocation())));
			}
		}
		const float W = 1.f + Nearest / 1000.f;
		Weights.Add(W);
		Total += W;
	}
	float Pick = FMath::FRand() * Total;
	for (int32 I = 0; I < Players.Num(); ++I)
	{
		if (Pick < Weights[I])
		{
			return Players[I];
		}
		Pick -= Weights[I];
	}
	return Players.Last();
}

SULogic::DirectorContext ASUHorrorDirector::BuildContext(const ASUCharacter* Target) const
{
	SULogic::DirectorContext C;
	const ASUGameState* GS = ASUGameState::Get(this);
	const ASUWorldBuilder* Builder = ASUWorldBuilder::Get(this);
	const USUGameUserSettings* UserSettings = USUGameUserSettings::Get();
	C.Now = Clock;
	C.Darkness = GS ? GS->GetDarkness() : 0.0;
	C.Rain = GS ? GS->GetRain() : 0.0;
	C.StoryStage = GS ? GS->GetStoryStage() : 0;
	C.Intensity = UserSettings ? UserSettings->HorrorIntensity : 1.0;
	int32 Players = 0;
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		++Players;
	}
	C.NumPlayers = Players;
	C.bCreatureActive = ASUCreature::CountActive(GetWorld()) > 0;
	C.bInScriptedSequence = ASUScriptedWindowFigure::IsAnySequenceRunning(GetWorld());
	if (!Target)
	{
		return C;
	}
	const FVector Loc = Target->GetActorLocation();
	C.bInSafeZone = IsInArea(Target, {SafeAreas[0], SafeAreas[1]});
	C.bIndoors = IsInArea(Target, {IndoorAreas[0], IndoorAreas[1], IndoorAreas[2], IndoorAreas[3]});
	C.bInCriticalInteraction = Target->GetHeldNozzlePump() != nullptr;
	ASUVehicle* Vehicle = Target->GetVehicle();
	C.bInVehicle = Vehicle != nullptr;
	C.VehicleSpeedKmh = Vehicle ? Vehicle->GetSpeedKmh() : 0.0;
	C.bOnRoad = Builder && Builder->DistanceToRoad(Vehicle ? Vehicle->GetActorLocation() : Loc) < 700.f;

	bool bRadio = Vehicle && Vehicle->IsRadioOn();
	for (TActorIterator<ASURadioStation> It(GetWorld()); It && !bRadio; ++It)
	{
		bRadio = FVector::Dist(It->GetActorLocation(), Loc) < 2000.f;
	}
	C.bRadioNearby = bRadio || Vehicle != nullptr; // the car always has its CB radio
	bool bLight = Target->IsFlashlightOn();
	for (TActorIterator<ASULamp> It(GetWorld()); It && !bLight; ++It)
	{
		bLight = It->IsLit() && FVector::Dist(It->GetActorLocation(), Loc) < 1800.f;
	}
	C.bLightNearby = bLight;
	C.bBuildingNearby = FindNearbyBuildingArea(Loc, 2500.f) != nullptr;
	const ASUVehicle* Main = GS ? GS->MainVehicle.Get() : nullptr;
	C.bVehicleNearbyOnFoot = !Vehicle && Main && !Main->IsEngineRunning() && !Main->HasOccupants() && FVector::Dist(Main->GetActorLocation(), Loc) < 4000.f;
	return C;
}

void ASUHorrorDirector::Evaluate()
{
	CacheAreas();
	ASUCharacter* Target = PickTarget();
	if (!Target)
	{
		return;
	}
	const SULogic::DirectorContext C = BuildContext(Target);
	const SULogic::DirectorDecision D = SULogic::DecideHorrorEvent(C, DirectorState, FMath::FRand(), FMath::FRand());
	if (GHorrorDebug)
	{
		UE_LOG(LogSignalUnknown, Log, TEXT("Director: chance %.2f eligible %d tension %.2f dark %.2f -> %s (target %s)"),
			D.Chance, D.EligibleCount, DirectorState.Tension, C.Darkness, ANSI_TO_TCHAR(SULogic::HorrorEventName(D.Event)), *Target->GetName());
	}
	if (D.Event != SULogic::HorrorEvent::None)
	{
		const bool bOk = ExecuteEvent(D.Event, Target);
		UE_LOG(LogSignalUnknown, Log, TEXT("Horror event %s %s"), ANSI_TO_TCHAR(SULogic::HorrorEventName(D.Event)), bOk ? TEXT("played") : TEXT("found no place, skipped"));
	}
}

bool ASUHorrorDirector::ForceEvent(SULogic::HorrorEvent Event, ASUCharacter* Target)
{
	CacheAreas();
	if (!Target)
	{
		Target = PickTarget();
	}
	if (!Target)
	{
		return false;
	}
	const bool bOk = ExecuteEvent(Event, Target);
	if (bOk)
	{
		DirectorState.LastEventTime = Clock;
		DirectorState.LastById[static_cast<size_t>(Event)] = Clock;
	}
	return bOk;
}

bool ASUHorrorDirector::ExecuteEvent(SULogic::HorrorEvent Event, ASUCharacter* Target)
{
	switch (Event)
	{
	case SULogic::HorrorEvent::DistantSound: return DoDistantSound(Target);
	case SULogic::HorrorEvent::LightFlicker: return DoLightFlicker(Target);
	case SULogic::HorrorEvent::RadioInterference: return DoRadioInterference(Target);
	case SULogic::HorrorEvent::TreeWatcher: return DoTreeWatcher(Target);
	case SULogic::HorrorEvent::RoadsideFigure: return DoRoadsideFigure(Target);
	case SULogic::HorrorEvent::StrangeRadioMessage: return DoStrangeRadioMessage(Target);
	case SULogic::HorrorEvent::CarWontStart: return DoCarWontStart(Target);
	case SULogic::HorrorEvent::BuildingSounds: return DoBuildingSounds(Target);
	case SULogic::HorrorEvent::HunterChase: return DoHunterChase(Target);
	default: return false;
	}
}

// ------------------------------------------------------------------------------------ placement helpers

bool ASUHorrorDirector::FindGroundPoint(const FVector& Around, FVector& OutPoint) const
{
	const ASUWorldBuilder* B = ASUWorldBuilder::Get(this);
	if (!B || !B->IsInsidePlayableArea(Around))
	{
		return false;
	}
	OutPoint = B->GroundPoint(Around.X, Around.Y, 0.f);
	// stand on real collision (trees, rocks and houses are not in the height function)
	FHitResult Hit;
	FCollisionQueryParams Params(FName(TEXT("SUDirectorGround")), false);
	if (GetWorld()->LineTraceSingleByChannel(Hit, OutPoint + FVector(0.f, 0.f, 600.f), OutPoint - FVector(0.f, 0.f, 600.f), ECC_Visibility, Params))
	{
		if (Hit.ImpactNormal.Z < 0.75f || Hit.Location.Z > OutPoint.Z + 150.f)
		{
			return false; // steep, or on a roof / rock
		}
		OutPoint = Hit.Location;
	}
	// enough room to stand
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(45.f, 120.f);
	return !GetWorld()->OverlapBlockingTestByChannel(OutPoint + FVector(0.f, 0.f, 130.f), FQuat::Identity, ECC_Pawn, Capsule);
}

bool ASUHorrorDirector::IsHiddenSpawnPoint(const FVector& Location, float MinDistance, float MaxViewDot) const
{
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		const ASUCharacter* C = *It;
		if (C->IsDead() || !C->GetFirstPersonCamera())
		{
			continue;
		}
		const FVector Eye = C->GetFirstPersonCamera()->GetComponentLocation();
		const FVector To = Location + FVector(0.f, 0.f, 120.f) - Eye;
		if (To.Size() < MinDistance)
		{
			return false;
		}
		if (FVector::DotProduct(C->GetBaseAimRotation().Vector(), To.GetSafeNormal()) > MaxViewDot)
		{
			// in the view cone: only acceptable when something blocks the view
			FHitResult Hit;
			FCollisionQueryParams Params(FName(TEXT("SUDirectorView")), false, C);
			if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Location + FVector(0.f, 0.f, 120.f), ECC_Visibility, Params))
			{
				return false;
			}
		}
	}
	return true;
}

// ------------------------------------------------------------------------------------ events

bool ASUHorrorDirector::DoDistantSound(ASUCharacter* Target)
{
	const float Yaw = FMath::FRandRange(0.f, 360.f);
	const FVector Around = Target->GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * FMath::FRandRange(6000.f, 14000.f);
	const ASUWorldBuilder* B = ASUWorldBuilder::Get(this);
	const FVector Loc = B ? B->GroundPoint(Around.X, Around.Y, 150.f) : Around;
	const int32 Pick = FMath::RandRange(0, 2);
	if (Pick == 0)
	{
		ASUSoundEmitter::PlayAt(this, Loc, ESUSoundKind::Creature, 0, 0.7f, 1.f, 22000.f, 8.f); // scream
	}
	else if (Pick == 1)
	{
		ASUSoundEmitter::PlayAt(this, Loc, ESUSoundKind::Impact, 0, 1.f, 1.f, 16000.f, 8.f, 3, 0.7f); // knocking on wood
	}
	else
	{
		ASUSoundEmitter::PlayAt(this, Loc, ESUSoundKind::Creature, 2, 1.f, 1.f, 12000.f, 8.f, 2, 1.4f); // clicking
	}
	return true;
}

bool ASUHorrorDirector::DoLightFlicker(ASUCharacter* Target)
{
	int32 Count = 0;
	for (TActorIterator<ASULamp> It(GetWorld()); It; ++It)
	{
		if (It->IsLit() && FVector::Dist(It->GetActorLocation(), Target->GetActorLocation()) < 1800.f)
		{
			It->MulticastFlicker(FMath::FRandRange(1.2f, 3.2f));
			++Count;
		}
	}
	return Count > 0;
}

bool ASUHorrorDirector::DoRadioInterference(ASUCharacter* Target)
{
	bool bDone = false;
	if (ASUVehicle* V = Target->GetVehicle())
	{
		if (V->IsRadioOn())
		{
			V->TriggerRadioIntrusion(INDEX_NONE); // distortion only, no voice
			bDone = true;
		}
	}
	for (TActorIterator<ASURadioStation> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), Target->GetActorLocation()) < 2000.f)
		{
			It->MulticastInterference(FMath::FRandRange(2.f, 4.5f));
			bDone = true;
		}
	}
	return bDone;
}

bool ASUHorrorDirector::DoTreeWatcher(ASUCharacter* Target)
{
	const ASUWorldBuilder* B = ASUWorldBuilder::Get(this);
	const SULogic::WorldLayout* Layout = B ? B->GetLayout() : nullptr;
	const float ViewYaw = Target->GetBaseAimRotation().Yaw;
	for (int32 Try = 0; Try < 16; ++Try)
	{
		// at the edge of the field of view or behind, between the trees
		const float Side = FMath::RandBool() ? 1.f : -1.f;
		const float Yaw = ViewYaw + Side * FMath::FRandRange(55.f, 150.f);
		const FVector Around = Target->GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * FMath::FRandRange(2800.f, 5200.f);
		if (Layout && Layout->TreeDensity(Around.X / 100.0, Around.Y / 100.0) < 0.25)
		{
			continue;
		}
		if (B && B->DistanceToRoad(Around) < 500.f)
		{
			continue;
		}
		FVector Ground;
		if (!FindGroundPoint(Around, Ground) || !IsHiddenSpawnPoint(Ground, 2500.f, 0.8f))
		{
			continue;
		}
		const float FaceYaw = (Target->GetActorLocation() - Ground).Rotation().Yaw;
		if (ASUCreature* C = ASUCreature::SpawnCreature(GetWorld(), ESUCreatureType::Observer, Ground, FaceYaw, ESUCreatureState::Watch, Target))
		{
			C->SetLifetime(45.f);
			return true;
		}
	}
	return false;
}

bool ASUHorrorDirector::DoRoadsideFigure(ASUCharacter* Target)
{
	ASUVehicle* V = Target->GetVehicle();
	const ASUWorldBuilder* B = ASUWorldBuilder::Get(this);
	if (!V || !B)
	{
		return false;
	}
	FVector Fwd = V->GetVelocity().GetSafeNormal2D();
	if (Fwd.IsNearlyZero())
	{
		Fwd = V->GetActorForwardVector();
	}
	for (int32 Try = 0; Try < 6; ++Try)
	{
		FVector RoadPoint, RoadDir;
		if (!B->FindRoadPointAhead(V->GetActorLocation(), Fwd, FMath::FRandRange(17000.f, 26000.f), RoadPoint, RoadDir))
		{
			return false;
		}
		const FVector Right = FVector::CrossProduct(FVector::UpVector, RoadDir).GetSafeNormal();
		const FVector Around = RoadPoint + Right * (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(650.f, 950.f);
		FVector Ground;
		if (!FindGroundPoint(Around, Ground) || !IsHiddenSpawnPoint(Ground, 6000.f, 0.97f))
		{
			continue;
		}
		const float FaceYaw = (RoadPoint - Ground).Rotation().Yaw;
		if (ASUCreature* C = ASUCreature::SpawnCreature(GetWorld(), ESUCreatureType::Observer, Ground, FaceYaw, ESUCreatureState::Watch, V))
		{
			C->SetLifetime(40.f);
			return true;
		}
	}
	return false;
}

bool ASUHorrorDirector::DoStrangeRadioMessage(ASUCharacter* Target)
{
	if (ASUVehicle* V = Target->GetVehicle())
	{
		const int32 Count = FSUMissionDatabase::StrangeTransmissions().Num();
		if (Count > 0)
		{
			V->TriggerRadioIntrusion(FMath::RandRange(0, Count - 1));
			return true;
		}
		return false;
	}
	ASUMissionManager* MM = ASUMissionManager::Get(this);
	return MM && MM->PlayStrangeTransmission();
}

bool ASUHorrorDirector::DoCarWontStart(ASUCharacter* Target)
{
	ASUGameState* GS = ASUGameState::Get(this);
	ASUVehicle* V = GS ? GS->MainVehicle.Get() : nullptr;
	if (!V || V->IsEngineRunning() || V->HasOccupants())
	{
		return false;
	}
	V->BlockNextStarts(2);
	// something brushes along the car in the dark
	ASUSoundEmitter::PlayAt(this, V->GetActorLocation() + FVector(0.f, 0.f, 60.f), ESUSoundKind::Impact, 2, 0.6f, 1.f, 2500.f, 6.f);
	return true;
}

bool ASUHorrorDirector::DoBuildingSounds(ASUCharacter* Target)
{
	ASUAreaTrigger* Area = FindNearbyBuildingArea(Target->GetActorLocation(), 2500.f);
	if (!Area)
	{
		return false;
	}
	// on the far side of the building, seen from the player
	FVector Away = (Area->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
	}
	const FVector Loc = Area->GetActorLocation() + Away * 500.f + FVector(0.f, 0.f, FMath::FRandRange(-50.f, 120.f));
	switch (FMath::RandRange(0, 3))
	{
	case 0: ASUSoundEmitter::PlayAt(this, Loc, ESUSoundKind::Impact, 0, 1.f, 1.f, 3000.f, 6.f, 3, 0.55f); break;  // three knocks
	case 1: ASUSoundEmitter::PlayAt(this, Loc, ESUSoundKind::Impact, 2, 0.9f, 1.f, 2500.f, 6.f, 2, 1.1f); break;  // scratching
	case 2: ASUSoundEmitter::PlayAt(this, Loc, ESUSoundKind::DoorCreak, 0, 1.f, 1.f, 2500.f, 6.f); break;        // a door somewhere
	default: ASUSoundEmitter::PlayAt(this, Loc + FVector(0.f, 0.f, 250.f), ESUSoundKind::Footstep, 2, 0.8f, 1.f, 2500.f, 8.f, 4, 0.7f); break; // steps on the roof / floor above
	}
	return true;
}

bool ASUHorrorDirector::DoHunterChase(ASUCharacter* Target)
{
	ASUVehicle* V = Target->GetVehicle();
	if (!V)
	{
		return false;
	}
	FVector Fwd = V->GetVelocity().GetSafeNormal2D();
	if (Fwd.IsNearlyZero())
	{
		Fwd = V->GetActorForwardVector();
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
	for (int32 Try = 0; Try < 10; ++Try)
	{
		const FVector Around = V->GetActorLocation() - Fwd * FMath::FRandRange(6000.f, 9000.f) + Right * (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(900.f, 2000.f);
		FVector Ground;
		if (!FindGroundPoint(Around, Ground) || !IsHiddenSpawnPoint(Ground, 4500.f, 0.6f))
		{
			continue;
		}
		const float FaceYaw = (V->GetActorLocation() - Ground).Rotation().Yaw;
		if (ASUCreature* C = ASUCreature::SpawnCreature(GetWorld(), ESUCreatureType::Hunter, Ground, FaceYaw, ESUCreatureState::Stalk, V))
		{
			C->SetLifetime(55.f);
			ASUSoundEmitter::PlayAt(this, Ground, ESUSoundKind::Stinger, 0, 0.7f, 1.f, 9000.f, 8.f);
			return true;
		}
	}
	return false;
}
