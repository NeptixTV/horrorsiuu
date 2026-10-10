#include "UI/HHStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FHHStyle::Instance = nullptr;

namespace HHStylePrivate
{
	FLinearColor Hex(uint8 R, uint8 G, uint8 B, float Alpha = 1.f)
	{
		FLinearColor Color = FLinearColor::FromSRGBColor(FColor(R, G, B));
		Color.A = Alpha;
		return Color;
	}

	FString FontPath(const TCHAR* File)
	{
		return FPaths::ProjectContentDir() / TEXT("Slate/Fonts") / File;
	}

	FString ImagePath(const TCHAR* File)
	{
		return FPaths::ProjectContentDir() / TEXT("Slate/Images") / File;
	}

	/** One composite font per face, shared by all sizes (keeps the font cache small). */
	const FSlateFontInfo& BaseFont(EHHFont Face)
	{
		static TMap<uint8, FSlateFontInfo> Cache;
		if (const FSlateFontInfo* Found = Cache.Find(static_cast<uint8>(Face)))
		{
			return *Found;
		}

		const TCHAR* File = TEXT("Barlow-Regular.ttf");
		switch (Face)
		{
		case EHHFont::Display:			File = TEXT("Oswald-Medium.ttf"); break;
		case EHHFont::DisplayLight:		File = TEXT("Oswald-Regular.ttf"); break;
		case EHHFont::DisplayBold:		File = TEXT("Oswald-SemiBold.ttf"); break;
		case EHHFont::Body:				File = TEXT("Barlow-Regular.ttf"); break;
		case EHHFont::BodyLight:		File = TEXT("Barlow-Light.ttf"); break;
		case EHHFont::BodyMedium:		File = TEXT("Barlow-Medium.ttf"); break;
		case EHHFont::BodySemi:			File = TEXT("Barlow-SemiBold.ttf"); break;
		case EHHFont::Condensed:		File = TEXT("BarlowCondensed-Medium.ttf"); break;
		case EHHFont::CondensedSemi:	File = TEXT("BarlowCondensed-SemiBold.ttf"); break;
		case EHHFont::Mono:				File = TEXT("IBMPlexMono-Regular.ttf"); break;
		case EHHFont::MonoMedium:		File = TEXT("IBMPlexMono-Medium.ttf"); break;
		case EHHFont::Typewriter:		File = TEXT("SpecialElite-Regular.ttf"); break;
		case EHHFont::Hand:				File = TEXT("Caveat-Medium.ttf"); break;
		case EHHFont::Scrawl:			File = TEXT("ReenieBeanie-Regular.ttf"); break;
		default: break;
		}
		return Cache.Add(static_cast<uint8>(Face), FSlateFontInfo(FontPath(File), 14.f));
	}
}

using namespace HHStylePrivate;

FLinearColor FHHStyle::Ink()			{ return Hex(10, 11, 13); }
FLinearColor FHHStyle::Panel()			{ return Hex(14, 15, 18, 0.9f); }
FLinearColor FHHStyle::PanelRaised()	{ return Hex(24, 26, 30, 0.92f); }
FLinearColor FHHStyle::Line()			{ return FLinearColor(1.f, 1.f, 1.f, 0.07f); }
FLinearColor FHHStyle::LineStrong()		{ return FLinearColor(1.f, 1.f, 1.f, 0.16f); }
FLinearColor FHHStyle::Text()			{ return Hex(233, 228, 218); }
FLinearColor FHHStyle::TextDim()		{ return Hex(154, 149, 140); }
FLinearColor FHHStyle::TextFaint()		{ return Hex(98, 95, 90); }
FLinearColor FHHStyle::Accent()			{ return Hex(214, 163, 92); }
FLinearColor FHHStyle::AccentDim()		{ return Hex(122, 94, 54); }
FLinearColor FHHStyle::Danger()			{ return Hex(184, 72, 60); }
FLinearColor FHHStyle::Ready()			{ return Hex(143, 178, 122); }
FLinearColor FHHStyle::Info()			{ return Hex(124, 152, 179); }
FLinearColor FHHStyle::Paper()			{ return Hex(222, 213, 192); }

FLinearColor FHHStyle::WithAlpha(const FLinearColor& Color, float Alpha)
{
	FLinearColor Result = Color;
	Result.A = Alpha;
	return Result;
}

