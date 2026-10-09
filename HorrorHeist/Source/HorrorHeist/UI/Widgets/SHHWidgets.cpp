#include "UI/Widgets/SHHWidgets.h"
#include "Audio/HHAudioSubsystem.h"
#include "Data/HHItemDefinition.h"
#include "Brushes/SlateImageBrush.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHWidgets"

namespace HHUI
{
	static TWeakObjectPtr<UObject> GContext;

	void SetContext(UObject* Context)
	{
		GContext = Context;
	}

	UObject* GetContext()
	{
		return GContext.Get();
	}

	void PlaySound(EHHUISound Sound)
	{
		if (UObject* Context = GContext.Get())
		{
			if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(Context))
			{
				Audio->PlayUI(Sound);
			}
		}
	}

	TSharedRef<SWidget> SectionLabel(const FText& Text)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 12.f, 220))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
			[
				Divider(0.7f)
			];
	}

	TSharedRef<SWidget> Divider(float Alpha)
	{
		return SNew(SBox)
			.HeightOverride(1.f)
			[
				SNew(SImage)
				.Image(FHHStyle::Brush("HH.White"))
				.ColorAndOpacity(FSlateColor(FHHStyle::WithAlpha(FLinearColor::White, 0.08f * Alpha)))
			];
	}

	TSharedRef<SWidget> PanelHeader(const FText& Index, const FText& Title, const FText& Subtitle)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.f, 0.f, 14.f, 7.f)
				[
					SNew(STextBlock)
					.Text(Index)
					.Font(FHHStyle::Font(EHHFont::MonoMedium, 13.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
				[
					SNew(STextBlock)
					.Text(Title)
					.Font(FHHStyle::Font(EHHFont::Display, 38.f, 90))
					.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Subtitle)
				.Font(FHHStyle::Font(EHHFont::Typewriter, 14.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
			[
				Divider(1.4f)
			];
	}

	TSharedRef<SWidget> Pill(const FText& Text, const FLinearColor& Color)
	{
		return SNew(SBorder)
			.BorderImage(FHHStyle::Brush("HH.Pill"))
			.BorderBackgroundColor(FSlateColor(FHHStyle::WithAlpha(Color, 0.16f)))
			.Padding(FMargin(9.f, 2.f))
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 11.f, 120))
				.ColorAndOpacity(FSlateColor(Color))
			];
	}

	TSharedRef<SWidget> StatBar(const FText& Label, float Value, float Max, bool bLowerIsBetter)
	{
		const float Ratio = Max > 0.f ? FMath::Clamp(Value / Max, 0.f, 1.f) : 0.f;
		const float Goodness = bLowerIsBetter ? 1.f - Ratio : Ratio;
		const FLinearColor BarColor = FMath::Lerp(FHHStyle::Danger(), FHHStyle::Accent(), FMath::Clamp(Goodness * 1.4f, 0.f, 1.f));

		FNumberFormattingOptions Format;
		Format.SetMaximumFractionalDigits(Value < 10.f ? 1 : 0);

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).Text(Label).Font(FHHStyle::Font(EHHFont::Condensed, 13.f, 60)).ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::AsNumber(Value, &Format)).Font(FHHStyle::Font(EHHFont::Mono, 12.f)).ColorAndOpacity(FSlateColor(FHHStyle::Text()))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f)
			[
				SNew(SBox).HeightOverride(3.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(FMath::Max(Ratio, 0.001f))
					[
						SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(BarColor))
					]
					+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.f - Ratio, 0.001f))
					[
						SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.08f)))
					]
				]
			];
	}

	TSharedRef<SWidget> PanelFrame(TSharedRef<SWidget> Header, TSharedRef<SWidget> Body, TSharedRef<SWidget> Footer, TSharedPtr<SWidget> LeftArea, float PanelFraction)
	{
		const float Panel = FMath::Clamp(PanelFraction, 0.3f, 0.9f);
		TSharedRef<SWidget> Left = LeftArea.IsValid() ? LeftArea.ToSharedRef() : StaticCastSharedRef<SWidget>(SNew(SSpacer));

		return SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Right)
			[
				SNew(SBox)
				.WidthOverride(1400.f)
				[
					SNew(SImage)
					.Image(FHHStyle::Brush("HH.GradientRight"))
					.Visibility(EVisibility::HitTestInvisible)
				]
			]
			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f - Panel)
				[
					Left
				]
				+ SHorizontalBox::Slot().FillWidth(Panel)
				[
					SNew(SBackgroundBlur)
					.BlurStrength(6.f)
					.Padding(FMargin(0.f))
					[
						SNew(SBorder)
						.BorderImage(FHHStyle::Brush("HH.Panel"))
						.Padding(FMargin(56.f, 46.f, 64.f, 34.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								Header
							]
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 24.f, 0.f, 0.f)
							[
								Body
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 20.f, 0.f, 0.f)
							[
								Divider(1.f)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
							[
								Footer
							]
						]
					]
				]
			];
	}

	TSharedPtr<FSlateBrush> MakeTextureBrush(UTexture2D* Texture, const FVector2D& Size)
	{
		TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
		Brush->DrawAs = ESlateBrushDrawType::Image;
		Brush->ImageSize = Size;
		if (Texture)
		{
			Brush->SetResourceObject(Texture);
		}
		else
		{
			Brush->DrawAs = ESlateBrushDrawType::NoDrawType;
		}
		return Brush;
	}
}

