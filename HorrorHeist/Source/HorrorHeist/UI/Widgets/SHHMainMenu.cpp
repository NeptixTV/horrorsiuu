#include "UI/Widgets/SHHMainMenu.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/Widgets/SHHCrewList.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Lobby/HHLobbyGameState.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHMissionDefinition.h"
#include "Progression/HHProfileSubsystem.h"
#include "Core/HHDeveloperSettings.h"
#include "Misc/App.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/World.h"
#include "GeneralProjectSettings.h"

#define LOCTEXT_NAMESPACE "HHMainMenu"

void SHHMainMenu::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;

	auto Open = [WeakHUD](EHHLobbyScreen Screen)
	{
		return FSimpleDelegate::CreateLambda([WeakHUD, Screen]()
		{
			if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
			{
				LobbyHUD->OpenScreen(Screen);
			}
		});
	};

	ChildSlot
	[
		SNew(SOverlay)

		// Darken the left third so the type reads over the live scene.
		+ SOverlay::Slot().HAlign(HAlign_Left)
		[
			SNew(SBox)
			.WidthOverride(980.f)
			[
				SNew(SImage)
				.Image(FHHStyle::Brush("HH.GradientLeft"))
				.Visibility(EVisibility::HitTestInvisible)
			]
		]

		// Left column: title + navigation.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(110.f, 0.f, 0.f, 0.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "THE QUIET JOB"))
				.Font(FHHStyle::Font(EHHFont::Display, 44.f, 260))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Subtitle", "the hideout under Marlowe's Laundromat"))
				.Font(FHHStyle::Font(EHHFont::Typewriter, 14.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 54.f, 0.f, 0.f)
			[
				SNew(SHHNavButton).Index(FText::FromString(TEXT("01"))).Label(LOCTEXT("Play", "PLAY")).OnClicked(Open(EHHLobbyScreen::Play))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHHNavButton).Index(FText::FromString(TEXT("02"))).Label(LOCTEXT("Loadout", "LOADOUT")).OnClicked(Open(EHHLobbyScreen::Loadout))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHHNavButton).Index(FText::FromString(TEXT("03"))).Label(LOCTEXT("Customize", "CUSTOMIZE")).OnClicked(Open(EHHLobbyScreen::Customization))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHHNavButton).Index(FText::FromString(TEXT("04"))).Label(LOCTEXT("Store", "STORE")).OnClicked(Open(EHHLobbyScreen::Store))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHHNavButton).Index(FText::FromString(TEXT("05"))).Label(LOCTEXT("Settings", "SETTINGS")).OnClicked(Open(EHHLobbyScreen::Settings))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHHNavButton).Index(FText::FromString(TEXT("06"))).Label(LOCTEXT("Quit", "QUIT"))
				.OnClicked(FSimpleDelegate::CreateLambda([WeakHUD]()
				{
					if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
					{
						LobbyHUD->RequestQuit();
					}
				}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 44.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(&FHHStyle::Button("HH.Button.Invisible"))
				.ContentPadding(FMargin(0.f))
				.OnHovered_Lambda([]() { HHUI::PlaySound(EHHUISound::Hover); })
				.OnClicked_Lambda([WeakHUD]()
				{
					HHUI::PlaySound(EHHUISound::Back);
					if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
					{
						LobbyHUD->CloseMenu();
					}
					return FReply::Handled();
				})
				[
					SNew(SHHKeyHint)
					.Key(LOCTEXT("EscKey", "ESC"))
					.Label(LOCTEXT("Explore", "Walk around the hideout"))
				]
			]
		]

		// Bottom left: build + network status.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(110.f, 0.f, 0.f, 40.f))
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return GetNetworkStatus(); })
			.Font(FHHStyle::Font(EHHFont::Condensed, 12.f, 300))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
		]

		// Right column: profile, crew, next job.
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 54.f, 64.f, 0.f))
		[
			SNew(SBox)
			.WidthOverride(400.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					BuildPlayerCard()
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 34.f, 0.f, 10.f)
				[
					HHUI::SectionLabel(LOCTEXT("Crew", "THE CREW"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHHCrewList).HUD(HUD)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 30.f, 0.f, 10.f)
				[
					HHUI::SectionLabel(LOCTEXT("NextJob", "NEXT JOB"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					BuildJobSummary()
				]
			]
		]
	];
}

