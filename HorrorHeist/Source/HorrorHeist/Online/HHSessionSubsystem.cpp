#include "Online/HHSessionSubsystem.h"
#include "Core/HHDeveloperSettings.h"
#include "Progression/HHProfileSubsystem.h"
#include "HorrorHeist.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "HHSession"

namespace
{
	const FName HostNameKey(TEXT("HH_HOST"));
}

UHHSessionSubsystem* UHHSessionSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UHHSessionSubsystem>() : nullptr;
}

void UHHSessionSubsystem::Deinitialize()
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	Super::Deinitialize();
}

IOnlineSessionPtr UHHSessionSubsystem::GetSessionInterface() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	IOnlineSubsystem* OnlineSubsystem = World ? Online::GetSubsystem(World) : IOnlineSubsystem::Get();
	return OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
}

bool UHHSessionSubsystem::IsNullSubsystem() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const IOnlineSubsystem* OnlineSubsystem = World ? Online::GetSubsystem(World) : IOnlineSubsystem::Get();
	return !OnlineSubsystem || OnlineSubsystem->GetSubsystemName() == FName(TEXT("NULL"));
}

bool UHHSessionSubsystem::HasSession() const
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

FString UHHSessionSubsystem::GetLobbyMapPath() const
{
	const UHHDeveloperSettings* Settings = UHHDeveloperSettings::Get();
	return Settings && !Settings->LobbyMap.IsNull() ? Settings->LobbyMap.GetLongPackageName() : FString(TEXT("/Game/HorrorHeist/Maps/L_Hideout"));
}

void UHHSessionSubsystem::OpenLobby(bool bListen)
{
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		UGameplayStatics::OpenLevel(World, FName(*GetLobbyMapPath()), true, bListen ? TEXT("listen") : TEXT(""));
	}
}

void UHHSessionSubsystem::DestroyThen(TFunction<void()> Continuation)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		Continuation();
		return;
	}

	PendingAfterDestroy = MoveTemp(Continuation);
	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UHHSessionSubsystem::HandleDestroyComplete));
	if (!Sessions->DestroySession(NAME_GameSession))
	{
		HandleDestroyComplete(NAME_GameSession, false);
	}
}

void UHHSessionSubsystem::HandleDestroyComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	TFunction<void()> Continuation = MoveTemp(PendingAfterDestroy);
	PendingAfterDestroy = nullptr;
	if (Continuation)
	{
		Continuation();
	}
}

// ---------------------------------------------------------------------------------------

void UHHSessionSubsystem::HostSession(bool bLAN, int32 MaxPlayers)
{
	if (bBusy)
	{
		return;
	}
	bBusy = true;

	DestroyThen([this, bLAN, MaxPlayers]()
	{
		IOnlineSessionPtr Sessions = GetSessionInterface();
		if (!Sessions.IsValid())
		{
			bBusy = false;
			OnSessionStatus.Broadcast(false, LOCTEXT("NoOnline", "Online services are not available."));
			return;
		}

		FOnlineSessionSettings Settings;
		Settings.bIsLANMatch = bLAN || IsNullSubsystem();
		Settings.NumPublicConnections = FMath::Clamp(MaxPlayers, 1, 8);
		Settings.NumPrivateConnections = 0;
		Settings.bShouldAdvertise = true;
		Settings.bAllowJoinInProgress = true;
		Settings.bAllowJoinViaPresence = true;
		Settings.bUsesPresence = true;
		Settings.bUseLobbiesIfAvailable = true;
		Settings.bAllowInvites = true;

		const UHHProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UHHProfileSubsystem>();
		const FString HostName = Profile ? Profile->GetPlayerName() : FString(TEXT("Host"));
		Settings.Set(HostNameKey, HostName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateUObject(this, &UHHSessionSubsystem::HandleCreateComplete));

		if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
		{
			HandleCreateComplete(NAME_GameSession, false);
		}
	});
}

void UHHSessionSubsystem::HandleCreateComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}
	bBusy = false;

	if (!bWasSuccessful)
	{
		OnSessionStatus.Broadcast(false, LOCTEXT("HostFailed", "Could not open the hideout to a crew."));
		return;
	}

	OnSessionStatus.Broadcast(true, LOCTEXT("Hosting", "Opening the hideout..."));
	OpenLobby(true);
}

