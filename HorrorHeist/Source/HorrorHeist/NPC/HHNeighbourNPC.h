#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HHNeighbourNPC.generated.h"

class USpotLightComponent;
class UHHCosmeticComponent;
class UHHCharacterDefinition;
class USoundBase;
class APawn;

DECLARE_MULTICAST_DELEGATE_OneParam(FHHOnNeighbourSpotted, APawn* /*Who*/);
DECLARE_MULTICAST_DELEGATE(FHHOnNeighbourGaveUp);

UENUM()
enum class EHHNeighbourState : uint8
{
	/** At home (hidden). */
	Home,
	/** Walking over to have a look, torch in hand. */
	Investigating,
	/** Standing in the yard, looking around. */
	Looking,
	/** Saw someone: hurries back inside to call the police. */
	Fleeing,
	/** Nothing there: strolls back. */
	Returning
};

/**
 * The neighbour (Mr Pruitt) who comes out when the street gets loud. Walks a path of tagged
 * points (HH_NeighbourPath_<n>) from his porch to the job's house with a torch, looks around,
 * and goes home again. If he sees a crew member long enough he runs inside and the heist mode
 * calls the police. Server-driven; clients see a replicated character.
 */
UCLASS()
class HORRORHEIST_API AHHNeighbourNPC : public ACharacter
{
	GENERATED_BODY()

public:
	AHHNeighbourNPC(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AHHNeighbourNPC* Get(const UObject* WorldContextObject);

	/** Server: come out and look towards Location. False if he is already out. */
	bool SendOut(const FVector& Location);
	bool IsOut() const { return State != EHHNeighbourState::Home; }
	EHHNeighbourState GetState() const { return State; }
	/** 0..1 how close he is to recognising someone (HUD warning). */
	float GetSuspicion() const { return Suspicion; }

	const FText& GetDisplayName() const { return DisplayName; }

	/** Server: he recognised someone (the heist mode calls the police). */
	FHHOnNeighbourSpotted OnSpotted;
	/** Server: he found nothing and went back inside. */
	FHHOnNeighbourGaveUp OnGaveUp;

protected:
	UFUNCTION()
	void OnRep_State();

	void SetState(EHHNeighbourState NewState);
	void TickServer(float DeltaSeconds);
	void Walk(const FVector& Point, float SpeedScale);
	void UpdateSight(float DeltaSeconds);
	bool CanSee(const APawn* Pawn, float& OutDistance) const;
	void GatherPath();
	void UpdateAnimation();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSound(USoundBase* Sound);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neighbour")
	TObjectPtr<UHHCosmeticComponent> Cosmetics;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neighbour")
	TObjectPtr<USpotLightComponent> Torch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neighbour")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neighbour")
	TArray<FName> CosmeticIds;

	/** Tag prefix of his path points; HH_NeighbourPath_0 is his porch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neighbour")
	FString PathTagPrefix = TEXT("HH_NeighbourPath_");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neighbour")
	float SightRange = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neighbour")
	float SightHalfAngle = 65.f;

	/** Seconds to recognise someone standing in plain view close by. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neighbour")
	float RecogniseSeconds = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neighbour")
	float LookSeconds = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neighbour")
	TObjectPtr<USoundBase> DoorSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neighbour")
	TObjectPtr<USoundBase> ShoutSound;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	EHHNeighbourState State = EHHNeighbourState::Home;

	UPROPERTY(Replicated)
	float Suspicion = 0.f;

private:
	TArray<FVector> PathPoints;
	FVector Interest = FVector::ZeroVector;
	FTransform HomeTransform;
	int32 PathIndex = 0;
	float StateTime = 0.f;
	float SightTimer = 0.f;
	float LookYaw = 0.f;
	TWeakObjectPtr<APawn> Suspect;
	TWeakObjectPtr<const UHHCharacterDefinition> Definition;
};
