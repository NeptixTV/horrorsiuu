// SIGNAL UNKNOWN – trigger volume that reports Area.Entered / Area.Exited (server).
// Exists locally on every machine (created by the sites); only the server posts events.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SUAreaTrigger.generated.h"

class UBoxComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FSUAreaSignature, class ASUAreaTrigger* /*Area*/, AActor* /*Actor*/);

UCLASS()
class SIGNALUNKNOWN_API ASUAreaTrigger : public AActor
{
	GENERATED_BODY()

public:
	ASUAreaTrigger();

	void Setup(FName InAreaId, const FVector& Extent);
	FName GetAreaId() const { return AreaId; }
	bool ContainsLocation(const FVector& Location) const;
	/** Characters / vehicles currently inside (server). */
	int32 GetOccupantCount() const { return Occupants.Num(); }
	bool IsActorInside(const AActor* Actor) const;

	/** Server: fired after the event bus was informed. */
	FSUAreaSignature OnActorEntered;
	FSUAreaSignature OnActorExited;

private:
	UPROPERTY(VisibleAnywhere, Category = "Area")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> Occupants;

	FName AreaId;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool IsRelevantActor(const AActor* Actor) const;
};
