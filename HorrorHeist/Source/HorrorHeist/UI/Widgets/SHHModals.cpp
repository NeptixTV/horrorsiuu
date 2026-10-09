#include "UI/Widgets/SHHModals.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHModals"

namespace
{
	TSharedRef<SWidget> ModalFrame(const FText& Title, const FText& Body, TSharedRef<SWidget> Extra, TSharedRef<SWidget> Buttons)
	{
		return SNew(SBox)
			.WidthOverride(560.f)
			[
				SNew(SBorder)
				.BorderImage(FHHStyle::Brush("HH.PanelFrame"))
				.Padding(FMargin(40.f, 34.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).WidthOverride(40.f).HeightOverride(2.f).HAlign(HAlign_Left)
						[
							SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Title)
						.Font(FHHStyle::Font(EHHFont::Display, 26.f, 120))
						.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Body)
						.Font(FHHStyle::Font(EHHFont::Body, 15.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 20.f, 0.f, 0.f)
					[
						Extra
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 26.f, 0.f, 0.f)
					[
						Buttons
					]
				]
			];
	}
}

// ---------------------------------------------------------------------------------------

void SHHMessageModal::Construct(const FArguments& InArgs)
{
	OnClose = InArgs._OnClose;
	ChildSlot
	[
		ModalFrame(InArgs._Title, InArgs._Body, SNullWidget::NullWidget,
			SNew(SHHActionButton)
			.Text(LOCTEXT("Ok", "UNDERSTOOD"))
			.Kind(EHHButtonKind::Primary)
			.MinWidth(160.f)
			.ClickSound(EHHUISound::Confirm)
			.OnClicked(OnClose))
	];
}

FReply SHHMessageModal::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Bottom)
	{
		OnClose.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

// ---------------------------------------------------------------------------------------

void SHHConfirmModal::Construct(const FArguments& InArgs)
{
	OnConfirm = InArgs._OnConfirm;
	OnCancel = InArgs._OnCancel;

	ChildSlot
	[
		ModalFrame(InArgs._Title, InArgs._Body, SNullWidget::NullWidget,
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SHHActionButton)
				.Text(LOCTEXT("Cancel", "CANCEL"))
				.Kind(EHHButtonKind::Ghost)
				.ClickSound(EHHUISound::Back)
				.OnClicked(OnCancel)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SHHActionButton)
				.Text(InArgs._ConfirmLabel)
				.Kind(EHHButtonKind::Primary)
				.MinWidth(160.f)
				.ClickSound(EHHUISound::Confirm)
				.OnClicked(OnConfirm)
			])
	];
}

// ---------------------------------------------------------------------------------------

void SHHTextEntryModal::Construct(const FArguments& InArgs)
{
	OnSubmit = InArgs._OnSubmit;
	OnCancel = InArgs._OnCancel;
	MaxLength = FMath::Max(1, InArgs._MaxLength);

	static const FEditableTextBoxStyle FieldStyle = FEditableTextBoxStyle()
		.SetBackgroundImageNormal(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.04f)))
		.SetBackgroundImageHovered(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.06f)))
		.SetBackgroundImageFocused(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.08f)))
		.SetBackgroundImageReadOnly(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.02f)))
		.SetPadding(FMargin(14.f, 11.f));

	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (InArgs._CanCancel)
	{
		Buttons->AddSlot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
		[
			SNew(SHHActionButton)
			.Text(LOCTEXT("Cancel", "CANCEL"))
			.Kind(EHHButtonKind::Ghost)
			.ClickSound(EHHUISound::Back)
			.OnClicked(OnCancel)
		];
	}
	Buttons->AddSlot().AutoWidth()
	[
		SNew(SHHActionButton)
		.Text(InArgs._ConfirmLabel)
		.Kind(EHHButtonKind::Primary)
		.MinWidth(160.f)
		.ClickSound(EHHUISound::Confirm)
		.OnClicked_Lambda([this]() { Submit(); })
	];

	ChildSlot
	[
		ModalFrame(InArgs._Title, InArgs._Body,
			SNew(SBorder)
			.BorderImage(FHHStyle::Brush("HH.Outline"))
			.Padding(FMargin(0.f))
			[
				SAssignNew(TextBox, SEditableTextBox)
				.Style(&FieldStyle)
				.Text(InArgs._InitialText)
				.Font(FHHStyle::Font(EHHFont::MonoMedium, 18.f))
				.ForegroundColor(FSlateColor(FHHStyle::Text()))
				.SelectAllTextWhenFocused(true)
				.ClearKeyboardFocusOnCommit(false)
				.OnTextChanged_Lambda([this](const FText& NewText)
				{
					const FString Value = NewText.ToString();
					if (Value.Len() > MaxLength)
					{
						TextBox->SetText(FText::FromString(Value.Left(MaxLength)));
					}
				})
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter)
					{
						Submit();
					}
				})
			],
			Buttons)
	];

	// Focus the field once the modal is in the widget tree.
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		if (TextBox.IsValid())
		{
			FSlateApplication::Get().SetKeyboardFocus(TextBox, EFocusCause::SetDirectly);
		}
		return EActiveTimerReturnType::Stop;
	}));
}

void SHHTextEntryModal::Submit()
{
	const FText Value = FText::FromString(TextBox.IsValid() ? TextBox->GetText().ToString().TrimStartAndEnd() : FString());
	if (Value.IsEmpty())
	{
		HHUI::PlaySound(EHHUISound::Error);
		return;
	}
	OnSubmit.ExecuteIfBound(Value);
}

#undef LOCTEXT_NAMESPACE
