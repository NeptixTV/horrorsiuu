#include "UI/Widgets/SHHSettingsPanel.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Player/HHPlayerController.h"
#include "Settings/HHGameUserSettings.h"
#include "Settings/HHInputSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindowDefinition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHSettingsPanel"

namespace
{
	const TArray<FText>& QualityNames()
	{
		static const TArray<FText> Names = {
			LOCTEXT("Low", "Low"), LOCTEXT("Medium", "Medium"), LOCTEXT("High", "High"), LOCTEXT("Epic", "Epic"), LOCTEXT("Cinematic", "Cinematic") };
		return Names;
	}

	const TArray<FText>& OnOff()
	{
		static const TArray<FText> Names = { LOCTEXT("Off", "Off"), LOCTEXT("On", "On") };
		return Names;
	}

	TSharedRef<SWidget> Section(const FText& Title)
	{
		return SNew(SBox)
			.Padding(FMargin(0.f, 18.f, 0.f, 8.f))
			[
				HHUI::SectionLabel(Title)
			];
	}

	const int32 FrameLimits[] = { 30, 60, 90, 120, 144, 165, 240, 0 };
	const int32 AntiAliasingValues[] = { 0, 1, 2, 4 };
}

void SHHSettingsPanel::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;

	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	if (Resolutions.Num() == 0)
	{
		Resolutions = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160) };
	}

	TSharedRef<SWidget> Header = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			HHUI::PanelHeader(FText::FromString(TEXT("05")), LOCTEXT("Title", "SETTINGS"),
				LOCTEXT("Subtitle", "Tune the night to your liking."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
		[
			SNew(SHHTabBar)
			.Tabs({ LOCTEXT("Graphics", "GRAPHICS"), LOCTEXT("Audio", "AUDIO"), LOCTEXT("Controls", "CONTROLS"), LOCTEXT("Gameplay", "GAMEPLAY"), LOCTEXT("Accessibility", "ACCESSIBILITY") })
			.Selected_Lambda([this]() { return Tab; })
			.OnSelected_Lambda([this](int32 Index) { SetTab(Index); })
		];

	TSharedRef<SWidget> Footer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHHActionButton)
			.Text(LOCTEXT("Defaults", "RESTORE DEFAULTS"))
			.Kind(EHHButtonKind::Ghost)
			.OnClicked_Lambda([this]() { RestoreDefaults(); })
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(16.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SavedHint", "Changes apply and save immediately."))
			.Font(FHHStyle::Font(EHHFont::Body, 13.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHHKeyHint).Key(LOCTEXT("Esc", "ESC")).Label(LOCTEXT("Back", "Back"))
		];

	ChildSlot
	[
		HHUI::PanelFrame(Header, SAssignNew(TabHost, SBox), Footer, nullptr, 0.6f)
	];

	SetTab(0);
}

void SHHSettingsPanel::SetTab(int32 NewTab)
{
	Tab = NewTab;
	TSharedRef<SWidget> Content = SNullWidget::NullWidget;
	switch (Tab)
	{
	case 0: Content = BuildGraphics(); break;
	case 1: Content = BuildAudio(); break;
	case 2: Content = BuildControls(); break;
	case 3: Content = BuildGameplay(); break;
	default: Content = BuildAccessibility(); break;
	}
	TabHost->SetContent(
		SNew(SScrollBox)
		.ScrollBarThickness(FVector2D(2.0, 2.0))
		+ SScrollBox::Slot().Padding(FMargin(0.f, 0.f, 14.f, 0.f))
		[
			Content
		]);
}

void SHHSettingsPanel::Change(TFunctionRef<void(UHHGameUserSettings&)> Mutator)
{
	if (UHHGameUserSettings* Settings = UHHGameUserSettings::Get())
	{
		Mutator(*Settings);
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
	}
}

// ---------------------------------------------------------------------------------------
// Row helpers

TSharedRef<SWidget> SHHSettingsPanel::Toggle(const FText& Label, const FText& Tooltip, TFunction<bool(const UHHGameUserSettings&)> Get, TFunction<void(UHHGameUserSettings&, bool)> Set)
{
	return SNew(SHHOptionRow)
		.Label(Label)
		.Tooltip(Tooltip)
		.Options(OnOff())
		.Value_Lambda([Get]() { const UHHGameUserSettings* S = UHHGameUserSettings::Get(); return S && Get(*S) ? 1 : 0; })
		.OnChanged_Lambda([this, Set](int32 Index) { Change([&](UHHGameUserSettings& S) { Set(S, Index == 1); }); });
}

TSharedRef<SWidget> SHHSettingsPanel::Quality(const FText& Label, const FText& Tooltip, TFunction<int32(const UHHGameUserSettings&)> Get, TFunction<void(UHHGameUserSettings&, int32)> Set)
{
	return SNew(SHHOptionRow)
		.Label(Label)
		.Tooltip(Tooltip)
		.Options(QualityNames())
		.Value_Lambda([Get]() { const UHHGameUserSettings* S = UHHGameUserSettings::Get(); return S ? Get(*S) : 2; })
		.OnChanged_Lambda([this, Set](int32 Index) { Change([&](UHHGameUserSettings& S) { Set(S, Index); }); });
}

TSharedRef<SWidget> SHHSettingsPanel::Slider(const FText& Label, const FText& Tooltip, float Min, float Max, float Step, float DisplayScale, int32 Decimals, const FText& Suffix,
	TFunction<float(const UHHGameUserSettings&)> Get, TFunction<void(UHHGameUserSettings&, float)> Set)
{
	return SNew(SHHSliderRow)
		.Label(Label)
		.Tooltip(Tooltip)
		.Min(Min)
		.Max(Max)
		.Step(Step)
		.DisplayScale(DisplayScale)
		.DisplayDecimals(Decimals)
		.Suffix(Suffix)
		.Value_Lambda([Get]() { const UHHGameUserSettings* S = UHHGameUserSettings::Get(); return S ? Get(*S) : 0.f; })
		.OnChanged_Lambda([this, Set](float Value) { Change([&](UHHGameUserSettings& S) { Set(S, Value); }); });
}

// ---------------------------------------------------------------------------------------
// Graphics

TSharedRef<SWidget> SHHSettingsPanel::BuildGraphics()
{
	TArray<FText> ResolutionNames;
	for (const FIntPoint& Resolution : Resolutions)
	{
		ResolutionNames.Add(FText::Format(LOCTEXT("ResFmt", "{0} x {1}"), FText::AsNumber(Resolution.X, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(Resolution.Y, &FNumberFormattingOptions::DefaultNoGrouping())));
	}

	TArray<FText> FrameNames;
	for (int32 Limit : FrameLimits)
	{
		FrameNames.Add(Limit > 0 ? FText::AsNumber(Limit) : LOCTEXT("Unlimited", "Unlimited"));
	}

	// Only offer upscalers whose plugin is actually present.
	TArray<FText> UpscalerNames = { LOCTEXT("UpscalerEngine", "Unreal TSR") };
	TArray<int32> UpscalerModes = { 0 };
	const FText PluginNames[] = { LOCTEXT("DLSS", "NVIDIA DLSS"), LOCTEXT("FSR", "AMD FSR"), LOCTEXT("XeSS", "Intel XeSS") };
	for (int32 Mode = 1; Mode <= 3; ++Mode)
	{
		if (UHHGameUserSettings::IsUpscalerAvailable(Mode))
		{
			UpscalerNames.Add(PluginNames[Mode - 1]);
			UpscalerModes.Add(Mode);
		}
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Display", "DISPLAY"))];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHOptionRow)
		.Label(LOCTEXT("WindowMode", "Display Mode"))
		.Options({ LOCTEXT("Fullscreen", "Fullscreen"), LOCTEXT("Borderless", "Borderless Window"), LOCTEXT("Windowed", "Windowed") })
		.Value_Lambda([this]()
		{
			const UHHGameUserSettings* S = UHHGameUserSettings::Get();
			return PendingWindowMode != INDEX_NONE ? PendingWindowMode : (S ? static_cast<int32>(S->GetFullscreenMode()) : 1);
		})
		.OnChanged_Lambda([this](int32 Index) { PendingWindowMode = Index; })
	];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHOptionRow)
		.Label(LOCTEXT("Resolution", "Resolution"))
		.Options(ResolutionNames)
		.Value_Lambda([this]()
		{
			if (PendingResolution != INDEX_NONE)
			{
				return PendingResolution;
			}
			const UHHGameUserSettings* S = UHHGameUserSettings::Get();
			return S ? Resolutions.IndexOfByKey(S->GetScreenResolution()) : INDEX_NONE;
		})
		.OnChanged_Lambda([this](int32 Index) { PendingResolution = Index; })
	];
	Box->AddSlot().AutoHeight().Padding(12.f, 8.f, 0.f, 4.f).HAlign(HAlign_Left)
	[
		SNew(SHHActionButton)
		.Text(LOCTEXT("ApplyDisplay", "APPLY DISPLAY CHANGES"))
		.Kind(EHHButtonKind::Secondary)
		.Height(36.f)
		.ClickSound(EHHUISound::Confirm)
		.OnClicked_Lambda([this]() { ApplyDisplay(); })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("VSync", "VSync"), LOCTEXT("VSyncTip", "Prevents tearing. Adds a little input latency."),
			[](const UHHGameUserSettings& S) { return S.IsVSyncEnabled(); },
			[](UHHGameUserSettings& S, bool b) { S.SetVSyncEnabled(b); })
	];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHOptionRow)
		.Label(LOCTEXT("FrameLimit", "Frame Rate Limit"))
		.Options(FrameNames)
		.Value_Lambda([]()
		{
			const UHHGameUserSettings* S = UHHGameUserSettings::Get();
			const int32 Current = S ? FMath::RoundToInt(S->GetFrameRateLimit()) : 0;
			for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(FrameLimits)); ++Index)
			{
				if (FrameLimits[Index] == Current)
				{
					return Index;
				}
			}
			return static_cast<int32>(UE_ARRAY_COUNT(FrameLimits)) - 1;
		})
		.OnChanged_Lambda([this](int32 Index) { Change([Index](UHHGameUserSettings& S) { S.SetFrameRateLimit(static_cast<float>(FrameLimits[Index])); }); })
	];
	Box->AddSlot().AutoHeight()
	[
		Slider(LOCTEXT("Fov", "Field of View"), LOCTEXT("FovTip", "Horizontal field of view in first person."),
			70.f, 110.f, 1.f, 1.f, 0, LOCTEXT("Deg", "\u00B0"),
			[](const UHHGameUserSettings& S) { return S.FieldOfView; },
			[](UHHGameUserSettings& S, float V) { S.FieldOfView = V; })
	];
	Box->AddSlot().AutoHeight()
	[
		Slider(LOCTEXT("Brightness", "Brightness"), LOCTEXT("BrightnessTip", "Raise until the darkest corner is barely visible, not more. Darkness is part of the game."),
			-1.f, 1.f, 0.05f, 100.f, 0, FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.Brightness; },
			[](UHHGameUserSettings& S, float V) { S.Brightness = V; })
	];
	Box->AddSlot().AutoHeight()
	[
		Slider(LOCTEXT("Gamma", "Gamma"), LOCTEXT("GammaTip", "Display gamma (2.2 is standard)."),
			1.8f, 2.6f, 0.05f, 1.f, 2, FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.DisplayGamma; },
			[](UHHGameUserSettings& S, float V) { S.DisplayGamma = V; })
	];

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Quality", "QUALITY"))];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHOptionRow)
		.Label(LOCTEXT("Preset", "Quality Preset"))
		.Tooltip(LOCTEXT("PresetTip", "Sets all quality groups at once."))
		.Options(QualityNames())
		.Value_Lambda([]() { const UHHGameUserSettings* S = UHHGameUserSettings::Get(); return S ? S->GetOverallScalabilityLevel() : 2; })
		.OnChanged_Lambda([this](int32 Index) { Change([Index](UHHGameUserSettings& S) { S.SetOverallScalabilityLevel(Index); }); })
	];
	Box->AddSlot().AutoHeight().Padding(12.f, 6.f, 0.f, 8.f).HAlign(HAlign_Left)
	[
		SNew(SHHActionButton)
		.Text(LOCTEXT("Benchmark", "AUTO-DETECT FOR THIS PC"))
		.Kind(EHHButtonKind::Secondary)
		.Height(36.f)
		.OnClicked_Lambda([this]()
		{
			Change([](UHHGameUserSettings& S)
			{
				S.RunHardwareBenchmark();
				S.ApplyHardwareBenchmarkResults();
			});
			SetTab(0);
		})
	];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHSliderRow)
		.Label(LOCTEXT("RenderScale", "Render Resolution"))
		.Tooltip(LOCTEXT("RenderScaleTip", "Internal resolution; the upscaler reconstructs the rest."))
		.Min(50.f)
		.Max(100.f)
		.Step(1.f)
		.DisplayScale(1.f)
		.Suffix(LOCTEXT("Percent", "%"))
		.Value_Lambda([]()
		{
			float Normalized = 1.f, Value = 100.f, MinValue = 50.f, MaxValue = 100.f;
			if (const UHHGameUserSettings* S = UHHGameUserSettings::Get())
			{
				S->GetResolutionScaleInformationEx(Normalized, Value, MinValue, MaxValue);
			}
			return Value;
		})
		.OnChanged_Lambda([this](float Value) { Change([Value](UHHGameUserSettings& S) { S.SetResolutionScaleValueEx(Value); }); })
	];
	if (UpscalerNames.Num() > 1)
	{
		Box->AddSlot().AutoHeight()
		[
			SNew(SHHOptionRow)
			.Label(LOCTEXT("Upscaler", "Upscaler"))
			.Options(UpscalerNames)
			.Value_Lambda([UpscalerModes]() { const UHHGameUserSettings* S = UHHGameUserSettings::Get(); return S ? FMath::Max(0, UpscalerModes.IndexOfByKey(S->UpscalerMode)) : 0; })
			.OnChanged_Lambda([this, UpscalerModes](int32 Index) { Change([&](UHHGameUserSettings& S) { S.UpscalerMode = UpscalerModes.IsValidIndex(Index) ? UpscalerModes[Index] : 0; }); })
		];
	}
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHOptionRow)
		.Label(LOCTEXT("AAMethod", "Anti-Aliasing"))
		.Options({ LOCTEXT("AAOff", "Off"), LOCTEXT("FXAA", "FXAA"), LOCTEXT("TAA", "TAA"), LOCTEXT("TSR", "TSR") })
		.Value_Lambda([]()
		{
			const UHHGameUserSettings* S = UHHGameUserSettings::Get();
			const int32 Method = S ? S->AntiAliasingMethod : 4;
			for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(AntiAliasingValues)); ++Index)
			{
				if (AntiAliasingValues[Index] == Method)
				{
					return Index;
				}
			}
			return 3;
		})
		.OnChanged_Lambda([this](int32 Index) { Change([Index](UHHGameUserSettings& S) { S.AntiAliasingMethod = AntiAliasingValues[FMath::Clamp(Index, 0, 3)]; }); })
	];
	Box->AddSlot().AutoHeight()
	[
		Quality(LOCTEXT("AAQuality", "Anti-Aliasing Quality"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.GetAntiAliasingQuality(); },
			[](UHHGameUserSettings& S, int32 V) { S.SetAntiAliasingQuality(V); })
	];
	Box->AddSlot().AutoHeight()
	[
		Quality(LOCTEXT("ViewDistance", "View Distance"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.GetViewDistanceQuality(); },
			[](UHHGameUserSettings& S, int32 V) { S.SetViewDistanceQuality(V); })
	];
	Box->AddSlot().AutoHeight()
	[
		Quality(LOCTEXT("Shadows", "Shadows"), LOCTEXT("ShadowsTip", "Virtual shadow map resolution and soft shadow quality."),
			[](const UHHGameUserSettings& S) { return S.GetShadowQuality(); },
			[](UHHGameUserSettings& S, int32 V) { S.SetShadowQuality(V); })
	];
	Box->AddSlot().AutoHeight()
	[
		Quality(LOCTEXT("GI", "Global Illumination"), LOCTEXT("GITip", "Lumen bounce lighting. Low falls back to screen space."),
			[](const UHHGameUserSettings& S) { return S.GetGlobalIlluminationQuality(); },
			[](UHHGameUserSettings& S, int32 V) { S.SetGlobalIlluminationQuality(V); })
	];
	Box->AddSlot().AutoHeight()
	[
		Quality(LOCTEXT("Reflections", "Reflections"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.GetReflectionQuality(); },
			[](UHHGameUserSettings& S, int32 V) { S.SetReflectionQuality(V); })
	];
	Box->AddSlot().AutoHeight()
	[
		Quality(LOCTEXT("Textures", "Textures"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.GetTextureQuality(); },
			[](UHHGameUserSettings& S, int32 V) { S.SetTextureQuality(V); })
	];
	Box->AddSlot().AutoHeight()
	[
		Quality(LOCTEXT("Effects", "Effects"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.GetVisualEffectQuality(); },
			[](UHHGameUserSettings& S, int32 V) { S.SetVisualEffectQuality(V); })
	];
	Box->AddSlot().AutoHeight()
	[
		Quality(LOCTEXT("PostProcess", "Post Processing"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.GetPostProcessingQuality(); },
			[](UHHGameUserSettings& S, int32 V) { S.SetPostProcessingQuality(V); })
	];
	Box->AddSlot().AutoHeight()
	[
		Quality(LOCTEXT("Foliage", "Foliage"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.GetFoliageQuality(); },
			[](UHHGameUserSettings& S, int32 V) { S.SetFoliageQuality(V); })
	];
	Box->AddSlot().AutoHeight()
	[
		Quality(LOCTEXT("Shading", "Shading"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.GetShadingQuality(); },
			[](UHHGameUserSettings& S, int32 V) { S.SetShadingQuality(V); })
	];

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Image", "IMAGE"))];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("VolFog", "Volumetric Fog"), LOCTEXT("VolFogTip", "Light shafts and haze in the air. Costly on older GPUs."),
			[](const UHHGameUserSettings& S) { return S.bVolumetricFog; },
			[](UHHGameUserSettings& S, bool b) { S.bVolumetricFog = b; })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("MotionBlur", "Motion Blur"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.bMotionBlur; },
			[](UHHGameUserSettings& S, bool b) { S.bMotionBlur = b; })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("Grain", "Film Grain"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.bFilmGrain; },
			[](UHHGameUserSettings& S, bool b) { S.bFilmGrain = b; })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("ChromAb", "Chromatic Aberration"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.bChromaticAberration; },
			[](UHHGameUserSettings& S, bool b) { S.bChromaticAberration = b; })
	];

	return Box;
}