// ---------------------------------------------------------------------------------------
// SHHNavButton

void SHHNavButton::Construct(const FArguments& InArgs)
{
	IsSelected = InArgs._IsSelected;
	OnClicked = InArgs._OnClicked;

	ChildSlot
	[
		SAssignNew(Button, SButton)
		.ButtonStyle(&FHHStyle::Button("HH.Button.Invisible"))
		.ContentPadding(FMargin(0.f, 7.f))
		.OnHovered_Lambda([]() { HHUI::PlaySound(EHHUISound::Hover); })
		.OnClicked_Lambda([this]()
		{
			HHUI::PlaySound(EHHUISound::Click);
			OnClicked.ExecuteIfBound();
			return FReply::Handled();
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f, 6.f)
			[
				SNew(STextBlock)
				.Text(InArgs._Index)
				.Font(FHHStyle::Font(EHHFont::Mono, 11.f))
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(FMath::Lerp(FHHStyle::TextFaint(), FHHStyle::Accent(), GetEmphasis())); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(InArgs._Label)
					.Font(FHHStyle::Font(EHHFont::Display, 29.f, 150))
					.ColorAndOpacity_Lambda([this]() { return FSlateColor(FMath::Lerp(FHHStyle::TextDim(), FHHStyle::Text(), GetEmphasis())); })
					.RenderTransform_Lambda([this]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(GetEmphasis() * 10.f, 0.f))); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f).HAlign(HAlign_Left)
				[
					SNew(SBox)
					.HeightOverride(1.f)
					.WidthOverride_Lambda([this]() { return FOptionalSize(6.f + 70.f * GetEmphasis()); })
					[
						SNew(SImage)
						.Image(FHHStyle::Brush("HH.White"))
						.ColorAndOpacity_Lambda([this]() { return FSlateColor(FHHStyle::WithAlpha(FHHStyle::Accent(), 0.25f + 0.75f * GetEmphasis())); })
					]
				]
			]
		]
	];
}

void SHHNavButton::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const bool bActive = Button.IsValid() && (Button->IsHovered() || Button->HasKeyboardFocus());
	Hover = FMath::FInterpTo(Hover, bActive ? 1.f : 0.f, InDeltaTime, 14.f);
}

float SHHNavButton::GetEmphasis() const
{
	return FMath::Max(Hover, IsSelected.Get() ? 1.f : 0.f);
}

// ---------------------------------------------------------------------------------------
// SHHActionButton

void SHHActionButton::Construct(const FArguments& InArgs)
{
	OnClicked = InArgs._OnClicked;
	ClickSound = InArgs._ClickSound;

	FName StyleName = TEXT("HH.Button.Secondary");
	FLinearColor TextColor = FHHStyle::Text();
	switch (InArgs._Kind)
	{
	case EHHButtonKind::Primary:	StyleName = TEXT("HH.Button.Primary"); TextColor = FHHStyle::Ink(); break;
	case EHHButtonKind::Danger:		StyleName = TEXT("HH.Button.Danger"); TextColor = FHHStyle::Text(); break;
	case EHHButtonKind::Ghost:		StyleName = TEXT("HH.Button.Ghost"); TextColor = FHHStyle::TextDim(); break;
	default: break;
	}

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(InArgs._MinWidth)
		.HeightOverride(InArgs._Height)
		[
			SNew(SButton)
			.ButtonStyle(&FHHStyle::Button(StyleName))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(20.f, 0.f))
			.OnHovered_Lambda([]() { HHUI::PlaySound(EHHUISound::Hover); })
			.OnClicked_Lambda([this]()
			{
				HHUI::PlaySound(ClickSound);
				OnClicked.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(InArgs._Text)
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 15.f, 170))
				.ColorAndOpacity(FSlateColor(TextColor))
			]
		]
	];
}

// ---------------------------------------------------------------------------------------
// SHHKeyHint

