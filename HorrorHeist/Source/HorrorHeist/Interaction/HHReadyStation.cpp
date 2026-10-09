#include "Interaction/HHReadyStation.h"
#include "Lobby/HHLobbyGameMode.h"
#include "Player/HHPlayerState.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "HHReadyStation"

AHHReadyStation::AHHReadyStation()
{
	Prompt = LOCTEXT("ReadyUp", "Load up - ready");
}

FText AHHReadyStation::GetInteractionPrompt(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
	if (State && State->IsReady())
	{
		return LOCTEXT("StandDown", "Step out - not ready");
	}
	return Prompt;
}

void AHHReadyStation::Interact(AActor* Interactor)
{
	Super::Interact(Interactor);

	const APawn* Pawn = Cast<APawn>(Interactor);
	AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
	if (!State || !HasAuthority())
	{
		return;
	}
	State->SetReady(!State->IsReady());
	if (AHHLobbyGameMode* GameMode = GetWorld()->GetAuthGameMode<AHHLobbyGameMode>())
	{
		GameMode->HandleReadyChanged(State);
	}
}

#undef LOCTEXT_NAMESPACE