void SHHSettingsPanel::ApplyDisplay()
{
	UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	AHHLobbyHUD* LobbyHUD = HUD.Get();
	if (!Settings || !LobbyHUD)
	{
		return;
	}
	if (PendingWindowMode != INDEX_NONE)
	{
		Settings->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(PendingWindowMode));
	}
	if (Resolutions.IsValidIndex(PendingResolution))
	{
		Settings->SetScreenResolution(Resolutions[PendingResolution]);
	}
	PendingWindowMode = INDEX_NONE;
	PendingResolution = INDEX_NONE;
	Settings->ApplyResolutionSettings(false);

	// Keep or revert, like every console and PC game should.
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	TSharedRef<bool> bDecided = MakeShared<bool>(false);
	LobbyHUD->ShowConfirm(
		LOCTEXT("KeepTitle", "KEEP THESE DISPLAY SETTINGS?"),
		LOCTEXT("KeepBody", "If the picture is wrong, wait: they revert automatically in 12 seconds."),
		LOCTEXT("Keep", "KEEP"),
		[bDecided]()
		{
			*bDecided = true;
			if (UHHGameUserSettings* S = UHHGameUserSettings::Get())
			{
				S->ConfirmVideoMode();
				S->SaveSettings();
			}
		});

	RegisterActiveTimer(12.f, FWidgetActiveTimerDelegate::CreateLambda([WeakHUD, bDecided](double, float)
	{
		if (!*bDecided)
		{
			if (UHHGameUserSettings* S = UHHGameUserSettings::Get())
			{
				S->RevertVideoMode();
				S->ApplyResolutionSettings(false);
				S->SaveSettings();
			}
			if (AHHLobbyHUD* HUDPtr = WeakHUD.Get())
			{
				HUDPtr->ShowMessage(LOCTEXT("RevertedTitle", "DISPLAY SETTINGS REVERTED"), LOCTEXT("RevertedBody", "The previous display settings were restored."));
			}
		}
		return EActiveTimerReturnType::Stop;
	}));
}

