#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHHauntProp.generated.h"

class UStaticMeshComponent;

/**
 * Ordinary furniture or decor the house can move while nobody is looking: a chair turned to
 * face the wall, a portrait hanging upside down, the teddy bear now sitting in the hallway.
 * Never loot, never on a walking path (the level places them), so nothing gets blocked.
 */
UCLASS()
class HORRORHEIST_API AHHHauntProp : public AActor
{
	GENERATED_BODY()

public:
	AHHHauntProp();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsMoved() const { return bMoved; }
	/** Server. */
	void MoveToAlternate();

protected:
	UFUNCTION()
	void OnRep_Moved();
	void ApplyTransform();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Haunt")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Where the prop ends up (world space). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	FVector AltLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	FRotator AltRotation = FRotator::ZeroRotator;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Moved)
	bool bMoved = false;

	FTransform Original;
};
