#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHHauntProp.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;

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

	/** A rocking chair (or cradle) that can rock by itself. */
	bool CanRock() const { return bCanRock && !IsRocking(); }
	bool IsRocking() const;
	/** Server: rocks for Seconds (every machine animates it from the replicated end time). */
	void Rock(float Seconds);

	virtual void Tick(float DeltaSeconds) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	bool bCanRock = false;

	/** Degrees of the swing and seconds per swing while rocking (around the prop's own right axis). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	float RockAngle = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	float RockPeriod = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haunt")
	TObjectPtr<USoundBase> RockSound;

	UFUNCTION()
	void OnRep_Rock();
	float GetServerTime() const;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Moved)
	bool bMoved = false;

	/** Game-state clock (server world time) when the rocking stops. */
	UPROPERTY(ReplicatedUsing = OnRep_Rock)
	float RockUntil = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RockAudio;

	FTransform Original;
	FTransform RockBase;
};
