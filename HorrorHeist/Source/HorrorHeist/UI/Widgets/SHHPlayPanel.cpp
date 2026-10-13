#include "UI/Widgets/SHHPlayPanel.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/Widgets/SHHCrewList.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Lobby/HHLobbyGameState.h"
#include "Heist/HHEscapeZone.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHMissionDefinition.h"
#include "Progression/HHProfileSubsystem.h"
#include "Core/HHDeveloperSettings.h"
#include "Core/HHGameData.h"
#include "World/HHTimeOfDay.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHPlayPanel"

namespace HHPlay
{
	static FText MoneyRange(int32 Min, int32 Max)
	{
		if (Max <= 0)
		{
			return FText::GetEmpty();
		}
		return Min == Max || Min <= 0 ? HHText::Money(Max) : FText::Format(LOCTEXT("MoneyRange", "{0} - {1}"), HHText::Money(Min), HHText::Money(Max));
	}

	static TSharedRef<SWidget> Fact(const FText& Label, const TAttribute<FText>& Value, const FLinearColor& Color = FHHStyle::Text())
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Label).Font(FHHStyle::Font(EHHFont::CondensedSemi, 10.f, 260)).ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Value).Font(FHHStyle::Font(EHHFont::BodyMedium, 14.f)).ColorAndOpacity(FSlateColor(Color))
			];
	}
}

