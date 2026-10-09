#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHStationActor.h"
#include "HHPreviewStation.generated.h"

class USpringArmComponent;
class AHHPreviewMannequin;

/**
 * The dressing corner: a cracked mirror, a lamp and a mannequin of you. While the
 * customization screen is open the camera orbits a local preview body that the player
 * can rotate (drag / Q-E / right stick) and zoom (wheel / triggers).
 */
UCLASS()
class HORRORHEIST_API AHHPreviewStation : public AHHStationActor
{
	GENERATED_BODY()

public:
	AHHPreviewStation();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnViewActivated(APlayerController* Viewer) override;
	virtual void OnViewDeactivated(APlayerController* Viewer) override;

	void AddYaw(float Degrees);
	/** -1 (closer) .. +1 (further) steps. */
	void AddZoom(float Steps);
	/** Frames the camera on the body region of a slot (head for hats/masks...). */
	void FocusSlot(EHHCosmeticSlot Slot);
	/** Shows a loadout on the mannequin without equipping it (store / locked previews). */
	void PreviewLoadout(const FHHCosmeticLoadout& Loadout);
	/** Back to the player's equipped outfit. */
	void ShowEquipped();

protected:
	void HandleProfileChanged();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<USceneComponent> PreviewSpot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(EditAnywhere, Category = "Preview")
	float MinArmLength = 90.f;

	UPROPERTY(EditAnywhere, Category = "Preview")
	float MaxArmLength = 360.f;

	UPROPERTY(Transient)
	TObjectPtr<AHHPreviewMannequin> Mannequin;

	TWeakObjectPtr<APlayerController> CurrentViewer;
	FDelegateHandle ProfileHandle;

	float TargetYaw = 0.f;
	float CurrentYaw = 0.f;
	float TargetArm = 260.f;
	float TargetHeight = 100.f;
	bool bShowingPreview = false;
};
