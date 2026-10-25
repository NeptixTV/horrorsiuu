// SIGNAL UNKNOWN – an item lying in the world that can be picked up.
#pragma once

#include "CoreMinimal.h"
#include "Interaction/SUInteractableActor.h"
#include "SUPickupItem.generated.h"

UCLASS()
class SIGNALUNKNOWN_API ASUPickupItem : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUPickupItem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;
	virtual float GetInteractionRange() const override { return 220.f; }

	/** Server: spawn an item in the world (e.g. dropped by a player). */
	static ASUPickupItem* SpawnItem(UWorld* World, FName ItemId, int32 Count, float Charge, const FTransform& Transform, AActor* AttachTo = nullptr);

	UPROPERTY(Replicated) FName ItemId;
	UPROPERTY(Replicated) int32 Count = 1;
	UPROPERTY(Replicated) float Charge = 1.f;

protected:
	virtual void BuildGeometry() override;
};
