#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHSwing.generated.h"

class UStaticMeshComponent;
class UAudioComponent;

/**
 * A playground swing: the seat and chains hang from the actor's pivot (the top bar) and swing
 * about the bar's axis (local X). With a child on it (a townsperson riding it by day) it swings
 * high; empty, it only drifts in the wind - and creaks. Local, driven by the game-state clock.
 */
UCLASS()
class HORRORHEIST_API AHHSwing : public AActor
{
	GENERATED_BODY()

public:
	AHHSwing();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UStaticMeshComponent* GetSeat() const { return Seat; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Swing")
	TObjectPtr<USceneComponent> Pivot;

	/** Seat + chains, modelled hanging from the pivot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Swing")
	TObjectPtr<UStaticMeshComponent> Seat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Swing")
	TObjectPtr<UAudioComponent> Creak;

	/** Degrees with a rider / when empty (wind). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swing")
	float RiddenAngle = 32.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swing")
	float EmptyAngle = 4.f;

	/** Seconds per full swing (a 2 m chain swings at about 2.8 s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swing")
	float Period = 2.8f;

private:
	float Amplitude = 0.f;
	float Phase = 0.f;
};
