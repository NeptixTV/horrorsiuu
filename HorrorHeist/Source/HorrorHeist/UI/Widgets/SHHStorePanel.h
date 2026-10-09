#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AHHLobbyHUD;
class SBox;
class SWrapBox;
class UHHItemDefinition;
struct FSlateBrush;

/** "The Fence": buy gear and clothes with cash earned on jobs. */
class SHHStorePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHStorePanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	virtual ~SHHStorePanel() override;
	void Construct(const FArguments& InArgs);

private:
	void Refresh();
	void RebuildGrid();
	void RebuildDetails();
	bool PassesFilter(const UHHItemDefinition* Item) const;
	void Buy(const UHHItemDefinition* Item);

	TWeakObjectPtr<AHHLobbyHUD> HUD;
	TSharedPtr<SWrapBox> Grid;
	TSharedPtr<SBox> Details;
	int32 Category = 0;
	FName Selected;
	TArray<TSharedPtr<FSlateBrush>> Brushes;
	FDelegateHandle ProfileHandle;
};
