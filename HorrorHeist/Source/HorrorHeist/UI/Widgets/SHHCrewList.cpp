#include "UI/Widgets/SHHCrewList.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Lobby/HHLobbyGameState.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Core/HHDeveloperSettings.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHCrewList"

void SHHCrewList::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;
	bShowEmptySlots = InArgs._ShowEmptySlots;

	ChildSlot
	[
		SAssignNew(List, SVerticalBox)
	];
	Rebuild();
}

void SHHCrewList::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
	const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;

	uint32 NewSignature = 17;
	if (State)
	{
		for (const AHHPlayerState* Member : State->GetCrew())
		{
			NewSignature = HashCombine(NewSignature, GetTypeHash(Member->GetPlayerName()));
			NewSignature = HashCombine(NewSignature, static_cast<uint32>(Member->GetPlayerLevel()));
			NewSignature = HashCombine(NewSignature, Member->IsReady() ? 1u : 2u);
			NewSignature = HashCombine(NewSignature, Member->IsCrewLeader() ? 3u : 4u);
			NewSignature = HashCombine(NewSignature, PC && PC->IsPlayerTalking(Member) ? 5u : 6u);
		}
	}
	if (NewSignature != Signature)
	{
		Signature = NewSignature;
		Rebuild();
	}
}

void SHHCrewList::Rebuild()
{
	List->ClearChildren();

	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
	const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
	const TArray<AHHPlayerState*> Crew = State ? State->GetCrew() : TArray<AHHPlayerState*>();

	for (const AHHPlayerState* Member : Crew)
	{
		const bool bSelf = PC && PC->GetPlayerState<AHHPlayerState>() == Member;
		const bool bTalking = PC && PC->IsPlayerTalking(Member);

		List->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SBorder)
			.BorderImage(FHHStyle::Brush("HH.Card"))
			.Padding(FMargin(0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(3.f)
					[
						SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(Member->GetCrewColor()))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 10.f, 0.f, 10.f)
				[
					SNew(SBox).WidthOverride(34.f)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("Lvl", "{0}"), FText::AsNumber(Member->GetPlayerLevel())))
						.Font(FHHStyle::Font(EHHFont::MonoMedium, 13.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Member->GetPlayerName()))
						.Font(FHHStyle::Font(bSelf ? EHHFont::BodySemi : EHHFont::BodyMedium, 15.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(SImage)
						.Image(FHHStyle::Brush("HH.Icon.Crown"))
						.DesiredSizeOverride(FVector2D(14.0, 14.0))
						.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
						.ToolTipText(LOCTEXT("Leader", "Crew leader - picks the job"))
						.Visibility(Member->IsCrewLeader() ? EVisibility::Visible : EVisibility::Collapsed)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(SImage)
						.Image(FHHStyle::Brush("HH.Icon.Mic"))
						.DesiredSizeOverride(FVector2D(14.0, 14.0))
						.ColorAndOpacity(FSlateColor(FHHStyle::Ready()))
						.Visibility(bTalking ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 14.f, 0.f)
				[
					HHUI::Pill(Member->IsReady() ? LOCTEXT("Ready", "READY") : LOCTEXT("NotReady", "GEARING UP"),
						Member->IsReady() ? FHHStyle::Ready() : FHHStyle::TextFaint())
				]
			]
		];
	}

	if (bShowEmptySlots)
	{
		const int32 MaxCrew = UHHDeveloperSettings::Get()->MaxCrewSize;
		for (int32 Index = Crew.Num(); Index < MaxCrew; ++Index)
		{
			List->AddSlot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(SBorder)
				.BorderImage(FHHStyle::Brush("HH.Outline"))
				.Padding(FMargin(17.f, 11.f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EmptySlot", "-  empty seat in the van"))
					.Font(FHHStyle::Font(EHHFont::Body, 14.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
				]
			];
		}
	}
}

#undef LOCTEXT_NAMESPACE
