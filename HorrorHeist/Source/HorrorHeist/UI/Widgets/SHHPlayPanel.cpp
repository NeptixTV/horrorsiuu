#include "UI/Widgets/SHHPlayPanel.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/Widgets/SHHCrewList.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Lobby/HHLobbyGameState.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHMissionDefinition.h"
#include "Progression/HHProfileSubsystem.h"
#include "Core/HHDeveloperSettings.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHPlayPanel"

SHHPlayPanel::~SHHPlayPanel()
{
	if (UHHSessionSubsystem* Sessions = HUD.IsValid() ? UHHSessionSubsystem::Get(HUD.Get()) : nullptr)
	{
		Sessions->OnSessionsFound.Remove(SessionsFoundHandle);
	}
}

void SHHPlayPanel::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;
	Inspected = GetPinnedMission();
	if (Inspected.IsNone())
	{
		if (const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr)
		{
			const TArray<const UHHMissionDefinition*> Missions = Registry->GetMissions();
			Inspected = Missions.Num() > 0 ? Missions[0]->GetMissionId() : NAME_None;
		}
	}

	if (UHHSessionSubsystem* Sessions = HUD.IsValid() ? UHHSessionSubsystem::Get(HUD.Get()) : nullptr)
	{
		SessionsFoundHandle = Sessions->OnSessionsFound.AddSP(this, &SHHPlayPanel::HandleSessionsFound);
	}

	TSharedRef<SWidget> Header = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			HHUI::PanelHeader(FText::FromString(TEXT("01")), LOCTEXT("Title", "THE JOB BOARD"),
				LOCTEXT("Subtitle", "Pick the house. Everyone walks out, or nobody does."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
		[
			SNew(SHHTabBar)
			.Tabs({ LOCTEXT("TabJobs", "JOBS"), LOCTEXT("TabCrew", "CREW & SESSION") })
			.Selected_Lambda([this]() { return Tab; })
			.OnSelected(FHHOnIndexSelected::CreateSP(this, &SHHPlayPanel::SetTab))
		];

	ChildSlot
	[
		HHUI::PanelFrame(Header, SAssignNew(TabHost, SBox), BuildFooter())
	];

	SetTab(0);
}

FName SHHPlayPanel::GetPinnedMission() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
	return State ? State->GetSelectedMission() : NAME_None;
}

bool SHHPlayPanel::IsLeader() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
	return PC && PC->IsCrewLeader();
}

int32 SHHPlayPanel::GetPlayerLevel() const
{
	const UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
	return Profile ? Profile->GetLevel() : 1;
}

void SHHPlayPanel::SetTab(int32 NewTab)
{
	Tab = NewTab;
	TabHost->SetContent(Tab == 0 ? BuildJobsTab() : BuildCrewTab());
}

void SHHPlayPanel::Inspect(FName MissionId)
{
	if (Inspected != MissionId)
	{
		Inspected = MissionId;
		RebuildDossier();
	}
}

// ---------------------------------------------------------------------------------------
// Jobs

