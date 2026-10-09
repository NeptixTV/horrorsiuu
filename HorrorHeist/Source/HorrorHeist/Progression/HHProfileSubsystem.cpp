#include "Progression/HHProfileSubsystem.h"
#include "Progression/HHProfileSaveGame.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHCosmeticDefinition.h"
#include "Data/HHEquipmentDefinition.h"
#include "Core/HHGameData.h"
#include "Core/HHGameInstance.h"
#include "Core/HHDeveloperSettings.h"
#include "HorrorHeist.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "HHProfile"

UHHProfileSubsystem* UHHProfileSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UHHProfileSubsystem>() : nullptr;
}

void UHHProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UHHItemRegistrySubsystem>();
	Super::Initialize(Collection);
	LoadOrCreate();
}

void UHHProfileSubsystem::Deinitialize()
{
	Save();
	Super::Deinitialize();
}

FString UHHProfileSubsystem::GetSlotName() const
{
	const UHHDeveloperSettings* Settings = UHHDeveloperSettings::Get();
	return Settings && !Settings->ProfileSlotName.IsEmpty() ? Settings->ProfileSlotName : FString(TEXT("HH_Profile"));
}

void UHHProfileSubsystem::LoadOrCreate()
{
	const FString Slot = GetSlotName();
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		Profile = Cast<UHHProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	}

	if (!Profile || !Profile->bInitialized)
	{
		InitializeNewProfile();
	}

	Sanitize();
	Save();
}

void UHHProfileSubsystem::InitializeNewProfile()
{
	Profile = Cast<UHHProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UHHProfileSaveGame::StaticClass()));
	Profile->CreatedAt = FDateTime::UtcNow();
	Profile->bInitialized = true;

	const UHHGameInstance* GameInstance = Cast<UHHGameInstance>(GetGameInstance());
	const UHHGameData* GameData = GameInstance ? GameInstance->GetGameData() : nullptr;
	Profile->Cash = GameData ? GameData->StartingCash : 1500;

	UHHItemRegistrySubsystem* Registry = GetGameInstance()->GetSubsystem<UHHItemRegistrySubsystem>();
	if (GameData && Registry)
	{
		for (const FName& Id : GameData->DefaultCosmetics)
		{
			if (const UHHCosmeticDefinition* Item = Registry->FindCosmetic(Id))
			{
				Profile->OwnedItems.AddUnique(Item->GetItemId());
				Profile->Cosmetics.Set(Item->Slot, Item->GetItemId());
			}
		}
		for (const FName& Id : GameData->DefaultEquipment)
		{
			if (const UHHEquipmentDefinition* Item = Registry->FindEquipment(Id))
			{
				Profile->OwnedItems.AddUnique(Item->GetItemId());
				Profile->Equipment.Set(Item->Slot, Item->GetItemId());
			}
		}
	}
}

void UHHProfileSubsystem::Sanitize()
{
	if (!Profile)
	{
		return;
	}

	UHHItemRegistrySubsystem* Registry = GetGameInstance()->GetSubsystem<UHHItemRegistrySubsystem>();
	if (!Registry || Registry->NumItems() == 0)
	{
		// Content not set up yet; keep whatever is saved untouched.
		return;
	}

	// Starter items are always owned.
	for (EHHCosmeticSlot Slot : TEnumRange<EHHCosmeticSlot>())
	{
		for (const UHHCosmeticDefinition* Item : Registry->GetCosmeticsForSlot(Slot))
		{
			if (Item->bOwnedByDefault)
			{
				Profile->OwnedItems.AddUnique(Item->GetItemId());
			}
		}
	}
	for (EHHEquipmentSlot Slot : TEnumRange<EHHEquipmentSlot>())
	{
		for (const UHHEquipmentDefinition* Item : Registry->GetEquipmentForSlot(Slot))
		{
			if (Item->bOwnedByDefault)
			{
				Profile->OwnedItems.AddUnique(Item->GetItemId());
			}
		}
	}

	// Cosmetic loadout: drop unknown / unowned / wrong-slot items, refill required slots.
	Profile->Cosmetics.Items.SetNum(static_cast<int32>(EHHCosmeticSlot::MAX));
	for (EHHCosmeticSlot Slot : TEnumRange<EHHCosmeticSlot>())
	{
		const FName Current = Profile->Cosmetics.Get(Slot);
		const UHHCosmeticDefinition* Item = Registry->FindCosmetic(Current);
		const bool bValid = Item && Item->Slot == Slot && IsOwned(Current);
		if (!bValid)
		{
			Profile->Cosmetics.Set(Slot, NAME_None);
			if (!UHHCosmeticDefinition::IsSlotOptional(Slot))
			{
				for (const UHHCosmeticDefinition* Candidate : Registry->GetCosmeticsForSlot(Slot))
				{
					if (IsOwned(Candidate->GetItemId()))
					{
						Profile->Cosmetics.Set(Slot, Candidate->GetItemId());
						break;
					}
				}
			}
		}
	}

	Profile->Equipment.Items.SetNum(static_cast<int32>(EHHEquipmentSlot::MAX));
	for (EHHEquipmentSlot Slot : TEnumRange<EHHEquipmentSlot>())
	{
		const FName Current = Profile->Equipment.Get(Slot);
		const UHHEquipmentDefinition* Item = Registry->FindEquipment(Current);
		if (!(Item && Item->Slot == Slot && IsOwned(Current)))
		{
			Profile->Equipment.Set(Slot, NAME_None);
		}
	}

	Profile->Version = UHHProfileSaveGame::CurrentVersion;
}

