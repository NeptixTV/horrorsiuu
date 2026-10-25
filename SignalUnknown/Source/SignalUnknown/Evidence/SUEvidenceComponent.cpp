#include "Evidence/SUEvidenceComponent.h"
#include "Evidence/SUEvidenceSubsystem.h"
#include "Engine/World.h"

USUEvidenceComponent::USUEvidenceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USUEvidenceComponent::OnRegister()
{
	Super::OnRegister();
	if (UWorld* World = GetWorld())
	{
		if (USUEvidenceSubsystem* Sub = World->GetSubsystem<USUEvidenceSubsystem>())
		{
			Sub->Register(this);
		}
	}
}

void USUEvidenceComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
	{
		if (USUEvidenceSubsystem* Sub = World->GetSubsystem<USUEvidenceSubsystem>())
		{
			Sub->Unregister(this);
		}
	}
	Super::OnUnregister();
}

void USUEvidenceComponent::GetCorners(FVector OutCorners[8]) const
{
	const FTransform& T = GetComponentTransform();
	int32 K = 0;
	for (int32 SX = -1; SX <= 1; SX += 2)
	{
		for (int32 SY = -1; SY <= 1; SY += 2)
		{
			for (int32 SZ = -1; SZ <= 1; SZ += 2)
			{
				OutCorners[K++] = T.TransformPosition(FVector(SX * BoxExtent.X, SY * BoxExtent.Y, SZ * BoxExtent.Z));
			}
		}
	}
}
