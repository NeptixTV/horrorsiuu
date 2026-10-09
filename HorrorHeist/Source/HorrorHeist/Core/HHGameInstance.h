#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "HHGameInstance.generated.h"

class UHHGameData;
class UNetDriver;

/**
 * Lives for the whole application. Keeps the game data asset loaded and turns network /
 * travel failures into a message that the hideout shows once the player is back.
 */
UCLASS()
class HORRORHEIST_API UHHGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	const UHHGameData* GetGameData() const { return GameData; }

	/** Queue a message for the next lobby that loads (e.g. "Host closed the session"). */
	void SetPendingMessage(const FText& Title, const FText& Body);
	bool ConsumePendingMessage(FText& OutTitle, FText& OutBody);

	/** The title card is only shown once per application start. */
	bool HasShownTitle() const { return bTitleShown; }
	void MarkTitleShown() { bTitleShown = true; }

private:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	UPROPERTY(Transient)
	TObjectPtr<UHHGameData> GameData;

	FText PendingTitle;
	FText PendingBody;
	bool bHasPendingMessage = false;
	bool bTitleShown = false;
};