void SHHKeyHint::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FHHStyle::Brush("HH.KeyCap"))
			.Padding(FMargin(7.f, 2.f))
			[
				SNew(STextBlock)
				.Text(InArgs._Key)
				.Font(FHHStyle::Font(EHHFont::MonoMedium, 11.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(InArgs._Label)
			.Font(FHHStyle::Font(EHHFont::Condensed, 13.f, 80))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		]
	];
}

// ---------------------------------------------------------------------------------------
// SHHTabBar

void SHHTabBar::Construct(const FArguments& InArgs)
{
	Selected = InArgs._Selected;
	OnSelected = InArgs._OnSelected;

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < InArgs._Tabs.Num(); ++Index)
	{
		Row->AddSlot()
		.AutoWidth()
		.Padding(0.f, 0.f, 26.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(&FHHStyle::Button("HH.Button.Invisible"))
			.ContentPadding(FMargin(0.f, 4.f))
			.OnHovered_Lambda([]() { HHUI::PlaySound(EHHUISound::Hover); })
			.OnClicked_Lambda([this, Index]()
			{
				if (Selected.Get() != Index)
				{
					HHUI::PlaySound(EHHUISound::Click);
					OnSelected.ExecuteIfBound(Index);
				}
				return FReply::Handled();
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(InArgs._Tabs[Index])
					.Font(FHHStyle::Font(EHHFont::CondensedSemi, 14.f, 200))
					.ColorAndOpacity_Lambda([this, Index]() { return FSlateColor(Selected.Get() == Index ? FHHStyle::Text() : FHHStyle::TextFaint()); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(SBox)
					.HeightOverride(2.f)
					[
						SNew(SImage)
						.Image(FHHStyle::Brush("HH.White"))
						.ColorAndOpacity_Lambda([this, Index]() { return FSlateColor(Selected.Get() == Index ? FHHStyle::Accent() : FLinearColor::Transparent); })
					]
				]
			]
		];
	}

	ChildSlot
	[
		Row
	];
}

// ---------------------------------------------------------------------------------------
// SHHOptionRow

void SHHOptionRow::Construct(const FArguments& InArgs)
{
	Options = InArgs._Options;
	Value = InArgs._Value;
	OnChanged = InArgs._OnChanged;

	auto Arrow = [this](const FName& IconName, int32 Direction) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(&FHHStyle::Button("HH.Button.Ghost"))
			.ContentPadding(FMargin(8.f, 6.f))
			.IsFocusable(false)
			.OnHovered_Lambda([]() { HHUI::PlaySound(EHHUISound::Hover); })
			.OnClicked_Lambda([this, Direction]() { Step(Direction); return FReply::Handled(); })
			[
				SNew(SImage)
				.Image(FHHStyle::Brush(IconName))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
				.DesiredSizeOverride(FVector2D(12.0, 12.0))
			];
	};

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FHHStyle::Brush("HH.White"))
		.BorderBackgroundColor_Lambda([this]() { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, IsHovered() || HasKeyboardFocus() ? 0.035f : 0.f)); })
		.Padding(FMargin(12.f, 4.f))
		.ToolTipText(InArgs._Tooltip)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InArgs._Label)
				.Font(FHHStyle::Font(EHHFont::Body, 15.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Arrow(TEXT("HH.Icon.ChevronLeft"), -1)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(170.f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const int32 Index = Value.Get();
						return Options.IsValidIndex(Index) ? Options[Index] : LOCTEXT("Custom", "Custom");
					})
					.Font(FHHStyle::Font(EHHFont::CondensedSemi, 15.f, 120))
					.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Arrow(TEXT("HH.Icon.ChevronRight"), 1)
			]
		]
	];
}

void SHHOptionRow::Step(int32 Direction)
{
	if (Options.Num() == 0)
	{
		return;
	}
	const int32 Current = Value.Get();
	const int32 Next = Options.IsValidIndex(Current)
		? (Current + Direction + Options.Num()) % Options.Num()
		: (Direction > 0 ? 0 : Options.Num() - 1);
	HHUI::PlaySound(EHHUISound::Click);
	OnChanged.ExecuteIfBound(Next);
}