TSharedRef<SWidget> SHHPlayPanel::BuildJobsTab()
{
	TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.42f).Padding(0.f, 0.f, 22.f, 0.f)
		[
			SNew(SScrollBox)
			.ScrollBarThickness(FVector2D(2.0, 2.0))
			+ SScrollBox::Slot()
			[
				SAssignNew(JobList, SVerticalBox)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(0.58f)
		[
			SAssignNew(DossierHost, SBox)
		];

	RebuildJobList();
	RebuildDossier();
	return Content;
}

void SHHPlayPanel::RebuildJobList()
{
	JobList->ClearChildren();
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	if (!Registry)
	{
		return;
	}

	const TArray<const UHHMissionDefinition*> Missions = Registry->GetMissions();
	if (Missions.Num() == 0)
	{
		JobList->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoJobs", "The board is empty. (No mission data assets found - run the project setup.)"))
			.AutoWrapText(true)
			.Font(FHHStyle::Font(EHHFont::Body, 14.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		];
		return;
	}

	for (const UHHMissionDefinition* Mission : Missions)
	{
		const FName Id = Mission->GetMissionId();
		const bool bLocked = GetPlayerLevel() < Mission->UnlockLevel;
		TSharedPtr<FSlateBrush> Thumb = HHUI::MakeTextureBrush(Registry->GetMissionPhoto(Mission), FVector2D(64.0, 64.0));
		Brushes.Add(Thumb);

		JobList->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SButton)
			.ButtonStyle(&FHHStyle::Button("HH.Button.Card"))
			.ContentPadding(FMargin(0.f))
			.OnHovered_Lambda([]() { HHUI::PlayUISound(EHHUISound::Hover); })
			.OnClicked_Lambda([this, Id]()
			{
				HHUI::PlayUISound(EHHUISound::Click);
				Inspect(Id);
				return FReply::Handled();
			})
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(10.f)
					[
						SNew(SBox).WidthOverride(64.f).HeightOverride(64.f)
						[
							SNew(SImage)
							.Image(Thumb.Get())
							.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, bLocked ? 0.35f : 1.f)))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(4.f, 0.f, 10.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(Mission->DisplayName)
							.Font(FHHStyle::Font(EHHFont::BodySemi, 15.f))
							.ColorAndOpacity(FSlateColor(bLocked ? FHHStyle::TextFaint() : FHHStyle::Text()))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(Mission->Address)
							.Font(FHHStyle::Font(EHHFont::Typewriter, 12.f))
							.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
							[
								HHUI::Pill(bLocked
									? FText::Format(LOCTEXT("LockedLvl", "LEVEL {0}"), FText::AsNumber(Mission->UnlockLevel))
									: HHText::DifficultyName(Mission->Difficulty).ToUpper(),
									bLocked ? FHHStyle::TextFaint() : HHText::DifficultyColor(Mission->Difficulty))
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
							[
								SNew(SBox)
								.Visibility_Lambda([this, Id]() { return GetPinnedMission() == Id ? EVisibility::Visible : EVisibility::Collapsed; })
								[
									HHUI::Pill(LOCTEXT("Pinned", "PINNED"), FHHStyle::Accent())
								]
							]
						]
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FHHStyle::Brush("HH.OutlineAccent"))
					.Visibility_Lambda([this, Id]() { return Inspected == Id ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				]
			]
		];
	}
}