TSharedRef<SWidget> SHHMainMenu::BuildPlayerCard()
{
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	auto Profile = [WeakHUD]() -> const UHHProfileSubsystem*
	{
		return WeakHUD.IsValid() ? UHHProfileSubsystem::Get(WeakHUD.Get()) : nullptr;
	};

	return SNew(SBorder)
		.BorderImage(FHHStyle::Brush("HH.PanelFrame"))
		.Padding(FMargin(22.f, 18.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				// Level badge
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.BorderImage(FHHStyle::Brush("HH.OutlineAccent"))
					.Padding(FMargin(0.f))
					[
						SNew(SBox).WidthOverride(46.f).HeightOverride(46.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([Profile]() { const UHHProfileSubsystem* P = Profile(); return FText::AsNumber(P ? P->GetLevel() : 1); })
							.Font(FHHStyle::Font(EHHFont::Display, 20.f))
							.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SButton)
						.ButtonStyle(&FHHStyle::Button("HH.Button.Invisible"))
						.ContentPadding(FMargin(0.f))
						.ToolTipText(LOCTEXT("ChangeAlias", "Change alias"))
						.OnClicked_Lambda([WeakHUD]()
						{
							if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
							{
								HHUI::PlaySound(EHHUISound::Click);
								LobbyHUD->ShowNameEntry(false);
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Text_Lambda([Profile]() { const UHHProfileSubsystem* P = Profile(); return FText::FromString(P ? P->GetPlayerName() : FString()); })
							.Font(FHHStyle::Font(EHHFont::BodySemi, 19.f))
							.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text_Lambda([Profile]()
						{
							const UHHProfileSubsystem* P = Profile();
							if (!P)
							{
								return FText::GetEmpty();
							}
							if (P->GetLevel() >= UHHProfileSubsystem::MaxLevel)
							{
								return LOCTEXT("MaxRep", "Legend of the underground");
							}
							return FText::Format(LOCTEXT("XP", "{0} / {1} XP"), FText::AsNumber(P->GetXPIntoLevel()), FText::AsNumber(P->GetXPForNextLevel()));
						})
						.Font(FHHStyle::Font(EHHFont::Mono, 11.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
					]
				]
				// Cash
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Cash", "CASH"))
						.Font(FHHStyle::Font(EHHFont::CondensedSemi, 10.f, 300))
						.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(STextBlock)
						.Text_Lambda([Profile]() { const UHHProfileSubsystem* P = Profile(); return HHText::Money(P ? P->GetCash() : 0); })
						.Font(FHHStyle::Font(EHHFont::MonoMedium, 20.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
					]
				]
			]
			// XP bar
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
			[
				SNew(SBox).HeightOverride(2.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Profile]() { const UHHProfileSubsystem* P = Profile(); return FMath::Max(P ? P->GetLevelProgress() : 0.f, 0.001f); }))
					[
						SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
					]
					+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Profile]() { const UHHProfileSubsystem* P = Profile(); return FMath::Max(1.f - (P ? P->GetLevelProgress() : 0.f), 0.001f); }))
					[
						SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.08f)))
					]
				]
			]
		];
}

TSharedRef<SWidget> SHHMainMenu::BuildJobSummary()
{
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	auto Mission = [WeakHUD]() -> const UHHMissionDefinition*
	{
		const AHHLobbyHUD* LobbyHUD = WeakHUD.Get();
		const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
		const UHHItemRegistrySubsystem* Registry = LobbyHUD ? UHHItemRegistrySubsystem::Get(LobbyHUD) : nullptr;
		return State && Registry ? Registry->FindMission(State->GetSelectedMission()) : nullptr;
	};

	return SNew(SButton)
		.ButtonStyle(&FHHStyle::Button("HH.Button.Card"))
		.ContentPadding(FMargin(18.f, 14.f))
		.OnHovered_Lambda([]() { HHUI::PlaySound(EHHUISound::Hover); })
		.OnClicked_Lambda([WeakHUD]()
		{
			HHUI::PlaySound(EHHUISound::Click);
			if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
			{
				LobbyHUD->OpenScreen(EHHLobbyScreen::Play);
			}
			return FReply::Handled();
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([Mission]() { const UHHMissionDefinition* M = Mission(); return M ? M->DisplayName : LOCTEXT("NoJob", "No job pinned to the board"); })
				.Font(FHHStyle::Font(EHHFont::Display, 20.f, 60))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([Mission]() { const UHHMissionDefinition* M = Mission(); return M ? M->Address : LOCTEXT("PickOne", "Open the job board to choose a house."); })
				.Font(FHHStyle::Font(EHHFont::Typewriter, 13.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([Mission]()
				{
					const UHHMissionDefinition* M = Mission();
					if (!M)
					{
						return FText::GetEmpty();
					}
					return FText::Format(LOCTEXT("JobLine", "{0}   ·   {1} - {2}"), HHText::DifficultyName(M->Difficulty), HHText::Money(M->PayoutMin), HHText::Money(M->PayoutMax));
				})
				.Font(FHHStyle::Font(EHHFont::Condensed, 13.f, 120))
				.ColorAndOpacity_Lambda([Mission]() { const UHHMissionDefinition* M = Mission(); return FSlateColor(M ? HHText::DifficultyColor(M->Difficulty) : FHHStyle::TextFaint()); })
			]
		];
}

FText SHHMainMenu::GetNetworkStatus() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const UWorld* World = LobbyHUD ? LobbyHUD->GetWorld() : nullptr;
	const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
	const int32 Crew = State ? State->GetCrew().Num() : 1;
	const int32 MaxCrew = UHHDeveloperSettings::Get()->MaxCrewSize;
	const FString Version = GetDefault<UGeneralProjectSettings>()->ProjectVersion;

	FText Mode = LOCTEXT("Private", "PRIVATE HIDEOUT");
	if (World)
	{
		switch (World->GetNetMode())
		{
		case NM_ListenServer:	Mode = FText::Format(LOCTEXT("Hosting", "HOSTING A CREW  {0}/{1}"), FText::AsNumber(Crew), FText::AsNumber(MaxCrew)); break;
		case NM_Client:			Mode = FText::Format(LOCTEXT("Client", "IN A CREW  {0}/{1}"), FText::AsNumber(Crew), FText::AsNumber(MaxCrew)); break;
		default: break;
		}
	}
	return FText::Format(LOCTEXT("Status", "{0}      BUILD {1}"), Mode, FText::FromString(Version));
}

#undef LOCTEXT_NAMESPACE
