#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHHauntMark.generated.h"

class UDecalComponent;
class USoundBase;

/**
 * Handprints on a mirror, words scratched into the plaster: hidden until the house shows them.
 * The decal fades in on every machine; the camera can photograph a revealed mark once (evidence).
 */
UCLASS()
class HORRORHEIST_API AHHHauntMark : public AActor
{
	GENERATED_BODY()

public:
	AHHHauntMark();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsRevealed() const { return bRevealed; }
	/** Minimum haunt stage at which the house uses this mark. */
	int32 GetMinStage() const { return MinStage; }

	/** Server. */
	void Reveal();
	/** Server: a camera flash from Eye along Direction; true (once) if this mark is in the picture. */
	bool TryPhotograph(const FVector& Eye, const FVector& Direction, const AActor* Photographer);

protected:
	UFUNCTION()
	void OnRep_Revealed();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mark")
	TObjectPtr<UDecalComponent> Decal;

	/** Played where it appears (squeak on glass, nails on plaster). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mark")
	TObjectPtr<USoundBase> RevealSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mark")
	FText Caption;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mark")
	int32 MinStage = 2;

	/** Seconds the mark takes to come through. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mark")
	float FadeSeconds = 2.5f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Revealed)
	bool bRevealed = false;

	bool bPhotographed = false;
	float Fade = 0.f;
};
