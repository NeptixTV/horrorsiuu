#include "UI/SUHUD.h"
#include "SignalUnknown.h"
#include "Core/SUPlayerController.h"
#include "Core/SUGameState.h"
#include "Core/SUGameUserSettings.h"
#include "Core/SUGameInstance.h"
#include "Core/SUItemDatabase.h"
#include "Player/SUCharacter.h"
#include "Player/SUInteractionComponent.h"
#include "Player/SUInventoryComponent.h"
#include "Player/SUPhotoCameraComponent.h"
#include "Vehicle/SUVehicle.h"
#include "Mission/SUMissionManager.h"
#include "Mission/SUMissionDatabase.h"
#include "Base/SUBaseDevices.h"
#include "World/SUWorldBuilder.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CanvasItem.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

namespace SUColors
{
	const FLinearColor Text(0.92f, 0.90f, 0.85f);
	const FLinearColor Dim(0.62f, 0.60f, 0.56f);
	const FLinearColor Accent(1.f, 0.72f, 0.32f);
	const FLinearColor Good(0.55f, 0.95f, 0.6f);
	const FLinearColor Bad(1.f, 0.42f, 0.36f);
	const FLinearColor PanelBg(0.03f, 0.035f, 0.04f);
	const FLinearColor ButtonBg(0.12f, 0.12f, 0.12f, 0.9f);
	const FLinearColor ButtonFocus(0.32f, 0.22f, 0.1f, 0.95f);
}

ASUHUD::ASUHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

ASUPlayerController* ASUHUD::GetPC() const
{
	return Cast<ASUPlayerController>(PlayerOwner);
}

ASUCharacter* ASUHUD::GetChar() const
{
	const ASUPlayerController* PC = GetPC();
	return PC ? PC->GetSUCharacter() : nullptr;
}

float ASUHUD::W() const
{
	return Canvas ? Canvas->ClipX : 1920.f;
}

float ASUHUD::H() const
{
	return Canvas ? Canvas->ClipY : 1080.f;
}

// ====================================================================================== helpers

FSlateFontInfo ASUHUD::Font(float Size, bool bBold, bool bMono) const
{
	const int32 Pixels = FMath::Max(6, FMath::RoundToInt(Size * UIScale));
	return FCoreStyle::GetDefaultFontStyle(bMono ? TEXT("Mono") : (bBold ? TEXT("Bold") : TEXT("Regular")), Pixels);
}

FVector2D ASUHUD::Measure(const FString& InText, const FSlateFontInfo& InFont) const
{
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		const TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return M->Measure(InText, InFont, 1.f);
	}
	return FVector2D(InText.Len() * InFont.Size * 0.55f, InFont.Size * 1.3f);
}

FVector2D ASUHUD::Text(const FString& InText, float X, float Y, float Size, const FLinearColor& Color, int32 Align, bool bBold, bool bShadow, bool bMono)
{
	if (!Canvas || InText.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	const FSlateFontInfo F = Font(Size, bBold, bMono);
	const FVector2D Sz = Measure(InText, F);
	float PX = X;
	if (Align == 1)
	{
		PX -= Sz.X * 0.5f;
	}
	else if (Align == 2)
	{
		PX -= Sz.X;
	}
	FCanvasTextItem Item(FVector2D(PX, Y), FText::FromString(InText), F, Color);
	if (bShadow)
	{
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, Color.A * 0.85f), FVector2D(1.f, 1.f));
	}
	Canvas->DrawItem(Item);
	return Sz;
}

TArray<FString> ASUHUD::WrapLines(const FString& InText, float Width, const FSlateFontInfo& InFont) const
{
	TArray<FString> Out;
	TArray<FString> Paragraphs;
	InText.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	for (const FString& P : Paragraphs)
	{
		if (P.IsEmpty())
		{
			Out.Add(FString());
			continue;
		}
		TArray<FString> Words;
		P.ParseIntoArray(Words, TEXT(" "), true);
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			if (!Line.IsEmpty() && Measure(Candidate, InFont).X > Width)
			{
				Out.Add(Line);
				Line = Word;
			}
			else
			{
				Line = Candidate;
			}
		}
		Out.Add(Line);
	}
	return Out;
}

float ASUHUD::TextBlock(const FString& InText, float X, float Y, float Width, float Size, const FLinearColor& Color, bool bMono, int32 MaxLines)
{
	const FSlateFontInfo F = Font(Size, false, bMono);
	const TArray<FString> Lines = WrapLines(InText, Width, F);
	const float LineH = S(Size) * 1.45f;
	float CY = Y;
	int32 N = 0;
	for (const FString& L : Lines)
	{
		if (MaxLines > 0 && N >= MaxLines)
		{
			break;
		}
		Text(L, X, CY, Size, Color, 0, false, false, bMono);
		CY += LineH;
		++N;
	}
	return CY - Y;
}

void ASUHUD::Rect(float X, float Y, float InW, float InH, const FLinearColor& Color)
{
	DrawRect(Color, X, Y, InW, InH);
}

void ASUHUD::Frame(float X, float Y, float InW, float InH, const FLinearColor& Color, float Thickness)
{
	DrawLine(X, Y, X + InW, Y, Color, Thickness);
	DrawLine(X + InW, Y, X + InW, Y + InH, Color, Thickness);
	DrawLine(X + InW, Y + InH, X, Y + InH, Color, Thickness);
	DrawLine(X, Y + InH, X, Y, Color, Thickness);
}

