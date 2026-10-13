#include "UI/Widgets/SHHMissionSummary.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Data/HHItemDefinition.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHMissionDefinition.h"
#include "Player/HHPlayerState.h"
#include "Styling/StyleDefaults.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHMissionSummary"

namespace HHSummary
{
	static float EaseOut(float T)
	{
		const float Clamped = FMath::Clamp(T, 0.f, 1.f);
		return 1.f - FMath::Pow(1.f - Clamped, 3.f);
	}

	static FText Duration(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::RoundToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
	}

	static FLinearColor FateColor(EHHCrewFate Fate)
	{
		switch (Fate)
		{
		case EHHCrewFate::Escaped:		return FHHStyle::Ready();
		case EHHCrewFate::LeftBehind:	return FHHStyle::Accent();
		default:						return FHHStyle::Danger();
		}
	}
}

void SHHMissionSummary::Construct(const FArguments& InArgs)
{
	Summary = InArgs._Summary;
	HUD = InArgs._HUD;

	const bool bGood = Summary.Outcome == EHHMissionOutcome::Success;
	const bool bBad = Summary.Outcome == EHHMissionOutcome::Caught || Summary.Outcome == EHHMissionOutcome::ChaseBusted || Summary.Outcome == EHHMissionOutcome::Abandoned;
	OutcomeColor = bGood ? FHHStyle::Ready() : (bBad ? FHHStyle::Danger() : FHHStyle::Accent());

	// Timeline: the loot list sets how long the money counts.
	const int32 Shown = FMath::Min(Summary.Loot.Num(), 10);
	MoneyEnd = FMath::Max(LootStart + Shown * LootStep + 0.4f, 3.6f);
	CutAt = MoneyEnd + 0.3f;
	CrewStart = CutAt + 2.1f;
	XPAt = CrewStart + 0.4f * FMath::Max(1, Summary.Crew.Num()) + 0.6f;

	Cues.Add({ 1.f, bBad ? EHHUISound::Error : EHHUISound::ObjectiveComplete });
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		Cues.Add({ LootStart + Index * LootStep, EHHUISound::CountdownTick });
	}
	if (Summary.PayoutPerPlayer > 0)
	{
		Cues.Add({ CutAt + 1.6f, EHHUISound::Purchase });
	}
	for (int32 Index = 0; Index < Summary.Crew.Num(); ++Index)
	{
		Cues.Add({ CrewStart + Index * 0.4f, EHHUISound::Click });
	}
	Cues.Add({ XPAt + 1.f, EHHUISound::LevelUp });

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FHHStyle::Brush("HH.White"))
			.ColorAndOpacity_Lambda([this]() { return FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.86f * Progress(0.f, 0.6f))); })
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(1100.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					BuildHeader()
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 28.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					// Left: the haul and the money.
					+ SHorizontalBox::Slot().FillWidth(0.55f).Padding(0.f, 0.f, 40.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							BuildLoot()
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
						[
							BuildMoney()
						]
					]
					// Right: the crew.
					+ SHorizontalBox::Slot().FillWidth(0.45f)
					[
						BuildCrew()
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 0.f)
				[
					Reveal(SNew(STextBlock)
						.Text(LOCTEXT("Returning", "Back to the hideout shortly..."))
						.Font(FHHStyle::Font(EHHFont::Condensed, 13.f, 200))
						.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint())), XPAt + 1.4f, 0.6f, FVector2D::ZeroVector)
				]
			]
		]
	];
}

float SHHMissionSummary::Progress(float Start, float Duration) const
{
	return HHSummary::EaseOut((Elapsed - Start) / FMath::Max(Duration, 0.01f));
}

void SHHMissionSummary::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Elapsed += InDeltaTime;
	for (FCue& Item : Cues)
	{
		if (!Item.bPlayed && Elapsed >= Item.At)
		{
			Item.bPlayed = true;
			HHUI::PlayUISound(Item.Sound);
		}
	}
}

