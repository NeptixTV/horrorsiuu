#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AHHLobbyHUD;
class SBox;
class SVerticalBox;
class UHHGameUserSettings;

/**
 * Settings: graphics, audio, controls (with key rebinding), gameplay and accessibility.
 * Every option writes UHHGameUserSettings and is applied immediately, except display mode
 * and resolution, which use the engine's apply / confirm / revert flow.
 */
class SHHSettingsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHSettingsPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void SetTab(int32 NewTab);
	TSharedRef<SWidget> BuildGraphics();
	TSharedRef<SWidget> BuildAudio();
	TSharedRef<SWidget> BuildControls();
	TSharedRef<SWidget> BuildGameplay();
	TSharedRef<SWidget> BuildAccessibility();

	/** Applies a change to the settings object, applies non-resolution settings and saves. */
	void Change(TFunctionRef<void(UHHGameUserSettings&)> Mutator);
	void ApplyDisplay();
	void RestoreDefaults();

	TSharedRef<SWidget> Toggle(const FText& Label, const FText& Tooltip, TFunction<bool(const UHHGameUserSettings&)> Get, TFunction<void(UHHGameUserSettings&, bool)> Set);
	TSharedRef<SWidget> Quality(const FText& Label, const FText& Tooltip, TFunction<int32(const UHHGameUserSettings&)> Get, TFunction<void(UHHGameUserSettings&, int32)> Set);
	TSharedRef<SWidget> Slider(const FText& Label, const FText& Tooltip, float Min, float Max, float Step, float DisplayScale, int32 Decimals, const FText& Suffix,
		TFunction<float(const UHHGameUserSettings&)> Get, TFunction<void(UHHGameUserSettings&, float)> Set);

	TWeakObjectPtr<AHHLobbyHUD> HUD;
	TSharedPtr<SBox> TabHost;
	int32 Tab = 0;

	TArray<FIntPoint> Resolutions;
	int32 PendingResolution = INDEX_NONE;
	int32 PendingWindowMode = INDEX_NONE;
};

/** One row in the key binding list: click, then press the new key (Escape cancels). */
class SHHKeyBindRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHKeyBindRow) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
		SLATE_ARGUMENT(FName, BindingId)
		SLATE_ARGUMENT(FText, Label)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;

private:
	void Commit(const FKey& Key);
	FText GetKeyText() const;

	TWeakObjectPtr<AHHLobbyHUD> HUD;
	FName BindingId;
	bool bListening = false;
};
