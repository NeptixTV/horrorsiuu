// SIGNAL UNKNOWN – game instance: session flow (menu, solo, host, join) across level loads.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "SUGameInstance.generated.h"

class UNetDriver;

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
};
