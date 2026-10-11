#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HHInteractionComponent.generated.h"

class UAudioComponent;
class UTexture2D;

/**
 * Finds what the local player is looking at (10-20 Hz sweep, no per-frame cost on remote
 * pawns) and routes interactions: UI interactions run locally, world interactions are
 * validated and executed on the server. Timed interactions (lockpicking, searching) are
 * held locally with a progress bar; the server re-checks the hold time before executing.
 */
UCLASS(ClassGroup = (HorrorHeist), meta = (BlueprintSpawnableComponent))
class HORRORHEIST_API UHHInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHHInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Local: interact key pressed - use whatever is focused (or start holding). */
	void TryInteract();
	/** Local: interact key released - a hold that has not finished is cancelled. */
	void StopInteract();

	/** Server: a timed interaction started (the hold length is checked on completion). */
	void BeginServerHold(AActor* Target);
	/** Server: validate range (and hold time) and execute. */
	void ExecuteInteraction(AActor* Target);

	AActor* GetFocusedActor() const { return Focused.Get(); }
	const FText& GetFocusedPrompt() const { return FocusedPrompt; }
	const FText& GetFocusedDetail() const { return FocusedDetail; }
	UTexture2D* GetFocusedIcon() const { return FocusedIcon.Get(); }
	bool CanInteractWithFocus() const { return bFocusUsable; }
	/** Hold length of the focused interaction (0 = instant press). */
	float GetFocusedDuration() const { return FocusedDuration; }

	bool IsHolding() const { return HoldTarget.IsValid(); }
	/** 0..1 while holding. */
	float GetHoldProgress() const;

	/** Suspends focus (menus open). */
	void SetSuspended(bool bInSuspended);

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float InteractDistance = 230.f;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float SweepRadius = 7.f;

private:
	void UpdateFocus();
	void SetFocus(AActor* NewFocus);
	void UpdateHold();
	void CancelHold();

	TWeakObjectPtr<AActor> Focused;
	FText FocusedPrompt;
	FText FocusedDetail;
	TWeakObjectPtr<UTexture2D> FocusedIcon;
	float FocusedDuration = 0.f;
	bool bFocusUsable = false;
	bool bSuspended = false;

	// Local hold
	TWeakObjectPtr<AActor> HoldTarget;
	double HoldStartTime = 0.0;
	float HoldDuration = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HoldAudio;

	// Server-side record of the hold, to reject instant "completed" requests.
	TWeakObjectPtr<AActor> ServerHoldTarget;
	double ServerHoldStart = 0.0;
};