// ---------------------------------------------------------------------------------------

void UHHSessionSubsystem::FindSessions(bool bLAN)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || bBusy)
	{
		OnSessionsFound.Broadcast(false, TArray<FHHSessionInfo>());
		return;
	}
	bBusy = true;

	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = bLAN || IsNullSubsystem();
	Search->MaxSearchResults = 64;
	Search->PingBucketSize = 50;
#if defined(SEARCH_LOBBIES)
	Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
#endif

	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UHHSessionSubsystem::HandleFindComplete));

	if (!Sessions->FindSessions(0, Search.ToSharedRef()))
	{
		HandleFindComplete(false);
	}
}

void UHHSessionSubsystem::HandleFindComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}
	bBusy = false;

	TArray<FHHSessionInfo> Results;
	if (bWasSuccessful && Search.IsValid())
	{
		for (int32 Index = 0; Index < Search->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = Search->SearchResults[Index];
			if (!Result.IsValid())
			{
				continue;
			}
			FHHSessionInfo Info;
			Info.ResultIndex = Index;
			Info.PingMs = Result.PingInMs;
			Info.MaxSlots = Result.Session.SessionSettings.NumPublicConnections;
			Info.OpenSlots = Result.Session.NumOpenPublicConnections;
			if (!Result.Session.SessionSettings.Get(HostNameKey, Info.HostName) || Info.HostName.IsEmpty())
			{
				Info.HostName = Result.Session.OwningUserName;
			}
			Results.Add(Info);
		}
	}
	OnSessionsFound.Broadcast(bWasSuccessful, Results);
}

// ---------------------------------------------------------------------------------------

void UHHSessionSubsystem::JoinSession(int32 ResultIndex)
{
	if (bBusy || !Search.IsValid() || !Search->SearchResults.IsValidIndex(ResultIndex))
	{
		OnSessionStatus.Broadcast(false, LOCTEXT("JoinInvalid", "That crew is no longer available."));
		return;
	}
	bBusy = true;

	const FOnlineSessionSearchResult Result = Search->SearchResults[ResultIndex];
	DestroyThen([this, Result]()
	{
		IOnlineSessionPtr Sessions = GetSessionInterface();
		if (!Sessions.IsValid())
		{
			bBusy = false;
			OnSessionStatus.Broadcast(false, LOCTEXT("NoOnline", "Online services are not available."));
			return;
		}
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateUObject(this, &UHHSessionSubsystem::HandleJoinComplete));
		if (!Sessions->JoinSession(0, NAME_GameSession, Result))
		{
			HandleJoinComplete(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
		}
	});
}

void UHHSessionSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}
	bBusy = false;

	FString ConnectString;
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions.IsValid() || !Sessions->GetResolvedConnectString(SessionName, ConnectString))
	{
		const FText Message = Result == EOnJoinSessionCompleteResult::SessionIsFull
			? LOCTEXT("JoinFull", "That crew is full.")
			: LOCTEXT("JoinFailed", "Could not reach that crew.");
		OnSessionStatus.Broadcast(false, Message);
		return;
	}

	OnSessionStatus.Broadcast(true, LOCTEXT("Joining", "Heading over..."));
	if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
	{
		PC->ClientTravel(ConnectString, TRAVEL_Absolute);
	}
}

void UHHSessionSubsystem::JoinByAddress(const FString& Address)
{
	const FString Trimmed = Address.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || bBusy)
	{
		return;
	}
	DestroyThen([this, Trimmed]()
	{
		OnSessionStatus.Broadcast(true, LOCTEXT("Connecting", "Connecting..."));
		if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
		{
			PC->ClientTravel(Trimmed, TRAVEL_Absolute);
		}
	});
}

void UHHSessionSubsystem::LeaveSession()
{
	DestroyThen([this]()
	{
		OpenLobby(false);
	});
}

void UHHSessionSubsystem::CleanupStaleSession()
{
	if (HasSession() && !bBusy)
	{
		DestroyThen([]() {});
	}
}

#undef LOCTEXT_NAMESPACE
