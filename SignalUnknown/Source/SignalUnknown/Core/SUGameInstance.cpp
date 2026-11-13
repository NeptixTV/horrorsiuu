#include "Core/SUGameInstance.h"
#include "SignalUnknown.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineExternalUIInterface.h"

namespace
{
	// marks our lobbies: the Steam test app id is shared with many other projects
	const FName SUSessionKey(TEXT("SUGAME"));
	const FString SUSessionValue(TEXT("SignalUnknown"));
}

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

	// invitations accepted in the Steam overlay (also while the game is in the menu)
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		InviteHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &USUGameInstance::HandleInviteAccepted));
	}
	UE_LOG(LogSignalUnknown, Log, TEXT("Online service: %s"), GetOnlineServiceName().IsEmpty() ? TEXT("none") : *GetOnlineServiceName());
}

// ------------------------------------------------------------------------------------ online sessions

IOnlineSessionPtr USUGameInstance::GetSessions() const
{
	IOnlineSubsystem* Online = Online::GetSubsystem(GetWorld());
	return Online ? Online->GetSessionInterface() : nullptr;
}

bool USUGameInstance::IsSteamActive() const
{
	const IOnlineSubsystem* Online = Online::GetSubsystem(GetWorld());
	return Online && Online->GetSubsystemName() == FName(TEXT("STEAM"));
}

FString USUGameInstance::GetOnlineServiceName() const
{
	const IOnlineSubsystem* Online = Online::GetSubsystem(GetWorld());
	if (!Online)
	{
		return FString();
	}
	return IsSteamActive() ? FString(TEXT("Steam")) : FString(TEXT("LAN"));
}

bool USUGameInstance::HasOnlineSession() const
{
	const IOnlineSessionPtr Sessions = GetSessions();
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

void USUGameInstance::OpenListenLevel()
{
	UGameplayStatics::OpenLevel(this, FName(GetGameMapPath()), true, TEXT("listen"));
}

void USUGameInstance::CreateOnlineSession()
{
	const IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		OpenListenLevel();
		return;
	}
	const bool bLan = !IsSteamActive();
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = 4;
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = bLan;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = !bLan;
	Settings.bAllowJoinViaPresence = !bLan;
	Settings.bUseLobbiesIfAvailable = !bLan;
	Settings.Set(SUSessionKey, SUSessionValue, EOnlineDataAdvertisementType::ViaOnlineService);
	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &USUGameInstance::HandleCreateSession));
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		UE_LOG(LogSignalUnknown, Warning, TEXT("Online session could not be created - hosting without it (direct IP only)."));
		OpenListenLevel();
	}
}

void USUGameInstance::HandleCreateSession(FName SessionName, bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}
	UE_LOG(LogSignalUnknown, Log, TEXT("Online session %s"), bWasSuccessful ? TEXT("created") : TEXT("failed - hosting without it"));
	OpenListenLevel();
}

void USUGameInstance::HandleDestroySession(FName SessionName, bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	if (bHostAfterSessionChange)
	{
		bHostAfterSessionChange = false;
		CreateOnlineSession();
	}
}

void USUGameInstance::FindGames()
{
	const IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		OnlineStatus = TEXT("Kein Online-Dienst verfügbar. Beitreten geht nur über die IP-Adresse.");
		return;
	}
	if (bSearching)
	{
		return;
	}
	const bool bLan = !IsSteamActive();
	FoundGames.Reset();
	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 200;
	SessionSearch->bIsLanQuery = bLan;
	if (!bLan)
	{
		SessionSearch->QuerySettings.Set(FName(TEXT("LOBBYSEARCH")), true, EOnlineComparisonOp::Equals);
		SessionSearch->QuerySettings.Set(SUSessionKey, SUSessionValue, EOnlineComparisonOp::Equals);
	}
	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &USUGameInstance::HandleFindSessions));
	bSearching = Sessions->FindSessions(0, SessionSearch.ToSharedRef());
	OnlineStatus = bSearching ? FString(TEXT("Suche läuft …")) : FString(TEXT("Die Suche konnte nicht gestartet werden."));
	if (!bSearching)
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}
}

