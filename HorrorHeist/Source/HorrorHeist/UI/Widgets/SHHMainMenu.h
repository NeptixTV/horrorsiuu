#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AHHLobbyHUD;

/** The hideout's main menu: navigation on the left, you and your crew on the right. */
class SHHMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHMainMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildPlayerCard();
	TSharedRef<SWidget> BuildJobSummary();
	FText GetNetworkStatus() const;

	TWeakObjectPtr<AHHLobbyHUD> HUD;
};