// ---------------------------------------------------------------------------------------
// Audio

TSharedRef<SWidget> SHHSettingsPanel::BuildAudio()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	auto Volume = [this](const FText& Label, float UHHGameUserSettings::* Member) -> TSharedRef<SWidget>
	{
		return Slider(Label, FText::GetEmpty(), 0.f, 1.f, 0.01f, 100.f, 0, FText::GetEmpty(),
			[Member](const UHHGameUserSettings& S) { return S.*Member; },
			[Member](UHHGameUserSettings& S, float V) { S.*Member = V; });
	};

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Volume", "VOLUME"))];
	Box->AddSlot().AutoHeight()[Volume(LOCTEXT("Master", "Master"), &UHHGameUserSettings::MasterVolume)];
	Box->AddSlot().AutoHeight()[Volume(LOCTEXT("Music", "Music"), &UHHGameUserSettings::MusicVolume)];
	Box->AddSlot().AutoHeight()[Volume(LOCTEXT("Sfx", "Sound Effects"), &UHHGameUserSettings::SfxVolume)];
	Box->AddSlot().AutoHeight()[Volume(LOCTEXT("Ambience", "Ambience"), &UHHGameUserSettings::AmbientVolume)];
	Box->AddSlot().AutoHeight()[Volume(LOCTEXT("Dialogue", "Voices & Dialogue"), &UHHGameUserSettings::DialogueVolume)];
	Box->AddSlot().AutoHeight()[Volume(LOCTEXT("Interface", "Interface"), &UHHGameUserSettings::UiVolume)];

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("VoiceChat", "VOICE CHAT"))];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("VoiceEnabled", "Voice Chat"), LOCTEXT("VoiceEnabledTip", "Talk to your crew (works in LAN / direct connect sessions)."),
			[](const UHHGameUserSettings& S) { return S.bVoiceChatEnabled; },
			[](UHHGameUserSettings& S, bool b) { S.bVoiceChatEnabled = b; })
	];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHOptionRow)
		.Label(LOCTEXT("VoiceMode", "Microphone Mode"))
		.Options({ LOCTEXT("OpenMic", "Open Mic"), LOCTEXT("PTT", "Push to Talk") })
		.Value_Lambda([]() { const UHHGameUserSettings* S = UHHGameUserSettings::Get(); return S && S->bPushToTalk ? 1 : 0; })
		.OnChanged_Lambda([this](int32 Index) { Change([Index](UHHGameUserSettings& S) { S.bPushToTalk = Index == 1; }); })
	];
	Box->AddSlot().AutoHeight()[Volume(LOCTEXT("VoiceVolume", "Voice Chat Volume"), &UHHGameUserSettings::VoiceChatVolume)];
	Box->AddSlot().AutoHeight()
	[
		Slider(LOCTEXT("MicGain", "Microphone Volume"), LOCTEXT("MicGainTip", "Input gain applied to your microphone."),
			0.f, 2.f, 0.05f, 100.f, 0, LOCTEXT("Pct", "%"),
			[](const UHHGameUserSettings& S) { return S.MicrophoneGain; },
			[](UHHGameUserSettings& S, float V) { S.MicrophoneGain = V; })
	];
	return Box;
}