FSlateFontInfo FHHStyle::Font(EHHFont Face, float Size, int32 LetterSpacing)
{
	FSlateFontInfo FontInfo = BaseFont(Face);
	FontInfo.Size = Size;
	FontInfo.LetterSpacing = LetterSpacing;
	return FontInfo;
}

void FHHStyle::Initialize()
{
	if (!Instance.IsValid())
	{
		Instance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*Instance);
	}
}

void FHHStyle::Shutdown()
{
	if (Instance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Instance);
		Instance.Reset();
	}
}

const ISlateStyle& FHHStyle::Get()
{
	Initialize();
	return *Instance;
}

const FSlateBrush* FHHStyle::Brush(const FName& Name)
{
	return Get().GetBrush(Name);
}

const FButtonStyle& FHHStyle::Button(const FName& Name)
{
	return Get().GetWidgetStyle<FButtonStyle>(Name);
}

const FSliderStyle& FHHStyle::Slider()
{
	return Get().GetWidgetStyle<FSliderStyle>(TEXT("HH.Slider"));
}

TSharedRef<FSlateStyleSet> FHHStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(TEXT("HHStyle")));
	Style->SetContentRoot(FPaths::ProjectContentDir() / TEXT("Slate"));

	const FLinearColor Clear(0.f, 0.f, 0.f, 0.f);

	// --- Solid fills & panels ---
	Style->Set("HH.White", new FSlateColorBrush(FLinearColor::White));
	Style->Set("HH.Panel", new FSlateColorBrush(Panel()));
	Style->Set("HH.PanelFrame", new FSlateRoundedBoxBrush(Panel(), 2.f, Line(), 1.f));
	Style->Set("HH.Card", new FSlateRoundedBoxBrush(PanelRaised(), 2.f, Line(), 1.f));
	Style->Set("HH.CardHover", new FSlateRoundedBoxBrush(Hex(34, 36, 41, 0.95f), 2.f, LineStrong(), 1.f));
	Style->Set("HH.CardSelected", new FSlateRoundedBoxBrush(Hex(38, 34, 28, 0.95f), 2.f, Accent(), 1.f));
	Style->Set("HH.Outline", new FSlateRoundedBoxBrush(Clear, 2.f, LineStrong(), 1.f));
	Style->Set("HH.OutlineAccent", new FSlateRoundedBoxBrush(Clear, 2.f, Accent(), 1.f));
	Style->Set("HH.Pill", new FSlateRoundedBoxBrush(FLinearColor::White, 9.f));
	Style->Set("HH.KeyCap", new FSlateRoundedBoxBrush(Hex(30, 31, 35, 0.95f), 3.f, LineStrong(), 1.f));
	Style->Set("HH.Paper", new FSlateRoundedBoxBrush(Paper(), 1.f));

	// --- Images (Content/Slate/Images, loaded at runtime) ---
	Style->Set("HH.Vignette", new FSlateImageBrush(ImagePath(TEXT("Vignette.png")), FVector2D(1024.0, 1024.0)));
	Style->Set("HH.GradientLeft", new FSlateImageBrush(ImagePath(TEXT("GradientLeft.png")), FVector2D(512.0, 8.0)));
	Style->Set("HH.GradientRight", new FSlateImageBrush(ImagePath(TEXT("GradientRight.png")), FVector2D(512.0, 8.0)));
	Style->Set("HH.GradientBottom", new FSlateImageBrush(ImagePath(TEXT("GradientBottom.png")), FVector2D(8.0, 512.0)));
	Style->Set("HH.Grain", new FSlateImageBrush(ImagePath(TEXT("Grain.png")), FVector2D(256.0, 256.0), FLinearColor::White, ESlateBrushTileType::Both));
	Style->Set("HH.Scratches", new FSlateImageBrush(ImagePath(TEXT("Scratches.png")), FVector2D(1024.0, 1024.0)));

	const TCHAR* Icons[] = {
		TEXT("Mic"), TEXT("Lock"), TEXT("Check"), TEXT("Crown"), TEXT("ChevronLeft"), TEXT("ChevronRight"),
		TEXT("Dot"), TEXT("Cash"), TEXT("Person"), TEXT("Signal"), TEXT("Warning"), TEXT("Info"), TEXT("Close"),
		TEXT("Rotate"), TEXT("Zoom"), TEXT("Star"), TEXT("Pin"), TEXT("Van"), TEXT("Gear"), TEXT("Door")
	};
	for (const TCHAR* Icon : Icons)
	{
		const FString File = FString::Printf(TEXT("Icon_%s.png"), Icon);
		Style->Set(*FString::Printf(TEXT("HH.Icon.%s"), Icon), new FSlateImageBrush(ImagePath(*File), FVector2D(24.0, 24.0)));
	}

	// --- Buttons (visual feedback beyond fills is animated in the widgets) ---
	const FButtonStyle Ghost = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.035f)))
		.SetPressed(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.06f)))
		.SetDisabled(FSlateNoResource())
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));
	Style->Set("HH.Button.Ghost", Ghost);

	const FButtonStyle Invisible = FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateNoResource())
		.SetPressed(FSlateNoResource())
		.SetDisabled(FSlateNoResource())
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));
	Style->Set("HH.Button.Invisible", Invisible);

	const FButtonStyle Primary = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(Accent(), 2.f))
		.SetHovered(FSlateRoundedBoxBrush(Hex(232, 184, 112), 2.f))
		.SetPressed(FSlateRoundedBoxBrush(Hex(176, 132, 72), 2.f))
		.SetDisabled(FSlateRoundedBoxBrush(Hex(60, 58, 54, 0.8f), 2.f))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f, 1.f, 0.f, -1.f));
	Style->Set("HH.Button.Primary", Primary);

	const FButtonStyle Secondary = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.02f), 2.f, LineStrong(), 1.f))
		.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.05f), 2.f, Accent(), 1.f))
		.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.08f), 2.f, Accent(), 1.f))
		.SetDisabled(FSlateRoundedBoxBrush(Clear, 2.f, Line(), 1.f))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f, 1.f, 0.f, -1.f));
	Style->Set("HH.Button.Secondary", Secondary);

	const FButtonStyle DangerStyle = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.02f), 2.f, WithAlpha(FHHStyle::Danger(), 0.6f), 1.f))
		.SetHovered(FSlateRoundedBoxBrush(WithAlpha(FHHStyle::Danger(), 0.18f), 2.f, FHHStyle::Danger(), 1.f))
		.SetPressed(FSlateRoundedBoxBrush(WithAlpha(FHHStyle::Danger(), 0.3f), 2.f, FHHStyle::Danger(), 1.f))
		.SetDisabled(FSlateRoundedBoxBrush(Clear, 2.f, Line(), 1.f))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f, 1.f, 0.f, -1.f));
	Style->Set("HH.Button.Danger", DangerStyle);

	const FButtonStyle CardStyle = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(PanelRaised(), 2.f, Line(), 1.f))
		.SetHovered(FSlateRoundedBoxBrush(Hex(34, 36, 41, 0.95f), 2.f, LineStrong(), 1.f))
		.SetPressed(FSlateRoundedBoxBrush(Hex(40, 42, 47, 0.95f), 2.f, Accent(), 1.f))
		.SetDisabled(FSlateRoundedBoxBrush(PanelRaised(), 2.f, Line(), 1.f))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));
	Style->Set("HH.Button.Card", CardStyle);

	// --- Slider ---
	const FSliderStyle SliderStyle = FSliderStyle()
		.SetNormalBarImage(FSlateColorBrush(LineStrong()))
		.SetHoveredBarImage(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.24f)))
		.SetDisabledBarImage(FSlateColorBrush(Line()))
		.SetNormalThumbImage(FSlateRoundedBoxBrush(Accent(), 7.f, Clear, 0.f, FVector2D(14.0, 14.0)))
		.SetHoveredThumbImage(FSlateRoundedBoxBrush(Hex(240, 196, 128), 7.f, Clear, 0.f, FVector2D(16.0, 16.0)))
		.SetDisabledThumbImage(FSlateRoundedBoxBrush(TextFaint(), 7.f, Clear, 0.f, FVector2D(14.0, 14.0)))
		.SetBarThickness(2.f);
	Style->Set("HH.Slider", SliderStyle);

	return Style;
}
