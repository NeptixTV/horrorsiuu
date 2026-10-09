#include "Data/HHEquipmentDefinition.h"

float UHHEquipmentDefinition::GetStat(FName StatId, float DefaultValue) const
{
	for (const FHHItemStat& Stat : Stats)
	{
		if (Stat.StatId == StatId)
		{
			return Stat.Value;
		}
	}
	return DefaultValue;
}