TSharedRef<SWidget> SHHMissionSummary::Reveal(TSharedRef<SWidget> Content, float At, float Duration, FVector2D From) const
{
	return SNew(SBorder)
		.BorderImage(FStyleDefaults::GetNoBrush())
		.Padding(0.f)
		.ColorAndOpacity_Lambda([this, At, Duration]() { return FLinearColor(1.f, 1.f, 1.f, Progress(At, Duration)); })
		.RenderTransform_Lambda([this, At, Duration, From]()
		{
			const float T = Progress(At, Duration);
			return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(static_cast<float>(From.X) * (1.f - T), static_cast<float>(From.Y) * (1.f - T))));
		})
		[
			Content
		];
}

// ---------------------------------------------------------------------------------------

TSharedRef<SWidget> SHHMissionSummary::BuildHeader()
{
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	const UHHMissionDefinition* Job = Registry ? Registry->FindMission(Summary.MissionId) : nullptr;
	const FText JobName = Job ? Job->DisplayName.ToUpper() : LOCTEXT("TheJob", "THE JOB");
	const FText Where = HHText::IsGetaway(Summary.Outcome) && !Summary.SafehouseName.IsEmpty()
		? FText::Format(LOCTEXT("Where", "{0}  -  the van made it to {1}"), JobName, Summary.SafehouseName)
		: JobName;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			Reveal(SNew(STextBlock)
				.Text(Where)
				.Font(FHHStyle::Font(EHHFont::Condensed, 14.f, 400))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim())), 0.3f, 0.5f)
		]
		// The stamp: slams down from big and transparent.
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f).HAlign(HAlign_Left)
		[
			SNew(SBorder)
			.BorderImage(FHHStyle::Brush("HH.Outline"))
			.BorderBackgroundColor(FSlateColor(OutcomeColor))
			.Padding(FMargin(18.f, 4.f))
			.RenderTransformPivot(FVector2D(0.5, 0.5))
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, FMath::Clamp((Elapsed - 0.8f) / 0.2f, 0.f, 1.f)); })
			.RenderTransform_Lambda([this]()
			{
				const float T = FMath::Clamp((Elapsed - 0.8f) / 0.22f, 0.f, 1.f);
				const float Scale = FMath::Lerp(2.4f, 1.f, T * T);
				const TMatrix2x2<float> Matrix = TMatrix2x2<float>(TScale2<float>(Scale)).Concatenate(TMatrix2x2<float>(TQuat2<float>(FMath::DegreesToRadians(-4.f))));
				return TOptional<FSlateRenderTransform>(FSlateRenderTransform(Matrix));
			})
			[
				SNew(STextBlock)
				.Text(HHText::OutcomeName(Summary.Outcome).ToUpper())
				.Font(FHHStyle::Font(EHHFont::Display, 58.f, 200))
				.ColorAndOpacity(FSlateColor(OutcomeColor))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
		[
			Reveal(SNew(STextBlock)
				.Text(HHText::OutcomeLine(Summary.Outcome))
				.AutoWrapText(true)
				.Font(FHHStyle::Font(EHHFont::Typewriter, 16.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text())), 1.35f, 0.5f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			Reveal(SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("Facts", "{0} on the job  -  the house was {1}{2}"), HHSummary::Duration(Summary.DurationSeconds),
					Summary.HauntStage >= 4 ? LOCTEXT("Showed", "showing itself") : (Summary.HauntStage >= 3 ? LOCTEXT("Hostile", "hostile") : (Summary.HauntStage >= 1 ? LOCTEXT("Uneasy", "uneasy") : LOCTEXT("Quiet", "quiet"))),
					Summary.bPoliceCalled ? LOCTEXT("PoliceCalled", "  -  the police were called") : FText::GetEmpty()))
				.Font(FHHStyle::Font(EHHFont::Body, 14.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim())), 1.6f, 0.5f)
		];
}