// ---------------------------------------------------------------------------------------
// Controls

TSharedRef<SWidget> SHHSettingsPanel::BuildControls()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Look", "LOOK"))];
	Box->AddSlot().AutoHeight()
	[
		Slider(LOCTEXT("MouseSens", "Mouse Sensitivity"), FText::GetEmpty(), 0.1f, 3.f, 0.05f, 1.f, 2, FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.MouseSensitivity; },
			[](UHHGameUserSettings& S, float V) { S.MouseSensitivity = V; })
	];
	Box->AddSlot().AutoHeight()
	[
		Slider(LOCTEXT("PadSens", "Controller Sensitivity"), FText::GetEmpty(), 0.2f, 3.f, 0.05f, 1.f, 2, FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.ControllerSensitivity; },
			[](UHHGameUserSettings& S, float V) { S.ControllerSensitivity = V; })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("InvertY", "Invert Vertical Look"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.bInvertY; },
			[](UHHGameUserSettings& S, bool b) { S.bInvertY = b; })
	];

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Movement", "MOVEMENT"))];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHOptionRow)
		.Label(LOCTEXT("CrouchMode", "Crouch"))
		.Options({ LOCTEXT("Hold", "Hold"), LOCTEXT("Toggle", "Toggle") })
		.Value_Lambda([]() { const UHHGameUserSettings* S = UHHGameUserSettings::Get(); return S && S->bToggleCrouch ? 1 : 0; })
		.OnChanged_Lambda([this](int32 Index) { Change([Index](UHHGameUserSettings& S) { S.bToggleCrouch = Index == 1; }); })
	];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHOptionRow)
		.Label(LOCTEXT("SprintMode", "Sprint"))
		.Options({ LOCTEXT("Hold", "Hold"), LOCTEXT("Toggle", "Toggle") })
		.Value_Lambda([]() { const UHHGameUserSettings* S = UHHGameUserSettings::Get(); return S && S->bToggleSprint ? 1 : 0; })
		.OnChanged_Lambda([this](int32 Index) { Change([Index](UHHGameUserSettings& S) { S.bToggleSprint = Index == 1; }); })
	];

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Keys", "KEY BINDINGS"))];
	for (const FHHBindingInfo& Binding : UHHInputSubsystem::GetRebindableBindings())
	{
		Box->AddSlot().AutoHeight()
		[
			SNew(SHHKeyBindRow).HUD(HUD).BindingId(Binding.Id).Label(Binding.DisplayName)
		];
	}
	Box->AddSlot().AutoHeight().Padding(12.f, 10.f, 0.f, 0.f).HAlign(HAlign_Left)
	[
		SNew(SHHActionButton)
		.Text(LOCTEXT("ResetKeys", "RESET KEY BINDINGS"))
		.Kind(EHHButtonKind::Ghost)
		.Height(36.f)
		.OnClicked_Lambda([this]()
		{
			const AHHLobbyHUD* LobbyHUD = HUD.Get();
			if (UHHInputSubsystem* Input = LobbyHUD ? UHHInputSubsystem::Get(LobbyHUD->GetHHController()) : nullptr)
			{
				Input->ResetBindingsToDefault();
			}
		})
	];
	Box->AddSlot().AutoHeight().Padding(12.f, 12.f, 0.f, 0.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("PadHint", "Controller: left stick move, right stick look, A jump, B crouch, X interact, Y loadout, D-pad up light, D-pad down ready, RB talk, Start menu."))
		.AutoWrapText(true)
		.Font(FHHStyle::Font(EHHFont::Body, 13.f))
		.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
	];
	return Box;
}