void ASUHUD::Panel(float X, float Y, float InW, float InH, float Alpha)
{
	FLinearColor C = SUColors::PanelBg;
	C.A = Alpha;
	Rect(X, Y, InW, InH, C);
	Frame(X, Y, InW, InH, FLinearColor(0.4f, 0.36f, 0.3f, 0.6f));
}

void ASUHUD::Image(UTexture* Texture, float X, float Y, float InW, float InH, const FLinearColor& Tint, bool bFlipX)
{
	if (!Texture)
	{
		Rect(X, Y, InW, InH, FLinearColor(0.05f, 0.05f, 0.05f, 1.f));
		return;
	}
	DrawTexture(Texture, X, Y, InW, InH, bFlipX ? 1.f : 0.f, 0.f, bFlipX ? -1.f : 1.f, 1.f, Tint, BLEND_Translucent);
}

bool ASUHUD::IsFocused(int32 ButtonIndex) const
{
	return ButtonIndex == FocusIndex && !bMouseMovedSinceNav;
}

bool ASUHUD::Button(const FString& Label, float X, float Y, float InW, float InH, TFunction<void()> OnClick, bool bEnabled, float FontSize)
{
	const int32 Index = Buttons.Num();
	FButton& B = Buttons.AddDefaulted_GetRef();
	B.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + InW, Y + InH));
	B.OnClick = MoveTemp(OnClick);
	B.bEnabled = bEnabled;
	const bool bHover = B.Rect.IsInside(LastMouse) && bMouseMovedSinceNav;
	const bool bFocus = IsFocused(Index) || bHover;
	Rect(X, Y, InW, InH, bFocus && bEnabled ? SUColors::ButtonFocus : SUColors::ButtonBg);
	Frame(X, Y, InW, InH, bFocus && bEnabled ? SUColors::Accent : FLinearColor(0.35f, 0.33f, 0.3f, 0.8f));
	Text(Label, X + InW * 0.5f, Y + InH * 0.5f - S(FontSize) * 0.68f, FontSize, bEnabled ? SUColors::Text : SUColors::Dim, 1);
	return bFocus;
}

void ASUHUD::SettingRow(const FString& Label, const FString& Value, float X, float Y, float InW, TFunction<void(int32)> OnAdjust)
{
	const float RowH = S(34.f);
	const int32 Index = Buttons.Num();
	FButton& B = Buttons.AddDefaulted_GetRef();
	B.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + InW, Y + RowH));
	TFunction<void(int32)> Adjust = OnAdjust;
	B.OnAdjust = OnAdjust;
	B.OnClick = [Adjust]() { Adjust(1); };
	const bool bHover = B.Rect.IsInside(LastMouse) && bMouseMovedSinceNav;
	const bool bFocus = IsFocused(Index) || bHover;
	if (bFocus)
	{
		Rect(X, Y, InW, RowH, FLinearColor(0.25f, 0.17f, 0.08f, 0.6f));
	}
	Text(Label, X + S(12.f), Y + S(7.f), 16.f, SUColors::Text);
	Text(FString::Printf(TEXT("‹  %s  ›"), *Value), X + InW - S(12.f), Y + S(7.f), 16.f, bFocus ? SUColors::Accent : SUColors::Text, 2);
}

void ASUHUD::DrawScreenTitle(const FString& Title, const FString& Hint)
{
	Text(Title, S(60.f), S(40.f), 30.f, SUColors::Accent, 0, true);
	if (!Hint.IsEmpty())
	{
		Text(Hint, W() - S(60.f), H() - S(44.f), 14.f, SUColors::Dim, 2);
	}
}

// ====================================================================================== API

void ASUHUD::AddNotification(const FSUNotification& Notification)
{
	FNotice N;
	N.Data = Notification;
	Notices.Insert(N, 0);
	if (Notices.Num() > 6)
	{
		Notices.SetNum(6);
	}
}

void ASUHUD::AddSpeech(const FSUSpeechLine& Line, bool bRadio)
{
	FSubtitle Sub;
	Sub.Line = Line;
	Sub.bRadio = bRadio;
	SpeechQueue.Add(Sub);
	if (SpeechQueue.Num() > 4)
	{
		SpeechQueue.RemoveAt(0);
	}
}

void ASUHUD::OnScreenChanged(ESUScreen OldScreen, ESUScreen NewScreen)
{
	Screen = NewScreen;
	ResetScreenState();
	if (NewScreen == ESUScreen::Join)
	{
		const USUGameInstance* GI = GetPC() ? GetPC()->GetGameInstance<USUGameInstance>() : nullptr;
		JoinAddress = GI ? GI->GetLastJoinAddress() : FString(TEXT("127.0.0.1"));
	}
	if (NewScreen == ESUScreen::Terminal)
	{
		const ASUPlayerController* PC = GetPC();
		bShowReportResult = PC && PC->HasReportToShow();
	}
}

void ASUHUD::ResetScreenState()
{
	Tab = 0;
	Scroll = 0.f;
	SelectedIndex = INDEX_NONE;
	FocusIndex = 0;
	bShowReportResult = false;
	bConfirmQuit = false;
	bMapFollowPlayer = true;
	MapZoom = 1.f;
	ReportSelection.Reset();
	LastButtons.Reset();
}

void ASUHUD::FlashDamage(float Strength)
{
	DamageFlash = FMath::Max(DamageFlash, Strength);
}

