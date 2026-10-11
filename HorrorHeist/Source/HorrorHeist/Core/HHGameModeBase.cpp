#include "Core/HHGameModeBase.h"
#include "Player/HHCharacter.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "UI/HHLobbyHUD.h"
#include "HorrorHeist.h"
#include "CollisionQueryParams.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"

AHHGameModeBase::AHHGameModeBase()
{
	DefaultPawnClass = AHHCharacter::StaticClass();
	PlayerControllerClass = AHHPlayerController::StaticClass();
	PlayerStateClass = AHHPlayerState::StaticClass();
	HUDClass = AHHLobbyHUD::StaticClass();
	bUseSeamlessTravel = false;
}

int32 AHHGameModeBase::FindFreeCrewSlot() const
{
	TSet<int32> Used;
	if (GameState)
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (const AHHPlayerState* HHPlayerState = Cast<AHHPlayerState>(PlayerState))
			{
				Used.Add(HHPlayerState->GetCrewSlot());
			}
		}
	}
	int32 Slot = 0;
	while (Used.Contains(Slot))
	{
		++Slot;
	}
	return Slot;
}

bool AHHGameModeBase::HasCrewLeader() const
{
	if (GameState)
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const AHHPlayerState* HHPlayerState = Cast<AHHPlayerState>(PlayerState);
			if (HHPlayerState && HHPlayerState->IsCrewLeader())
			{
				return true;
			}
		}
	}
	return false;
}

void AHHGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	// Slot and leadership before Super spawns the pawn, so colours and the preferred start
	// are right from the first frame. The host leads; on a dedicated server the first arrival.
	if (AHHPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AHHPlayerState>() : nullptr)
	{
		PlayerState->SetCrewSlot(FindFreeCrewSlot());
		PlayerState->SetCrewLeader(NewPlayer->IsLocalController() || !HasCrewLeader());
	}

	Super::PostLogin(NewPlayer);
}

bool AHHGameModeBase::IsStartUsable(const AActor* Start, const AController* Player)
{
	UWorld* World = GetWorld();
	if (!Start || !World)
	{
		return false;
	}

	// Somebody already standing there?
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (*It != (Player ? Player->GetPawn() : nullptr) && FVector::DistSquared2D(It->GetActorLocation(), Start->GetActorLocation()) < FMath::Square(80.f)
			&& FMath::Abs(It->GetActorLocation().Z - Start->GetActorLocation().Z) < 200.f)
		{
			return false;
		}
	}

	// Would the capsule be stuck in a wall or a prop that was moved onto the start?
	UClass* PawnClass = GetDefaultPawnClassForController(const_cast<AController*>(Player));
	const APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;
	if (PawnToFit && World->EncroachingBlockingGeometry(PawnToFit, Start->GetActorLocation(), Start->GetActorRotation()))
	{
		return false;
	}
	return true;
}

AActor* AHHGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	const AHHPlayerState* State = Player ? Player->GetPlayerState<AHHPlayerState>() : nullptr;
	const FName SlotTag = State ? FName(*FString::Printf(TEXT("Crew%d"), State->GetCrewSlot())) : NAME_None;

	APlayerStart* SlotStart = nullptr;
	TArray<APlayerStart*> Free;
	TArray<APlayerStart*> Blocked;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!IsStartUsable(Start, Player))
		{
			Blocked.Add(Start);
			continue;
		}
		if (!SlotTag.IsNone() && Start->PlayerStartTag == SlotTag)
		{
			SlotStart = Start;
		}
		Free.Add(Start);
	}

	if (SlotStart)
	{
		return SlotStart;
	}
	if (Free.Num() > 0)
	{
		return Free[FMath::RandRange(0, Free.Num() - 1)];
	}
	if (Blocked.Num() > 0)
	{
		// Every start is occupied or encroached: spawning adjusts the location (see below).
		UE_LOG(LogHorrorHeist, Warning, TEXT("All %d player starts are blocked; spawning with location adjustment."), Blocked.Num());
		return Blocked[FMath::RandRange(0, Blocked.Num() - 1)];
	}

	UE_LOG(LogHorrorHeist, Error, TEXT("Map %s has no PlayerStart. Using a fallback spawn point - rebuild the level ('The Quiet Job > Build / Rebuild All Content')."),
		*GetNameSafe(World));
	return GetFallbackStart();
}

AActor* AHHGameModeBase::GetFallbackStart()
{
	if (FallbackStart)
	{
		return FallbackStart;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Drop a probe from above the origin and stand on whatever is there.
	FVector Location(0.f, 0.f, 200.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHFallbackStart), false);
	if (World->LineTraceSingleByChannel(Hit, FVector(0.f, 0.f, 5000.f), FVector(0.f, 0.f, -5000.f), ECC_Visibility, Params))
	{
		Location = Hit.ImpactPoint + FVector(0.f, 0.f, 100.f);
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags |= RF_Transient;
	FallbackStart = World->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), Location, FRotator::ZeroRotator, SpawnInfo);
	return FallbackStart;
}

APawn* AHHGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	// Never leave a player without a body because something overlaps the start: nudge the
	// capsule out of the way, and if that fails spawn anyway (the movement code resolves it).
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	APawn* Pawn = PawnClass ? GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo) : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogHorrorHeist, Error, TEXT("Could not spawn %s for %s at %s."), *GetNameSafe(PawnClass), *GetNameSafe(NewPlayer), *SpawnTransform.GetLocation().ToString());
	}
	return Pawn;
}

void AHHGameModeBase::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (NewPlayer && !NewPlayer->GetPawn() && !NewPlayer->IsPendingKillPending())
	{
		// The chosen start did not work out (deleted, out of the world): try the fallback.
		if (AActor* Fallback = GetFallbackStart())
		{
			UE_LOG(LogHorrorHeist, Warning, TEXT("%s had no pawn after RestartPlayer; retrying at the fallback start."), *GetNameSafe(NewPlayer));
			RestartPlayerAtTransform(NewPlayer, Fallback->GetActorTransform());
		}
	}
	if (NewPlayer && NewPlayer->GetPawn())
	{
		UE_LOG(LogHorrorHeist, Log, TEXT("%s spawned as %s at %s."), *GetNameSafe(NewPlayer), *GetNameSafe(NewPlayer->GetPawn()),
			*NewPlayer->GetPawn()->GetActorLocation().ToCompactString());
	}
}

bool AHHGameModeBase::TravelCrewTo(const FString& MapPackage, const FString& ExtraOptions)
{
	UWorld* World = GetWorld();
	if (!World || MapPackage.IsEmpty())
	{
		return false;
	}

	FString URL = MapPackage + ExtraOptions;
	if (World->GetNetMode() == NM_ListenServer)
	{
		URL += TEXT("?listen");
	}
	UE_LOG(LogHorrorHeist, Log, TEXT("Crew travel: %s"), *URL);
	return World->ServerTravel(URL, true);
}
