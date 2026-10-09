#include "Data/HHItemDefinition.h"

FPrimaryAssetId UHHItemDefinition::GetPrimaryAssetId() const
{
	const FPrimaryAssetType Type = GetItemAssetType();
	if (Type.IsValid())
	{
		return FPrimaryAssetId(Type, GetFName());
	}
	return Super::GetPrimaryAssetId();
}
