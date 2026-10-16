#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AHHLobbyHUD;
class UHHProfileSubsystem;

/**
 * Hollowmere Savings & Loan (teller window, ATM): the player's account. Money in the bank is
 * safe from bail and earns interest every job night; shops only take what is in the pocket.
 */
class SHHBankPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHBankPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	UHHProfileSubsystem* GetProfile() const;
	/** Amount < 0 = everything. */
	void Deposit(int64 Amount);
	void Withdraw(int64 Amount);
	TSharedRef<SWidget> MakeAmountRow(bool bDeposit);
	TSharedRef<SWidget> MakeBalance(const FText& Label, TAttribute<FText> Value, const FLinearColor& Color);

	TWeakObjectPtr<AHHLobbyHUD> HUD;
};
