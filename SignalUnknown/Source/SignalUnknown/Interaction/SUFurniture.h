// SIGNAL UNKNOWN – interactive furniture: drawers and the bed.
#pragma once

#include "CoreMinimal.h"
#include "Interaction/SUInteractableActor.h"
#include "SUFurniture.generated.h"

/**
 * Small cabinet with one drawer. Items spawned into GetDrawerComponent() move with the drawer and
 * can only be reached (traced) while it is open.
 * Origin: floor, front centre; +X = front.
 */
UCLASS()
class SIGNALUNKNOWN_API ASUDrawer : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUDrawer();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;

	USceneComponent* GetDrawerComponent() const { return DrawerRoot; }
	/** Local position (relative to the drawer box) where items can be placed. */
	FVector GetItemSlot(int32 Index) const;

	UPROPERTY(Replicated) float Width = 60.f;
	UPROPERTY(Replicated) float Depth = 45.f;
	UPROPERTY(Replicated) float CabinetHeight = 70.f;
	UPROPERTY(Replicated) ESUMat Material = ESUMat::WoodLight;

protected:
	virtual void BuildGeometry() override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;

	UFUNCTION()
	void OnRep_Open();

	/** Default subobject so items can be attached right after spawning. */
	UPROPERTY(VisibleAnywhere, Category = "Drawer")
	TObjectPtr<USceneComponent> DrawerRoot;

	float Slide = 0.f;
};

/** Sleep to skip time (only at the base, outside of active field work). Saves the game. */
UCLASS()
class SIGNALUNKNOWN_API ASUBed : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUBed();

	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;

protected:
	virtual void BuildGeometry() override;

private:
	/** Returns an empty text if sleeping is allowed, otherwise the reason. */
	FText GetSleepBlocker() const;
};
