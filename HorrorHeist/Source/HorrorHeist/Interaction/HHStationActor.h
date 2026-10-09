#pragma once

#include "CoreMinimal.h"
#include "Interaction/HHInteractableActor.h"
#include "Core/HHTypes.h"
#include "HHStationActor.generated.h"

class UCameraComponent;
class APlayerController;

/**
 * A physical place in the hideout bound to a lobby screen (job board -> Play, locker ->
 * Loadout, mirror -> Customization, terminal -> Store). Using it, or opening its screen from
 * the menu, blends the local camera to the station's view so the menus feel like walking
 * up to real furniture. Stations with bUsable = false are pure camera spots (menu camera).
 */
UCLASS()
class HORRORHEIST_API AHHStationActor : public AHHInteractableActor
{
	GENERATED_BODY()

public:
	AHHStationActor();

	virtual void Tick(float DeltaSeconds) override;

	// IHHInteractable
	virtual bool IsLocalInteraction() const override { return true; }
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

	/** Local view target notifications from the HUD. */
	virtual void OnViewActivated(APlayerController* Viewer);
	virtual void OnViewDeactivated(APlayerController* Viewer);

	EHHLobbyScreen GetScreen() const { return Screen; }
	bool IsUsable() const { return bUsable; }

	/** Finds the station for a screen in the given world (first match, prefers usable ones). */
	static AHHStationActor* FindForScreen(const UWorld* World, EHHLobbyScreen InScreen);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UCameraComponent> ViewCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	EHHLobbyScreen Screen = EHHLobbyScreen::Play;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	bool bUsable = true;

	/** Hand-held camera drift while viewed (degrees). 0 = locked off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	float CameraDrift = 0.6f;

	FRotator BaseCameraRotation;
	int32 ActiveViewers = 0;
	float DriftTime = 0.f;
};
