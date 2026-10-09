#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"

/** Typefaces shipped in Content/Slate/Fonts (SIL Open Font License, see Licenses/). */
enum class EHHFont : uint8
{
	Display,		// Oswald Medium - titles, navigation
	DisplayLight,	// Oswald Regular
	DisplayBold,	// Oswald SemiBold
	Body,			// Barlow Regular
	BodyLight,		// Barlow Light
	BodyMedium,		// Barlow Medium
	BodySemi,		// Barlow SemiBold
	Condensed,		// Barlow Condensed Medium - labels, tags
	CondensedSemi,	// Barlow Condensed SemiBold
	Mono,			// IBM Plex Mono - numbers, money, keys
	MonoMedium,
	Typewriter,		// Special Elite - dossiers, flavour text
	Hand,			// Caveat - notes pinned to the board
	Scrawl			// Reenie Beanie - frantic handwriting
};

/**
 * Visual language of the lobby UI: near-black glass panels, hairline borders, warm sodium
 * amber as the only accent, typewriter dossiers. Everything is defined here so restyling
 * the UI never means hunting through widget code.
 */
class HORRORHEIST_API FHHStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static const ISlateStyle& Get();

	static FSlateFontInfo Font(EHHFont Face, float Size, int32 LetterSpacing = 0);

	static const FSlateBrush* Brush(const FName& Name);
	static const FButtonStyle& Button(const FName& Name);
	static const FSliderStyle& Slider();

	// Palette (linear colours).
	static FLinearColor Ink();			// screen black
	static FLinearColor Panel();		// glass panel
	static FLinearColor PanelRaised();	// cards, rows
	static FLinearColor Line();			// hairlines
	static FLinearColor LineStrong();
	static FLinearColor Text();
	static FLinearColor TextDim();
	static FLinearColor TextFaint();
	static FLinearColor Accent();		// sodium amber
	static FLinearColor AccentDim();
	static FLinearColor Danger();
	static FLinearColor Ready();
	static FLinearColor Info();
	static FLinearColor Paper();		// dossier paper tone

	static FLinearColor WithAlpha(const FLinearColor& Color, float Alpha);

private:
	static TSharedRef<class FSlateStyleSet> Create();
	static TSharedPtr<class FSlateStyleSet> Instance;
};
