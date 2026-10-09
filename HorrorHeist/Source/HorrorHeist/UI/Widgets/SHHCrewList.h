#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AHHLobbyHUD;
class SVerticalBox;

/** Live crew roster: colour, alias, level, leader, ready state, voice activity. */
class SHHCrewList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHCrewList) : _ShowEmptySlots(true) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
		SLATE_ARGUMENT(bool, ShowEmptySlots)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	void Rebuild();

	TWeakObjectPtr<AHHLobbyHUD> HUD;
	TSharedPtr<SVerticalBox> List;
	bool bShowEmptySlots = true;
	uint32 Signature = 0;
};
