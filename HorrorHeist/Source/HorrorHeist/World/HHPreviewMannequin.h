#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HHTypes.h"
#include "HHPreviewMannequin.generated.h"

class USkeletalMeshComponent;
class UHHCosmeticComponent;

/** Local-only dressed body used by the customization mirror. Never replicated. */
UCLASS(NotPlaceable)
class HORRORHEIST_API AHHPreviewMannequin : public AActor
{
	GENERATED_BODY()

public:
	AHHPreviewMannequin();

	virtual void BeginPlay() override;

	void ShowLoadout(const FHHCosmeticLoadout& Loadout);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UHHCosmeticComponent> Cosmetics;
};
