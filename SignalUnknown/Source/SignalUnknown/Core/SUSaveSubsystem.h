// SIGNAL UNKNOWN – saving and loading (server/host only), with version migration and backup.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SUSaveSubsystem.generated.h"

class USUSaveGame;

UCLASS()
class SIGNALUNKNOWN_API USUSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	bool HasSave() const;
	FString GetSlotName() const;

	/** Collects the world state and writes it. Keeps the previous file as "<slot>_Backup". */
	bool SaveWorld(UWorld* World, FString& OutMessage);

	/** Reads the slot and applies it to the running world. */
	bool LoadWorld(UWorld* World, FString& OutMessage);

	/** Short description of the existing save for the main menu ("Tag 2, 21:14 – 420,00 $"). */
	FString DescribeSave() const;

private:
	/** Upgrades older save versions in place. Returns false if the save can't be used. */
	bool MigrateSave(USUSaveGame* Save, FString& OutMessage) const;
	void ApplySave(UWorld* World, const USUSaveGame* Save) const;
	USUSaveGame* ReadSlot(const FString& Slot) const;
};