void ASUHUD::StartFade(float From, float To, float Seconds)
{
	FadeFrom = From;
	FadeTo = To;
	FadeDuration = FMath::Max(0.01f, Seconds);
	FadeTime = 0.f;
}

// ====================================================================================== input

void ASUHUD::UINavigate(FIntPoint Direction)
{
	if (LastButtons.Num() == 0)
	{
		if (Screen == ESUScreen::Map)
		{
			bMapFollowPlayer = false;
			MapCenter += FVector2D(Direction.X, Direction.Y) * 0.04f / MapZoom;
		}
		return;
	}
	if (bMouseMovedSinceNav)
	{
		bMouseMovedSinceNav = false;
		FocusIndex = FMath::Clamp(FocusIndex, 0, LastButtons.Num() - 1);
		return;
	}
	if (Direction.Y != 0)
	{
		FocusIndex = (FocusIndex + Direction.Y + LastButtons.Num()) % LastButtons.Num();
	}
	else if (Direction.X != 0 && LastButtons.IsValidIndex(FocusIndex))
	{
		const FButton& B = LastButtons[FocusIndex];
		if (B.OnAdjust)
		{
			B.OnAdjust(Direction.X);
		}
		else
		{
			FocusIndex = (FocusIndex + Direction.X + LastButtons.Num()) % LastButtons.Num();
		}
	}
}

void ASUHUD::UIAccept()
{
	if (LastButtons.IsValidIndex(FocusIndex) && !bMouseMovedSinceNav)
	{
		const FButton B = LastButtons[FocusIndex];
		if (B.bEnabled && B.OnClick)
		{
			B.OnClick();
		}
	}
	else
	{
		UIClick();
	}
}

void ASUHUD::UIClick()
{
	for (int32 I = 0; I < LastButtons.Num(); ++I)
	{
		if (LastButtons[I].Rect.IsInside(LastMouse))
		{
			const FButton B = LastButtons[I];
			FocusIndex = I;
			if (B.bEnabled && B.OnClick)
			{
				B.OnClick();
			}
			return;
		}
	}
}

bool ASUHUD::UIBack()
{
	if (bConfirmQuit)
	{
		bConfirmQuit = false;
		return true;
	}
	if (Screen == ESUScreen::Terminal && bShowReportResult)
	{
		bShowReportResult = false;
		if (ASUPlayerController* PC = GetPC())
		{
			PC->ClearReportToShow();
		}
		return true;
	}
	if (Screen == ESUScreen::Gallery && SelectedIndex != INDEX_NONE)
	{
		SelectedIndex = INDEX_NONE;
		return true;
	}
	if (Screen == ESUScreen::Settings)
	{
		if (USUGameUserSettings* Settings = USUGameUserSettings::Get())
		{
			Settings->ApplySettings(false);
		}
		if (ASUPlayerController* PC = GetPC())
		{
			if (!PC->IsInMainMenu())
			{
				PC->OpenScreen(ESUScreen::Pause);
				return true;
			}
		}
	}
	if (Screen == ESUScreen::Help && GetPC() && !GetPC()->IsInMainMenu())
	{
		GetPC()->OpenScreen(ESUScreen::Pause);
		return true;
	}
	return false;
}

void ASUHUD::UIScroll(float Delta)
{
	if (Screen == ESUScreen::Map)
	{
		MapZoom = FMath::Clamp(MapZoom * (Delta > 0.f ? 1.25f : 0.8f), 1.f, 8.f);
		return;
	}
	Scroll = FMath::Max(0.f, Scroll - Delta * 60.f);
}

void ASUHUD::UITab(int32 Direction)
{
	int32 Tabs = 0;
	switch (Screen)
	{
	case ESUScreen::Terminal: Tabs = 4; break;
	case ESUScreen::Settings: Tabs = 4; break;
	default: break;
	}
	if (Tabs > 0 && !bShowReportResult)
	{
		Tab = (Tab + Direction + Tabs) % Tabs;
		Scroll = 0.f;
		SelectedIndex = INDEX_NONE;
		FocusIndex = 0;
	}
}

// ====================================================================================== frame

void ASUHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	const float Dt = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.1f);
	RealTime += Dt;
	UIScale = FMath::Clamp(H() / 1080.f, 0.6f, 2.f);
	if (bHiddenForScreenshot)
	{
		return; // the photo must not contain any HUD element
	}

	// mouse
	if (PlayerOwner)
	{
		float MX = 0.f, MY = 0.f;
		if (PlayerOwner->GetMousePosition(MX, MY))
		{
			const FVector2D M(MX, MY);
			if (FVector2D::Distance(M, LastMouse) > 2.f)
			{
				bMouseMovedSinceNav = true;
			}
			LastMouse = M;
		}
	}

	Buttons.Reset();
	const ASUPlayerController* PC = GetPC();
	if (Screen != ESUScreen::MainMenu && PC && !PC->IsInMainMenu())
	{
		DrawGameHUD(Dt);
	}
	if (Screen != ESUScreen::None)
	{
		DrawScreen();
	}
	else
	{
		DrawNotifications(Dt);
	}
	LastButtons = MoveTemp(Buttons);
	Buttons.Reset();
	if (LastButtons.Num() > 0)
	{
		FocusIndex = FMath::Clamp(FocusIndex, 0, LastButtons.Num() - 1);
	}

	// fade on top of everything
	if (FadeDuration > 0.f)
	{
		FadeTime += Dt;
		const float A = FMath::Lerp(FadeFrom, FadeTo, FMath::Clamp(FadeTime / FadeDuration, 0.f, 1.f));
		if (A > 0.001f)
		{
			Rect(0.f, 0.f, W(), H(), FLinearColor(0.f, 0.f, 0.f, A));
		}
		if (FadeTime > FadeDuration && FadeTo <= 0.f)
		{
			FadeDuration = 0.f;
		}
	}
}

