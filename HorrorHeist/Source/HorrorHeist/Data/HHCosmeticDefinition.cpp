#include "Data/HHCosmeticDefinition.h"

bool UHHCosmeticDefinition::IsSlotOptional(EHHCosmeticSlot InSlot)
{
	switch (InSlot)
	{
	case EHHCosmeticSlot::Body:
	case EHHCosmeticSlot::Top:
	case EHHCosmeticSlot::Pants:
	case EHHCosmeticSlot::Shoes:
		return false;
	default:
		return true;
	}
}
