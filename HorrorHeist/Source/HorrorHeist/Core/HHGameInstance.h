#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/HHTypes.h"
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

	/** Job the crew is travelling to / playing (also passed as ?Mission= on the travel URL). */
	void SetActiveMission(FName MissionId) { ActiveMission = MissionId; }
	FName GetActiveMission() const { return ActiveMission; }

	/** Getaway destination picked on the job board (AHHEscapeZone id), kept across maps. */
	void SetSafehouse(FName ZoneId) { Safehouse = ZoneId; }
	FName GetSafehouse() const { return Safehouse; }

	/**
	 * In-game clock carried across map loads (hub -> job -> hub). Negative = not set yet,
	 * the time-of-day actor then starts at its configured hour.
	 */
	void SetCarriedTimeOfDay(float Hours) { CarriedTimeOfDay = Hours; }
	float GetCarriedTimeOfDay() const { return CarriedTimeOfDay; }

	/** Host: what the town saw of the last job (crime scene in the hub), kept across the map load. */
	void SetAftermath(const FHHAftermath& InAftermath) { Aftermath = InAftermath; }
	const FHHAftermath& GetAftermath() const { return Aftermath; }

private:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	UPROPERTY(Transient)
	TObjectPtr<UHHGameData> GameData;

	FText PendingTitle;
	FText PendingBody;
	bool bHasPendingMessage = false;
	bool bTitleShown = false;
	FName ActiveMission;
	FName Safehouse;
	float CarriedTimeOfDay = -1.f;
	FHHAftermath Aftermath;
};
