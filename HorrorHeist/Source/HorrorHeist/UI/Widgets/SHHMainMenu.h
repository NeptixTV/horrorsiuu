#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AHHLobbyHUD;

/**
 * The hideout's main menu: navigation on the left, you and your crew on the right. On a job
 * the same screen is the pause menu (resume, settings, call the job off, quit) - the game keeps
 * running for the crew.
 */
class SHHMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHMainMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildNavigation();
	TSharedRef<SWidget> BuildJobNavigation();
	TSharedRef<SWidget> BuildPlayerCard();
	TSharedRef<SWidget> BuildJobSummary();
	FText GetNetworkStatus() const;

	TWeakObjectPtr<AHHLobbyHUD> HUD;
};
