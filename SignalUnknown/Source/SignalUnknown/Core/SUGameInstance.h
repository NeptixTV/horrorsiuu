// SIGNAL UNKNOWN – game instance: session flow (menu, solo, host, join) across level loads.
//
// Online play: when the Steam client is running, hosting creates a Steam lobby. Friends find the
// game in the list on the join screen or get an invitation through the Steam overlay; the
// connection goes through Steam's relay, so no port has to be opened. Without Steam the same
// code uses the engine's LAN sessions (games in the same network appear in the list) and direct
// IP connections still work.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "SUGameInstance.generated.h"

class UNetDriver;
class FOnlineSessionSearch;

/** One game found by the session search. */
struct FSUFoundGame
{
	FString HostName;
	int32 Players = 0;
	int32 MaxPlayers = 0;
	int32 Ping = 0;
};

UENUM(BlueprintType)
enum class ESUStartMode : uint8
{
	/** First start: show the main menu over the live world. */
	Menu,
	/** Solo game, started from the menu. */
	Solo,
	/** Listen server for co-op. */
	Host,
	/** Connected to a host. */
	Client
};

UCLASS()
class SIGNALUNKNOWN_API USUGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	ESUStartMode GetStartMode() const { return StartMode; }
	void SetStartMode(ESUStartMode Mode) { StartMode = Mode; }

	/** Load the save slot right after the world is ready. */
	bool ShouldLoadSaveOnStart() const { return bLoadSaveOnStart; }
	void ClearLoadSaveOnStart() { bLoadSaveOnStart = false; }

	/** Starts playing in the already loaded world (solo). */
	void StartSolo(bool bContinueSave);

	/** Reloads the map as listen server. */
	void HostCoop(bool bContinueSave);

	/** Connects to a host (IPv4 or host name, optional :port). */
	void JoinCoop(const FString& Address);

	/** Back to the main menu (disconnects). */
	void ReturnToMenu();

	const FString& GetSaveSlot() const { return SaveSlot; }
	FString GetLastJoinAddress() const { return LastJoinAddress; }

	// ---------------------------------------------------------------- online sessions
	/** "Steam", "LAN" or empty when no online service is available. */
	FString GetOnlineServiceName() const;
	bool IsSteamActive() const;
	/** Starts searching for hosted games (asynchronous). */
	void FindGames();
	bool IsSearchingGames() const { return bSearching; }
	const TArray<FSUFoundGame>& GetFoundGames() const { return FoundGames; }
	/** Status line for the join screen. */
	const FString& GetOnlineStatus() const { return OnlineStatus; }
	void JoinFoundGame(int32 Index);
	/** Host: opens the Steam overlay to invite friends. */
	void ShowInviteDialog();
	bool HasOnlineSession() const;

	/** Map that hosts the procedural world. */
	static const TCHAR* GetGameMapPath() { return TEXT("/Engine/Maps/Entry"); }

private:
	UPROPERTY()
	ESUStartMode StartMode = ESUStartMode::Menu;

	UPROPERTY()
	bool bLoadSaveOnStart = false;

	UPROPERTY()
	FString SaveSlot = TEXT("SignalUnknown_Slot1");

	UPROPERTY()
	FString LastJoinAddress = TEXT("127.0.0.1");

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	TArray<FSUFoundGame> FoundGames;
	FString OnlineStatus;
	bool bSearching = false;
	bool bHostAfterSessionChange = false;
	FDelegateHandle CreateHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle InviteHandle;

	IOnlineSessionPtr GetSessions() const;
	void CreateOnlineSession();
	void OpenListenLevel();
	void JoinSessionResult(const FOnlineSessionSearchResult& Result);
	void HandleCreateSession(FName SessionName, bool bWasSuccessful);
	void HandleDestroySession(FName SessionName, bool bWasSuccessful);
	void HandleFindSessions(bool bWasSuccessful);
	void HandleJoinSession(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
};
