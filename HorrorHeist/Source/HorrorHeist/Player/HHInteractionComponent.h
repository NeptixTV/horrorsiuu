#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HHInteractionComponent.generated.h"

/**
 * Finds what the local player is looking at (10-20 Hz sweep, no per-frame cost on remote
 * pawns) and routes interactions: UI interactions run locally, world interactions are
 * validated and executed on the server.
 */
UCLASS(ClassGroup = (HorrorHeist), meta = (BlueprintSpawnableComponent))
class HORRORHEIST_API UHHInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHHInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Local: use whatever is focused. */
	void TryInteract();

	/** Server: validate range and execute. */
	void ExecuteInteraction(AActor* Target);

	AActor* GetFocusedActor() const { return Focused.Get(); }
	const FText& GetFocusedPrompt() const { return FocusedPrompt; }
	bool CanInteractWithFocus() const { return bFocusUsable; }

	/** Suspends focus (menus open). */
	void SetSuspended(bool bInSuspended);

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float InteractDistance = 230.f;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float SweepRadius = 7.f;

private:
	void UpdateFocus();
	void SetFocus(AActor* NewFocus);

	TWeakObjectPtr<AActor> Focused;
	FText FocusedPrompt;
	bool bFocusUsable = false;
	bool bSuspended = false;
};