// ====================================================================================== in-game HUD

void ASUHUD::DrawGameHUD(float DeltaSeconds)
{
	const ASUCharacter* Char = GetChar();
	const ASUGameState* GS = ASUGameState::Get(this);
	const ASUMissionManager* MM = ASUMissionManager::Get(this);
	const bool bAiming = Char && Char->GetPhotoCamera() && Char->GetPhotoCamera()->IsAiming();

	DrawEffects(Char, DeltaSeconds);
	if (bAiming)
	{
		DrawViewfinder(Char);
		DrawSubtitles(Char, MM, DeltaSeconds);
		return;
	}
	if (Char && !Char->IsDead() && Screen == ESUScreen::None)
	{
		DrawPrompt(Char);
		DrawCompass(Char, MM);
	}
	if (Screen == ESUScreen::None || Screen == ESUScreen::Map)
	{
		DrawObjectives(MM, Screen == ESUScreen::Map);
	}
	if (Char && Char->IsSeated() && Screen == ESUScreen::None)
	{
		DrawVehicleHUD(Char);
	}
	if (Screen == ESUScreen::None)
	{
		DrawVitals(Char, GS);
	}
	DrawSubtitles(Char, MM, DeltaSeconds);

	if (Char && Char->IsDead())
	{
		Rect(0.f, 0.f, W(), H(), FLinearColor(0.08f, 0.f, 0.f, 0.55f));
		Text(TEXT("Du bist bewusstlos …"), W() * 0.5f, H() * 0.45f, 30.f, SUColors::Bad, 1, true);
		Text(TEXT("Du wachst gleich im Turm wieder auf."), W() * 0.5f, H() * 0.45f + S(48.f), 17.f, SUColors::Dim, 1);
	}
}

void ASUHUD::DrawEffects(const ASUCharacter* Char, float DeltaSeconds)
{
	if (DamageFlash > 0.f)
	{
		Rect(0.f, 0.f, W(), H(), FLinearColor(0.6f, 0.f, 0.f, DamageFlash * 0.45f));
		DamageFlash = FMath::Max(0.f, DamageFlash - DeltaSeconds * 1.6f);
	}
	if (Char && !Char->IsDead() && Char->GetHealth() < 35.f)
	{
		// low health: pulsing dark red border
		const float Pulse = 0.25f + 0.15f * FMath::Sin(RealTime * 4.f);
		const float B = S(70.f);
		const FLinearColor C(0.35f, 0.f, 0.f, Pulse * (1.f - Char->GetHealth() / 35.f) + 0.1f);
		Rect(0.f, 0.f, W(), B, C);
		Rect(0.f, H() - B, W(), B, C);
		Rect(0.f, B, B, H() - 2.f * B, C);
		Rect(W() - B, B, B, H() - 2.f * B, C);
	}
}

void ASUHUD::DrawPrompt(const ASUCharacter* Char)
{
	const float CX = W() * 0.5f;
	const float CY = H() * 0.5f;
	const USUInteractionComponent* I = Char->GetInteraction();
	const bool bFocus = I && I->HasFocus();
	// small centre dot, brighter while something can be used
	const float Dot = S(bFocus ? 3.f : 2.f);
	Rect(CX - Dot * 0.5f, CY - Dot * 0.5f, Dot, Dot, bFocus ? FLinearColor(1.f, 0.95f, 0.85f, 0.95f) : FLinearColor(1.f, 1.f, 1.f, 0.45f));
	if (!bFocus)
	{
		return;
	}
	const FSUInteractionPrompt& P = I->GetPrompt();
	float Y = CY + S(26.f);
	if (P.bPrimary)
	{
		Text(FString::Printf(TEXT("[E]  %s"), *P.Primary.ToString()), CX, Y, 18.f, SUColors::Text, 1, true);
		Y += S(26.f);
	}
	if (P.bSecondary)
	{
		Text(FString::Printf(TEXT("[Q]  %s"), *P.Secondary.ToString()), CX, Y, 16.f, SUColors::Text, 1);
		Y += S(24.f);
	}
	if (!P.Info.IsEmpty())
	{
		Text(P.Info.ToString(), CX, Y, 14.f, SUColors::Dim, 1);
	}
}