// ---------------------------------------------------------------------------------------
// Gameplay & accessibility

TSharedRef<SWidget> SHHSettingsPanel::BuildGameplay()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Hud", "HUD"))];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("Prompts", "Interaction Prompts"), LOCTEXT("PromptsTip", "Show what you can use and which key does it."),
			[](const UHHGameUserSettings& S) { return S.bShowInteractionPrompts; },
			[](UHHGameUserSettings& S, bool b) { S.bShowInteractionPrompts = b; })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("Highlight", "Interaction Highlight"), LOCTEXT("HighlightTip", "Faint outline on the object you are looking at."),
			[](const UHHGameUserSettings& S) { return S.bInteractionHighlight; },
			[](UHHGameUserSettings& S, bool b) { S.bInteractionHighlight = b; })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("CrewHud", "Crew & Key Hints"), FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return S.bShowCrewHUD; },
			[](UHHGameUserSettings& S, bool b) { S.bShowCrewHUD = b; })
	];

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Camera", "CAMERA"))];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("HeadBob", "Head Bob"), LOCTEXT("HeadBobTip", "Camera sways with your steps."),
			[](const UHHGameUserSettings& S) { return S.bHeadBob; },
			[](UHHGameUserSettings& S, bool b) { S.bHeadBob = b; })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("Shake", "Camera Shake"), LOCTEXT("ShakeTip", "Landing jolts and other camera impulses."),
			[](const UHHGameUserSettings& S) { return S.bCameraShake; },
			[](UHHGameUserSettings& S, bool b) { S.bCameraShake = b; })
	];

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Text", "SUBTITLES"))];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("Subtitles", "Subtitles"), LOCTEXT("SubtitlesTip", "Spoken lines."),
			[](const UHHGameUserSettings& S) { return S.bSubtitles; },
			[](UHHGameUserSettings& S, bool b) { S.bSubtitles = b; })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("Captions", "Closed Captions"), LOCTEXT("CaptionsTip", "Describe important sounds: [Footsteps overhead], [Thunder]..."),
			[](const UHHGameUserSettings& S) { return S.bClosedCaptions; },
			[](UHHGameUserSettings& S, bool b) { S.bClosedCaptions = b; })
	];

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Profile", "PROFILE"))];
	Box->AddSlot().AutoHeight().Padding(12.f, 4.f, 0.f, 0.f).HAlign(HAlign_Left)
	[
		SNew(SHHActionButton)
		.Text(LOCTEXT("Alias", "CHANGE ALIAS"))
		.Kind(EHHButtonKind::Secondary)
		.Height(36.f)
		.OnClicked_Lambda([this]()
		{
			if (AHHLobbyHUD* LobbyHUD = HUD.Get())
			{
				LobbyHUD->ShowNameEntry(false);
			}
		})
	];
	return Box;
}

