#include "UI/Widgets/SHHLobbyRoot.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/Widgets/SHHMainMenu.h"
#include "UI/Widgets/SHHPlayPanel.h"
#include "UI/Widgets/SHHLoadoutPanel.h"
#include "UI/Widgets/SHHCustomizationPanel.h"
#include "UI/Widgets/SHHStorePanel.h"
#include "UI/Widgets/SHHSettingsPanel.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Lobby/HHLobbyGameState.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Player/HHInteractionComponent.h"
#include "Settings/HHGameUserSettings.h"
#include "Settings/HHInputSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/StyleDefaults.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHLobbyRoot"

void SHHLobbyRoot::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;

	PanelIntro = FCurveSequence(0.f, 0.45f, ECurveEaseFunction::CubicOut);
	MenuIntro = FCurveSequence(0.f, 0.6f, ECurveEaseFunction::CubicOut);

	ChildSlot
	[
		SNew(SOverlay)

		// Cinematic vignette, always on (very subtle while exploring).
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FHHStyle::Brush("HH.Vignette"))
			.Visibility(EVisibility::HitTestInvisible)
			.ColorAndOpacity_Lambda([this]() { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, IsExploring() ? 0.45f : 0.85f)); })
		]

		// First-person HUD.
		+ SOverlay::Slot()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsExploring() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			[
				BuildExplorationHud()
			]
		]

		// Main menu (left column).
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.Padding(0.f)
			.Visibility_Lambda([this]() { return CurrentScreen == EHHLobbyScreen::MainMenu && !bTitleVisible ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, MenuIntro.GetLerp()); })
			[
				SAssignNew(MenuHost, SBox)
			]
		]

		// Screen panels.
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.Padding(0.f)
			.Visibility_Lambda([this]() { return CurrentScreen != EHHLobbyScreen::None && CurrentScreen != EHHLobbyScreen::MainMenu ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, PanelIntro.GetLerp()); })
			.RenderTransform_Lambda([this]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f((1.f - PanelIntro.GetLerp()) * 40.f, 0.f))); })
			[
				SAssignNew(PanelHost, SBox)
			]
		]

		// Toasts, captions, countdown.
		+ SOverlay::Slot()
		[
			BuildFeedbackLayer()
		]

		// Title card.
		+ SOverlay::Slot()
		[
			BuildTitle()
		]

		// Modals.
		+ SOverlay::Slot()
		[
			SAssignNew(ModalLayer, SOverlay)
			.Visibility(EVisibility::SelfHitTestInvisible)
		]

		// Fade to / from black.
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FHHStyle::Brush("HH.White"))
			.Visibility(EVisibility::HitTestInvisible)
			.ColorAndOpacity_Lambda([this]() { return FSlateColor(FLinearColor(0.f, 0.f, 0.f, Fade)); })
		]
	];

	SetScreen(EHHLobbyScreen::None);
}

bool SHHLobbyRoot::IsExploring() const
{
	return CurrentScreen == EHHLobbyScreen::None && !bTitleVisible;
}

// ---------------------------------------------------------------------------------------
// Screens

TSharedRef<SWidget> SHHLobbyRoot::BuildPanel(EHHLobbyScreen Screen)
{
	switch (Screen)
	{
	case EHHLobbyScreen::Play:			return SNew(SHHPlayPanel).HUD(HUD);
	case EHHLobbyScreen::Loadout:		return SNew(SHHLoadoutPanel).HUD(HUD);
	case EHHLobbyScreen::Customization:	return SNew(SHHCustomizationPanel).HUD(HUD);
	case EHHLobbyScreen::Store:			return SNew(SHHStorePanel).HUD(HUD);
	case EHHLobbyScreen::Settings:		return SNew(SHHSettingsPanel).HUD(HUD);
	default:							return SNullWidget::NullWidget;
	}
}

