#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/HHTypes.h"
#include "HHProfileSubsystem.generated.h"

class UHHProfileSaveGame;
class UHHItemDefinition;
class UHHCosmeticDefinition;
class UHHEquipmentDefinition;

DECLARE_MULTICAST_DELEGATE(FHHOnProfileChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FHHOnLevelUp, int32 /*NewLevel*/);

UENUM()
enum class EHHPurchaseCheck : uint8
{
	Ok,
	AlreadyOwned,
	NotEnoughCash,
	LevelTooLow,
	NotForSale
};

/**
 * Owns the local profile save: name, cash, XP/level, owned items and loadouts.
 * The profile is client-side for now (listen-server co-op); the server only validates that
 * replicated item ids exist. A backend can replace the persistence later behind this API.
 */
UCLASS()
class HORRORHEIST_API UHHProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UHHProfileSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Identity ---
	FString GetPlayerName() const;
	void SetPlayerName(const FString& NewName);
	/** True on first launch, before the player picked a name. */
	bool NeedsName() const;
	static FString SuggestName();

	// --- Economy ---
	/** Cash in the pocket: what shops take, and what bail eats into. */
	int64 GetCash() const;
	void AddCash(int64 Amount);

	// --- The bank (Hollowmere Savings & Loan) ---
	int64 GetSavings() const;
	/** Pocket -> account. False if the pocket does not hold Amount. */
	bool Deposit(int64 Amount);
	/** Account -> pocket. False if the account does not hold Amount. */
	bool Withdraw(int64 Amount);
	/** Interest the account will earn on the next job night. */
	int64 GetNextInterest() const;
	static constexpr double InterestRate = 0.02;
	static constexpr int64 MaxInterestPerNight = 400;
	/** Share of the pocket cash an arrested crew member pays in bail (savings are untouched). */
	static constexpr double BailShare = 0.25;
	int32 GetDay() const;
	/** What the last booked job did to the account (shown as notices back in the hideout). */
	int64 GetLastInterest() const { return LastInterest; }
	int64 GetLastBail() const { return LastBail; }

	// --- Progression ---
	static constexpr int32 MaxLevel = 50;
	static int64 XPRequiredForLevel(int32 Level);
	int64 GetTotalXP() const;
	int32 GetLevel() const;
	/** 0..1 progress towards the next level. */
	float GetLevelProgress() const;
	int64 GetXPIntoLevel() const;
	int64 GetXPForNextLevel() const;
	void AddXP(int64 Amount);

	// --- Ownership ---
	bool IsOwned(FName ItemId) const;
	EHHPurchaseCheck CanPurchase(const UHHItemDefinition* Item) const;
	static FText DescribePurchaseCheck(EHHPurchaseCheck Check, const UHHItemDefinition* Item);
	bool Purchase(const UHHItemDefinition* Item);

	// --- Loadouts ---
	const FHHCosmeticLoadout& GetCosmetics() const;
	bool EquipCosmetic(const UHHCosmeticDefinition* Item);
	void ClearCosmeticSlot(EHHCosmeticSlot Slot);

	const FHHEquipmentLoadout& GetEquipment() const;
	bool EquipItem(const UHHEquipmentDefinition* Item);
	void ClearEquipmentSlot(EHHEquipmentSlot Slot);

	FName GetLastMission() const;
	void SetLastMission(FName MissionId);

	int32 GetJobsCompleted() const;

	// --- Scouting notebook ---
	TArray<FName> GetKnownIntel() const;
	/** Adds the ids that are new (saves when anything changed). */
	void RememberIntel(const TArray<FName>& IntelIds);
	void ForgetAllIntel();

	/**
	 * Books a finished job: payout, XP, job counter, the night's interest and - if this player
	 * (OwnCrewSlot) was caught - bail from the pocket (one save).
	 */
	void RecordJob(const FHHMissionSummary& Summary, int32 OwnCrewSlot = INDEX_NONE);
	/** Summary of the last job of this session, shown once back in the hideout. */
	bool ConsumeLastJobSummary(FHHMissionSummary& OutSummary);

	// --- Persistence / development ---
	void Save();
	void ResetProfile();
	void UnlockEverything();

	FHHOnProfileChanged OnProfileChanged;
	FHHOnLevelUp OnLevelUp;

private:
	void LoadOrCreate();
	void InitializeNewProfile();
	/** Repairs loadouts after content changed (removed items, missing required slots). */
	void Sanitize();
	void NotifyChanged();
	FString GetSlotName() const;

	UPROPERTY(Transient)
	TObjectPtr<UHHProfileSaveGame> Profile;

	FHHMissionSummary LastJob;
	bool bHasLastJob = false;
	int64 LastInterest = 0;
	int64 LastBail = 0;
};