TSharedRef<SWidget> SHHSettingsPanel::BuildAccessibility()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Vision", "VISION"))];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHOptionRow)
		.Label(LOCTEXT("ColorBlind", "Colour Blind Mode"))
		.Tooltip(LOCTEXT("ColorBlindTip", "Corrects colours on screen for the selected deficiency."))
		.Options({ LOCTEXT("CBOff", "Off"), LOCTEXT("Deuteranope", "Deuteranopia"), LOCTEXT("Protanope", "Protanopia"), LOCTEXT("Tritanope", "Tritanopia") })
		.Value_Lambda([]() { const UHHGameUserSettings* S = UHHGameUserSettings::Get(); return S ? S->ColorBlindMode : 0; })
		.OnChanged_Lambda([this](int32 Index) { Change([Index](UHHGameUserSettings& S) { S.ColorBlindMode = Index; }); })
	];
	Box->AddSlot().AutoHeight()
	[
		Slider(LOCTEXT("ColorBlindStrength", "Correction Strength"), FText::GetEmpty(), 0.f, 10.f, 1.f, 1.f, 0, FText::GetEmpty(),
			[](const UHHGameUserSettings& S) { return static_cast<float>(S.ColorBlindStrength); },
			[](UHHGameUserSettings& S, float V) { S.ColorBlindStrength = FMath::RoundToInt(V); })
	];

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Comfort", "COMFORT"))];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("ReduceMotion", "Reduce Motion"), LOCTEXT("ReduceMotionTip", "Disables head bob, camera shake and menu camera drift."),
			[](const UHHGameUserSettings& S) { return S.bReduceMotion; },
			[](UHHGameUserSettings& S, bool b) { S.bReduceMotion = b; })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("ReduceFlicker", "Reduce Flicker & Flashes"), LOCTEXT("ReduceFlickerTip", "Lights dim gently instead of strobing; lightning becomes a soft glow."),
			[](const UHHGameUserSettings& S) { return S.bReduceFlicker; },
			[](UHHGameUserSettings& S, bool b) { S.bReduceFlicker = b; })
	];

	Box->AddSlot().AutoHeight()[Section(LOCTEXT("Reading", "READING"))];
	Box->AddSlot().AutoHeight()
	[
		SNew(SHHOptionRow)
		.Label(LOCTEXT("SubSize", "Subtitle Size"))
		.Options({ LOCTEXT("Small", "Small"), LOCTEXT("MediumSize", "Medium"), LOCTEXT("Large", "Large"), LOCTEXT("XL", "Extra Large") })
		.Value_Lambda([]() { const UHHGameUserSettings* S = UHHGameUserSettings::Get(); return S ? S->SubtitleSize : 1; })
		.OnChanged_Lambda([this](int32 Index) { Change([Index](UHHGameUserSettings& S) { S.SubtitleSize = Index; }); })
	];
	Box->AddSlot().AutoHeight()
	[
		Slider(LOCTEXT("SubBg", "Subtitle Background"), FText::GetEmpty(), 0.f, 1.f, 0.05f, 100.f, 0, LOCTEXT("Pct2", "%"),
			[](const UHHGameUserSettings& S) { return S.SubtitleBackgroundOpacity; },
			[](UHHGameUserSettings& S, float V) { S.SubtitleBackgroundOpacity = V; })
	];
	Box->AddSlot().AutoHeight()
	[
		Toggle(LOCTEXT("Captions2", "Closed Captions"), LOCTEXT("CaptionsTip2", "Text descriptions of important sounds."),
			[](const UHHGameUserSettings& S) { return S.bClosedCaptions; },
			[](UHHGameUserSettings& S, bool b) { S.bClosedCaptions = b; })
	];
	return Box;
}

