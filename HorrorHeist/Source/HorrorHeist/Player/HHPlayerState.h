#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/HHTypes.h"
#include "HHPlayerState.generated.h"

class AHHPlayerState;

DECLARE_MULTICAST_DELEGATE_OneParam(FHHOnPlayerStateEvent, AHHPlayerState* /*PlayerState*/);

/**
 * Replicated identity of a crew member: name, level, ready state, cosmetics and loadout.
 * Every client can render every other player's appearance from this alone.
 */
UCLASS()
class HORRORHEIST_API AHHPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AHHPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OnRep_PlayerName() override;

	// --- Accessors ---
	bool IsReady() const { return bReady; }
	const FHHCosmeticLoadout& GetCosmetics() const { return Cosmetics; }
	const FHHEquipmentLoadout& GetEquipment() const { return Equipment; }
	int32 GetPlayerLevel() const { return PlayerLevel; }
	bool IsCrewLeader() const { return bCrewLeader; }
	int32 GetCrewSlot() const { return CrewSlot; }
	FLinearColor GetCrewColor() const;

	// --- Server-only mutators ---
	void SetReady(bool bNewReady);
	void SetCosmetics(const FHHCosmeticLoadout& NewCosmetics);
	void SetEquipment(const FHHEquipmentLoadout& NewEquipment);
	void SetPlayerLevel(int32 NewLevel);
	void SetCrewLeader(bool bNewLeader);
	void SetCrewSlot(int32 NewSlot);

	/** Fired on every machine when this player's cosmetics change (character re-dresses). */
	FHHOnPlayerStateEvent OnCosmeticsChanged;
	/** Fired when the equipment loadout changes (flashlight beam, visible gear). */
	FHHOnPlayerStateEvent OnEquipmentChanged;

	/** Any crew-relevant change on any player state (join, leave, ready, name, level). UI listens here. */
	static FHHOnPlayerStateEvent& OnAnyCrewChange();

private:
	UFUNCTION()
	void OnRep_Ready();

	UFUNCTION()
	void OnRep_Cosmetics();

	UFUNCTION()
	void OnRep_Equipment();

	UFUNCTION()
	void OnRep_CrewInfo();

	void BroadcastCrewChange();

	UPROPERTY(ReplicatedUsing = OnRep_Ready)
	bool bReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_Cosmetics)
	FHHCosmeticLoadout Cosmetics;

	UPROPERTY(ReplicatedUsing = OnRep_Equipment)
	FHHEquipmentLoadout Equipment;

	UPROPERTY(ReplicatedUsing = OnRep_CrewInfo)
	int32 PlayerLevel = 1;

	UPROPERTY(ReplicatedUsing = OnRep_CrewInfo)
	bool bCrewLeader = false;

	UPROPERTY(ReplicatedUsing = OnRep_CrewInfo)
	int32 CrewSlot = 0;
};