void SHHLobbyRoot::SetScreen(EHHLobbyScreen Screen)
{
	const EHHLobbyScreen Previous = CurrentScreen;
	CurrentScreen = Screen;

	if (Screen == EHHLobbyScreen::MainMenu)
	{
		MenuHost->SetContent(SNew(SHHMainMenu).HUD(HUD));
		if (Previous != EHHLobbyScreen::MainMenu)
		{
			MenuIntro.Play(AsShared());
		}
	}
	else
	{
		MenuHost->SetContent(SNullWidget::NullWidget);
	}

	if (Screen != EHHLobbyScreen::None && Screen != EHHLobbyScreen::MainMenu)
	{
		PanelHost->SetContent(BuildPanel(Screen));
		PanelIntro.Play(AsShared());
	}
	else
	{
		PanelHost->SetContent(SNullWidget::NullWidget);
	}
}

// ---------------------------------------------------------------------------------------
// Input

FReply SHHLobbyRoot::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	AHHLobbyHUD* LobbyHUD = HUD.Get();
	const FKey Key = InKeyEvent.GetKey();

	if (bTitleVisible)
	{
		if (LobbyHUD)
		{
			LobbyHUD->DismissTitle();
		}
		return FReply::Handled();
	}

	const bool bBack = Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_FaceButton_Right;
	if (HasModal())
	{
		if (bBack && ModalBack.IsBound())
		{
			HHUI::PlayUISound(EHHUISound::Back);
			// Copy first: the delegate usually closes the modal, which unbinds it.
			const FSimpleDelegate Back = ModalBack;
			Back.ExecuteIfBound();
		}
		return FReply::Handled();
	}

	if (bBack && LobbyHUD)
	{
		HHUI::PlayUISound(EHHUISound::Back);
		LobbyHUD->Back();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SHHLobbyRoot::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bTitleVisible)
	{
		if (AHHLobbyHUD* LobbyHUD = HUD.Get())
		{
			LobbyHUD->DismissTitle();
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// ---------------------------------------------------------------------------------------
// Feedback

void SHHLobbyRoot::PushToast(const FText& Message, EHHNotifyType Type)
{
	FLinearColor Color = FHHStyle::Info();
	FName Icon = TEXT("HH.Icon.Info");
	switch (Type)
	{
	case EHHNotifyType::Success:	Color = FHHStyle::Ready(); Icon = TEXT("HH.Icon.Check"); break;
	case EHHNotifyType::Warning:	Color = FHHStyle::Accent(); Icon = TEXT("HH.Icon.Warning"); break;
	case EHHNotifyType::Error:		Color = FHHStyle::Danger(); Icon = TEXT("HH.Icon.Warning"); break;
	case EHHNotifyType::Crew:		Color = FHHStyle::Accent(); Icon = TEXT("HH.Icon.Person"); break;
	default: break;
	}

	FToast Toast;
	Toast.Created = Now;
	Toast.Expires = Now + 5.0;
	const double Created = Now;

	Toast.Widget = SNew(SBox)
		.WidthOverride(380.f)
		.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
		[
			SNew(SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.Padding(0.f)
			.ColorAndOpacity_Lambda([this, Created]()
			{
				const double Age = Now - Created;
				const float In = FMath::Clamp(static_cast<float>(Age / 0.25), 0.f, 1.f);
				const float Out = FMath::Clamp(static_cast<float>((5.0 - Age) / 0.6), 0.f, 1.f);
				return FLinearColor(1.f, 1.f, 1.f, FMath::Min(In, Out));
			})
			[
			SNew(SBorder)
			.BorderImage(FHHStyle::Brush("HH.PanelFrame"))
			.Padding(FMargin(0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(2.f)
					[
						SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(Color))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 12.f, 10.f, 12.f)
				[
					SNew(SImage).Image(FHHStyle::Brush(Icon)).DesiredSizeOverride(FVector2D(16.0, 16.0)).ColorAndOpacity(FSlateColor(Color))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 10.f, 14.f, 10.f)
				[
					SNew(STextBlock)
					.Text(Message)
					.AutoWrapText(true)
					.Font(FHHStyle::Font(EHHFont::Body, 14.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
				]
			]
			]
		];

	ToastBox->AddSlot().AutoHeight()[Toast.Widget.ToSharedRef()];
	Toasts.Add(Toast);

	// Keep the stack short.
	while (Toasts.Num() > 4)
	{
		ToastBox->RemoveSlot(Toasts[0].Widget.ToSharedRef());
		Toasts.RemoveAt(0);
	}
}

void SHHLobbyRoot::ShowCaption(const FText& Text, float Duration, bool bIsSoundCaption)
{
	CaptionText = Text;
	CaptionExpires = Now + FMath::Max(Duration, 1.f);
	bCaptionIsSound = bIsSoundCaption;
}

TSharedRef<SWidget> SHHLobbyRoot::BuildFeedbackLayer()
{
	return SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)

		// Toasts (top right)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 120.f, 36.f, 0.f))
		[
			SAssignNew(ToastBox, SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)
		]

		// Captions / subtitles (bottom centre)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 150.f))
		[
			SNew(SBorder)
			.BorderImage(FHHStyle::Brush("HH.White"))
			.Visibility_Lambda([this]() { return Now < CaptionExpires && !CaptionText.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.BorderBackgroundColor_Lambda([]()
			{
				const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
				return FSlateColor(FLinearColor(0.f, 0.f, 0.f, Settings ? Settings->SubtitleBackgroundOpacity : 0.55f));
			})
			.Padding(FMargin(16.f, 8.f))
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return CaptionText; })
				.WrapTextAt(860.f)
				.Justification(ETextJustify::Center)
				.Font_Lambda([this]()
				{
					const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
					const float Scale = Settings ? Settings->GetSubtitleFontScale() : 1.f;
					return FHHStyle::Font(bCaptionIsSound ? EHHFont::BodyMedium : EHHFont::Body, 19.f * Scale);
				})
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(bCaptionIsSound ? FHHStyle::Paper() : FHHStyle::Text()); })
			]
		]

		// Departure countdown (top centre)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 70.f, 0.f, 0.f))
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this]()
			{
				const AHHLobbyHUD* LobbyHUD = HUD.Get();
				const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
				return State && State->GetPhase() != EHHLobbyPhase::Gathering && !bTitleVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					const AHHLobbyHUD* LobbyHUD = HUD.Get();
					const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
					return State && State->GetPhase() == EHHLobbyPhase::Departing ? LOCTEXT("OnTheRoad", "ON THE ROAD") : LOCTEXT("DepartingIn", "THE VAN LEAVES IN");
				})
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 14.f, 420))
				.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					const AHHLobbyHUD* LobbyHUD = HUD.Get();
					const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
					if (!State || State->GetPhase() == EHHLobbyPhase::Departing)
					{
						return FText::GetEmpty();
					}
					return FText::AsNumber(FMath::CeilToInt(State->GetCountdownRemaining()));
				})
				.Font(FHHStyle::Font(EHHFont::Display, 72.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CancelHint", "Change your mind? Step out of the van or press Ready again."))
				.Font(FHHStyle::Font(EHHFont::Body, 13.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
		]

		// Voice transmit indicator (bottom right)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 36.f, 36.f))
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]()
			{
				const AHHLobbyHUD* LobbyHUD = HUD.Get();
				const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
				return PC && PC->IsTransmittingVoice() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SImage).Image(FHHStyle::Brush("HH.Icon.Mic")).DesiredSizeOverride(FVector2D(18.0, 18.0)).ColorAndOpacity(FSlateColor(FHHStyle::Ready()))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("OnAir", "ON AIR")).Font(FHHStyle::Font(EHHFont::CondensedSemi, 12.f, 300)).ColorAndOpacity(FSlateColor(FHHStyle::Ready()))
			]
		];
}