void SHHSettingsPanel::RestoreDefaults()
{
	AHHLobbyHUD* LobbyHUD = HUD.Get();
	if (!LobbyHUD)
	{
		return;
	}
	TWeakPtr<SHHSettingsPanel> WeakPanel = SharedThis(this);
	LobbyHUD->ShowConfirm(
		LOCTEXT("DefaultsTitle", "RESTORE DEFAULTS?"),
		LOCTEXT("DefaultsBody", "Every setting, including key bindings, goes back to factory values."),
		LOCTEXT("DefaultsConfirm", "RESTORE"),
		[WeakPanel]()
		{
			if (UHHGameUserSettings* S = UHHGameUserSettings::Get())
			{
				S->SetToDefaults();
				S->ApplySettings(false);
				S->SaveSettings();
			}
			if (TSharedPtr<SHHSettingsPanel> Panel = WeakPanel.Pin())
			{
				Panel->SetTab(Panel->Tab);
			}
		});
}

// ---------------------------------------------------------------------------------------
// Key binding row

void SHHKeyBindRow::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;
	BindingId = InArgs._BindingId;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FHHStyle::Brush("HH.White"))
		.BorderBackgroundColor_Lambda([this]() { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, bListening ? 0.06f : (IsHovered() ? 0.035f : 0.f))); })
		.Padding(FMargin(12.f, 4.f))
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
				.WidthOverride(200.f)
				[
					SNew(SButton)
					.ButtonStyle(&FHHStyle::Button("HH.Button.Secondary"))
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(10.f, 6.f))
					.OnHovered_Lambda([]() { HHUI::PlayUISound(EHHUISound::Hover); })
					.OnClicked_Lambda([this]()
					{
						HHUI::PlayUISound(EHHUISound::Click);
						bListening = true;
						FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return GetKeyText(); })
						.Font(FHHStyle::Font(EHHFont::MonoMedium, 13.f))
						.ColorAndOpacity_Lambda([this]() { return FSlateColor(bListening ? FHHStyle::Accent() : FHHStyle::Text()); })
					]
				]
			]
		]
	];
}