void SHHPlayPanel::RebuildDossier()
{
	if (!DossierHost.IsValid())
	{
		return;
	}
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	const UHHMissionDefinition* Mission = Registry ? Registry->FindMission(Inspected) : nullptr;
	if (!Mission)
	{
		DossierHost->SetContent(SNullWidget::NullWidget);
		return;
	}

	const FName Id = Mission->GetMissionId();
	const bool bLocked = GetPlayerLevel() < Mission->UnlockLevel;
	TSharedPtr<FSlateBrush> Photo = HHUI::MakeTextureBrush(Registry->GetMissionPhoto(Mission), FVector2D(250.0, 250.0));
	Brushes.Add(Photo);

	auto Fact = [](const FText& Label, const FText& Value) -> TSharedRef<SWidget>
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Label).Font(FHHStyle::Font(EHHFont::CondensedSemi, 10.f, 260)).ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Value).Font(FHHStyle::Font(EHHFont::BodyMedium, 14.f)).ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			];
	};

	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;

	DossierHost->SetContent(
		SNew(SScrollBox)
		.ScrollBarThickness(FVector2D(2.0, 2.0))
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			// Polaroid + headline
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(200.f)
					.HeightOverride(200.f)
					[
						SNew(SImage).Image(Photo.Get())
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Top).Padding(24.f, 4.f, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Mission->DisplayName)
						.AutoWrapText(true)
						.Font(FHHStyle::Font(EHHFont::Display, 26.f, 60))
						.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Mission->Address)
						.Font(FHHStyle::Font(EHHFont::Typewriter, 14.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							HHUI::Pill(HHText::DifficultyName(Mission->Difficulty).ToUpper(), HHText::DifficultyColor(Mission->Difficulty))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							Fact(LOCTEXT("Payout", "PAYOUT"), FText::Format(LOCTEXT("PayoutRange", "{0} - {1}"), HHText::Money(Mission->PayoutMin), HHText::Money(Mission->PayoutMax)))
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							Fact(LOCTEXT("Residents", "RESIDENTS"), FText::AsNumber(Mission->Residents))
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							Fact(LOCTEXT("CrewSize", "CREW"), FText::Format(LOCTEXT("CrewSizeValue", "{0}+"), FText::AsNumber(Mission->RecommendedCrew)))
						]
					]
				]
			]
			// Target
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 8.f)
			[
				HHUI::SectionLabel(LOCTEXT("Target", "THE CLIENT WANTS"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Mission->PrimaryTarget)
				.AutoWrapText(true)
				.Font(FHHStyle::Font(EHHFont::BodyMedium, 16.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
			]
			// Briefing
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 8.f)
			[
				HHUI::SectionLabel(LOCTEXT("Briefing", "DOSSIER"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Mission->Briefing)
				.AutoWrapText(true)
				.LineHeightPercentage(1.15f)
				.Font(FHHStyle::Font(EHHFont::Typewriter, 14.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
			// Rumour, pinned like a handwritten note
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FHHStyle::Brush("HH.Paper"))
				.BorderBackgroundColor(FSlateColor(FHHStyle::WithAlpha(FHHStyle::Paper(), 0.92f)))
				.Padding(FMargin(18.f, 12.f))
				.Visibility(Mission->Rumor.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				[
					SNew(STextBlock)
					.Text(Mission->Rumor)
					.AutoWrapText(true)
					.Font(FHHStyle::Font(EHHFont::Hand, 21.f))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.08f, 0.07f, 0.08f, 1.f)))
				]
			]
			// Pin
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 4.f).HAlign(HAlign_Left)
			[
				SNew(SBox)
				.Visibility_Lambda([this, Id, bLocked]()
				{
					return IsLeader() && !bLocked && GetPinnedMission() != Id ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SHHActionButton)
					.Text(LOCTEXT("PinJob", "PIN THIS JOB"))
					.Kind(EHHButtonKind::Secondary)
					.MinWidth(200.f)
					.ClickSound(EHHUISound::Confirm)
					.OnClicked_Lambda([WeakHUD, Id]()
					{
						if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
						{
							if (AHHPlayerController* PC = LobbyHUD->GetHHController())
							{
								PC->RequestSelectMission(Id);
							}
						}
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this, Id, bLocked, Mission]()
				{
					if (bLocked)
					{
						return FText::Format(LOCTEXT("NeedLevel", "Nobody hires you for this one before level {0}."), FText::AsNumber(Mission->UnlockLevel));
					}
					if (GetPinnedMission() == Id)
					{
						return Mission->IsPlayable()
							? LOCTEXT("IsPinned", "Pinned. Ready up when the crew is set.")
							: LOCTEXT("IsPinnedNoMap", "Pinned. Note: this job has no level assigned yet, the van will not leave.");
					}
					return IsLeader() ? FText::GetEmpty() : LOCTEXT("NotLeader", "Only the crew leader can pin a job.");
				})
				.AutoWrapText(true)
				.Font(FHHStyle::Font(EHHFont::Body, 13.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
		]);
}

// ---------------------------------------------------------------------------------------
// Crew & session

TSharedRef<SWidget> SHHPlayPanel::BuildCrewTab()
{
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	auto Sessions = [WeakHUD]() -> UHHSessionSubsystem*
	{
		return WeakHUD.IsValid() ? UHHSessionSubsystem::Get(WeakHUD.Get()) : nullptr;
	};
	auto NetMode = [WeakHUD]() -> ENetMode
	{
		const AHHLobbyHUD* LobbyHUD = WeakHUD.Get();
		return LobbyHUD && LobbyHUD->GetWorld() ? LobbyHUD->GetWorld()->GetNetMode() : NM_Standalone;
	};

	TSharedRef<SWidget> Content = SNew(SScrollBox)
		.ScrollBarThickness(FVector2D(2.0, 2.0))
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				HHUI::SectionLabel(LOCTEXT("Crew", "IN THE HIDEOUT"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHHCrewList).HUD(HUD)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 10.f)
			[
				HHUI::SectionLabel(LOCTEXT("Session", "SESSION"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([NetMode]()
				{
					switch (NetMode())
					{
					case NM_ListenServer:	return LOCTEXT("StatusHost", "You are hosting. Friends on your network can find this hideout; others can join by your IP address.");
					case NM_Client:			return LOCTEXT("StatusClient", "You are in someone else's hideout.");
					default:				return LOCTEXT("StatusSolo", "Private hideout. Open it to a crew, look for one, or go in alone.");
					}
				})
				.AutoWrapText(true)
				.Font(FHHStyle::Font(EHHFont::Body, 14.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SBox)
					.Visibility_Lambda([NetMode]() { return NetMode() == NM_Standalone ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SHHActionButton)
						.Text(LOCTEXT("Host", "OPEN TO CREW"))
						.Kind(EHHButtonKind::Primary)
						.OnClicked_Lambda([Sessions]()
						{
							if (UHHSessionSubsystem* Subsystem = Sessions())
							{
								Subsystem->HostSession(true, UHHDeveloperSettings::Get()->MaxCrewSize);
							}
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SHHActionButton)
					.Text_Lambda([this]() { return bSearching ? LOCTEXT("Searching", "SEARCHING...") : LOCTEXT("Find", "FIND CREWS"); })
					.OnClicked_Lambda([this, Sessions]()
					{
						if (UHHSessionSubsystem* Subsystem = Sessions())
						{
							bSearching = true;
							SessionStatus = LOCTEXT("Listening", "Listening on the local network...");
							RebuildSessionList();
							Subsystem->FindSessions(true);
						}
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SHHActionButton)
					.Text(LOCTEXT("JoinIp", "JOIN BY IP"))
					.OnClicked_Lambda([WeakHUD]()
					{
						if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
						{
							LobbyHUD->ShowAddressEntry();
						}
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox)
					.Visibility_Lambda([NetMode]() { return NetMode() != NM_Standalone ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SHHActionButton)
						.Text_Lambda([NetMode]() { return NetMode() == NM_ListenServer ? LOCTEXT("Close", "CLOSE HIDEOUT") : LOCTEXT("Leave", "LEAVE CREW"); })
						.Kind(EHHButtonKind::Danger)
						.ClickSound(EHHUISound::Back)
						.OnClicked_Lambda([WeakHUD, Sessions]()
						{
							AHHLobbyHUD* LobbyHUD = WeakHUD.Get();
							if (!LobbyHUD)
							{
								return;
							}
							LobbyHUD->ShowConfirm(LOCTEXT("LeaveTitle", "WALK AWAY?"),
								LOCTEXT("LeaveBody", "You will return to your private hideout. Anyone in your hideout will be sent home."),
								LOCTEXT("LeaveConfirm", "LEAVE"),
								[Sessions]()
								{
									if (UHHSessionSubsystem* Subsystem = Sessions())
									{
										Subsystem->LeaveSession();
									}
								});
						})
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
			[
				SAssignNew(SessionList, SVerticalBox)
			]
		];

	RebuildSessionList();
	return Content;
}

void SHHPlayPanel::HandleSessionsFound(bool bSuccess, const TArray<FHHSessionInfo>& Results)
{
	bSearching = false;
	SessionResults = Results;
	SessionStatus = !bSuccess
		? LOCTEXT("FindFailed", "Could not search for crews.")
		: (Results.Num() == 0 ? LOCTEXT("NoneFound", "No crews found on your network. Try joining by IP.") : FText::GetEmpty());
	RebuildSessionList();
}

void SHHPlayPanel::RebuildSessionList()
{
	if (!SessionList.IsValid())
	{
		return;
	}
	SessionList->ClearChildren();

	if (!SessionStatus.IsEmpty())
	{
		SessionList->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(STextBlock)
			.Text(SessionStatus)
			.Font(FHHStyle::Font(EHHFont::Body, 13.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		];
	}

	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	for (const FHHSessionInfo& Info : SessionResults)
	{
		const int32 ResultIndex = Info.ResultIndex;
		const int32 Taken = Info.MaxSlots - Info.OpenSlots;
		SessionList->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SButton)
			.ButtonStyle(&FHHStyle::Button("HH.Button.Card"))
			.ContentPadding(FMargin(16.f, 12.f))
			.IsEnabled(Info.OpenSlots > 0)
			.OnHovered_Lambda([]() { HHUI::PlayUISound(EHHUISound::Hover); })
			.OnClicked_Lambda([WeakHUD, ResultIndex]()
			{
				HHUI::PlayUISound(EHHUISound::Confirm);
				if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
				{
					if (UHHSessionSubsystem* Subsystem = UHHSessionSubsystem::Get(LobbyHUD))
					{
						Subsystem->JoinSession(ResultIndex);
					}
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("HostedBy", "{0}'s hideout"), FText::FromString(Info.HostName)))
					.Font(FHHStyle::Font(EHHFont::BodyMedium, 15.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("Slots", "{0}/{1}"), FText::AsNumber(Taken), FText::AsNumber(Info.MaxSlots)))
					.Font(FHHStyle::Font(EHHFont::Mono, 13.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("Ping", "{0} ms"), FText::AsNumber(Info.PingMs)))
					.Font(FHHStyle::Font(EHHFont::Mono, 13.f))
					.ColorAndOpacity(FSlateColor(Info.PingMs < 80 ? FHHStyle::Ready() : FHHStyle::Accent()))
				]
			]
		];
	}
}

// ---------------------------------------------------------------------------------------
// Footer: ready up

TSharedRef<SWidget> SHHPlayPanel::BuildFooter()
{
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	auto Self = [WeakHUD]() -> const AHHPlayerState*
	{
		const AHHLobbyHUD* LobbyHUD = WeakHUD.Get();
		const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
		return PC ? PC->GetHHPlayerState() : nullptr;
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHHActionButton)
			.Text_Lambda([Self]() { const AHHPlayerState* State = Self(); return State && State->IsReady() ? LOCTEXT("Unready", "NOT READY") : LOCTEXT("ReadyUp", "READY UP"); })
			.Kind(EHHButtonKind::Primary)
			.MinWidth(220.f)
			.Height(48.f)
			.ClickSound(EHHUISound::Confirm)
			.OnClicked_Lambda([WeakHUD]()
			{
				if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
				{
					if (AHHPlayerController* PC = LobbyHUD->GetHHController())
					{
						PC->RequestToggleReady();
					}
				}
			})
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(20.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text_Lambda([WeakHUD]()
			{
				const AHHLobbyHUD* LobbyHUD = WeakHUD.Get();
				const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
				if (!State)
				{
					return FText::GetEmpty();
				}
				if (State->GetSelectedMission().IsNone())
				{
					return LOCTEXT("NoPinned", "No job pinned yet.");
				}
				return FText::Format(LOCTEXT("ReadyCount", "{0} of {1} ready"), FText::AsNumber(State->NumReady()), FText::AsNumber(State->GetCrew().Num()));
			})
			.Font(FHHStyle::Font(EHHFont::Condensed, 15.f, 120))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHHKeyHint).Key(LOCTEXT("Esc", "ESC")).Label(LOCTEXT("Back", "Back"))
		];
}

#undef LOCTEXT_NAMESPACE
