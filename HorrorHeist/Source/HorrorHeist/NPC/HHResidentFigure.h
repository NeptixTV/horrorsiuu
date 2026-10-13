#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HHTypes.h"
#include "HHResidentFigure.generated.h"

class USkeletalMeshComponent;
class UHHCosmeticComponent;

/**
 * A resident seen from the street (Mrs Vance at her upstairs window). Purely local and cosmetic:
 * shows at the configured time of day, slowly turns to face the nearest crew member on the
 * street. Watching her from across the lane is how the crew learns what she wears to bed.
 */
UCLASS()
class HORRORHEIST_API AHHResidentFigure : public AActor
{
	GENERATED_BODY()

public:
	AHHResidentFigure();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	void UpdatePresence();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resident")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resident")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resident")
	TObjectPtr<UHHCosmeticComponent> Cosmetics;

	/** Cosmetic item ids she wears (any order). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resident")
	TArray<FName> CosmeticIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resident")
	EHHIntelTime VisibleWhen = EHHIntelTime::DayOnly;

	/** Only in the hub (the house has other plans during a job). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resident")
	bool bHubOnly = true;

	/** Turns towards players closer than this (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resident")
	float NoticeRadius = 3000.f;

	/** Degrees per second. Slow is worse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resident")
	float TurnSpeed = 12.f;

	/** How far she turns away from where she was placed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resident")
	float MaxTurn = 70.f;

private:
	float BaseYaw = 0.f;
	float CheckTimer = 0.f;
	bool bShown = true;
	TWeakObjectPtr<AActor> Watched;
};
