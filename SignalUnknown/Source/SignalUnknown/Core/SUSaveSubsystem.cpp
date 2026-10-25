#include "Core/SUSaveSubsystem.h"
#include "SignalUnknown.h"
#include "Core/SUSaveGame.h"
#include "Core/SUGameInstance.h"
#include "Core/SUGameState.h"
#include "Core/SUPlayerState.h"
#include "Player/SUCharacter.h"
#include "Mission/SUMissionManager.h"
#include "Vehicle/SUVehicle.h"
#include "World/SUWorldBuilder.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

FString USUSaveSubsystem::GetSlotName() const
{
	const USUGameInstance* GI = Cast<USUGameInstance>(GetGameInstance());
	return GI ? GI->GetSaveSlot() : FString(TEXT("SignalUnknown_Slot1"));
}

bool USUSaveSubsystem::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(GetSlotName(), 0);
}

USUSaveGame* USUSaveSubsystem::ReadSlot(const FString& Slot) const
{
	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		return nullptr;
	}
	return Cast<USUSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
}

FString USUSaveSubsystem::DescribeSave() const
{
	const USUSaveGame* Save = ReadSlot(GetSlotName());
	if (!Save)
	{
		return FString();
	}
	return FString::Printf(TEXT("Tag %d, %s  ·  %s  ·  %d Einsätze"), Save->Day, *SUFormatTime(Save->Hours),
		*SUFormatMoney(Save->MoneyCents), Save->Mission.CompletedMissions.Num());
}

bool USUSaveSubsystem::SaveWorld(UWorld* World, FString& OutMessage)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		OutMessage = TEXT("Nur der Host kann speichern.");
		return false;
	}
	ASUGameState* GS = World->GetGameState<ASUGameState>();
	if (!GS)
	{
		OutMessage = TEXT("Speichern fehlgeschlagen: kein Spielzustand.");
		return false;
	}

	USUSaveGame* Save = Cast<USUSaveGame>(UGameplayStatics::CreateSaveGameObject(USUSaveGame::StaticClass()));
	if (!Save)
	{
		OutMessage = TEXT("Speichern fehlgeschlagen.");
		return false;
	}
	Save->SaveVersion = USUSaveGame::CurrentVersion;
	Save->GameVersion = TEXT("0.1.0");
	Save->SavedAt = FDateTime::Now();
	Save->MoneyCents = GS->GetMoneyCents();
	Save->Transactions = GS->GetTransactions();
	Save->Day = GS->GetDay();
	Save->Hours = GS->GetTimeOfDay();
	Save->StoryStage = GS->GetStoryStage();
	Save->WorldFlags = GS->GetWorldFlags();
	Save->UnpaidFuelIncidents = GS->GetUnpaidFuelIncidents();

	if (GS->MissionManager)
	{
		Save->Mission = GS->MissionManager->GetSaveData();
	}
	if (GS->MainVehicle)
	{
		Save->Vehicle = GS->MainVehicle->GetSaveData();
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		ASUPlayerState* SUPS = Cast<ASUPlayerState>(PS);
		if (!SUPS)
		{
			continue;
		}
		FSUPlayerSave P;
		P.PlayerName = SUPS->GetPlayerName();
		P.Photos = SUPS->GetPhotos();
		if (const ASUCharacter* Char = SUPS->GetPawn<ASUCharacter>())
		{
			P.Items = Char->GetInventory()->GetSlots();
			P.Health = Char->GetHealth();
		}
		Save->Players.Add(P);
	}

	// keep the previous save as backup
	const FString Slot = GetSlotName();
	TArray<uint8> Previous;
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0) && UGameplayStatics::LoadDataFromSlot(Previous, Slot, 0))
	{
		UGameplayStatics::SaveDataToSlot(Previous, Slot + TEXT("_Backup"), 0);
	}

	if (!UGameplayStatics::SaveGameToSlot(Save, Slot, 0))
	{
		OutMessage = TEXT("Speichern fehlgeschlagen (Datei konnte nicht geschrieben werden).");
		return false;
	}
	OutMessage = FString::Printf(TEXT("Gespeichert – Tag %d, %s"), Save->Day, *SUFormatTime(Save->Hours));
	UE_LOG(LogSignalUnknown, Log, TEXT("%s"), *OutMessage);
	return true;
}

