#include "Core/HHGameInstance.h"
#include "Core/HHGameData.h"
#include "Core/HHDeveloperSettings.h"
#include "HorrorHeist.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "HHGameInstance"

void UHHGameInstance::Init()
{
	// Load the game data before Super::Init() initializes the subsystems that depend on it
	// (the profile needs starter items and starting cash).
	if (const UHHDeveloperSettings* Settings = UHHDeveloperSettings::Get())
	{
		GameData = Settings->GameData.LoadSynchronous();
	}
	if (!GameData)
	{
		UE_LOG(LogHorrorHeist, Warning, TEXT("No HHGameData asset found. In the editor use 'The Quiet Job > Build / Rebuild All Content' (see README)."));
	}

	Super::Init();

	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UHHGameInstance::HandleNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &UHHGameInstance::HandleTravelFailure);
	}
}

void UHHGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	Super::Shutdown();
}

void UHHGameInstance::SetPendingMessage(const FText& Title, const FText& Body)
{
	PendingTitle = Title;
	PendingBody = Body;
	bHasPendingMessage = true;
}

bool UHHGameInstance::ConsumePendingMessage(FText& OutTitle, FText& OutBody)
{
	if (!bHasPendingMessage)
	{
		return false;
	}
	OutTitle = PendingTitle;
	OutBody = PendingBody;
	bHasPendingMessage = false;
	return true;
}

void UHHGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogHorrorHeist, Warning, TEXT("Network failure (%s): %s"), ENetworkFailure::ToString(FailureType), *ErrorString);

	FText Body;
	switch (FailureType)
	{
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::ConnectionTimeout:
		Body = LOCTEXT("ConnLost", "The connection to the crew was lost.");
		break;
	case ENetworkFailure::FailureReceived:
	case ENetworkFailure::PendingConnectionFailure:
		Body = ErrorString.IsEmpty()
			? LOCTEXT("ConnRefused", "The host refused the connection.")
			: FText::FromString(ErrorString);
		break;
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		Body = LOCTEXT("Outdated", "Your game version does not match the host's.");
		break;
	default:
		Body = FText::FromString(ErrorString);
		break;
	}
	SetPendingMessage(LOCTEXT("NetFailTitle", "Disconnected"), Body);
}

void UHHGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogHorrorHeist, Warning, TEXT("Travel failure (%s): %s"), ETravelFailure::ToString(FailureType), *ErrorString);
	SetPendingMessage(LOCTEXT("TravelFailTitle", "Could not travel"), FText::FromString(ErrorString));
}

#undef LOCTEXT_NAMESPACE
