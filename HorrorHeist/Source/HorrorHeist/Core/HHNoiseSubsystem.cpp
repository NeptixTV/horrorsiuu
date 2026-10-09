#include "Core/HHNoiseSubsystem.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDebugNoise(
		TEXT("hh.DebugNoise"),
		0,
		TEXT("Draw reported noise events (radius scales with loudness)."),
		ECVF_Cheat);
}

void UHHNoiseSubsystem::ReportNoise(const FVector& Location, float Loudness, AActor* Instigator, FName Tag)
{
	const float Clamped = FMath::Clamp(Loudness, 0.f, 1.f);

#if ENABLE_DRAW_DEBUG
	if (CVarDebugNoise.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(GetWorld(), Location, 100.f + Clamped * 1400.f, 12, FColor::Orange, false, 0.6f);
	}
#endif

	OnNoise.Broadcast(Location, Clamped, Instigator, Tag);
}