FText SHHKeyBindRow::GetKeyText() const
{
	if (bListening)
	{
		return LOCTEXT("PressKey", "PRESS A KEY...");
	}
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const UHHInputSubsystem* Input = LobbyHUD ? UHHInputSubsystem::Get(LobbyHUD->GetHHController()) : nullptr;
	return Input ? Input->GetKeyDisplayText(BindingId) : FText::GetEmpty();
}

void SHHKeyBindRow::Commit(const FKey& Key)
{
	bListening = false;
	AHHLobbyHUD* LobbyHUD = HUD.Get();
	UHHInputSubsystem* Input = LobbyHUD ? UHHInputSubsystem::Get(LobbyHUD->GetHHController()) : nullptr;
	if (!Input)
	{
		return;
	}
	if (UHHInputSubsystem::IsReservedKey(Key))
	{
		HHUI::PlayUISound(EHHUISound::Error);
		return;
	}

	const FName Swapped = Input->RebindKey(BindingId, Key);
	HHUI::PlayUISound(EHHUISound::Confirm);
	if (!Swapped.IsNone())
	{
		for (const FHHBindingInfo& Info : UHHInputSubsystem::GetRebindableBindings())
		{
			if (Info.Id == Swapped)
			{
				LobbyHUD->ShowNotification(FText::Format(LOCTEXT("Swapped", "{0} was on that key - it now uses your old key."), Info.DisplayName), EHHNotifyType::Info);
				break;
			}
		}
	}
}

FReply SHHKeyBindRow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bListening)
	{
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		bListening = false;
		HHUI::PlayUISound(EHHUISound::Back);
		return FReply::Handled();
	}
	Commit(InKeyEvent.GetKey());
	return FReply::Handled();
}

FReply SHHKeyBindRow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bListening)
	{
		Commit(MouseEvent.GetEffectingButton());
		return FReply::Handled();
	}
	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

void SHHKeyBindRow::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	SCompoundWidget::OnFocusLost(InFocusEvent);
	bListening = false;
}

#undef LOCTEXT_NAMESPACE