FReply SHHOptionRow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::A)
	{
		Step(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::D)
	{
		Step(1);
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

// ---------------------------------------------------------------------------------------
// SHHSliderRow

void SHHSliderRow::Construct(const FArguments& InArgs)
{
	Value = InArgs._Value;
	OnChanged = InArgs._OnChanged;
	Min = InArgs._Min;
	Max = FMath::Max(InArgs._Max, InArgs._Min + KINDA_SMALL_NUMBER);
	const float DisplayScale = InArgs._DisplayScale;
	const int32 Decimals = InArgs._DisplayDecimals;
	const FText Suffix = InArgs._Suffix;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FHHStyle::Brush("HH.White"))
		.BorderBackgroundColor_Lambda([this]() { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, IsHovered() ? 0.035f : 0.f)); })
		.Padding(FMargin(12.f, 8.f))
		.ToolTipText(InArgs._Tooltip)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InArgs._Label)
				.Font(FHHStyle::Font(EHHFont::Body, 15.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(190.f)
				[
					SNew(SSlider)
					.Style(&FHHStyle::Slider())
					.MinValue(Min)
					.MaxValue(Max)
					.StepSize(InArgs._Step)
					.Value(Value)
					.OnValueChanged_Lambda([this](float NewValue) { OnChanged.ExecuteIfBound(NewValue); })
					.OnMouseCaptureEnd_Lambda([]() { HHUI::PlaySound(EHHUISound::Click); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(58.f)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text_Lambda([this, DisplayScale, Decimals, Suffix]()
					{
						FNumberFormattingOptions Format;
						Format.SetMinimumFractionalDigits(Decimals);
						Format.SetMaximumFractionalDigits(Decimals);
						const FText Number = FText::AsNumber(Value.Get() * DisplayScale, &Format);
						return Suffix.IsEmpty() ? Number : FText::Format(LOCTEXT("ValueSuffix", "{0}{1}"), Number, Suffix);
					})
					.Font(FHHStyle::Font(EHHFont::Mono, 13.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
				]
			]
		]
	];
}

// ---------------------------------------------------------------------------------------
// SHHItemCard

void SHHItemCard::Construct(const FArguments& InArgs)
{
	IsSelected = InArgs._IsSelected;
	IsEquipped = InArgs._IsEquipped;
	IsLocked = InArgs._IsLocked;
	OnClicked = InArgs._OnClicked;

	const UHHItemDefinition* Item = InArgs._Item;
	const float Size = InArgs._Size;
	IconBrush = HHUI::MakeTextureBrush(InArgs._Icon, FVector2D(Size - 26.f, Size - 26.f));

	const FText Name = Item ? Item->DisplayName : InArgs._EmptyLabel;
	const FLinearColor RarityColor = Item ? HHText::RarityColor(Item->Rarity) : FHHStyle::TextFaint();
	const TAttribute<FText> Badge = InArgs._Badge;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(Size)
		.HeightOverride(Size + 34.f)
		[
			SNew(SButton)
			.ButtonStyle(&FHHStyle::Button("HH.Button.Card"))
			.ContentPadding(FMargin(0.f))
			.OnHovered_Lambda([]() { HHUI::PlaySound(EHHUISound::Hover); })
			.OnClicked_Lambda([this]()
			{
				HHUI::PlaySound(EHHUISound::Click);
				OnClicked.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(SOverlay)
				// Icon + name
				+ SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().FillHeight(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(8.f, 12.f, 8.f, 0.f)
					[
						SNew(SImage)
						.Image(IconBrush.Get())
						.ColorAndOpacity_Lambda([this]() { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, IsLocked.Get() ? 0.35f : 1.f)); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 4.f, 10.f, 10.f)
					[
						SNew(STextBlock)
						.Text(Name)
						.Font(FHHStyle::Font(EHHFont::BodyMedium, 12.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]
				]
				// Rarity rule along the top edge
				+ SOverlay::Slot().VAlign(VAlign_Top)
				[
					SNew(SBox)
					.HeightOverride(2.f)
					[
						SNew(SImage)
						.Image(FHHStyle::Brush("HH.White"))
						.ColorAndOpacity(FSlateColor(FHHStyle::WithAlpha(RarityColor, Item ? 0.85f : 0.f)))
					]
				]
				// Badge (price / owned / level)
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(6.f, 8.f)
				[
					SNew(STextBlock)
					.Text(Badge)
					.Font(FHHStyle::Font(EHHFont::Mono, 10.f))
					.ColorAndOpacity_Lambda([this]() { return FSlateColor(IsLocked.Get() ? FHHStyle::TextFaint() : FHHStyle::Accent()); })
				]
				// Equipped tick
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(7.f, 8.f)
				[
					SNew(SImage)
					.Image(FHHStyle::Brush("HH.Icon.Check"))
					.DesiredSizeOverride(FVector2D(14.0, 14.0))
					.ColorAndOpacity(FSlateColor(FHHStyle::Ready()))
					.Visibility_Lambda([this]() { return IsEquipped.Get() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				]
				// Lock
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FHHStyle::Brush("HH.Icon.Lock"))
					.DesiredSizeOverride(FVector2D(22.0, 22.0))
					.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
					.Visibility_Lambda([this]() { return IsLocked.Get() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				]
				// Selection frame
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FHHStyle::Brush("HH.OutlineAccent"))
					.Visibility_Lambda([this]() { return IsSelected.Get() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