void ASUHUD::DrawObjectives(const ASUMissionManager* MM, bool bCompact)
{
	if (!MM)
	{
		return;
	}
	const ASUPlayerController* PC = GetPC();
	FString Title;
	TArray<TPair<FString, int32>> Lines; // 0 open, 1 done, 2 optional, 3 hint
	const FSUMissionDef* Def = MM->GetActiveDef();
	switch (MM->GetPhase())
	{
	case ESUMissionPhase::Incoming:
		Title = TEXT("Funkspruch");
		Lines.Add({TEXT("Das Funkgerät im Turm meldet sich – [E] am Funkgerät"), 0});
		break;
	case ESUMissionPhase::Briefing:
		Title = TEXT("Funkspruch");
		Lines.Add({TEXT("Die Zentrale spricht …"), 3});
		break;
	case ESUMissionPhase::Offered:
		Title = Def ? Def->Title.ToString() : TEXT("Auftrag");
		Lines.Add({TEXT("Auftrag am Funkgerät bestätigen [E]"), 0});
		Lines.Add({TEXT("Wiederholen lassen: [Q] am Funkgerät"), 3});
		break;
	case ESUMissionPhase::Active:
		if (Def)
		{
			Title = Def->Title.ToString();
			const int32 Stage = MM->GetCurrentStage();
			const TArray<FSUObjectiveState>& States = MM->GetObjectiveStates();
			for (int32 I = 0; I < Def->Objectives.Num(); ++I)
			{
				const FSUObjectiveDef& O = Def->Objectives[I];
				const bool bDone = States.IsValidIndex(I) && States[I].bDone;
				if (O.bHidden || (O.Stage != Stage && !(O.bOptional && O.Stage <= Stage && !bDone)))
				{
					continue;
				}
				FString L = O.Text.ToString();
				if (O.Required > 1 && States.IsValidIndex(I))
				{
					L += FString::Printf(TEXT(" (%d/%d)"), FMath::Min(States[I].Count, O.Required), O.Required);
				}
				Lines.Add({L, bDone ? 1 : (O.bOptional ? 2 : 0)});
			}
		}
		break;
	case ESUMissionPhase::Completed:
	case ESUMissionPhase::None:
	default:
		if (!PC || !PC->IsJournalPinned())
		{
			return;
		}
		Title = TEXT("Kein aktiver Einsatz");
		Lines.Add({TEXT("Warte auf den nächsten Funkspruch der Zentrale."), 3});
		break;
	}

	// show for a while after a change, permanently when pinned (J)
	int32 Hash = GetTypeHash(Title);
	for (const auto& L : Lines)
	{
		Hash = HashCombine(Hash, GetTypeHash(L.Key) + L.Value);
	}
	if (Hash != LastObjectiveHash)
	{
		LastObjectiveHash = Hash;
		ObjectiveFlash = 12.f;
	}
	ObjectiveFlash = FMath::Max(0.f, ObjectiveFlash - static_cast<float>(FApp::GetDeltaTime()));
	const bool bPinned = PC && PC->IsJournalPinned();
	if (!bPinned && !bCompact && ObjectiveFlash <= 0.f)
	{
		Text(TEXT("[J] Aufgaben"), S(36.f), S(30.f), 13.f, FLinearColor(1.f, 1.f, 1.f, 0.35f));
		return;
	}
	const float Alpha = bPinned || bCompact ? 1.f : FMath::Clamp(ObjectiveFlash, 0.f, 1.f);
	const float X = S(36.f);
	float Y = S(30.f);
	Text(Title, X, Y, 19.f, FLinearColor(SUColors::Accent.R, SUColors::Accent.G, SUColors::Accent.B, Alpha), 0, true);
	Y += S(30.f);
	for (const auto& L : Lines)
	{
		const FString Prefix = L.Value == 1 ? TEXT("✓ ") : (L.Value == 3 ? TEXT("") : TEXT("○ "));
		FLinearColor C = L.Value == 1 ? SUColors::Dim : (L.Value == 2 ? FLinearColor(0.75f, 0.8f, 0.9f) : SUColors::Text);
		C.A = Alpha;
		Text(Prefix + L.Key, X, Y, 15.f, C);
		Y += S(23.f);
	}
}

