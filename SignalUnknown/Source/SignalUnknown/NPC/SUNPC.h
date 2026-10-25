// SIGNAL UNKNOWN – simple civilian NPCs (gas station clerk, residents).
//
// Server-driven: walks a route of points back and forth (direct steering, the procedural map has
// no navmesh bounds), waits, turns towards players who talk to it. Residents with bDaytimeOnly
// go inside at dusk. Dialogue is subtitled; the voice is the procedural babble placeholder until
// recorded voice lines exist.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/SUInteractable.h"
#include "SUNPC.generated.h"

class UStaticMeshComponent;
class USUProceduralAudioComponent;

UENUM()
enum class ESUNPCRole : uint8
{
	Clerk,
	Resident
};

UCLASS()
class SIGNALUNKNOWN_API ASUNPC : public ACharacter, public ISUInteractable
{
	GENERATED_BODY()

public:
	ASUNPC();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ISUInteractable
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;
	virtual float GetInteractionRange() const override { return 280.f; }

	UPROPERTY(Replicated) ESUNPCRole Role = ESUNPCRole::Resident;
	UPROPERTY(Replicated) FText DisplayName;
	UPROPERTY(Replicated) bool bDaytimeOnly = false;
	/** Server: route, first point is "home" (register / front door). */
	TArray<FVector> WanderPoints;

	// ---------------------------------------------------------------- server
	/** Go to the home point and stay there for a while (customer at the register). */
	void Summon();
	bool IsAtHome(float Tolerance = 120.f) const;
	/** Says a line to one player (subtitle on their screen + spatial voice for everyone). */
	void SayTo(ASUCharacter* Listener, const FString& Text, float Duration = 0.f);
	/** Turn towards an actor for a few seconds. */
	void FaceActor(AActor* Target, float Seconds);
	bool IsAway() const { return bAway; }

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpeak(float Duration);

private:
	UPROPERTY(ReplicatedUsing = OnRep_Away) bool bAway = false;
	UFUNCTION() void OnRep_Away();

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LegLeft;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LegRight;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ArmLeft;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ArmRight;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> VoiceAudio;
	UPROPERTY(Transient) TObjectPtr<USUProceduralAudioComponent> FootstepAudio;

	int32 RouteIndex = 0;
	int32 RouteDirection = 1;
	float WaitTimer = 2.f;
	float SummonTimer = 0.f;
	float FaceTimer = 0.f;
	TWeakObjectPtr<AActor> FaceTarget;
	float StuckTimer = 0.f;
	FVector LastLocation = FVector::ZeroVector;
	float SpeakUntil = 0.f;
	float WalkPhase = 0.f;
	float StepTimer = 0.f;
	int32 TalkCounter = 0;
	float NextAwayCheck = 0.f;

	void BuildBody();
	void UpdateRoute(float DeltaSeconds);
	void UpdateDaytime();
	void AnimateBody(float DeltaSeconds);
	FString PickLine(const ASUCharacter* Listener);
	FString Greeting() const;
};
