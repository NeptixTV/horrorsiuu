#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHCosmeticDefinition.h"
#include "Data/HHEquipmentDefinition.h"
#include "Data/HHMissionDefinition.h"
#include "HorrorHeist.h"
#include "Algo/Sort.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"

UHHItemRegistrySubsystem* UHHItemRegistrySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UHHItemRegistrySubsystem>() : nullptr;
}

void UHHItemRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Reload();
}

template <typename T>
void UHHItemRegistrySubsystem::GatherAssets(const FName PrimaryAssetType, TArray<TObjectPtr<T>>& Out)
{
	TSet<FSoftObjectPath> Paths;

	if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<FPrimaryAssetId> Ids;
		AssetManager->GetPrimaryAssetIdList(FPrimaryAssetType(PrimaryAssetType), Ids);
		for (const FPrimaryAssetId& Id : Ids)
		{
			const FSoftObjectPath Path = AssetManager->GetPrimaryAssetPath(Id);
			if (Path.IsValid())
			{
				Paths.Add(Path);
			}
		}
	}

	// The asset registry also sees assets created in this editor session before the
	// asset manager rescans, and anything placed outside the configured folders.
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Found;
	AssetRegistry.GetAssetsByClass(T::StaticClass()->GetClassPathName(), Found, true);
	for (const FAssetData& Data : Found)
	{
		Paths.Add(Data.GetSoftObjectPath());
	}

	for (const FSoftObjectPath& Path : Paths)
	{
		if (T* Asset = Cast<T>(Path.TryLoad()))
		{
			Out.AddUnique(Asset);
		}
	}
}

void UHHItemRegistrySubsystem::Reload()
{
	Cosmetics.Reset();
	Equipment.Reset();
	Missions.Reset();
	ItemsById.Reset();

	GatherAssets<UHHCosmeticDefinition>(TEXT("HHCosmetic"), Cosmetics);
	GatherAssets<UHHEquipmentDefinition>(TEXT("HHEquipment"), Equipment);
	GatherAssets<UHHMissionDefinition>(TEXT("HHMission"), Missions);

	auto ByOrder = [](const auto& A, const auto& B)
	{
		if (A->SortOrder != B->SortOrder)
		{
			return A->SortOrder < B->SortOrder;
		}
		return A->GetName() < B->GetName();
	};
	Algo::Sort(Cosmetics, ByOrder);
	Algo::Sort(Equipment, ByOrder);
	Algo::Sort(Missions, ByOrder);

	auto Register = [this](UHHItemDefinition* Item)
	{
		const FName Id = Item->GetItemId();
		if (const TObjectPtr<UHHItemDefinition>* Existing = ItemsById.Find(Id))
		{
			UE_LOG(LogHorrorHeist, Error, TEXT("Duplicate item id '%s' (%s and %s). The second one is ignored."),
				*Id.ToString(), *GetNameSafe(*Existing), *GetNameSafe(Item));
			return;
		}
		ItemsById.Add(Id, Item);
	};
	for (UHHCosmeticDefinition* Item : Cosmetics)
	{
		Register(Item);
	}
	for (UHHEquipmentDefinition* Item : Equipment)
	{
		Register(Item);
	}

	UE_LOG(LogHorrorHeist, Log, TEXT("Item registry: %d cosmetics, %d equipment, %d missions."),
		Cosmetics.Num(), Equipment.Num(), Missions.Num());
}

const UHHItemDefinition* UHHItemRegistrySubsystem::FindItem(FName ItemId) const
{
	if (ItemId.IsNone())
	{
		return nullptr;
	}
	const TObjectPtr<UHHItemDefinition>* Found = ItemsById.Find(ItemId);
	return Found ? Found->Get() : nullptr;
}

const UHHCosmeticDefinition* UHHItemRegistrySubsystem::FindCosmetic(FName ItemId) const
{
	return Cast<UHHCosmeticDefinition>(FindItem(ItemId));
}

const UHHEquipmentDefinition* UHHItemRegistrySubsystem::FindEquipment(FName ItemId) const
{
	return Cast<UHHEquipmentDefinition>(FindItem(ItemId));
}

const UHHMissionDefinition* UHHItemRegistrySubsystem::FindMission(FName MissionId) const
{
	if (MissionId.IsNone())
	{
		return nullptr;
	}
	for (const UHHMissionDefinition* Mission : Missions)
	{
		if (Mission && Mission->GetMissionId() == MissionId)
		{
			return Mission;
		}
	}
	return nullptr;
}

TArray<const UHHCosmeticDefinition*> UHHItemRegistrySubsystem::GetCosmeticsForSlot(EHHCosmeticSlot Slot) const
{
	TArray<const UHHCosmeticDefinition*> Result;
	for (const UHHCosmeticDefinition* Item : Cosmetics)
	{
		if (Item && Item->Slot == Slot)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<const UHHEquipmentDefinition*> UHHItemRegistrySubsystem::GetEquipmentForSlot(EHHEquipmentSlot Slot) const
{
	TArray<const UHHEquipmentDefinition*> Result;
	for (const UHHEquipmentDefinition* Item : Equipment)
	{
		if (Item && Item->Slot == Slot)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<const UHHMissionDefinition*> UHHItemRegistrySubsystem::GetMissions() const
{
	TArray<const UHHMissionDefinition*> Result;
	for (const UHHMissionDefinition* Mission : Missions)
	{
		if (Mission)
		{
			Result.Add(Mission);
		}
	}
	return Result;
}

TArray<const UHHItemDefinition*> UHHItemRegistrySubsystem::GetStoreItems() const
{
	TArray<const UHHItemDefinition*> Result;
	for (const UHHEquipmentDefinition* Item : Equipment)
	{
		if (Item && !Item->bHiddenInStore && !Item->bOwnedByDefault)
		{
			Result.Add(Item);
		}
	}
	for (const UHHCosmeticDefinition* Item : Cosmetics)
	{
		if (Item && !Item->bHiddenInStore && !Item->bOwnedByDefault)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

UTexture2D* UHHItemRegistrySubsystem::PinTexture(const TSoftObjectPtr<UTexture2D>& Texture) const
{
	if (Texture.IsNull())
	{
		return nullptr;
	}
	UTexture2D* Loaded = Texture.LoadSynchronous();
	if (Loaded)
	{
		const_cast<UHHItemRegistrySubsystem*>(this)->PinnedAssets.AddUnique(Loaded);
	}
	return Loaded;
}

UTexture2D* UHHItemRegistrySubsystem::GetIcon(const UHHItemDefinition* Item) const
{
	return Item ? PinTexture(Item->Icon) : nullptr;
}

UTexture2D* UHHItemRegistrySubsystem::GetMissionPhoto(const UHHMissionDefinition* Mission) const
{
	return Mission ? PinTexture(Mission->Photo) : nullptr;
}
