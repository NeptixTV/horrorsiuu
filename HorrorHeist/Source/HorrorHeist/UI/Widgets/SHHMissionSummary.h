#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Core/HHTypes.h"

class AHHLobbyHUD;

/**
 * The end-of-job card, played as a short sequence (about ten seconds):
 *   the ending stamped onto the screen, the haul counting up item by item, the target bonus and
 *   the fence's cut, your share counting up, then the crew roll call (made it / left behind /
 *   arrested, who drove, what each one stashed or lost) and the reputation gained.
 * Purely local presentation of the replicated FHHMissionSummary.
 */
class SHHMissionSummary : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHMissionSummary) {}
		SLATE_ARGUMENT(FHHMissionSummary, Summary)
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** Fades and slides Content in at At seconds. */
	TSharedRef<SWidget> Reveal(TSharedRef<SWidget> Content, float At, float Duration = 0.35f, FVector2D From = FVector2D(-28.0, 0.0)) const;
	TSharedRef<SWidget> BuildHeader();
	TSharedRef<SWidget> BuildMoney();
	TSharedRef<SWidget> BuildLoot();
	TSharedRef<SWidget> BuildCrew();

	/** 0..1 eased progress of a span. */
	float Progress(float Start, float Duration) const;
	void Cue(float At, EHHUISound Sound);

	FHHMissionSummary Summary;
	TWeakObjectPtr<AHHLobbyHUD> HUD;
	FLinearColor OutcomeColor = FLinearColor::White;
	float Elapsed = 0.f;
	float LootStart = 1.9f;
	float LootStep = 0.16f;
	float MoneyEnd = 4.f;
	float CutAt = 4.3f;
	float CrewStart = 6.4f;
	float XPAt = 8.4f;

	struct FCue
	{
		float At = 0.f;
		EHHUISound Sound = EHHUISound::Click;
		bool bPlayed = false;
	};
	TArray<FCue> Cues;
};
