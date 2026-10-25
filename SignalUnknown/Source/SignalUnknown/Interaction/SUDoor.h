// SIGNAL UNKNOWN – hinged door (also used for fridge and cabinet doors).
//
// Actor origin = bottom centre of the opening, +X = outside normal, the hinge sits at -Y.
#pragma once

#include "CoreMinimal.h"
#include "Interaction/SUInteractableActor.h"
#include "SUDoor.generated.h"

UCLASS()
class SIGNALUNKNOWN_API ASUDoor : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUDoor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;

	/** Server: open/close. FromLocation decides the swing direction (away from it). */
	void SetOpen(bool bInOpen, const FVector& FromLocation, bool bSlam = false);
	bool IsOpen() const { return bOpen; }

	// ---- configuration (set before FinishSpawning)
	UPROPERTY(Replicated) float Width = 90.f;
	UPROPERTY(Replicated) float Height = 205.f;
	UPROPERTY(Replicated) float Thickness = 5.f;
	UPROPERTY(Replicated) float MaxAngle = 100.f;
	UPROPERTY(Replicated) ESUMat Material = ESUMat::WoodDark;
	UPROPERTY(Replicated) bool bLocked = false;
	UPROPERTY(Replicated) FText LockedText;
	/** Small doors (fridge, cabinet) use a quieter sound and no handle. */
	UPROPERTY(Replicated) bool bSmall = false;
	UPROPERTY(Replicated) FText DisplayName;

protected:
	virtual void BuildGeometry() override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;

	UPROPERTY(Replicated)
	float OpenSign = 1.f;

	UPROPERTY(ReplicatedUsing = OnRep_Slam)
	uint8 SlamCounter = 0;

	UFUNCTION()
	void OnRep_Open();

	UFUNCTION()
	void OnRep_Slam();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Hinge;

	float CurrentAngle = 0.f;
	float AngleSpeed = 160.f;
	void PlayMoveSound(bool bOpening);
};