TSharedRef<SWidget> SHHMissionSummary::BuildLoot()
{
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	List->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
	[
		Reveal(HHUI::SectionLabel(LOCTEXT("TheHaul", "THE HAUL")), LootStart - 0.2f, 0.4f)
	];

	const int32 Shown = FMath::Min(Summary.Loot.Num(), 10);
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		const FHHLootRecord& Record = Summary.Loot[Index];
		const UHHItemDefinition* Item = Registry ? Registry->FindItem(Record.ItemId) : nullptr;
		List->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
		[
			Reveal(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).Text(Item ? Item->DisplayName : FText::FromName(Record.ItemId)).Font(FHHStyle::Font(EHHFont::Body, 14.f)).ColorAndOpacity(FSlateColor(FHHStyle::Text()))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock).Text(FText::FromString(Record.StolenBy)).Font(FHHStyle::Font(EHHFont::Condensed, 12.f)).ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)
				[
					SNew(SBox).WidthOverride(90.f).HAlign(HAlign_Right)
					[
						SNew(STextBlock).Text(HHText::Money(Record.Value)).Font(FHHStyle::Font(EHHFont::Mono, 13.f)).ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
					]
				], LootStart + Index * LootStep, 0.25f)
		];
	}
	if (Summary.Loot.Num() > Shown)
	{
		List->AddSlot().AutoHeight()
		[
			Reveal(SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("MoreLoot", "... and {0} more"), FText::AsNumber(Summary.Loot.Num() - Shown)))
				.Font(FHHStyle::Font(EHHFont::Condensed, 12.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint())), LootStart + Shown * LootStep, 0.25f)
		];
	}
	if (Summary.Loot.Num() == 0)
	{
		List->AddSlot().AutoHeight()
		[
			Reveal(SNew(STextBlock)
				.Text(LOCTEXT("NoLoot", "Nothing made it into the van."))
				.Font(FHHStyle::Font(EHHFont::Typewriter, 14.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim())), LootStart, 0.4f)
		];
	}
	return List;
}

TSharedRef<SWidget> SHHMissionSummary::BuildMoney()
{
	auto Row = [](const FText& Label, const TAttribute<FText>& Value, const FLinearColor& Color, float Size)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
			[
				SNew(STextBlock).Text(Label).Font(FHHStyle::Font(EHHFont::CondensedSemi, 12.f, 260)).ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
			[
				SNew(STextBlock).Text(Value).Font(FHHStyle::Font(EHHFont::MonoMedium, Size)).ColorAndOpacity(FSlateColor(Color))
			];
	};

	const int32 Secured = Summary.SecuredValue;
	const int32 Payout = Summary.PayoutPerPlayer;
	const float Start = LootStart;
	const float Span = FMath::Max(MoneyEnd - LootStart, 0.5f);
	const float CutStart = CutAt;

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight()
	[
		Reveal(Row(LOCTEXT("InVan", "IN THE VAN"),
			TAttribute<FText>::CreateLambda([this, Secured, Start, Span]() { return HHText::Money(FMath::RoundToInt(Secured * Progress(Start, Span))); }),
			FHHStyle::Text(), 22.f), LootStart - 0.2f, 0.4f)
	];
	if (Summary.TargetBonus > 0)
	{
		Box->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			Reveal(Row(LOCTEXT("Bonus", "CLIENT'S BONUS (THE TARGET)"), FText::Format(LOCTEXT("Plus", "+{0}"), HHText::Money(Summary.TargetBonus)), FHHStyle::Ready(), 16.f), CutAt, 0.35f)
		];
	}
	if (Summary.FenceCut > 0)
	{
		Box->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			Reveal(Row(Summary.Outcome == EHHMissionOutcome::HotGetaway ? LOCTEXT("CutHot", "THE FENCE'S CUT (HOT GOODS)") : LOCTEXT("Cut", "THE FENCE'S CUT"),
				FText::Format(LOCTEXT("Minus", "-{0}"), HHText::Money(Summary.FenceCut)), FHHStyle::Danger(), 16.f), CutAt + 0.25f, 0.35f)
		];
	}
	if (Summary.LostValue > 0)
	{
		Box->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			Reveal(Row(LOCTEXT("Lost", "LOST (BAGS LEFT BEHIND / EVIDENCE)"), HHText::Money(Summary.LostValue), FHHStyle::TextDim(), 16.f), CutAt + 0.5f, 0.35f)
		];
	}
	Box->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
	[
		Reveal(Row(LOCTEXT("YourCut", "YOUR SHARE"),
			TAttribute<FText>::CreateLambda([this, Payout, CutStart]() { return HHText::Money(FMath::RoundToInt(Payout * Progress(CutStart + 0.6f, 1.f))); }),
			FHHStyle::Accent(), 34.f), CutStart + 0.5f, 0.4f)
	];
	return Box;
}

