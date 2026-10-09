#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Core/HHTypes.h"

class AHHLobbyHUD;
class SBox;
class SWrapBox;
class UHHCosmeticDefinition;
struct FSlateBrush;

/**
 * The dressing corner. The camera frames your mannequin on the left (drag to rotate, wheel
 * to zoom); the right side lists every cosmetic per slot. Owned items are worn instantly
 * (and replicated to the crew), others can be previewed and bought on the spot.
 */
class SHHCustomizationPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHCustomizationPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	virtual ~SHHCustomizationPanel() override;
	void Construct(const FArguments& InArgs);

private:
	void Refresh();
	void RebuildSlotTabs();
	void RebuildGrid();
	void RebuildDetails();
	void SelectSlot(EHHCosmeticSlot Slot);
	void Select(FName ItemId);
	void Preview(const UHHCosmeticDefinition* Item);
	void Buy(const UHHCosmeticDefinition* Item);
	class AHHPreviewStation* GetStation() const;

	TWeakObjectPtr<AHHLobbyHUD> HUD;
	TSharedPtr<SWrapBox> SlotTabs;
	TSharedPtr<SWrapBox> Grid;
	TSharedPtr<SBox> Details;

	EHHCosmeticSlot SelectedSlot = EHHCosmeticSlot::Mask;
	FName Selected;
	TArray<TSharedPtr<FSlateBrush>> Brushes;
	FDelegateHandle ProfileHandle;
};