// ---------------------------------------------------------------------------------------
// Exploration HUD

FText SHHLobbyRoot::GetInteractKeyText() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
	const UHHInputSubsystem* Input = PC ? UHHInputSubsystem::Get(PC) : nullptr;
	return Input ? Input->GetKeyDisplayText(TEXT("Interact")) : FText::FromString(TEXT("E"));
}

TSharedRef<SWidget> SHHLobbyRoot::BuildExplorationHud()
{
	auto Focused = [this]() -> bool
	{
		const AHHLobbyHUD* LobbyHUD = HUD.Get();
		const UHHInteractionComponent* Interaction = LobbyHUD ? LobbyHUD->GetInteraction() : nullptr;
		return Interaction && Interaction->GetFocusedActor() != nullptr && !Interaction->GetFocusedPrompt().IsEmpty();
	};

	auto KeyText = [this](const TCHAR* Binding) -> TAttribute<FText>
	{
		const FName BindingName(Binding);
		return TAttribute<FText>::CreateLambda([this, BindingName]()
		{
			const AHHLobbyHUD* LobbyHUD = HUD.Get();
			const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
			const UHHInputSubsystem* Input = PC ? UHHInputSubsystem::Get(PC) : nullptr;
			return Input ? Input->GetKeyDisplayText(BindingName) : FText::GetEmpty();
		});
	};

	return SNew(SOverlay)

		// Centre dot
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FHHStyle::Brush("HH.Icon.Dot"))
			.DesiredSizeOverride_Lambda([Focused]() { return TOptional<FVector2D>(Focused() ? FVector2D(8.0, 8.0) : FVector2D(4.0, 4.0)); })
			.ColorAndOpacity_Lambda([Focused]() { return FSlateColor(Focused() ? FHHStyle::Accent() : FLinearColor(1.f, 1.f, 1.f, 0.28f)); })
		]

		// Interaction prompt
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 96.f))
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([Focused]()
			{
				const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
				const bool bAllowed = !Settings || Settings->bShowInteractionPrompts;
				return bAllowed && Focused() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(FHHStyle::Brush("HH.KeyCap"))
				.Padding(FMargin(9.f, 3.f))
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return GetInteractKeyText(); })
					.Font(FHHStyle::Font(EHHFont::MonoMedium, 13.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					const AHHLobbyHUD* LobbyHUD = HUD.Get();
					const UHHInteractionComponent* Interaction = LobbyHUD ? LobbyHUD->GetInteraction() : nullptr;
					return Interaction ? Interaction->GetFocusedPrompt() : FText::GetEmpty();
				})
				.Font(FHHStyle::Font(EHHFont::BodyMedium, 17.f))
				.ShadowOffset(FVector2D(0.0, 1.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
				.ColorAndOpacity_Lambda([this]()
				{
					const AHHLobbyHUD* LobbyHUD = HUD.Get();
					const UHHInteractionComponent* Interaction = LobbyHUD ? LobbyHUD->GetInteraction() : nullptr;
					return FSlateColor(Interaction && Interaction->CanInteractWithFocus() ? FHHStyle::Text() : FHHStyle::TextFaint());
				})
			]
		]

		// Crew strip (top left)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(36.f, 32.f, 0.f, 0.f))
		[
			BuildCrewStrip()
		]

		// Key hints (bottom left)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(36.f, 0.f, 0.f, 32.f))
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([]()
			{
				const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
				return !Settings || Settings->bShowCrewHUD ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 22.f, 0.f)
			[
				SNew(SHHKeyHint).Key(LOCTEXT("EscKey", "ESC")).Label(LOCTEXT("Menu", "Menu"))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 22.f, 0.f)
			[
				SNew(SHHKeyHint).Key(KeyText(TEXT("Map"))).Label(LOCTEXT("JobBoard", "Job board"))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 22.f, 0.f)
			[
				SNew(SHHKeyHint).Key(KeyText(TEXT("Inventory"))).Label(LOCTEXT("Loadout", "Loadout"))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 22.f, 0.f)
			[
				SNew(SHHKeyHint).Key(KeyText(TEXT("Ready"))).Label(LOCTEXT("Ready", "Ready"))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SHHKeyHint).Key(KeyText(TEXT("Flashlight"))).Label(LOCTEXT("Flashlight", "Flashlight"))
			]
		];
}

