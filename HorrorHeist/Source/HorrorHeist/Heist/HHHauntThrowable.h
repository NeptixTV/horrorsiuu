#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "HHHauntThrowable.generated.h"

class UStaticMeshComponent;
class USoundBase;

/**
 * A book, a mug, a picture frame: small things the house throws (poltergeist). The flight is
 * a scripted arc replicated as start/end/time, so every machine sees the same throw without
 * physics replication; it lands with a crash the street can hear. Never loot, never in the way.
 */
UCLASS()
class HORRORHEIST_API AHHHauntThrowable : public AActor
{
	GENERATED_BODY()

public:
	AHHHauntThrowable();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool CanBeThrown() const { return !bThrown; }
	/** Server: fly to Target (on the floor) in about Duration seconds. */
	void ThrowTo(const FVector& Target, float Duration);

protected:
	UFUNCTION()
	void OnRep_Throw();
	float GetServerTime() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throwable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwable")
	TObjectPtr<USoundBase> ImpactSound;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Throw)
	bool bThrown = false;

	UPROPERTY(ReplicatedUsing = OnRep_Throw)
	FVector_NetQuantize From = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_Throw)
	FVector_NetQuantize To = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_Throw)
	float ThrowStart = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Throw)
	float ThrowDuration = 0.6f;

	FRotator StartRotation;
	FRotator EndRotation;
	bool bLanded = false;
};
