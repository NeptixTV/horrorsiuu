#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Core/HHTypes.h"

class AHHLobbyHUD;
class AHHHeistGameState;
class AHHPlayerState;
class AHHCharacter;
class UHHInventoryComponent;
class SVerticalBox;
struct FSlateBrush;

/**
 * On-the-job HUD, layered over the normal exploration HUD:
 *   top left     job name + objectives (optional ones marked)
 *   top right    clock, haul in the van, what you carry, crew at the van
 *   top centre   neighbourhood alert / police countdown
 *   left         crew with their psyche
 *   bottom left  your psyche
 *   bottom       the five inventory slots (1-5), selected item name
 * Everything polls the replicated heist state; lists rebuild only when their content changed.
 */
class SHHHeistHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHHeistHud) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	AHHHeistGameState* GetState() const;
	AHHCharacter* GetOwnCharacter() const;
	/** Display text of the key bound to an action ("Interact" -> "E"). */
	FText GetKeyText(const TCHAR* Binding) const;
	AHHPlayerState* GetMyState() const;
	UHHInventoryComponent* GetInventory() const;

	TSharedRef<SWidget> BuildObjectives();
	TSharedRef<SWidget> BuildTopRight();
	TSharedRef<SWidget> BuildAlert();
	TSharedRef<SWidget> BuildPsyche();
	TSharedRef<SWidget> BuildHotbar();
	TSharedRef<SWidget> BuildNoiseMeter();
	/** Tool tag of the equipment in the local player's hand (None for loot / empty hands). */
	FName GetHeldTool() const;
	/** 0..5, from the HUD (it also drives the ticking). */
	float GetMeterReading() const;
	TSharedRef<SWidget> BuildSlot(int32 Index);

	void RebuildObjectives();
	void RebuildCrew();
	void RefreshSlots();

	TWeakObjectPtr<AHHLobbyHUD> HUD;
	TSharedPtr<SVerticalBox> ObjectiveBox;
	TSharedPtr<SVerticalBox> CrewBox;

	TArray<TSharedPtr<FSlateBrush>> SlotBrushes;
	int32 ObjectiveSignature = INDEX_NONE;
	int32 CrewSignature = INDEX_NONE;
	int32 SlotSignature = INDEX_NONE;
	double Now = 0.0;
};