TSharedRef<SWidget> SHHLobbyRoot::BuildCrewStrip()
{
	return SNew(SBox)
		.Visibility_Lambda([]()
		{
			const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
			return !Settings || Settings->bShowCrewHUD ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			SAssignNew(CrewStripBox, SVerticalBox)
		];
}

// ---------------------------------------------------------------------------------------
// Title card

void SHHLobbyRoot::SetTitleVisible(bool bVisible)
{
	bTitleVisible = bVisible;
	if (bVisible)
	{
		TitleShownAt = Now;
	}
}

TSharedRef<SWidget> SHHLobbyRoot::BuildTitle()
{
	auto Age = [this]() { return static_cast<float>(Now - TitleShownAt); };

	return SNew(SOverlay)
		.Visibility_Lambda([this]() { return bTitleVisible ? EVisibility::Visible : EVisibility::Collapsed; })

		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FHHStyle::Brush("HH.GradientLeft"))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.9f)))
		]

		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(110.f, 0.f, 0.f, 40.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "THE QUIET JOB"))
				.Font(FHHStyle::Font(EHHFont::Display, 92.f, 260))
				.ColorAndOpacity_Lambda([Age]()
				{
					// Fade in like a failing neon tube.
					const float T = Age();
					float Alpha = FMath::Clamp((T - 0.6f) / 2.2f, 0.f, 1.f);
					if (T < 3.2f)
					{
						const float Flicker = FMath::PerlinNoise1D(T * 11.f);
						Alpha *= Flicker > -0.35f ? 1.f : 0.25f;
					}
					return FSlateColor(FHHStyle::WithAlpha(FHHStyle::Text(), Alpha));
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 14.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Tagline1", "We came here to steal something."))
				.Font(FHHStyle::Font(EHHFont::Typewriter, 19.f))
				.ColorAndOpacity_Lambda([Age]() { return FSlateColor(FHHStyle::WithAlpha(FHHStyle::TextDim(), FMath::Clamp((Age() - 2.6f) / 1.2f, 0.f, 1.f))); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Tagline2", "Something was already here."))
				.Font(FHHStyle::Font(EHHFont::Typewriter, 19.f))
				.ColorAndOpacity_Lambda([Age]() { return FSlateColor(FHHStyle::WithAlpha(FHHStyle::Danger(), FMath::Clamp((Age() - 4.4f) / 1.6f, 0.f, 0.9f))); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 70.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PressAnyKey", "PRESS ANY KEY"))
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 14.f, 600))
				.ColorAndOpacity_Lambda([Age]()
				{
					const float T = Age();
					const float Visible = FMath::Clamp((T - 5.2f) / 0.8f, 0.f, 1.f);
					const float Pulse = 0.55f + 0.45f * FMath::Sin(T * 2.4f);
					return FSlateColor(FHHStyle::WithAlpha(FHHStyle::Accent(), Visible * Pulse));
				})
			]
		]

		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(110.f, 0.f, 0.f, 46.f))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TitleFooter", "HOLLOWMERE   \u00B7   AFTER MIDNIGHT"))
			.Font(FHHStyle::Font(EHHFont::Condensed, 12.f, 500))
			.ColorAndOpacity_Lambda([Age]() { return FSlateColor(FHHStyle::WithAlpha(FHHStyle::TextFaint(), FMath::Clamp((Age() - 3.f) / 2.f, 0.f, 1.f))); })
		];
}

