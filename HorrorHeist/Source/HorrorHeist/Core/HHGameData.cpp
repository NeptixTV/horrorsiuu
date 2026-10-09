#include "Core/HHGameData.h"
#include "Core/HHGameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

const UHHGameData* UHHGameData::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (const UHHGameInstance* GI = World ? World->GetGameInstance<UHHGameInstance>() : nullptr)
	{
		return GI->GetGameData();
	}
	return nullptr;
}