void ASUHUD::DrawCompass(const ASUCharacter* Char, const ASUMissionManager* MM)
{
	const USUGameUserSettings* Settings = USUGameUserSettings::Get();
	if (Settings && !Settings->bShowCompass)
	{
		return;
	}
	const float Yaw = Char->GetFirstPersonCamera() ? Char->GetFirstPersonCamera()->GetComponentRotation().Yaw : Char->GetActorRotation().Yaw;
	const float CX = W() * 0.5f;
	const float Y = S(26.f);
	const float Width = S(520.f);
	const float Range = 100.f; // degrees visible
	Rect(CX - Width * 0.5f, Y + S(22.f), Width, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.25f));
	auto ToX = [&](float Deg) -> TOptional<float>
	{
		const float D = FRotator::NormalizeAxis(Deg - Yaw);
		if (FMath::Abs(D) > Range * 0.5f)
		{
			return TOptional<float>();
		}
		return CX + D / (Range * 0.5f) * Width * 0.5f;
	};
	static const TCHAR* Names[8] = {TEXT("N"), TEXT("NO"), TEXT("O"), TEXT("SO"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")};
	for (int32 I = 0; I < 24; ++I)
	{
		const float Deg = I * 15.f;
		if (TOptional<float> X = ToX(Deg))
		{
			const float Fade = 1.f - FMath::Abs(X.GetValue() - CX) / (Width * 0.5f);
			if (I % 3 == 0)
			{
				const bool bMain = I % 6 == 0;
				Text(Names[I / 3], X.GetValue(), Y, bMain ? 16.f : 13.f, FLinearColor(1.f, 1.f, 1.f, 0.4f + 0.5f * Fade), 1, bMain);
			}
			else
			{
				Rect(X.GetValue(), Y + S(16.f), 1.f, S(6.f), FLinearColor(1.f, 1.f, 1.f, 0.3f * Fade));
			}
		}
	}
	// mission marker
	FVector Target;
	FText Label;
	if (MM && MM->GetMarker(Target, Label))
	{
		const FVector To = Target - Char->GetActorLocation();
		const float Deg = To.Rotation().Yaw;
		const float D = FRotator::NormalizeAxis(Deg - Yaw);
		const float X = CX + FMath::Clamp(D / (Range * 0.5f), -1.f, 1.f) * Width * 0.5f;
		Text(TEXT("▼"), X, Y + S(24.f), 14.f, SUColors::Accent, 1);
		const float Dist = To.Size2D() / 100.f;
		const FString DistText = Dist > 1000.f ? FString::Printf(TEXT("%.1f km"), Dist / 1000.f) : FString::Printf(TEXT("%d m"), FMath::RoundToInt(Dist));
		Text(FString::Printf(TEXT("%s  %s"), *Label.ToString(), *DistText), X, Y + S(42.f), 12.f, SUColors::Accent, 1);
	}
}

void ASUHUD::DrawSubtitles(const ASUCharacter* Char, const ASUMissionManager* MM, float DeltaSeconds)
{
	const USUGameUserSettings* Settings = USUGameUserSettings::Get();
	const float Scale = Settings ? Settings->SubtitleScale : 1.f;
	const bool bEnabled = !Settings || Settings->bSubtitles;
	FString Speaker;
	FString Line;
	bool bRadio = false;

	// 1) dispatch radio: only heard near a radio set or in the car (CB radio)
	if (MM && Char)
	{
		FSUSpeechLine L;
		float LineTime = 0.f;
		if (MM->GetTransmissionLine(L, LineTime))
		{
			bool bHeard = Char->IsSeated();
			for (TActorIterator<ASURadioStation> It(GetWorld()); It && !bHeard; ++It)
			{
				bHeard = FVector::Dist(It->GetActorLocation(), Char->GetActorLocation()) < 1500.f;
			}
			if (bHeard)
			{
				Speaker = L.Speaker.ToString();
				Line = L.Text.ToString();
				bRadio = true;
			}
		}
		else if (MM->GetPhase() == ESUMissionPhase::Incoming && Char->IsSeated())
		{
			Speaker = TEXT("Funk");
			Line = TEXT("(Die Zentrale ruft den Posten. Zurück zum Funkgerät im Turm.)");
			bRadio = true;
		}
	}
	// 2) car radio (news / something else on the frequency)
	if (Line.IsEmpty() && Char && Char->GetVehicle())
	{
		FSUSpeechLine L;
		if (Char->GetVehicle()->GetRadioSubtitle(L))
		{
			Speaker = L.Speaker.ToString();
			Line = L.Text.ToString();
			bRadio = true;
		}
	}
	// 3) people talking to us
	if (SpeechQueue.Num() > 0)
	{
		FSubtitle& Sub = SpeechQueue[0];
		Sub.Age += DeltaSeconds;
		if (Sub.Age > Sub.Line.GetDuration())
		{
			SpeechQueue.RemoveAt(0);
		}
		else if (Line.IsEmpty() || !Sub.bRadio)
		{
			Speaker = Sub.Line.Speaker.ToString();
			Line = Sub.Line.Text.ToString();
			bRadio = Sub.bRadio;
		}
	}
	if (!bEnabled || Line.IsEmpty())
	{
		return;
	}
	const float Size = 19.f * Scale;
	const float MaxW = FMath::Min(W() * 0.7f, S(1100.f));
	const FSlateFontInfo F = Font(Size);
	const TArray<FString> Lines = WrapLines(FString::Printf(TEXT("%s: %s"), *Speaker, *Line), MaxW, F);
	const float LineH = S(Size) * 1.4f;
	float Y = H() - S(150.f) - Lines.Num() * LineH;
	const float BoxW = MaxW + S(30.f);
	Rect(W() * 0.5f - BoxW * 0.5f, Y - S(8.f), BoxW, Lines.Num() * LineH + S(14.f), FLinearColor(0.f, 0.f, 0.f, 0.45f));
	for (int32 I = 0; I < Lines.Num(); ++I)
	{
		Text(Lines[I], W() * 0.5f, Y, Size, bRadio ? FLinearColor(0.75f, 0.92f, 1.f) : SUColors::Text, 1);
		Y += LineH;
	}
}

void ASUHUD::DrawNotifications(float DeltaSeconds)
{
	float Y = S(110.f);
	for (int32 I = 0; I < Notices.Num();)
	{
		FNotice& N = Notices[I];
		N.Age += DeltaSeconds;
		if (N.Age > N.Data.Duration)
		{
			Notices.RemoveAt(I);
			continue;
		}
		const float A = FMath::Clamp((N.Data.Duration - N.Age) / 0.6f, 0.f, 1.f) * FMath::Clamp(N.Age / 0.15f, 0.f, 1.f);
		FLinearColor C = N.Data.Color;
		C.A = A;
		const FString T = N.Data.Text.ToString();
		const FVector2D Sz = Measure(T, Font(16.f));
		Rect(W() - S(40.f) - Sz.X - S(16.f), Y - S(5.f), Sz.X + S(32.f), Sz.Y + S(10.f), FLinearColor(0.f, 0.f, 0.f, 0.5f * A));
		Text(T, W() - S(40.f), Y, 16.f, C, 2);
		Y += Sz.Y + S(16.f);
		++I;
	}
}

void ASUHUD::DrawVitals(const ASUCharacter* Char, const ASUGameState* GS)
{
	if (!Char)
	{
		return;
	}
	const float X = S(36.f);
	float Y = H() - S(110.f);
	// health bar
	const float BarW = S(180.f);
	Rect(X, Y, BarW, S(6.f), FLinearColor(0.f, 0.f, 0.f, 0.5f));
	const float Hp = FMath::Clamp(Char->GetHealth() / 100.f, 0.f, 1.f);
	Rect(X, Y, BarW * Hp, S(6.f), Hp < 0.35f ? SUColors::Bad : FLinearColor(0.85f, 0.85f, 0.8f, 0.8f));
	Y += S(14.f);
	if (const USUInventoryComponent* Inv = Char->GetInventory())
	{
		if (Inv->HasItem(SUItem::Flashlight))
		{
			const float Charge = Char->GetFlashlightCharge();
			Text(FString::Printf(TEXT("Lampe %d %%%s"), FMath::RoundToInt(Charge * 100.f), Char->IsFlashlightOn() ? TEXT("  ●") : TEXT("")), X, Y, 13.f, Charge < 0.15f ? SUColors::Bad : SUColors::Dim);
			Y += S(18.f);
		}
		if (Inv->HasItem(SUItem::Camera) && Char->GetPhotoCamera())
		{
			const USUPhotoCameraComponent* Cam = Char->GetPhotoCamera();
			Text(FString::Printf(TEXT("Kamera %d %%  ·  %d/%d Fotos%s"), FMath::RoundToInt(Cam->GetBattery() * 100.f), Cam->GetPhotoCount(), Cam->GetCapacity(),
				Char->GetHeldItem() == ESUHeldItem::Camera ? TEXT("  [RMT] Sucher") : TEXT("  [C]")), X, Y, 13.f, SUColors::Dim);
			Y += S(18.f);
		}
	}
	if (GS)
	{
		Text(FString::Printf(TEXT("Tag %d  %s   ·   Konto %s"), GS->GetDay(), *SUFormatTime(GS->GetTimeOfDay()), *SUFormatMoney(GS->GetMoneyCents())), X, Y, 13.f, SUColors::Dim);
	}
	if (const ASUPlayerController* PC = GetPC())
	{
		const USUGameUserSettings* Settings = USUGameUserSettings::Get();
		if (PC->GetNetMode() != NM_Standalone && Settings && Settings->bVoiceChatEnabled)
		{
			Text(Settings->bPushToTalk ? TEXT("Sprechfunk: [T] halten") : TEXT("Sprechfunk: offen"), W() - S(36.f), H() - S(40.f), 12.f, SUColors::Dim, 2);
		}
	}
}

void ASUHUD::DrawVehicleHUD(const ASUCharacter* Char)
{
	const ASUVehicle* V = Char->GetVehicle();
	if (!V)
	{
		return;
	}
	// rear mirror (driver + front passenger)
	if (Char->GetSeatIndex() <= 1)
	{
		if (UTextureRenderTarget2D* Mirror = V->GetMirrorTexture())
		{
			const float MW = S(380.f);
			const float MH = MW * 160.f / 512.f;
			const float MX = W() * 0.5f - MW * 0.5f;
			Rect(MX - S(5.f), S(14.f) - S(5.f), MW + S(10.f), MH + S(10.f), FLinearColor(0.02f, 0.02f, 0.02f, 1.f));
			Image(Mirror, MX, S(14.f), MW, MH, FLinearColor::White, true);
		}
	}
	const float X = W() - S(300.f);
	const float Y = H() - S(170.f);
	Panel(X, Y, S(264.f), S(132.f), 0.55f);
	Text(FString::Printf(TEXT("%d"), FMath::RoundToInt(V->GetSpeedKmh())), X + S(20.f), Y + S(8.f), 40.f, SUColors::Text, 0, true);
	Text(TEXT("km/h"), X + S(110.f), Y + S(30.f), 14.f, SUColors::Dim);
	Text(V->GetGearText(), X + S(240.f), Y + S(14.f), 26.f, SUColors::Accent, 2, true);
	// fuel gauge
	const float Fuel = FMath::Clamp(V->GetFuelLiters() / FMath::Max(1.f, V->GetFuelCapacity()), 0.f, 1.f);
	Rect(X + S(20.f), Y + S(70.f), S(224.f), S(6.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Rect(X + S(20.f), Y + S(70.f), S(224.f) * Fuel, S(6.f), Fuel < 0.12f ? SUColors::Bad : FLinearColor(0.9f, 0.8f, 0.5f));
	Text(FString::Printf(TEXT("Tank %.0f l"), V->GetFuelLiters()), X + S(20.f), Y + S(80.f), 13.f, SUColors::Dim);
	Text(FString::Printf(TEXT("Motor %d %%"), FMath::RoundToInt(V->GetEngineHealth())), X + S(244.f), Y + S(80.f), 13.f, V->GetEngineHealth() < 40.f ? SUColors::Bad : SUColors::Dim, 2);
	// indicators
	FString Ind;
	if (V->GetLightsMode() == 1) { Ind += TEXT("Abblendlicht  "); }
	if (V->GetLightsMode() == 2) { Ind += TEXT("Fernlicht  "); }
	if (V->AreWipersOn()) { Ind += TEXT("Wischer  "); }
	const int32 Blink = V->GetBlinkerMode();
	const bool bBlinkPhase = FMath::Fmod(RealTime, 0.72f) < 0.36f;
	if (Blink == 1 || Blink == 3) { Text(TEXT("◄"), X + S(20.f), Y + S(100.f), 14.f, bBlinkPhase ? SUColors::Good : SUColors::Dim); }
	if (Blink == 2 || Blink == 3) { Text(TEXT("►"), X + S(244.f), Y + S(100.f), 14.f, bBlinkPhase ? SUColors::Good : SUColors::Dim, 2); }
	Text(Ind, X + S(132.f), Y + S(102.f), 12.f, SUColors::Dim, 1);
	if (V->IsRadioOn())
	{
		Text(FString::Printf(TEXT("♪ %s  %d %%"), *V->GetRadioStationName().ToString(), FMath::RoundToInt(V->GetRadioVolume() * 100.f)), X + S(244.f), Y - S(24.f), 13.f, SUColors::Dim, 2);
	}
	if (!V->IsEngineRunning() && Char->IsDriver())
	{
		Text(TEXT("[F] Motor starten   [E] Aussteigen"), W() * 0.5f, H() - S(90.f), 15.f, SUColors::Text, 1);
	}
}

void ASUHUD::DrawViewfinder(const ASUCharacter* Char)
{
	const USUPhotoCameraComponent* Cam = Char->GetPhotoCamera();
	if (!Cam)
	{
		return;
	}
	const float M = S(60.f);
	const float L = S(60.f);
	const FLinearColor C(1.f, 1.f, 1.f, 0.85f);
	// corner brackets
	DrawLine(M, M, M + L, M, C, 2.f);              DrawLine(M, M, M, M + L, C, 2.f);
	DrawLine(W() - M, M, W() - M - L, M, C, 2.f);  DrawLine(W() - M, M, W() - M, M + L, C, 2.f);
	DrawLine(M, H() - M, M + L, H() - M, C, 2.f);  DrawLine(M, H() - M, M, H() - M - L, C, 2.f);
	DrawLine(W() - M, H() - M, W() - M - L, H() - M, C, 2.f); DrawLine(W() - M, H() - M, W() - M, H() - M - L, C, 2.f);
	// focus frame in the centre: green when sharp
	const float F = S(46.f);
	const bool bSharp = Cam->GetFocusError() < 0.25f;
	const FLinearColor FC = bSharp ? SUColors::Good : FLinearColor(1.f, 1.f, 1.f, 0.6f);
	Frame(W() * 0.5f - F, H() * 0.5f - F * 0.7f, F * 2.f, F * 1.4f, FC, 1.5f);
	// rule of thirds
	for (int32 I = 1; I <= 2; ++I)
	{
		Rect(W() * I / 3.f, M + L, 1.f, H() - 2.f * (M + L), FLinearColor(1.f, 1.f, 1.f, 0.08f));
		Rect(M + L, H() * I / 3.f, W() - 2.f * (M + L), 1.f, FLinearColor(1.f, 1.f, 1.f, 0.08f));
	}
	// recording info
	const bool bRec = FMath::Fmod(RealTime, 1.2f) < 0.8f;
	if (bRec)
	{
		Text(TEXT("●"), M + S(10.f), M + S(10.f), 16.f, SUColors::Bad);
	}
	Text(FString::Printf(TEXT("%.1fx"), Cam->GetZoom()), M + S(36.f), M + S(10.f), 16.f, C);
	Text(Cam->GetExposureText(), W() * 0.5f, H() - M - S(30.f), 15.f, C, 1, false, true, true);
	Text(FString::Printf(TEXT("%d/%d"), Cam->GetPhotoCount(), Cam->GetCapacity()), W() - M - S(10.f), M + S(10.f), 16.f, C, 2);
	Text(FString::Printf(TEXT("Akku %d %%"), FMath::RoundToInt(Cam->GetBattery() * 100.f)), W() - M - S(10.f), M + S(34.f), 13.f, Cam->GetBattery() < 0.15f ? SUColors::Bad : C, 2);
	Text(Cam->IsFlashEnabled() ? TEXT("Blitz AN [G]") : TEXT("Blitz AUS [G]"), M + S(10.f), H() - M - S(30.f), 13.f, C);
	Text(FString::Printf(TEXT("Fokus %.1f m"), Cam->GetFocusDistance() / 100.f), W() - M - S(10.f), H() - M - S(30.f), 13.f, FC, 2);
	if (Cam->IsShaky())
	{
		Text(TEXT("VERWACKELT – ruhig halten"), W() * 0.5f, H() * 0.5f + F + S(10.f), 14.f, SUColors::Accent, 1);
	}
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() - Cam->GetLastShotTime() < 0.12)
	{
		Rect(0.f, 0.f, W(), H(), FLinearColor(1.f, 1.f, 1.f, 0.25f)); // shutter flash (drawn after the capture)
	}
}

// ====================================================================================== screens dispatch

void ASUHUD::DrawScreen()
{
	switch (Screen)
	{
	case ESUScreen::MainMenu: DrawMainMenu(); break;
	case ESUScreen::Pause: DrawPause(); break;
	case ESUScreen::Settings: DrawSettings(); break;
	case ESUScreen::Join: DrawJoin(); break;
	case ESUScreen::Inventory: DrawInventory(); break;
	case ESUScreen::Terminal: DrawTerminal(); break;
	case ESUScreen::Map: DrawMap(true); break;
	case ESUScreen::Shop: DrawShop(); break;
	case ESUScreen::Document: DrawDocument(); break;
	case ESUScreen::Gallery: DrawGallery(); break;
	case ESUScreen::Help: DrawHelp(); break;
	case ESUScreen::Trunk: DrawTrunk(); break;
	default: break;
	}
	DrawNotifications(static_cast<float>(FApp::GetDeltaTime()));
}
