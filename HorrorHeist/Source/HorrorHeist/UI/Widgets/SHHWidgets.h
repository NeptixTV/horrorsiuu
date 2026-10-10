#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Core/HHTypes.h"
#include "UI/HHStyle.h"

class SButton;
class UTexture2D;
class UHHItemDefinition;
struct FSlateBrush;

/** Shared helpers for the lobby UI widgets. */
namespace HHUI
{
	/** World context used to reach the audio subsystem from Slate. Set by the HUD. */
	void SetContext(UObject* Context);
	UObject* GetContext();
	void PlayUISound(EHHUISound Sound);

	TSharedRef<SWidget> SectionLabel(const FText& Text);
	TSharedRef<SWidget> PanelHeader(const FText& Index, const FText& Title, const FText& Subtitle);
	TSharedRef<SWidget> Pill(const FText& Text, const FLinearColor& Color);
	TSharedRef<SWidget> StatBar(const FText& Label, float Value, float Max, bool bLowerIsBetter);
	TSharedRef<SWidget> Divider(float Alpha = 1.f);
	/**
	 * Standard screen layout: the left part stays open so the station camera shot shows
	 * through (optionally hosting LeftArea), the right part is a blurred glass panel.
	 */
	TSharedRef<SWidget> PanelFrame(TSharedRef<SWidget> Header, TSharedRef<SWidget> Body, TSharedRef<SWidget> Footer, TSharedPtr<SWidget> LeftArea = nullptr, float PanelFraction = 0.58f);

	/** Brush for a texture; the returned brush must be kept alive by the caller. */
	TSharedPtr<FSlateBrush> MakeTextureBrush(UTexture2D* Texture, const FVector2D& Size);
}

/** Main menu entry: "01  PLAY" with an amber rule that grows on hover/selection. */
class SHHNavButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHNavButton) : _IsSelected(false) {}
		SLATE_ARGUMENT(FText, Index)
		SLATE_ARGUMENT(FText, Label)
		SLATE_ATTRIBUTE(bool, IsSelected)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	TSharedPtr<SButton> GetButton() const { return Button; }

private:
	float GetEmphasis() const;

	TSharedPtr<SButton> Button;
	TAttribute<bool> IsSelected;
	FSimpleDelegate OnClicked;
	float Hover = 0.f;
};

enum class EHHButtonKind : uint8
{
	Primary,
	Secondary,
	Danger,
	Ghost
};

/** Rectangular call-to-action button. */
class SHHActionButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHActionButton)
		: _Kind(EHHButtonKind::Secondary)
		, _MinWidth(0.f)
		, _Height(42.f)
		, _ClickSound(EHHUISound::Click)
	{}
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ARGUMENT(EHHButtonKind, Kind)
		SLATE_ARGUMENT(float, MinWidth)
		SLATE_ARGUMENT(float, Height)
		SLATE_ARGUMENT(EHHUISound, ClickSound)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FSimpleDelegate OnClicked;
	EHHUISound ClickSound = EHHUISound::Click;
};

/** "[ESC]  Back" */
class SHHKeyHint : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHKeyHint) {}
		SLATE_ATTRIBUTE(FText, Key)
		SLATE_ATTRIBUTE(FText, Label)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

DECLARE_DELEGATE_OneParam(FHHOnIndexSelected, int32);

/** Text tabs with a sliding amber underline. */
class SHHTabBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHTabBar) {}
		SLATE_ARGUMENT(TArray<FText>, Tabs)
		SLATE_ATTRIBUTE(int32, Selected)
		SLATE_EVENT(FHHOnIndexSelected, OnSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TAttribute<int32> Selected;
	FHHOnIndexSelected OnSelected;
};

/** Settings row with a left/right selector: "Shadows      < High >". */
class SHHOptionRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHOptionRow) {}
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, Tooltip)
		SLATE_ARGUMENT(TArray<FText>, Options)
		SLATE_ATTRIBUTE(int32, Value)
		SLATE_EVENT(FHHOnIndexSelected, OnChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void Step(int32 Direction);

	TArray<FText> Options;
	TAttribute<int32> Value;
	FHHOnIndexSelected OnChanged;
};

DECLARE_DELEGATE_OneParam(FHHOnFloatChanged, float);

/** Settings row with a slider and a formatted value. */
class SHHSliderRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHSliderRow)
		: _Min(0.f)
		, _Max(1.f)
		, _Step(0.01f)
		, _DisplayScale(100.f)
		, _DisplayDecimals(0)
	{}
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, Tooltip)
		SLATE_ARGUMENT(float, Min)
		SLATE_ARGUMENT(float, Max)
		SLATE_ARGUMENT(float, Step)
		/** Value shown = value * DisplayScale (100 -> percent). */
		SLATE_ARGUMENT(float, DisplayScale)
		SLATE_ARGUMENT(int32, DisplayDecimals)
		SLATE_ARGUMENT(FText, Suffix)
		SLATE_ATTRIBUTE(float, Value)
		SLATE_EVENT(FHHOnFloatChanged, OnChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TAttribute<float> Value;
	FHHOnFloatChanged OnChanged;
	float Min = 0.f;
	float Max = 1.f;
};

/** Store / wardrobe / locker tile. */
class SHHItemCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHItemCard)
		: _Item(nullptr)
		, _Icon(nullptr)
		, _Size(132.f)
		, _IsSelected(false)
		, _IsEquipped(false)
		, _IsLocked(false)
	{}
		SLATE_ARGUMENT(const UHHItemDefinition*, Item)
		SLATE_ARGUMENT(UTexture2D*, Icon)
		SLATE_ARGUMENT(float, Size)
		/** Shown when there is no item (e.g. "None"). */
		SLATE_ARGUMENT(FText, EmptyLabel)
		SLATE_ATTRIBUTE(bool, IsSelected)
		SLATE_ATTRIBUTE(bool, IsEquipped)
		SLATE_ATTRIBUTE(bool, IsLocked)
		SLATE_ATTRIBUTE(FText, Badge)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<FSlateBrush> IconBrush;
	TAttribute<bool> IsSelected;
	TAttribute<bool> IsEquipped;
	TAttribute<bool> IsLocked;
	FSimpleDelegate OnClicked;
};