bool USUSaveSubsystem::MigrateSave(USUSaveGame* Save, FString& OutMessage) const
{
	if (!Save)
	{
		return false;
	}
	if (Save->SaveVersion > USUSaveGame::CurrentVersion)
	{
		OutMessage = TEXT("Der Spielstand stammt aus einer neueren Spielversion und wird nicht geladen.");
		return false;
	}
	// Version 1 is the first format. Future migrations go here, e.g.:
	// if (Save->SaveVersion < 2) { ...convert...; Save->SaveVersion = 2; }
	if (Save->SaveVersion < 1)
	{
		OutMessage = TEXT("Unbekanntes Spielstandformat.");
		return false;
	}
	return true;
}

bool USUSaveSubsystem::LoadWorld(UWorld* World, FString& OutMessage)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		OutMessage = TEXT("Nur der Host kann laden.");
		return false;
	}
	const FString Slot = GetSlotName();
	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		OutMessage = TEXT("Kein Spielstand vorhanden.");
		return false;
	}
	USUSaveGame* Save = ReadSlot(Slot);
	if (!Save)
	{
		UE_LOG(LogSignalUnknown, Warning, TEXT("Save slot unreadable, trying backup."));
		Save = ReadSlot(Slot + TEXT("_Backup"));
		if (!Save)
		{
			OutMessage = TEXT("Spielstand beschädigt und keine Sicherung vorhanden.");
			return false;
		}
	}
	if (!MigrateSave(Save, OutMessage))
	{
		return false;
	}
	ApplySave(World, Save);
	OutMessage = FString::Printf(TEXT("Spielstand geladen – Tag %d, %s"), Save->Day, *SUFormatTime(Save->Hours));
	return true;
}

void USUSaveSubsystem::ApplySave(UWorld* World, const USUSaveGame* Save) const
{
	ASUGameState* GS = World->GetGameState<ASUGameState>();
	if (!GS)
	{
		return;
	}
	GS->SetMoneyDirect(Save->MoneyCents);
	GS->SetTimeOfDay(Save->Day, Save->Hours);
	GS->SetStoryStage(Save->StoryStage);
	GS->SetWorldFlags(Save->WorldFlags);
	GS->SetUnpaidFuelIncidents(Save->UnpaidFuelIncidents);

	if (GS->MissionManager)
	{
		GS->MissionManager->ApplySaveData(Save->Mission);
	}
	if (GS->MainVehicle && Save->Vehicle.bValid)
	{
		GS->MainVehicle->ApplySaveData(Save->Vehicle);
	}

	const ASUWorldBuilder* Builder = GS->GetWorldBuilder();
	int32 SpawnIndex = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		ASUPlayerState* SUPS = Cast<ASUPlayerState>(PS);
		if (!SUPS)
		{
			continue;
		}
		// match by name; the host gets the first entry if names changed (e.g. new PC)
		const FSUPlayerSave* Entry = Save->Players.FindByPredicate([SUPS](const FSUPlayerSave& P) { return P.PlayerName == SUPS->GetPlayerName(); });
		if (!Entry && SUPS->GetPawn() && SUPS->GetPawn()->IsLocallyControlled() && Save->Players.Num() > 0)
		{
			Entry = &Save->Players[0];
		}
		ASUCharacter* Char = SUPS->GetPawn<ASUCharacter>();
		if (Entry)
		{
			SUPS->SetPhotos(Entry->Photos);
			if (Char)
			{
				Char->GetInventory()->SetSlots(Entry->Items);
				Char->SetHealthDirect(Entry->Health);
			}
		}
		if (Char && Builder)
		{
			Char->TeleportToBase(Builder->GetPlayerSpawnTransform(SpawnIndex++));
		}
	}
}
