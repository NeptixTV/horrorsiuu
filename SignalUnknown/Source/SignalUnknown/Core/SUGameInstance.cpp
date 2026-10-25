#include "Core/SUGameInstance.h"
#include "SignalUnknown.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void USUGameInstance::Init()
{
	Super::Init();

	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &USUGameInstance::HandleNetworkFailure);
	}

	// "-SUHost" / "-SUSolo" on the command line skip the menu (handy for testing with two instances)
	if (FParse::Param(FCommandLine::Get(), TEXT("SUHost")))
	{
		StartMode = ESUStartMode::Host;
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("SUSolo")))
	{
		StartMode = ESUStartMode::Solo;
	}
}

void USUGameInstance::StartSolo(bool bContinueSave)
{
	StartMode = ESUStartMode::Solo;
	bLoadSaveOnStart = bContinueSave;
}

void USUGameInstance::HostCoop(bool bContinueSave)
{
	StartMode = ESUStartMode::Host;
	bLoadSaveOnStart = bContinueSave;
	UGameplayStatics::OpenLevel(this, FName(GetGameMapPath()), true, TEXT("listen"));
}

void USUGameInstance::JoinCoop(const FString& Address)
{
	const FString Trimmed = Address.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return;
	}
	LastJoinAddress = Trimmed;
	StartMode = ESUStartMode::Client;
	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		UE_LOG(LogSignalUnknown, Log, TEXT("Joining %s"), *Trimmed);
		PC->ClientTravel(Trimmed, TRAVEL_Absolute);
	}
}

void USUGameInstance::ReturnToMenu()
{
	StartMode = ESUStartMode::Menu;
	bLoadSaveOnStart = false;
	UGameplayStatics::OpenLevel(this, FName(GetGameMapPath()), true);
}

void USUGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogSignalUnknown, Warning, TEXT("Network failure: %s"), *ErrorString);
	// the engine travels back to the default map; show the menu there
	StartMode = ESUStartMode::Menu;
}
