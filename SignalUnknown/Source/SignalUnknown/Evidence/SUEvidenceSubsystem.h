// SIGNAL UNKNOWN – registry of evidence objects and the photo evaluation.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Evidence/SUPhotoTypes.h"
#include "SUEvidenceSubsystem.generated.h"

class USUEvidenceComponent;

/** Everything known about the moment the shutter was pressed. */
struct FSUPhotoShot
{
	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	float HorizontalFovDeg = 90.f;
	float AspectRatio = 16.f / 9.f;
	bool bFlash = false;
	bool bFlashlightOn = false;
	FVector FlashlightDirection = FVector::ForwardVector;
	bool bBlurred = false;
	float FocusError = 0.f;
	TArray<const AActor*> IgnoredActors;
};

UCLASS()
class SIGNALUNKNOWN_API USUEvidenceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Register(USUEvidenceComponent* Evidence);
	void Unregister(USUEvidenceComponent* Evidence);

	/** Which registered evidence is recognisable in a photo taken with these parameters. */
	TArray<FSUEvidenceHit> EvaluatePhoto(const FSUPhotoShot& Shot) const;

	const TArray<TWeakObjectPtr<USUEvidenceComponent>>& GetAll() const { return Evidence; }

private:
	TArray<TWeakObjectPtr<USUEvidenceComponent>> Evidence;

	float MeasureVisibility(const USUEvidenceComponent* E, const FSUPhotoShot& Shot) const;
	bool IsLit(const USUEvidenceComponent* E, const FSUPhotoShot& Shot) const;
};
