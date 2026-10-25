// SIGNAL UNKNOWN – evidence that appears at runtime (tracks left by a creature, ...).
// Replicated so every player sees (and can photograph) the same thing.
#pragma once

#include "CoreMinimal.h"
#include "World/SUProceduralActor.h"
#include "SUEvidenceProp.generated.h"

class USUEvidenceComponent;

UENUM()
enum class ESUEvidencePropLook : uint8
{
	CreatureTracks,
	ClawMarks,
	BloodDrops
};

UCLASS()
class SIGNALUNKNOWN_API ASUEvidenceProp : public ASUProceduralActor
{
	GENERATED_BODY()

public:
	ASUEvidenceProp();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: spawns the prop on the ground at Location. */
	static ASUEvidenceProp* SpawnProp(UWorld* World, FName EvidenceId, const FText& DisplayName, FName Category, ESUEvidencePropLook Look, const FVector& Location, float Yaw, int32 Seed);

	UPROPERTY(Replicated) FName EvidenceId;
	UPROPERTY(Replicated) FText DisplayName;
	UPROPERTY(Replicated) FName Category;
	UPROPERTY(Replicated) ESUEvidencePropLook Look = ESUEvidencePropLook::CreatureTracks;
	UPROPERTY(Replicated) int32 Seed = 1;

private:
	UPROPERTY(VisibleAnywhere, Category = "Evidence")
	TObjectPtr<USUEvidenceComponent> Evidence;
};
