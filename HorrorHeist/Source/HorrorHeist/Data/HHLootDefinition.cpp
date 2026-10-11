#include "Data/HHLootDefinition.h"

int32 UHHLootDefinition::RollValue(FRandomStream& Stream) const
{
	const int32 Low = FMath::Max(0, FMath::Min(ValueMin, ValueMax));
	const int32 High = FMath::Max(ValueMin, ValueMax);
	// Round to $5 so the numbers look like prices, not dice rolls.
	return FMath::RoundToInt(Stream.RandRange(Low, High) / 5.f) * 5;
}
