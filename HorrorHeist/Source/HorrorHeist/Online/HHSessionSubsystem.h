#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "HHSessionSubsystem.generated.h"

class FOnlineSessionSearch;

/** A found crew, flattened for the UI. */
struct FHHSessionInfo
{
	FString HostName;
	int32 OpenSlots = 0;
	int32 MaxSlots = 0;
	int32 PingMs = 0;
	int32 ResultIndex = INDEX_NONE;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHHOnSessionsFound, bool /*bSuccess*/, const TArray<FHHSessionInfo>& /*Results*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHHOnSessionStatus, bool /*bSuccess*/, const FText& /*Message*/);

/**
 * Host / browse / join crews through the Online Subsystem (Null = LAN out of the box,
 * Steam or EOS later by config) plus direct IP connect for internet play without a backend.
 */
UCLASS()
class HORRORHEIST_API UHHSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UHHSessionSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	/** Creates a session and reopens the hideout as a listen server. */
	void HostSession(bool bLAN, int32 MaxPlayers);
	void FindSessions(bool bLAN);
	void JoinSession(int32 ResultIndex);
	/** "192.168.0.12" or "my.host:7777" */
	void JoinByAddress(const FString& Address);
	/** Leave / close the crew and return to a private hideout. */
	void LeaveSession();
	/** Destroys a leftover session when we are back in a standalone world. */
	void CleanupStaleSession();

	bool IsBusy() const { return bBusy; }
	bool HasSession() const;

	FHHOnSessionsFound OnSessionsFound;
	FHHOnSessionStatus OnSessionStatus;

private:
	IOnlineSessionPtr GetSessionInterface() const;
	/** The Null subsystem only supports LAN discovery. */
	bool IsNullSubsystem() const;
	FString GetLobbyMapPath() const;
	void OpenLobby(bool bListen);
	void DestroyThen(TFunction<void()> Continuation);

	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindComplete(bool bWasSuccessful);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroyComplete(FName SessionName, bool bWasSuccessful);

	TSharedPtr<FOnlineSessionSearch> Search;
	TFunction<void()> PendingAfterDestroy;

	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle DestroyHandle;

	bool bBusy = false;
};
