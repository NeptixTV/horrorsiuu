#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Animation/CurveSequence.h"
#include "Core/HHTypes.h"

class AHHLobbyHUD;
class SBox;
class SOverlay;
class SVerticalBox;

/**
 * Root of the lobby UI. Layers, bottom to top: vignette, exploration HUD, main menu,
 * screen panels, always-on feedback (toasts, captions, countdown), title card, modals,
 * fade. Panels are rebuilt each time they open so they always show fresh data.
 */
class SHHLobbyRoot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHLobbyRoot) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	void SetScreen(EHHLobbyScreen Screen);
	void PushToast(const FText& Message, EHHNotifyType Type);
	void ShowCaption(const FText& Text, float Duration, bool bIsSoundCaption);

	/** OnBack runs for Escape / B while the modal is open (unbound = cannot be dismissed). */
	void ShowModal(TSharedRef<SWidget> Content, FSimpleDelegate OnBack);
	void CloseModal();
	bool HasModal() const;

	void SetTitleVisible(bool bVisible);
	void FadeFromBlack(float Seconds);
	void FadeToBlack(float Seconds);

private:
	TSharedRef<SWidget> BuildExplorationHud();
	TSharedRef<SWidget> BuildFeedbackLayer();
	TSharedRef<SWidget> BuildTitle();
	TSharedRef<SWidget> BuildPanel(EHHLobbyScreen Screen);
	TSharedRef<SWidget> BuildCrewStrip();

	bool IsExploring() const;
	FText GetInteractKeyText() const;

	TWeakObjectPtr<AHHLobbyHUD> HUD;

	TSharedPtr<SBox> MenuHost;
	TSharedPtr<SBox> PanelHost;
	TSharedPtr<SOverlay> ModalLayer;
	TSharedPtr<SVerticalBox> ToastBox;
	TSharedPtr<SVerticalBox> CrewStripBox;

	EHHLobbyScreen CurrentScreen = EHHLobbyScreen::None;
	FCurveSequence PanelIntro;
	FCurveSequence MenuIntro;

	struct FToast
	{
		TSharedPtr<SWidget> Widget;
		double Created = 0.0;
		double Expires = 0.0;
	};
	TArray<FToast> Toasts;

	FText CaptionText;
	double CaptionExpires = 0.0;
	bool bCaptionIsSound = false;

	bool bTitleVisible = false;
	double TitleShownAt = 0.0;

	float Fade = 0.f;
	float FadeTarget = 0.f;
	float FadeSpeed = 1.f;

	double Now = 0.0;
	int32 CrewSignature = INDEX_NONE;
	FSimpleDelegate ModalBack;
};