// ---------------------------------------------------------------------------------------
// Modals & fades

void SHHLobbyRoot::ShowModal(TSharedRef<SWidget> Content, FSimpleDelegate OnBack)
{
	ModalBack = OnBack;
	ModalLayer->ClearChildren();
	ModalLayer->AddSlot()
	[
		SNew(SImage)
		.Image(FHHStyle::Brush("HH.White"))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.72f)))
	];
	ModalLayer->AddSlot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		Content
	];
}

void SHHLobbyRoot::CloseModal()
{
	ModalBack.Unbind();
	ModalLayer->ClearChildren();
	FSlateApplication::Get().SetKeyboardFocus(AsShared(), EFocusCause::SetDirectly);
}

bool SHHLobbyRoot::HasModal() const
{
	return ModalLayer.IsValid() && ModalLayer->GetNumWidgets() > 0;
}

void SHHLobbyRoot::FadeFromBlack(float Seconds)
{
	Fade = 1.f;
	FadeTarget = 0.f;
	FadeSpeed = 1.f / FMath::Max(Seconds, 0.05f);
}

void SHHLobbyRoot::FadeToBlack(float Seconds)
{
	FadeTarget = 1.f;
	FadeSpeed = 1.f / FMath::Max(Seconds, 0.05f);
}

