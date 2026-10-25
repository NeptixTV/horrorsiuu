#include "Horror/SUScriptedWindowFigure.h"
#include "SignalUnknown.h"
#include "Horror/SUCreature.h"
#include "Player/SUCharacter.h"
#include "Interaction/SUDoor.h"
#include "Mission/SUAreaTrigger.h"
#include "Mission/SUMissionManager.h"
#include "Mission/SUMissionTypes.h"
#include "Evidence/SUEvidenceProp.h"
#include "Audio/SUSoundEmitter.h"
#include "Core/SUGameState.h"
#include "Core/SUEventSubsystem.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"

ASUScriptedWindowFigure::ASUScriptedWindowFigure()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = false; // server-side director of the sequence; everything it drives replicates
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASUScriptedWindowFigure::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		SetActorTickEnabled(false);
	}
}

bool ASUScriptedWindowFigure::IsAnySequenceRunning(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<ASUScriptedWindowFigure> It(World); It; ++It)
		{
			if (It->IsRunning())
			{
				return true;
			}
		}
	}
	return false;
}

FName ASUScriptedWindowFigure::DoneFlag() const
{
	return FName(*(SequenceId.ToString() + TEXT("_Done")));
}

void ASUScriptedWindowFigure::SetStage(ESUWindowSequenceStage NewStage)
{
	Stage = NewStage;
	StageTime = 0.f;
	UE_LOG(LogSignalUnknown, Log, TEXT("%s: stage %d"), *SequenceId.ToString(), static_cast<int32>(NewStage));
}

bool ASUScriptedWindowFigure::IsObjectiveActive() const
{
	const ASUMissionManager* MM = ASUMissionManager::Get(this);
	if (!MM || MM->GetActiveMissionId() != MissionId)
	{
		return false;
	}
	const FSUMissionDef* Def = MM->GetActiveDef();
	if (!Def)
	{
		return false;
	}
	for (int32 I = 0; I < Def->Objectives.Num(); ++I)
	{
		if (Def->Objectives[I].Id == ObjectiveId)
		{
			return MM->IsObjectiveActive(I);
		}
	}
	return false;
}

bool ASUScriptedWindowFigure::AnyPlayerInside() const
{
	if (!InteriorArea)
	{
		return false;
	}
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && InteriorArea->IsActorInside(*It))
		{
			return true;
		}
	}
	return false;
}

ASUCharacter* ASUScriptedWindowFigure::FindPlayerLookingThroughWindow() const
{
	const FVector Target = Figure ? Figure->GetActorLocation() + FVector(0.f, 0.f, 50.f) : FigureLocation;
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		ASUCharacter* C = *It;
		if (C->IsDead() || !C->GetFirstPersonCamera() || (InteriorArea && !InteriorArea->IsActorInside(C)))
		{
			continue;
		}
		const FVector Eye = C->GetFirstPersonCamera()->GetComponentLocation();
		const FVector ToWindow = WindowLocation - Eye;
		const FVector ToFigure = Target - Eye;
		const FVector View = C->GetBaseAimRotation().Vector();
		if (ToFigure.Size() > 1800.f || FVector::DotProduct(View, ToWindow.GetSafeNormal()) < 0.9f || FVector::DotProduct(View, ToFigure.GetSafeNormal()) < 0.85f)
		{
			continue;
		}
		FCollisionQueryParams Params(FName(TEXT("SUWindowLook")), false, C);
		FHitResult Hit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params);
		if (!bBlocked || Hit.GetActor() == Figure)
		{
			return C;
		}
	}
	return nullptr;
}

void ASUScriptedWindowFigure::SpawnTracks()
{
	const FVector Dir = (RetreatLocation - TracksLocation).GetSafeNormal2D();
	ASUEvidenceProp::SpawnProp(GetWorld(), TEXT("EV_M01_WindowTracks"), SUText(TEXT("Abdrücke unter dem Fenster")), TEXT("Traces"),
		ESUEvidencePropLook::CreatureTracks, TracksLocation, Dir.Rotation().Yaw, 1601);
}

void ASUScriptedWindowFigure::Complete()
{
	ASUGameState* GS = ASUGameState::Get(this);
	if (GS)
	{
		GS->SetWorldFlag(DoneFlag());
	}
	if (SlamDoor && SlamDoor->IsOpen())
	{
		SlamDoor->SetOpen(false, WindowLocation, true);
	}
	ASUSoundEmitter::PlayAt(this, WindowLocation, ESUSoundKind::Stinger, 1, 1.f, 1.f, 2500.f, 6.f);
	SpawnTracks();
	USUEventSubsystem::Post(this, SUEvent::HorrorScripted, SequenceId, nullptr);
}

