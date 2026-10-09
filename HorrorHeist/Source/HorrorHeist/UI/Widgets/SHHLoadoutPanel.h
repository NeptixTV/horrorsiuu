#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Core/HHTypes.h"

class AHHLobbyHUD;
class SBox;
class SVerticalBox;
class SWrapBox;
class UHHEquipmentDefinition;
struct FSlateBrush;

/** The equipment locker: one item per loadout slot, stats come from the item data. */
class SHHLoadoutPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHLoadoutPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	virtual ~SHHLoadoutPanel() override;
	void Construct(const FArguments& InArgs);

private:
	void Refresh();
	void RebuildSlots();
	void RebuildGrid();
	void RebuildDetails();
	void SelectSlot(EHHEquipmentSlot Slot);
	void Inspect(FName ItemId);

	TWeakObjectPtr<AHHLobbyHUD> HUD;
	TSharedPtr<SVerticalBox> SlotList;
	TSharedPtr<SWrapBox> Grid;
	TSharedPtr<SBox> Details;

	EHHEquipmentSlot SelectedSlot = EHHEquipmentSlot::Light;
	FName Inspected;
	TArray<TSharedPtr<FSlateBrush>> Brushes;
	FDelegateHandle ProfileHandle;
};