void UHHProfileSubsystem::Save()
{
	if (Profile)
	{
		if (!UGameplayStatics::SaveGameToSlot(Profile, GetSlotName(), 0))
		{
			UE_LOG(LogHorrorHeist, Warning, TEXT("Failed to write profile save '%s'."), *GetSlotName());
		}
	}
}

void UHHProfileSubsystem::NotifyChanged()
{
	Save();
	OnProfileChanged.Broadcast();
}

// ---------------------------------------------------------------------------------------
// Identity

FString UHHProfileSubsystem::GetPlayerName() const
{
	return Profile && !Profile->PlayerName.IsEmpty() ? Profile->PlayerName : FString(TEXT("Stranger"));
}

void UHHProfileSubsystem::SetPlayerName(const FString& NewName)
{
	FString Clean = NewName.TrimStartAndEnd();
	Clean = Clean.Left(20);
	if (Clean.IsEmpty() || !Profile)
	{
		return;
	}
	Profile->PlayerName = Clean;
	NotifyChanged();
}

bool UHHProfileSubsystem::NeedsName() const
{
	return !Profile || Profile->PlayerName.IsEmpty();
}

FString UHHProfileSubsystem::SuggestName()
{
	static const TCHAR* Names[] =
	{
		TEXT("Magpie"), TEXT("Jackdaw"), TEXT("Moth"), TEXT("Vesper"), TEXT("Rook"), TEXT("Wren"),
		TEXT("Hollis"), TEXT("Pike"), TEXT("Sable"), TEXT("Quill"), TEXT("Fennick"), TEXT("Marlowe"),
		TEXT("Ash"), TEXT("Juniper"), TEXT("Calloway"), TEXT("Nell"), TEXT("Dutch"), TEXT("Tamsin")
	};
	return Names[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Names)) - 1)];
}

// ---------------------------------------------------------------------------------------
// Economy & progression

int64 UHHProfileSubsystem::GetCash() const
{
	return Profile ? Profile->Cash : 0;
}

void UHHProfileSubsystem::AddCash(int64 Amount)
{
	if (!Profile || Amount == 0)
	{
		return;
	}
	Profile->Cash = FMath::Max<int64>(0, Profile->Cash + Amount);
	if (Amount > 0)
	{
		Profile->LifetimeEarnings += Amount;
	}
	NotifyChanged();
}

int64 UHHProfileSubsystem::XPRequiredForLevel(int32 Level)
{
	if (Level <= 1)
	{
		return 0;
	}
	// Gentle curve: L2 = 600, L5 = 4.8k, L10 = 16.2k, L25 = 70.5k, L50 = 205.8k.
	const double Raw = 600.0 * FMath::Pow(static_cast<double>(Level - 1), 1.5);
	return static_cast<int64>(FMath::RoundToDouble(Raw / 50.0) * 50.0);
}

int64 UHHProfileSubsystem::GetTotalXP() const
{
	return Profile ? Profile->TotalXP : 0;
}

int32 UHHProfileSubsystem::GetLevel() const
{
	const int64 XP = GetTotalXP();
	int32 Level = 1;
	while (Level < MaxLevel && XP >= XPRequiredForLevel(Level + 1))
	{
		++Level;
	}
	return Level;
}

int64 UHHProfileSubsystem::GetXPIntoLevel() const
{
	return GetTotalXP() - XPRequiredForLevel(GetLevel());
}

int64 UHHProfileSubsystem::GetXPForNextLevel() const
{
	const int32 Level = GetLevel();
	if (Level >= MaxLevel)
	{
		return 0;
	}
	return XPRequiredForLevel(Level + 1) - XPRequiredForLevel(Level);
}

float UHHProfileSubsystem::GetLevelProgress() const
{
	const int64 Needed = GetXPForNextLevel();
	return Needed > 0 ? FMath::Clamp(static_cast<float>(static_cast<double>(GetXPIntoLevel()) / static_cast<double>(Needed)), 0.f, 1.f) : 1.f;
}

void UHHProfileSubsystem::AddXP(int64 Amount)
{
	if (!Profile || Amount <= 0)
	{
		return;
	}
	const int32 OldLevel = GetLevel();
	Profile->TotalXP += Amount;
	const int32 NewLevel = GetLevel();
	NotifyChanged();
	if (NewLevel > OldLevel)
	{
		OnLevelUp.Broadcast(NewLevel);
	}
}

int32 UHHProfileSubsystem::GetJobsCompleted() const
{
	return Profile ? Profile->JobsCompleted : 0;
}

// ---------------------------------------------------------------------------------------
// Ownership

