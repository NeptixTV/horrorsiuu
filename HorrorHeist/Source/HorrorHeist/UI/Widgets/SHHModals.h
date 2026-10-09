#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SEditableTextBox;

DECLARE_DELEGATE_OneParam(FHHOnTextSubmitted, const FText&);

/** Title + body + OK. */
class SHHMessageModal : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHMessageModal) {}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, Body)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	FSimpleDelegate OnClose;
};

/** Title + body + confirm / cancel. */
class SHHConfirmModal : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHConfirmModal) {}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, Body)
		SLATE_ARGUMENT(FText, ConfirmLabel)
		SLATE_EVENT(FSimpleDelegate, OnConfirm)
		SLATE_EVENT(FSimpleDelegate, OnCancel)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FSimpleDelegate OnConfirm;
	FSimpleDelegate OnCancel;
};

/** Title + body + single line text field (alias, IP address). */
class SHHTextEntryModal : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHTextEntryModal)
		: _MaxLength(32)
		, _CanCancel(true)
	{}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, Body)
		SLATE_ARGUMENT(FText, InitialText)
		SLATE_ARGUMENT(FText, ConfirmLabel)
		SLATE_ARGUMENT(int32, MaxLength)
		SLATE_ARGUMENT(bool, CanCancel)
		SLATE_EVENT(FHHOnTextSubmitted, OnSubmit)
		SLATE_EVENT(FSimpleDelegate, OnCancel)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void Submit();

	TSharedPtr<SEditableTextBox> TextBox;
	FHHOnTextSubmitted OnSubmit;
	FSimpleDelegate OnCancel;
	int32 MaxLength = 32;
};