void USUGameInstance::HandleFindSessions(bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}
	bSearching = false;
	FoundGames.Reset();
	if (SessionSearch.IsValid())
	{
		// keep only our own games (the list index must match SessionSearch->SearchResults)
		for (int32 I = SessionSearch->SearchResults.Num() - 1; I >= 0; --I)
		{
			FString Value;
			const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[I];
			if (!Result.IsValid() || !Result.Session.SessionSettings.Get(SUSessionKey, Value) || Value != SUSessionValue)
			{
				SessionSearch->SearchResults.RemoveAt(I);
			}
		}
		for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
		{
			FSUFoundGame Game;
			Game.HostName = Result.Session.OwningUserName.IsEmpty() ? FString(TEXT("Unbekannt")) : Result.Session.OwningUserName;
			Game.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Game.Players = Game.MaxPlayers - Result.Session.NumOpenPublicConnections;
			Game.Ping = Result.PingInMs;
			FoundGames.Add(Game);
		}
	}
	OnlineStatus = !bWasSuccessful ? FString(TEXT("Die Suche ist fehlgeschlagen."))
		: (FoundGames.Num() == 0 ? FString(TEXT("Kein Spiel gefunden. Der Host muss zuerst „Koop hosten“ wählen."))
			: FString::Printf(TEXT("%d Spiel(e) gefunden."), FoundGames.Num()));
}

void USUGameInstance::JoinFoundGame(int32 Index)
{
	if (SessionSearch.IsValid() && SessionSearch->SearchResults.IsValidIndex(Index))
	{
		JoinSessionResult(SessionSearch->SearchResults[Index]);
	}
}

void USUGameInstance::JoinSessionResult(const FOnlineSessionSearchResult& InResult)
{
	const IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid() || !InResult.IsValid())
	{
		return;
	}
	FOnlineSessionSearchResult Result = InResult;
	if (IsSteamActive())
	{
		// Steam lobbies: the search result does not always carry these flags, joining fails without them
		Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
		Result.Session.SessionSettings.bUsesPresence = true;
	}
	if (Sessions->GetNamedSession(NAME_GameSession) != nullptr)
	{
		Sessions->DestroySession(NAME_GameSession);
	}
	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &USUGameInstance::HandleJoinSession));
	OnlineStatus = TEXT("Verbinde …");
	if (!Sessions->JoinSession(0, NAME_GameSession, Result))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		OnlineStatus = TEXT("Beitreten ist fehlgeschlagen.");
	}
}

void USUGameInstance::HandleJoinSession(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	const IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		return;
	}
	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	FString Address;
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions->GetResolvedConnectString(SessionName, Address))
	{
		OnlineStatus = TEXT("Beitreten ist fehlgeschlagen (Spiel voll oder nicht mehr vorhanden).");
		UE_LOG(LogSignalUnknown, Warning, TEXT("Joining the online session failed (%d)"), static_cast<int32>(Result));
		return;
	}
	StartMode = ESUStartMode::Client;
	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		UE_LOG(LogSignalUnknown, Log, TEXT("Joining online session at %s"), *Address);
		PC->ClientTravel(Address, TRAVEL_Absolute);
	}
}

void USUGameInstance::HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (bWasSuccessful && InviteResult.IsValid())
	{
		JoinSessionResult(InviteResult);
	}
}

void USUGameInstance::ShowInviteDialog()
{
	IOnlineSubsystem* Online = Online::GetSubsystem(GetWorld());
	const IOnlineExternalUIPtr ExternalUI = Online ? Online->GetExternalUIInterface() : nullptr;
	if (ExternalUI.IsValid())
	{
		ExternalUI->ShowInviteUI(0, NAME_GameSession);
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
	// announce the game first (Steam lobby or LAN session), then open the map as listen server
	const IOnlineSessionPtr Sessions = GetSessions();
	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr)
	{
		bHostAfterSessionChange = true;
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &USUGameInstance::HandleDestroySession));
		if (!Sessions->DestroySession(NAME_GameSession))
		{
			bHostAfterSessionChange = false;
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
			CreateOnlineSession();
		}
		return;
	}
	CreateOnlineSession();
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
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		if (Sessions->GetNamedSession(NAME_GameSession) != nullptr)
		{
			Sessions->DestroySession(NAME_GameSession);
		}
	}
	UGameplayStatics::OpenLevel(this, FName(GetGameMapPath()), true);
}

void USUGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogSignalUnknown, Warning, TEXT("Network failure: %s"), *ErrorString);
	// the engine travels back to the default map; show the menu there
	StartMode = ESUStartMode::Menu;
}