TSharedRef<SWidget> SHHMissionSummary::BuildCrew()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
	[
		Reveal(HHUI::SectionLabel(LOCTEXT("RollCall", "THE CREW")), CrewStart - 0.3f, 0.4f)
	];
	for (int32 Index = 0; Index < Summary.Crew.Num(); ++Index)
	{
		const FHHCrewRecord& Record = Summary.Crew[Index];
		const FLinearColor FateColor = HHSummary::FateColor(Record.Fate);
		FText Detail;
		if (Record.Fate == EHHCrewFate::Caught)
		{
			Detail = Record.Lost > 0 ? FText::Format(LOCTEXT("CaughtLost", "taken in with {0} in the bag"), HHText::Money(Record.Lost)) : LOCTEXT("CaughtPlain", "spent the night in a cell");
		}
		else if (Record.Fate == EHHCrewFate::LeftBehind)
		{
			Detail = Record.Lost > 0 ? FText::Format(LOCTEXT("LeftLost", "walked home; dumped {0}"), HHText::Money(Record.Lost)) : LOCTEXT("LeftPlain", "walked home through the woods");
		}
		else
		{
			Detail = Record.bDrove ? LOCTEXT("Drove", "drove the van") : LOCTEXT("Rode", "in the van");
		}

		Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
		[
			Reveal(SNew(SBorder)
				.BorderImage(FHHStyle::Brush("HH.Card"))
				.Padding(FMargin(14.f, 10.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
					[
						SNew(SBox).WidthOverride(6.f).HeightOverride(34.f)
						[
							SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(AHHPlayerState::CrewColorForSlot(Record.CrewSlot)))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(Record.Name)).Font(FHHStyle::Font(EHHFont::BodySemi, 15.f)).ColorAndOpacity(FSlateColor(FHHStyle::Text()))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(Record.Stashed > 0 ? FText::Format(LOCTEXT("StashedBy", "{0}  -  stashed {1}"), Detail, HHText::Money(Record.Stashed)) : Detail)
							.Font(FHHStyle::Font(EHHFont::Body, 12.f))
							.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
					[
						HHUI::Pill(HHText::CrewFateName(Record.Fate).ToUpper(), FateColor)
					]
				], CrewStart + Index * 0.4f, 0.3f, FVector2D(40.0, 0.0))
		];
	}

	const int32 XP = Summary.XPPerPlayer;
	const float XPStart = XPAt;
	Box->AddSlot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
	[
		Reveal(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
			[
				SNew(STextBlock).Text(LOCTEXT("Reputation", "REPUTATION")).Font(FHHStyle::Font(EHHFont::CondensedSemi, 12.f, 260)).ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
			[
				SNew(STextBlock)
				.Text_Lambda([this, XP, XPStart]() { return FText::Format(LOCTEXT("XP", "+{0} XP"), FText::AsNumber(FMath::RoundToInt(XP * Progress(XPStart, 1.f)))); })
				.Font(FHHStyle::Font(EHHFont::MonoMedium, 22.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			], XPAt - 0.2f, 0.4f)
	];
	return Box;
}

#undef LOCTEXT_NAMESPACE
