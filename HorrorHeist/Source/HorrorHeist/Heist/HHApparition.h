#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHApparition.generated.h"

class USkeletalMeshComponent;
class UAnimSequence;
class UMaterialInterface;
class UAudioComponent;
class USoundBase;
class AHHHauntDirector;

UENUM()
enum class EHHApparitionMode : uint8
{
	Hidden,
	/** Stands at the end of a corridor and is gone when you look properly or come close. */
	Glimpse,
	/** Comes for the nearest crew member inside the house. Touch = blackout. */
	Hunt
};

/**
 * Whatever lives in the house. Placed once per job map (hidden); the haunt director decides
 * when and where it shows itself. Server moves it (replicated movement); clients only render.
 * It never leaves the house volume during a hunt, so stepping outside is always an escape.
 */
UCLASS()
class HORRORHEIST_API AHHApparition : public AActor
{
	GENERATED_BODY()

public:
	AHHApparition();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server. */
	void Glimpse(const FVector& Location, const FVector& FaceTowards, float Lifetime);
	void Hunt(const FVector& Location, APawn* Target, float Duration);
	void Vanish();

	EHHApparitionMode GetMode() const { return Mode; }
	bool IsHunting() const { return Mode == EHHApparitionMode::Hunt; }

	void SetDirector(AHHHauntDirector* InDirector) { Director = InDirector; }

protected:
	UFUNCTION()
	void OnRep_Mode();

	void TickGlimpse(float DeltaSeconds);
	void TickHunt(float DeltaSeconds);
	APawn* PickHuntTarget() const;
	bool CanSee(const APawn* Pawn, float MaxAngleDegrees) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Apparition")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Apparition")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Apparition")
	TObjectPtr<UAudioComponent> Presence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Apparition")
	TObjectPtr<UAnimSequence> IdleAnim;

	/** Dark smoky material put on every slot of the body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Apparition")
	TObjectPtr<UMaterialInterface> GhostMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Apparition")
	TObjectPtr<USoundBase> AppearSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Apparition")
	float HuntSpeed = 135.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Apparition")
	float CatchDistance = 110.f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Mode)
	EHHApparitionMode Mode = EHHApparitionMode::Hidden;

	TWeakObjectPtr<AHHHauntDirector> Director;
	TWeakObjectPtr<APawn> HuntTarget;
	double EndTime = 0.0;
	float LookedAtTime = 0.f;
	TSet<TWeakObjectPtr<APawn>> Witnesses;
};
