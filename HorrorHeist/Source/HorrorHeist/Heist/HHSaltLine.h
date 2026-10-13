#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHSaltLine.generated.h"

class UStaticMeshComponent;

/**
 * A line of salt a crew member poured on the floor (Salt Pouch). Whatever hunts in the house
 * stops dead at it for a few seconds - once; afterwards the salt lies scattered. Spawned by the
 * server, replicated so everyone sees where it was poured and when it was crossed.
 */
UCLASS()
class HORRORHEIST_API AHHSaltLine : public AActor
{
	GENERATED_BODY()

public:
	AHHSaltLine();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsIntact() const { return !bScattered; }

	/** Server: an intact line within Radius of Location (on its length) is crossed and scatters. */
	static bool TryCross(const UObject* WorldContextObject, const FVector& Location, float Radius);

protected:
	UFUNCTION()
	void OnRep_Scattered();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Salt")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Half length of the poured line (cm), along the actor's X. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Salt")
	float HalfLength = 60.f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Scattered)
	bool bScattered = false;
};
