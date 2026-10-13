#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AHHLobbyHUD;
class SVerticalBox;
class UHHInventoryComponent;
struct FSlateBrush;

/**
 * Tab on a job: the five carry slots in detail - what each piece of gear does and how many
 * charges are left, what the loot is worth and which pieces are heavy. Hold or drop from here.
 * (In the hideout Tab opens the locker, where the loadout for the next job is chosen.)
 */
class SHHInventoryPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHInventoryPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	virtual ~SHHInventoryPanel() override;
	void Construct(const FArguments& InArgs);

private:
	UHHInventoryComponent* GetInventory() const;
	void Rebuild();
	void Hold(int32 Index);
	void Drop(int32 Index);

	TWeakObjectPtr<AHHLobbyHUD> HUD;
	TWeakObjectPtr<UHHInventoryComponent> BoundInventory;
	TSharedPtr<SVerticalBox> Rows;
	TArray<TSharedPtr<FSlateBrush>> Brushes;
	FDelegateHandle InventoryHandle;
};
