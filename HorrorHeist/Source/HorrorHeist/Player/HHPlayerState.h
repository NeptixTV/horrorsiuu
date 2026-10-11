#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/HHTypes.h"
#include "HHPlayerState.generated.h"

class AHHPlayerState;
class UHHInventoryComponent;

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
	UHHInventoryComponent* GetInventory() const { return Inventory; }

	// --- Job state (replicated, written by the heist rules) ---
	/** 0..100. Drops in the dark and when the house shows itself; recovers near the crew / van. */
	float GetPsyche() const { return Psyche; }
	bool IsInsideHouse() const { return bInsideHouse; }
	/** Total value this player stashed in the van on this job (summary). */
	int32 GetStashedValue() const { return StashedValue; }

	// --- Server-only mutators ---
	void SetReady(bool bNewReady);
	void SetCosmetics(const FHHCosmeticLoadout& NewCosmetics);
	void SetEquipment(const FHHEquipmentLoadout& NewEquipment);
	void SetPlayerLevel(int32 NewLevel);
	void SetCrewLeader(bool bNewLeader);
	void SetCrewSlot(int32 NewSlot);
	void SetPsyche(float NewPsyche);
	void SetInsideHouse(bool bNewInside);
	void AddStashedValue(int32 Amount);

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

	UPROPERTY(VisibleAnywhere, Category = "Job")
	TObjectPtr<UHHInventoryComponent> Inventory;

	UPROPERTY(Replicated)
	float Psyche = 100.f;

	UPROPERTY(Replicated)
	bool bInsideHouse = false;

	UPROPERTY(Replicated)
	int32 StashedValue = 0;
};