void ASUScriptedWindowFigure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StageTime += DeltaSeconds;
	switch (Stage)
	{
	case ESUWindowSequenceStage::Waiting:
	{
		const ASUGameState* GS = ASUGameState::Get(this);
		if (GS && GS->HasWorldFlag(DoneFlag()))
		{
			// loaded game: the event already happened
			SpawnTracks();
			SetStage(ESUWindowSequenceStage::Done);
			break;
		}
		const bool bInside = AnyPlayerInside();
		InsideTime = bInside ? InsideTime + DeltaSeconds : 0.f;
		if (InsideTime > 5.f && IsObjectiveActive())
		{
			// three knocks somewhere at the back of the house, the figure waits outside the window
			ASUSoundEmitter::PlayAt(this, NoiseLocation, ESUSoundKind::Impact, 0, 1.f, 1.f, 3000.f, 6.f, 3, 0.55f);
			USUEventSubsystem::Noise(this, NoiseLocation, 0.6f, this);
			const FVector Ground = FigureLocation - FVector(0.f, 0.f, 130.f);
			const float Yaw = (WindowLocation - FigureLocation).Rotation().Yaw;
			Figure = ASUCreature::SpawnCreature(GetWorld(), ESUCreatureType::Observer, Ground, Yaw, ESUCreatureState::Scripted, nullptr);
			if (Figure)
			{
				Figure->ScriptedFace(WindowLocation);
				Figure->SetLifetime(600.f);
			}
			bHintPlayed = false;
			SetStage(ESUWindowSequenceStage::Knocking);
		}
		break;
	}
	case ESUWindowSequenceStage::Knocking:
		if (StageTime > 2.5f)
		{
			SetStage(ESUWindowSequenceStage::WaitForLook);
		}
		break;

	case ESUWindowSequenceStage::WaitForLook:
	{
		if (ASUCharacter* Watcher = FindPlayerLookingThroughWindow())
		{
			ASUSoundEmitter::PlayAt(this, WindowLocation, ESUSoundKind::Stinger, 0, 1.f, 1.f, 2000.f, 8.f);
			if (Figure)
			{
				Figure->ScriptedFace(Watcher->GetActorLocation());
			}
			SetStage(ESUWindowSequenceStage::Revealed);
			break;
		}
		if (!AnyPlayerInside())
		{
			InsideTime += DeltaSeconds; // reused as "outside" timer here
			if (InsideTime > 8.f)
			{
				// everyone left: reset, the figure vanishes unseen
				if (Figure)
				{
					Figure->RequestDespawn();
					Figure = nullptr;
				}
				InsideTime = 0.f;
				SetStage(ESUWindowSequenceStage::Waiting);
			}
			break;
		}
		InsideTime = 0.f;
		if (!bHintPlayed && StageTime > 15.f)
		{
			// something scratches over the window frame
			bHintPlayed = true;
			ASUSoundEmitter::PlayAt(this, WindowLocation, ESUSoundKind::Impact, 2, 0.8f, 1.f, 2000.f, 6.f, 2, 0.9f);
		}
		if (StageTime > 45.f)
		{
			// fallback: nobody looked. Knock on the glass, slam the door, and the figure is gone.
			ASUSoundEmitter::PlayAt(this, WindowLocation, ESUSoundKind::Impact, 4, 1.f, 1.f, 2500.f, 6.f);
			if (Figure)
			{
				Figure->RequestDespawn();
			}
			Complete();
			SetStage(ESUWindowSequenceStage::Done);
		}
		break;
	}
	case ESUWindowSequenceStage::Revealed:
		if (StageTime > 1.8f || (Figure && Figure->GetLitTime() > 0.25f))
		{
			if (Figure)
			{
				Figure->ScriptedMoveTo(RetreatLocation, 950.f);
			}
			Complete();
			SetStage(ESUWindowSequenceStage::Retreating);
		}
		break;

	case ESUWindowSequenceStage::Retreating:
		if (!Figure || Figure->HasReachedScriptedGoal(150.f) || StageTime > 8.f)
		{
			if (Figure)
			{
				Figure->RequestDespawn();
				Figure = nullptr;
			}
			SetStage(ESUWindowSequenceStage::Done);
		}
		break;

	case ESUWindowSequenceStage::Done:
		SetActorTickEnabled(false);
		break;
	}
}