bool UHHProfileSubsystem::IsOwned(FName ItemId) const
{
	return Profile && !ItemId.IsNone() && Profile->OwnedItems.Contains(ItemId);
}

EHHPurchaseCheck UHHProfileSubsystem::CanPurchase(const UHHItemDefinition* Item) const
{
	if (!Item || Item->bHiddenInStore)
	{
		return EHHPurchaseCheck::NotForSale;
	}
	if (IsOwned(Item->GetItemId()))
	{
		return EHHPurchaseCheck::AlreadyOwned;
	}
	if (GetLevel() < Item->UnlockLevel)
	{
		return EHHPurchaseCheck::LevelTooLow;
	}
	if (GetCash() < Item->Price)
	{
		return EHHPurchaseCheck::NotEnoughCash;
	}
	return EHHPurchaseCheck::Ok;
}

FText UHHProfileSubsystem::DescribePurchaseCheck(EHHPurchaseCheck Check, const UHHItemDefinition* Item)
{
	switch (Check)
	{
	case EHHPurchaseCheck::AlreadyOwned:	return LOCTEXT("Owned", "Already owned");
	case EHHPurchaseCheck::NotEnoughCash:	return LOCTEXT("NoCash", "Not enough cash");
	case EHHPurchaseCheck::LevelTooLow:		return FText::Format(LOCTEXT("Level", "Requires level {0}"), FText::AsNumber(Item ? Item->UnlockLevel : 1));
	case EHHPurchaseCheck::NotForSale:		return LOCTEXT("NotForSale", "Not for sale");
	default:								return FText::GetEmpty();
	}
}

bool UHHProfileSubsystem::Purchase(const UHHItemDefinition* Item)
{
	if (CanPurchase(Item) != EHHPurchaseCheck::Ok)
	{
		return false;
	}
	Profile->Cash -= Item->Price;
	Profile->OwnedItems.AddUnique(Item->GetItemId());
	NotifyChanged();
	return true;
}

// ---------------------------------------------------------------------------------------
// Loadouts

const FHHCosmeticLoadout& UHHProfileSubsystem::GetCosmetics() const
{
	static const FHHCosmeticLoadout Empty;
	return Profile ? Profile->Cosmetics : Empty;
}

bool UHHProfileSubsystem::EquipCosmetic(const UHHCosmeticDefinition* Item)
{
	if (!Profile || !Item || !IsOwned(Item->GetItemId()))
	{
		return false;
	}
	if (Profile->Cosmetics.Get(Item->Slot) == Item->GetItemId())
	{
		return true;
	}
	Profile->Cosmetics.Set(Item->Slot, Item->GetItemId());
	NotifyChanged();
	return true;
}

void UHHProfileSubsystem::ClearCosmeticSlot(EHHCosmeticSlot Slot)
{
	if (!Profile || !UHHCosmeticDefinition::IsSlotOptional(Slot) || Profile->Cosmetics.Get(Slot).IsNone())
	{
		return;
	}
	Profile->Cosmetics.Set(Slot, NAME_None);
	NotifyChanged();
}

const FHHEquipmentLoadout& UHHProfileSubsystem::GetEquipment() const
{
	static const FHHEquipmentLoadout Empty;
	return Profile ? Profile->Equipment : Empty;
}

bool UHHProfileSubsystem::EquipItem(const UHHEquipmentDefinition* Item)
{
	if (!Profile || !Item || !IsOwned(Item->GetItemId()))
	{
		return false;
	}
	if (Profile->Equipment.Get(Item->Slot) == Item->GetItemId())
	{
		return true;
	}
	Profile->Equipment.Set(Item->Slot, Item->GetItemId());
	NotifyChanged();
	return true;
}

void UHHProfileSubsystem::ClearEquipmentSlot(EHHEquipmentSlot Slot)
{
	if (!Profile || Profile->Equipment.Get(Slot).IsNone())
	{
		return;
	}
	Profile->Equipment.Set(Slot, NAME_None);
	NotifyChanged();
}

FName UHHProfileSubsystem::GetLastMission() const
{
	return Profile ? Profile->LastMission : NAME_None;
}

void UHHProfileSubsystem::SetLastMission(FName MissionId)
{
	if (Profile && Profile->LastMission != MissionId)
	{
		Profile->LastMission = MissionId;
		Save();
	}
}

// ---------------------------------------------------------------------------------------
// Development helpers

void UHHProfileSubsystem::ResetProfile()
{
	const FString Name = Profile ? Profile->PlayerName : FString();
	InitializeNewProfile();
	Profile->PlayerName = Name;
	Sanitize();
	NotifyChanged();
}

void UHHProfileSubsystem::UnlockEverything()
{
	if (!Profile)
	{
		return;
	}
	if (UHHItemRegistrySubsystem* Registry = GetGameInstance()->GetSubsystem<UHHItemRegistrySubsystem>())
	{
		for (const UHHItemDefinition* Item : Registry->GetStoreItems())
		{
			Profile->OwnedItems.AddUnique(Item->GetItemId());
		}
	}
	NotifyChanged();
}

#undef LOCTEXT_NAMESPACE