SHHPlayPanel::~SHHPlayPanel()
{
	if (UHHSessionSubsystem* Sessions = HUD.IsValid() ? UHHSessionSubsystem::Get(HUD.Get()) : nullptr)
	{
		Sessions->OnSessionsFound.Remove(SessionsFoundHandle);
	}
	UnbindLobbyState();
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
	BindLobbyState();

	TSharedRef<SWidget> Header = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			HHUI::PanelHeader(FText::FromString(TEXT("01")), LOCTEXT("Title", "THE JOB BOARD"),
				LOCTEXT("Subtitle", "Pick the house, learn it, plan the way home. Everyone walks out, or nobody does."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
		[
			SNew(SHHTabBar)
			.Tabs({ LOCTEXT("TabJobs", "JOBS"), LOCTEXT("TabCrew", "CREW & LOBBY") })
			.Selected_Lambda([this]() { return Tab; })
			.OnSelected(FHHOnIndexSelected::CreateSP(this, &SHHPlayPanel::SetTab))
		];

	ChildSlot
	[
		HHUI::PanelFrame(Header, SAssignNew(TabHost, SBox), BuildFooter())
	];

	SetTab(0);
}

void SHHPlayPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// Clients may open the board before the game state replicated; bind as soon as it exists.
	if (!BoundState.IsValid())
	{
		BindLobbyState();
	}
	if (bDossierDirty)
	{
		bDossierDirty = false;
		RebuildDossier();
	}
}

void SHHPlayPanel::BindLobbyState()
{
	AHHLobbyGameState* State = HUD.IsValid() ? HUD->GetLobbyState() : nullptr;
	if (!State || State == BoundState.Get())
	{
		return;
	}
	UnbindLobbyState();
	BoundState = State;
	IntelHandle = State->OnIntelChanged.AddSP(this, &SHHPlayPanel::HandleLobbyDataChanged);
	MissionHandle = State->OnMissionChanged.AddSP(this, &SHHPlayPanel::HandleLobbyDataChanged);
	bDossierDirty = true;
}

void SHHPlayPanel::UnbindLobbyState()
{
	if (AHHLobbyGameState* State = BoundState.Get())
	{
		State->OnIntelChanged.Remove(IntelHandle);
		State->OnMissionChanged.Remove(MissionHandle);
	}
	BoundState.Reset();
}

void SHHPlayPanel::HandleLobbyDataChanged()
{
	// Rebuilt on the next tick: several values often arrive in the same frame.
	bDossierDirty = true;
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

bool SHHPlayPanel::IsIntelKnown(const FHHJobIntel& Intel) const
{
	if (Intel.bKnownFromStart)
	{
		return true;
	}
	const AHHLobbyGameState* State = HUD.IsValid() ? HUD->GetLobbyState() : nullptr;
	return State && State->IsIntelKnown(Intel.IntelId);
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

void SHHPlayPanel::Pin(FName MissionId) const
{
	if (AHHLobbyHUD* LobbyHUD = HUD.Get())
	{
		if (AHHPlayerController* PC = LobbyHUD->GetHHController())
		{
			PC->RequestSelectMission(MissionId);
		}
	}
}

FText SHHPlayPanel::GetContentWarning() const
{
	const UHHGameData* GameData = HUD.IsValid() ? UHHGameData::Get(HUD.Get()) : nullptr;
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	if (!GameData || !Registry || Registry->GetMissions().Num() == 0)
	{
		return LOCTEXT("NoContent", "The game content has not been built. In the Unreal Editor choose The Quiet Job > Build / Rebuild All Content, then start again.");
	}
	if (GameData->IsContentOutdated())
	{
		return FText::Format(LOCTEXT("OldContent", "The game content is out of date (version {0}, this build needs {1}): jobs may have no map and can't be pinned. In the Unreal Editor choose The Quiet Job > Build / Rebuild All Content."),
			FText::AsNumber(GameData->ContentVersion), FText::AsNumber(UHHGameData::ExpectedContentVersion));
	}
	for (const UHHMissionDefinition* Mission : Registry->GetMissions())
	{
		if (Mission->bAvailable && Mission->Map.IsNull())
		{
			return FText::Format(LOCTEXT("NoMapContent", "{0} has no map assigned. Rebuild the content: The Quiet Job > Build / Rebuild All Content."), Mission->DisplayName);
		}
	}
	return FText::GetEmpty();
}

// ---------------------------------------------------------------------------------------
// Jobs

TSharedRef<SWidget> SHHPlayPanel::BuildJobsTab()
{
	const FText Warning = GetContentWarning();
	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, Warning.IsEmpty() ? 0.f : 14.f)
		[
			SNew(SBorder)
			.Visibility(Warning.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			.BorderImage(FHHStyle::Brush("HH.Panel"))
			.BorderBackgroundColor(FSlateColor(FHHStyle::WithAlpha(FHHStyle::Danger(), 0.25f)))
			.Padding(FMargin(14.f, 10.f))
			[
				SNew(STextBlock)
				.Text(Warning)
				.AutoWrapText(true)
				.Font(FHHStyle::Font(EHHFont::BodyMedium, 14.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.38f).Padding(0.f, 0.f, 22.f, 0.f)
			[
				SNew(SScrollBox)
				.ScrollBarThickness(FVector2D(2.0, 2.0))
				+ SScrollBox::Slot()
				[
					SAssignNew(JobList, SVerticalBox)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(0.62f)
			[
				SAssignNew(DossierHost, SBox)
			]
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
			.Text(LOCTEXT("NoJobs", "The board is empty. (No mission data assets found - run The Quiet Job > Build / Rebuild All Content in the editor.)"))
			.AutoWrapText(true)
			.Font(FHHStyle::Font(EHHFont::Body, 14.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		];
		return;
	}

	JobList->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
	[
		SNew(STextBlock)
		.Text_Lambda([this]()
		{
			return IsLeader()
				? LOCTEXT("ListHintLeader", "Click a house to read its dossier. PIN puts it on the board for the crew.")
				: LOCTEXT("ListHintCrew", "Click a house to read its dossier. The crew leader pins the job.");
		})
		.AutoWrapText(true)
		.Font(FHHStyle::Font(EHHFont::Body, 13.f))
		.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
	];

	for (const UHHMissionDefinition* Mission : Missions)
	{
		const FName Id = Mission->GetMissionId();
		const bool bLocked = GetPlayerLevel() < Mission->UnlockLevel;
		const bool bPlayable = Mission->IsPlayable();
		TSharedPtr<FSlateBrush> Thumb = HHUI::MakeTextureBrush(Registry->GetMissionPhoto(Mission), FVector2D(64.0, 64.0));
		Brushes.Add(Thumb);

		int32 IntelTotal = 0;
		int32 IntelKnown = 0;
		for (const FHHJobIntel& Intel : Mission->Intel)
		{
			++IntelTotal;
			IntelKnown += IsIntelKnown(Intel) ? 1 : 0;
		}

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
							.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, bLocked || !bPlayable ? 0.4f : 1.f)))
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
							.Text(IntelTotal > 0
								? FText::Format(LOCTEXT("AddressIntel", "{0}  -  intel {1}/{2}"), Mission->Address, FText::AsNumber(IntelKnown), FText::AsNumber(IntelTotal))
								: Mission->Address)
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
									: (bPlayable ? HHText::DifficultyName(Mission->Difficulty).ToUpper() : LOCTEXT("Casing", "BEING CASED")),
									bLocked || !bPlayable ? FHHStyle::TextFaint() : HHText::DifficultyColor(Mission->Difficulty))
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
					// Quick pin for the leader, right on the card.
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
					[
						SNew(SBox)
						.Visibility_Lambda([this, Id, bLocked, bPlayable]()
						{
							return IsLeader() && !bLocked && bPlayable && GetPinnedMission() != Id ? EVisibility::Visible : EVisibility::Collapsed;
						})
						[
							SNew(SHHActionButton)
							.Text(LOCTEXT("PinShort", "PIN"))
							.Kind(EHHButtonKind::Secondary)
							.MinWidth(64.f)
							.Height(34.f)
							.ClickSound(EHHUISound::Confirm)
							.OnClicked_Lambda([this, Id]()
							{
								Inspect(Id);
								Pin(Id);
							})
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

TSharedRef<SWidget> SHHPlayPanel::BuildActionStrip(const UHHMissionDefinition* Mission, bool bLocked)
{
	const FName Id = Mission->GetMissionId();
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	TWeakObjectPtr<const UHHMissionDefinition> WeakMission = Mission;

	return SNew(SBorder)
		.BorderImage(FHHStyle::Brush("HH.Panel"))
		.BorderBackgroundColor(FSlateColor(FHHStyle::PanelRaised()))
		.Padding(FMargin(14.f, 12.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SBox)
					.Visibility_Lambda([this, Id, bLocked, WeakMission]()
					{
						const UHHMissionDefinition* Job = WeakMission.Get();
						return Job && IsLeader() && !bLocked && Job->IsPlayable() && GetPinnedMission() != Id ? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						SNew(SHHActionButton)
						.Text(LOCTEXT("PinJob", "PIN THIS JOB"))
						.Kind(EHHButtonKind::Primary)
						.MinWidth(200.f)
						.ClickSound(EHHUISound::Confirm)
						.OnClicked_Lambda([this, Id]() { Pin(Id); })
					]
				]
				// Pinned night job during the day: the leader can skip ahead to nightfall.
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox)
					.Visibility_Lambda([this, Id, WeakMission]()
					{
						const UHHMissionDefinition* Job = WeakMission.Get();
						const AHHTimeOfDay* Clock = HUD.IsValid() ? AHHTimeOfDay::Get(HUD.Get()) : nullptr;
						const bool bShow = Job && IsLeader() && GetPinnedMission() == Id && Job->IsPlayable() && Job->bRequiresNight && Clock && !Clock->IsNight();
						return bShow ? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						SNew(SHHActionButton)
						.Text(LOCTEXT("WaitDark", "WAIT UNTIL DARK"))
						.Kind(EHHButtonKind::Secondary)
						.MinWidth(200.f)
						.ClickSound(EHHUISound::Confirm)
						.OnClicked_Lambda([WeakHUD]()
						{
							if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
							{
								LobbyHUD->RequestWaitForNight();
							}
						})
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this, Id, bLocked, WeakMission]()
				{
					const UHHMissionDefinition* Job = WeakMission.Get();
					if (!Job)
					{
						return FText::GetEmpty();
					}
					if (bLocked)
					{
						return FText::Format(LOCTEXT("NeedLevel", "Nobody hires you for this one before level {0}."), FText::AsNumber(Job->UnlockLevel));
					}
					if (!Job->IsPlayable())
					{
						if (Job->bAvailable && Job->Map.IsNull())
						{
							return LOCTEXT("NoMapJob", "Can't be pinned: this job has no map. The game content is out of date - rebuild it in the editor (The Quiet Job > Build / Rebuild All Content).");
						}
						return Job->IsPrepared()
							? LOCTEXT("Prepared", "Still being cased: the house is in town, but this job is not open yet.")
							: LOCTEXT("NoMap", "Still being cased: this job is not available yet.");
					}
					if (GetPinnedMission() == Id)
					{
						const AHHTimeOfDay* Clock = HUD.IsValid() ? AHHTimeOfDay::Get(HUD.Get()) : nullptr;
						if (Job->bRequiresNight && Clock && !Clock->IsNight())
						{
							return FText::Format(LOCTEXT("PinnedDay", "Pinned. It's {0} - the van leaves after dark ({1}). Use the daylight: scout the house in town (windows, mailbox, the yard) to fill in the dossier."),
								HHText::ClockTime(Clock->GetTimeOfDay()), HHText::ClockTime(UHHDeveloperSettings::Get()->NightStartHour));
						}
						return LOCTEXT("IsPinned", "Pinned. Everyone gets ready at the van (or READY UP below).");
					}
					return IsLeader() ? LOCTEXT("NotPinnedLeader", "Not pinned yet.") : LOCTEXT("NotLeader", "Only the crew leader can pin a job.");
				})
				.AutoWrapText(true)
				.Font(FHHStyle::Font(EHHFont::Body, 13.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
		];
}

TSharedRef<SWidget> SHHPlayPanel::BuildIntelSection(const UHHMissionDefinition* Mission, EHHIntelKind Kind)
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	int32 Count = 0;
	int32 KnownMin = 0;
	int32 KnownMax = 0;
	int32 Unknown = 0;
	for (const FHHJobIntel& Intel : Mission->Intel)
	{
		if (Intel.Kind != Kind)
		{
			continue;
		}
		++Count;
		const bool bKnown = IsIntelKnown(Intel);
		if (Kind == EHHIntelKind::Valuable)
		{
			if (bKnown)
			{
				KnownMin += Intel.ValueMin;
				KnownMax += Intel.ValueMax;
			}
			else
			{
				++Unknown;
			}
		}
		const FText Value = bKnown ? HHPlay::MoneyRange(Intel.ValueMin, Intel.ValueMax) : FText::GetEmpty();
		const bool bKey = !Intel.KeyId.IsNone();

		Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 6.f, 10.f, 0.f)
			[
				SNew(SBox).WidthOverride(7.f).HeightOverride(7.f)
				[
					SNew(SBorder)
					.BorderImage(FHHStyle::Brush("HH.White"))
					.BorderBackgroundColor(FSlateColor(bKnown ? (bKey ? FHHStyle::Ready() : FHHStyle::Accent()) : FHHStyle::TextFaint()))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(bKnown ? Intel.Title : LOCTEXT("UnknownIntel", "???"))
					.Font(FHHStyle::Font(EHHFont::BodySemi, 14.f))
					.ColorAndOpacity(FSlateColor(bKnown ? FHHStyle::Text() : FHHStyle::TextFaint()))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(bKnown ? Intel.Detail : FText::Format(LOCTEXT("ScoutHint", "Scout: {0}"), Intel.HowToLearn))
					.AutoWrapText(true)
					.Font(bKnown ? FHHStyle::Font(EHHFont::Body, 13.f) : FHHStyle::Font(EHHFont::Hand, 18.f))
					.ColorAndOpacity(FSlateColor(bKnown ? FHHStyle::TextDim() : FHHStyle::AccentDim()))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(bKey && bKnown ? LOCTEXT("KeyInHand", "KEY") : Value)
				.Font(FHHStyle::Font(EHHFont::Mono, 13.f))
				.ColorAndOpacity(FSlateColor(bKey ? FHHStyle::Ready() : FHHStyle::Accent()))
			]
		];
	}
	if (Count == 0)
	{
		return SNullWidget::NullWidget;
	}
	if (Kind == EHHIntelKind::Valuable)
	{
		Box->AddSlot().AutoHeight().Padding(17.f, 2.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(Unknown > 0
				? FText::Format(LOCTEXT("ValueKnownPlus", "Known so far: {0}  (+ {1} more {1}|plural(one=lead,other=leads) to scout)"), HHPlay::MoneyRange(KnownMin, KnownMax), Unknown)
				: FText::Format(LOCTEXT("ValueKnown", "Known valuables: {0}"), HHPlay::MoneyRange(KnownMin, KnownMax)))
			.Font(FHHStyle::Font(EHHFont::Condensed, 13.f, 80))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 10.f)
		[
			HHUI::SectionLabel(HHText::IntelKindName(Kind).ToUpper())
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Box
		];
}

TSharedRef<SWidget> SHHPlayPanel::BuildGetawaySection()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const TArray<AHHEscapeZone*> Zones = HUD.IsValid() ? AHHEscapeZone::GetAll(HUD.Get()) : TArray<AHHEscapeZone*>();
	if (Zones.Num() == 0)
	{
		Box->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoZones", "No getaway spots in this town yet (the content is out of date - rebuild it in the editor)."))
			.AutoWrapText(true)
			.Font(FHHStyle::Font(EHHFont::Body, 13.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		];
	}

	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	for (const AHHEscapeZone* Zone : Zones)
	{
		const FName ZoneId = Zone->GetZoneId();
		Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SButton)
			.ButtonStyle(&FHHStyle::Button("HH.Button.Card"))
			.ContentPadding(FMargin(14.f, 10.f))
			.IsEnabled_Lambda([this]() { return IsLeader(); })
			.OnHovered_Lambda([]() { HHUI::PlayUISound(EHHUISound::Hover); })
			.OnClicked_Lambda([WeakHUD, ZoneId]()
			{
				HHUI::PlayUISound(EHHUISound::Confirm);
				if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
				{
					if (AHHPlayerController* PC = LobbyHUD->GetHHController())
					{
						PC->RequestSelectSafehouse(ZoneId);
					}
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
				[
					SNew(STextBlock)
					.Text_Lambda([WeakHUD, ZoneId]()
					{
						const AHHLobbyGameState* State = WeakHUD.IsValid() ? WeakHUD->GetLobbyState() : nullptr;
						return FText::FromString(State && State->GetSafehouse() == ZoneId ? TEXT("(X)") : TEXT("( )"));
					})
					.Font(FHHStyle::Font(EHHFont::Mono, 14.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Zone->GetDisplayName())
						.Font(FHHStyle::Font(EHHFont::BodySemi, 14.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Zone->GetDescription())
						.AutoWrapText(true)
						.Font(FHHStyle::Font(EHHFont::Body, 13.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
					]
				]
			]
		];
	}
	Box->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
	[
		SNew(STextBlock)
		.Text_Lambda([this]()
		{
			return IsLeader()
				? LOCTEXT("GetawayHintLeader", "At the end of the job, drive the van there with the crew inside. Whoever isn't in the van is left behind.")
				: LOCTEXT("GetawayHintCrew", "The crew leader picks where the van goes. Whoever isn't in the van is left behind.");
		})
		.AutoWrapText(true)
		.Font(FHHStyle::Font(EHHFont::Body, 13.f))
		.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
	];

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 10.f)
		[
			HHUI::SectionLabel(LOCTEXT("Getaway", "THE GETAWAY"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Box
		];
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

	const bool bLocked = GetPlayerLevel() < Mission->UnlockLevel;
	TSharedPtr<FSlateBrush> Photo = HHUI::MakeTextureBrush(Registry->GetMissionPhoto(Mission), FVector2D(250.0, 250.0));
	Brushes.Add(Photo);
	UTexture2D* PlanTexture = Mission->FloorPlan.LoadSynchronous();
	TSharedPtr<FSlateBrush> Plan = HHUI::MakeTextureBrush(PlanTexture, PlanTexture ? FVector2D(PlanTexture->GetSizeX(), PlanTexture->GetSizeY()) : FVector2D(16.0, 10.0));
	Brushes.Add(Plan);

	int32 IntelTotal = 0;
	int32 IntelKnown = 0;
	for (const FHHJobIntel& Intel : Mission->Intel)
	{
		++IntelTotal;
		IntelKnown += IsIntelKnown(Intel) ? 1 : 0;
	}

	DossierHost->SetContent(
		SNew(SScrollBox)
		.ScrollBarThickness(FVector2D(2.0, 2.0))
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			// Polaroid + headline + the numbers
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(170.f)
					.HeightOverride(170.f)
					[
						SNew(SImage).Image(Photo.Get())
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Top).Padding(22.f, 0.f, 0.f, 0.f)
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
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Mission->Address)
							.Font(FHHStyle::Font(EHHFont::Typewriter, 14.f))
							.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
						[
							HHUI::Pill(HHText::DifficultyName(Mission->Difficulty).ToUpper(), HHText::DifficultyColor(Mission->Difficulty))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							HHPlay::Fact(LOCTEXT("TargetHaul", "TARGET HAUL"), HHText::Money(Mission->MinimumHaul), FHHStyle::Accent())
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							HHPlay::Fact(LOCTEXT("Payout", "CLIENT PAYS"), HHPlay::MoneyRange(Mission->PayoutMin, Mission->PayoutMax))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							HHPlay::Fact(LOCTEXT("Residents", "RESIDENTS"), FText::AsNumber(Mission->Residents))
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							HHPlay::Fact(LOCTEXT("CrewSize", "CREW"), FText::Format(LOCTEXT("CrewSizeValue", "{0}+"), FText::AsNumber(Mission->RecommendedCrew)))
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							HHPlay::Fact(LOCTEXT("IntelFact", "INTEL"), IntelTotal > 0
								? FText::Format(LOCTEXT("IntelValue", "{0} / {1}"), FText::AsNumber(IntelKnown), FText::AsNumber(IntelTotal))
								: LOCTEXT("IntelNone", "-"))
						]
					]
				]
			]
			// Pin / wait for dark / why not - right under the headline so it is never scrolled away.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
			[
				BuildActionStrip(Mission, bLocked)
			]
			// Target
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 8.f)
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
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("HaulRule", "Get at least {0} worth into the van for a clean job. The target adds a bonus."), HHText::Money(Mission->MinimumHaul)))
				.AutoWrapText(true)
				.Font(FHHStyle::Font(EHHFont::Body, 13.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildIntelSection(Mission, EHHIntelKind::Valuable)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildIntelSection(Mission, EHHIntelKind::Owner)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildIntelSection(Mission, EHHIntelKind::Entry)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildIntelSection(Mission, EHHIntelKind::Security)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildIntelSection(Mission, EHHIntelKind::Note)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildGetawaySection()
			]
			// Floor plan (the crew's annotated blueprint)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 10.f)
			[
				SNew(SBox)
				.Visibility(PlanTexture ? EVisibility::Visible : EVisibility::Collapsed)
				[
					HHUI::SectionLabel(LOCTEXT("FloorPlan", "FLOOR PLAN"))
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox)
				.Visibility(PlanTexture ? EVisibility::Visible : EVisibility::Collapsed)
				.MaxDesiredHeight(420.f)
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFit)
					[
						SNew(SImage).Image(Plan.Get())
					]
				]
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
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 6.f)
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
		]);
}

// ---------------------------------------------------------------------------------------
// Crew & lobby

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
	// The host knows its code from the session; everyone else reads it from the game state.
	auto LobbyCode = [WeakHUD, Sessions]() -> FString
	{
		const AHHLobbyGameState* State = WeakHUD.IsValid() ? WeakHUD->GetLobbyState() : nullptr;
		if (State && !State->GetLobbyCode().IsEmpty())
		{
			return State->GetLobbyCode();
		}
		const UHHSessionSubsystem* Subsystem = Sessions();
		return Subsystem ? Subsystem->GetLobbyCode() : FString();
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
			// Lobby code
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 10.f)
			[
				HHUI::SectionLabel(LOCTEXT("Lobby", "LOBBY"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FHHStyle::Brush("HH.Panel"))
				.BorderBackgroundColor(FSlateColor(FHHStyle::PanelRaised()))
				.Padding(FMargin(16.f, 12.f))
				.Visibility_Lambda([LobbyCode]() { return LobbyCode().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CodeLabel", "LOBBY CODE"))
						.Font(FHHStyle::Font(EHHFont::CondensedSemi, 12.f, 220))
						.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f)
					[
						SNew(STextBlock)
						.Text_Lambda([LobbyCode]() { return FText::FromString(LobbyCode()); })
						.Font(FHHStyle::Font(EHHFont::MonoMedium, 30.f, 400))
						.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SHHActionButton)
						.Text(LOCTEXT("CopyCode", "COPY"))
						.Kind(EHHButtonKind::Ghost)
						.MinWidth(90.f)
						.OnClicked_Lambda([LobbyCode, WeakHUD]()
						{
							FPlatformApplicationMisc::ClipboardCopy(*LobbyCode());
							if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
							{
								LobbyHUD->ShowNotification(LOCTEXT("Copied", "Lobby code copied."), EHHNotifyType::Info);
							}
						})
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([NetMode]()
				{
					switch (NetMode())
					{
					case NM_ListenServer:	return LOCTEXT("StatusHost", "You are hosting. Give your friends the lobby code - they press JOIN BY CODE. (Network: same LAN with the default online subsystem; over the internet use JOIN BY IP or Steam/EOS, see README.)");
					case NM_Client:			return LOCTEXT("StatusClient", "You are in someone else's lobby.");
					default:				return LOCTEXT("StatusSolo", "Private hideout. Create a lobby to get a code for your crew, join a friend's code, or go in alone.");
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
						.Text(LOCTEXT("Host", "CREATE LOBBY"))
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
					SNew(SBox)
					.Visibility_Lambda([NetMode]() { return NetMode() == NM_Standalone ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SHHActionButton)
						.Text(LOCTEXT("JoinCode", "JOIN BY CODE"))
						.Kind(EHHButtonKind::Secondary)
						.OnClicked_Lambda([WeakHUD]()
						{
							if (AHHLobbyHUD* LobbyHUD = WeakHUD.Get())
							{
								LobbyHUD->ShowCodeEntry();
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
						.Text_Lambda([NetMode]() { return NetMode() == NM_ListenServer ? LOCTEXT("Close", "CLOSE LOBBY") : LOCTEXT("Leave", "LEAVE CREW"); })
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
		: (Results.Num() == 0 ? LOCTEXT("NoneFound", "No crews found on your network. Try a lobby code or joining by IP.") : FText::GetEmpty());
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
					.Text(Info.Code.IsEmpty()
						? FText::Format(LOCTEXT("HostedBy", "{0}'s hideout"), FText::FromString(Info.HostName))
						: FText::Format(LOCTEXT("HostedByCode", "{0}'s hideout  -  {1}"), FText::FromString(Info.HostName), FText::FromString(Info.Code)))
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

FText SHHPlayPanel::GetFooterStatus() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
	if (!State)
	{
		return FText::GetEmpty();
	}
	if (State->GetSelectedMission().IsNone())
	{
		return IsLeader()
			? LOCTEXT("NoPinnedLeader", "No job pinned yet - pick a house and press PIN.")
			: LOCTEXT("NoPinnedCrew", "Waiting for the crew leader to pin a job.");
	}
	const FText Count = FText::Format(LOCTEXT("ReadyCount", "{0} of {1} ready"), FText::AsNumber(State->NumReady()), FText::AsNumber(State->GetCrew().Num()));
	if (State->GetPhase() == EHHLobbyPhase::Countdown)
	{
		return FText::Format(LOCTEXT("Leaving", "The van leaves in {0}..."), FText::AsNumber(FMath::CeilToInt(State->GetCountdownRemaining())));
	}
	switch (State->GetDepartureBlock())
	{
	case EHHDepartureBlock::NotPlayable:
		return FText::Format(LOCTEXT("FooterNotPlayable", "{0}  -  the pinned job can't start (see the dossier)."), Count);
	case EHHDepartureBlock::NotNight:
		return FText::Format(LOCTEXT("FooterNight", "{0}  -  the van leaves after dark ({1})."), Count, HHText::ClockTime(UHHDeveloperSettings::Get()->NightStartHour));
	default:
		return Count;
	}
}

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
			.Text_Lambda([this]() { return GetFooterStatus(); })
			.AutoWrapText(true)
			.Font(FHHStyle::Font(EHHFont::Condensed, 15.f, 120))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHHKeyHint).Key(LOCTEXT("Esc", "ESC")).Label(LOCTEXT("Back", "Back"))
		];
}

#undef LOCTEXT_NAMESPACE