// ---------------------------------------------------------------------------------------
// Tick

void SHHLobbyRoot::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Now = InCurrentTime;

	Fade = FMath::FInterpConstantTo(Fade, FadeTarget, InDeltaTime, FadeSpeed);

	// Expire toasts.
	for (int32 Index = Toasts.Num() - 1; Index >= 0; --Index)
	{
		if (Now > Toasts[Index].Expires)
		{
			ToastBox->RemoveSlot(Toasts[Index].Widget.ToSharedRef());
			Toasts.RemoveAt(Index);
		}
	}

	// Rebuild the crew strip when the crew changes.
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHLobbyGameState* State = LobbyHUD ? LobbyHUD->GetLobbyState() : nullptr;
	const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
	if (State && CrewStripBox.IsValid())
	{
		const TArray<AHHPlayerState*> Crew = State->GetCrew();
		int32 Signature = Crew.Num();
		for (const AHHPlayerState* Member : Crew)
		{
			Signature = HashCombine(Signature, GetTypeHash(Member->GetPlayerName()));
			Signature = HashCombine(Signature, Member->IsReady() ? 7 : 3);
			Signature = HashCombine(Signature, (PC && PC->IsPlayerTalking(Member)) ? 11 : 5);
		}
		if (Signature != CrewSignature)
		{
			CrewSignature = Signature;
			CrewStripBox->ClearChildren();
			for (const AHHPlayerState* Member : Crew)
			{
				const bool bTalking = PC && PC->IsPlayerTalking(Member);
				CrewStripBox->AddSlot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(3.f).HeightOverride(16.f)
						[
							SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(Member->GetCrewColor()))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Member->GetPlayerName()))
						.Font(FHHStyle::Font(EHHFont::BodyMedium, 14.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
						.ShadowOffset(FVector2D(0.0, 1.0))
						.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Member->IsReady() ? LOCTEXT("ReadyTag", "READY") : LOCTEXT("NotReadyTag", "NOT READY"))
						.Font(FHHStyle::Font(EHHFont::CondensedSemi, 11.f, 200))
						.ColorAndOpacity(FSlateColor(Member->IsReady() ? FHHStyle::Ready() : FHHStyle::TextFaint()))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(SImage)
						.Image(FHHStyle::Brush("HH.Icon.Mic"))
						.DesiredSizeOverride(FVector2D(14.0, 14.0))
						.ColorAndOpacity(FSlateColor(FHHStyle::Ready()))
						.Visibility(bTalking ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					]
				];
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
