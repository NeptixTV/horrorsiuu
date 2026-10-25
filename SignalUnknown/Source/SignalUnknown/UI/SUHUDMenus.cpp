// SIGNAL UNKNOWN – HUD screens: main menu, pause, settings, join, help, map.
#include "UI/SUHUD.h"
#include "SignalUnknown.h"
#include "Core/SUPlayerController.h"
#include "Core/SUGameInstance.h"
#include "Core/SUGameState.h"
#include "Core/SUGameUserSettings.h"
#include "Core/SUSaveSubsystem.h"
#include "Player/SUCharacter.h"
#include "Vehicle/SUVehicle.h"
#include "Mission/SUMissionManager.h"
#include "World/SUWorldBuilder.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	const FLinearColor CText(0.92f, 0.90f, 0.85f);
	const FLinearColor CDim(0.62f, 0.60f, 0.56f);
	const FLinearColor CAccent(1.f, 0.72f, 0.32f);
	const FLinearColor CGood(0.55f, 0.95f, 0.6f);

	FString OnOff(bool b) { return b ? TEXT("An") : TEXT("Aus"); }
	FString Percent(float V) { return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(V * 100.f)); }
	float Step(float V, int32 Dir, float StepSize, float Min, float Max) { return FMath::Clamp(FMath::RoundToFloat((V + Dir * StepSize) / StepSize) * StepSize, Min, Max); }
}

// ====================================================================================== main menu

void ASUHUD::DrawMainMenu()
{
	ASUPlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	// darken the menu camera view on the left side
	Rect(0.f, 0.f, S(560.f), H(), FLinearColor(0.f, 0.f, 0.f, 0.62f));
	Text(TEXT("SIGNAL UNKNOWN"), S(70.f), S(120.f), 48.f, CText, 0, true);
	Text(TEXT("Beobachtungsposten 07 – Nördliches Waldgebiet"), S(72.f), S(186.f), 16.f, CDim);

	const USUSaveSubsystem* Save = PC->GetGameInstance() ? PC->GetGameInstance()->GetSubsystem<USUSaveSubsystem>() : nullptr;
	const bool bHasSave = Save && Save->HasSave();
	const ASUGameState* GS = ASUGameState::Get(this);
	const bool bReady = GS && GS->IsWorldReady();

	float Y = S(270.f);
	const float X = S(70.f);
	const float BW = S(380.f);
	const float BH = S(46.f);
	const float Gap = S(12.f);
	if (bConfirmQuit)
	{
		Text(TEXT("Spiel wirklich beenden?"), X, Y, 20.f, CText);
		Y += S(44.f);
		Button(TEXT("Ja, beenden"), X, Y, BW, BH, [PC]() { PC->MenuQuit(); });
		Y += BH + Gap;
		Button(TEXT("Zurück"), X, Y, BW, BH, [this]() { bConfirmQuit = false; FocusIndex = 0; });
		return;
	}
	if (bHasSave)
	{
		Button(TEXT("Fortsetzen (allein)"), X, Y, BW, BH, [PC]() { PC->MenuStartSolo(true); }, bReady);
		Text(Save->DescribeSave(), X + BW + S(16.f), Y + S(13.f), 14.f, CDim);
		Y += BH + Gap;
	}
	Button(TEXT("Neues Spiel (allein)"), X, Y, BW, BH, [PC]() { PC->MenuStartSolo(false); }, bReady);
	Y += BH + Gap * 2.f;
	if (bHasSave)
	{
		Button(TEXT("Koop hosten – Spielstand fortsetzen"), X, Y, BW, BH, [PC]() { PC->MenuHost(true); });
		Y += BH + Gap;
	}
	Button(TEXT("Koop hosten – neues Spiel"), X, Y, BW, BH, [PC]() { PC->MenuHost(false); });
	Y += BH + Gap;
	Button(TEXT("Koop beitreten"), X, Y, BW, BH, [PC]() { PC->OpenScreen(ESUScreen::Join); });
	Y += BH + Gap * 2.f;
	Button(TEXT("Einstellungen"), X, Y, BW, BH, [PC]() { PC->OpenScreen(ESUScreen::Settings); });
	Y += BH + Gap;
	Button(TEXT("Steuerung"), X, Y, BW, BH, [PC]() { PC->OpenScreen(ESUScreen::Help); });
	Y += BH + Gap;
	Button(TEXT("Beenden"), X, Y, BW, BH, [this]() { bConfirmQuit = true; FocusIndex = 0; });

	if (!bReady)
	{
		Text(TEXT("Die Welt wird erzeugt …"), X, H() - S(80.f), 15.f, CAccent);
	}
	Text(TEXT("Koop: 1–4 Spieler · Host: Port 7777 freigeben · Stimme über die Engine-VoIP (Push-to-Talk [T])"), X, H() - S(50.f), 12.f, CDim);
	Text(TEXT("Prototyp – Platzhaltergrafik"), W() - S(30.f), H() - S(40.f), 12.f, CDim, 2);
}

// ====================================================================================== pause

void ASUHUD::DrawPause()
{
	ASUPlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.f, 0.f, 0.f, 0.55f));
	DrawScreenTitle(TEXT("Pause"), PC->GetNetMode() == NM_Standalone ? TEXT("Das Spiel ist angehalten") : TEXT("Koop-Sitzung läuft weiter"));
	const bool bHost = PC->GetNetMode() != NM_Client;
	const float X = S(70.f);
	float Y = S(130.f);
	const float BW = S(360.f);
	const float BH = S(44.f);
	const float Gap = S(10.f);
	if (bConfirmQuit)
	{
		Text(TEXT("Ungespeicherter Fortschritt geht verloren."), X, Y, 18.f, CText);
		Y += S(40.f);
		Button(TEXT("Zum Hauptmenü"), X, Y, BW, BH, [PC]() { PC->LeaveToMainMenu(); });
		Y += BH + Gap;
		Button(TEXT("Abbrechen"), X, Y, BW, BH, [this]() { bConfirmQuit = false; FocusIndex = 0; });
		return;
	}
	Button(TEXT("Weiter"), X, Y, BW, BH, [PC]() { PC->CloseScreen(); });
	Y += BH + Gap;
	Button(TEXT("Speichern"), X, Y, BW, BH, [PC]() { PC->ServerRequestSave(); }, bHost);
	Y += BH + Gap;
	Button(TEXT("Letzten Spielstand laden"), X, Y, BW, BH, [PC]() { PC->ServerRequestLoad(); PC->CloseScreen(); }, bHost);
	Y += BH + Gap;
	Button(TEXT("Einstellungen"), X, Y, BW, BH, [PC]() { PC->OpenScreen(ESUScreen::Settings); });
	Y += BH + Gap;
	Button(TEXT("Steuerung"), X, Y, BW, BH, [PC]() { PC->OpenScreen(ESUScreen::Help); });
	Y += BH + Gap;
	Button(TEXT("Zum Hauptmenü"), X, Y, BW, BH, [this]() { bConfirmQuit = true; FocusIndex = 0; });
	Y += BH + Gap;
	Button(TEXT("Spiel beenden"), X, Y, BW, BH, [PC]() { PC->MenuQuit(); });

	// session info
	const float IX = X + BW + S(80.f);
	float IY = S(130.f);
	Text(TEXT("Sitzung"), IX, IY, 20.f, CAccent, 0, true);
	IY += S(34.f);
	const FString Mode = PC->GetNetMode() == NM_Standalone ? TEXT("Einzelspieler") : (bHost ? TEXT("Koop – du bist Host") : TEXT("Koop – verbunden"));
	Text(Mode, IX, IY, 15.f, CText);
	IY += S(26.f);
	if (const AGameStateBase* GSB = GetWorld()->GetGameState())
	{
		for (const APlayerState* PS : GSB->PlayerArray)
		{
			if (PS)
			{
				Text(FString::Printf(TEXT("• %s   %d ms"), *PS->GetPlayerName(), FMath::RoundToInt(PS->GetPingInMilliseconds())), IX, IY, 14.f, CDim);
				IY += S(22.f);
			}
		}
	}
	if (!bHost)
	{
		Text(TEXT("Speichern und Laden kann nur der Host."), IX, IY + S(10.f), 13.f, CDim);
	}
}

// ====================================================================================== settings

void ASUHUD::DrawSettings()
{
	USUGameUserSettings* Set = USUGameUserSettings::Get();
	ASUPlayerController* PC = GetPC();
	if (!Set || !PC)
	{
		return;
	}
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.f, 0.f, 0.f, 0.75f));
	DrawScreenTitle(TEXT("Einstellungen"), TEXT("[Q]/[E] Kategorie · Pfeiltasten / Klick ändern · [Esc] übernehmen und zurück"));

	static const TCHAR* Tabs[4] = {TEXT("Grafik"), TEXT("Steuerung"), TEXT("Audio"), TEXT("Spiel")};
	float TX = S(60.f);
	for (int32 I = 0; I < 4; ++I)
	{
		const FVector2D Sz = Text(Tabs[I], TX, S(96.f), 18.f, I == Tab ? CAccent : CDim, 0, I == Tab);
		if (I == Tab)
		{
			Rect(TX, S(122.f), Sz.X, 2.f, CAccent);
		}
		TX += Sz.X + S(36.f);
	}

	const float X = S(60.f);
	const float RowW = FMath::Min(S(760.f), W() - S(120.f));
	float Y = S(150.f);
	const float RowStep = S(38.f);
	auto Row = [&](const FString& Label, const FString& Value, TFunction<void(int32)> Adjust)
	{
		SettingRow(Label, Value, X, Y, RowW, MoveTemp(Adjust));
		Y += RowStep;
	};

	switch (Tab)
	{
	case 0:
	{
		static const TCHAR* Quality[5] = {TEXT("Niedrig"), TEXT("Mittel"), TEXT("Hoch"), TEXT("Episch"), TEXT("Kino")};
		const int32 Level = Set->GetOverallScalabilityLevel();
		Row(TEXT("Grafikvoreinstellung"), Level >= 0 && Level <= 4 ? Quality[Level] : TEXT("Benutzerdefiniert"), [Set](int32 D)
		{
			const int32 L = Set->GetOverallScalabilityLevel();
			Set->SetOverallScalabilityLevel(FMath::Clamp((L < 0 ? 2 : L) + D, 0, 4));
			Set->ApplyNonResolutionSettings();
		});
		TArray<FIntPoint> Resolutions;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
		const FIntPoint Current = Set->GetScreenResolution();
		Row(TEXT("Auflösung"), FString::Printf(TEXT("%d × %d"), Current.X, Current.Y), [Set, Resolutions](int32 D)
		{
			if (Resolutions.Num() == 0)
			{
				return;
			}
			int32 Index = Resolutions.IndexOfByKey(Set->GetScreenResolution());
			Index = Index == INDEX_NONE ? Resolutions.Num() - 1 : FMath::Clamp(Index + D, 0, Resolutions.Num() - 1);
			Set->SetScreenResolution(Resolutions[Index]);
		});
		const EWindowMode::Type Mode = Set->GetFullscreenMode();
		Row(TEXT("Anzeigemodus"), Mode == EWindowMode::Fullscreen ? TEXT("Vollbild") : (Mode == EWindowMode::WindowedFullscreen ? TEXT("Rahmenloses Fenster") : TEXT("Fenster")), [Set](int32 D)
		{
			const int32 M = (static_cast<int32>(Set->GetFullscreenMode()) + D + 3) % 3;
			Set->SetFullscreenMode(static_cast<EWindowMode::Type>(M));
		});
		Row(TEXT("Auflösungsskalierung"), Percent(Set->GetResolutionScaleNormalized()), [Set](int32 D)
		{
			Set->SetResolutionScaleNormalized(Step(Set->GetResolutionScaleNormalized(), D, 0.05f, 0.f, 1.f));
			Set->ApplyNonResolutionSettings();
		});
		static const float Limits[] = {30.f, 60.f, 90.f, 120.f, 144.f, 0.f};
		const float Limit = Set->GetFrameRateLimit();
		Row(TEXT("FPS-Limit"), Limit <= 0.f ? FString(TEXT("Unbegrenzt")) : FString::Printf(TEXT("%d"), FMath::RoundToInt(Limit)), [Set](int32 D)
		{
			int32 Index = 5;
			for (int32 I = 0; I < 6; ++I)
			{
				if (FMath::IsNearlyEqual(Limits[I], Set->GetFrameRateLimit(), 0.5f))
				{
					Index = I;
				}
			}
			Set->SetFrameRateLimit(Limits[(Index + D + 6) % 6]);
			Set->ApplyNonResolutionSettings();
		});
		Row(TEXT("VSync"), OnOff(Set->IsVSyncEnabled()), [Set](int32) { Set->SetVSyncEnabled(!Set->IsVSyncEnabled()); Set->ApplyNonResolutionSettings(); });
		Row(TEXT("Sichtfeld (FOV)"), FString::Printf(TEXT("%d°"), FMath::RoundToInt(Set->FieldOfView)), [Set](int32 D) { Set->FieldOfView = Step(Set->FieldOfView, D, 5.f, 70.f, 110.f); });
		Row(TEXT("Bewegungsunschärfe"), OnOff(Set->bMotionBlur), [Set](int32) { Set->bMotionBlur = !Set->bMotionBlur; });
		Row(TEXT("Filmkorn"), OnOff(Set->bFilmGrain), [Set](int32) { Set->bFilmGrain = !Set->bFilmGrain; });
		Row(TEXT("Kopfbewegung beim Gehen"), OnOff(Set->bHeadBob), [Set](int32) { Set->bHeadBob = !Set->bHeadBob; });
		Row(TEXT("Rückspiegel im Auto"), OnOff(Set->bRearMirror), [Set](int32) { Set->bRearMirror = !Set->bRearMirror; });
		Y += S(10.f);
		Button(TEXT("Auflösung / Anzeigemodus anwenden"), X, Y, S(380.f), S(40.f), [Set]() { Set->ApplyResolutionSettings(false); Set->SaveSettings(); });
		break;
	}
	case 1:
		Row(TEXT("Mausempfindlichkeit"), FString::Printf(TEXT("%.2f"), Set->MouseSensitivity), [Set](int32 D) { Set->MouseSensitivity = Step(Set->MouseSensitivity, D, 0.05f, 0.1f, 3.f); });
		Row(TEXT("Y-Achse invertieren"), OnOff(Set->bInvertY), [Set](int32) { Set->bInvertY = !Set->bInvertY; });
		Row(TEXT("Ducken umschalten (statt halten)"), OnOff(Set->bToggleCrouch), [Set](int32) { Set->bToggleCrouch = !Set->bToggleCrouch; });
		Y += S(10.f);
		Text(TEXT("Die Tastenbelegung steht unter „Steuerung“. Gamepad wird für Gehen, Fahren und Menüs unterstützt."), X, Y, 14.f, CDim);
		break;
	case 2:
		Row(TEXT("Gesamtlautstärke"), Percent(Set->MasterVolume), [Set](int32 D) { Set->MasterVolume = Step(Set->MasterVolume, D, 0.05f, 0.f, 1.f); });
		Row(TEXT("Umgebung"), Percent(Set->AmbienceVolume), [Set](int32 D) { Set->AmbienceVolume = Step(Set->AmbienceVolume, D, 0.05f, 0.f, 1.f); });
		Row(TEXT("Effekte"), Percent(Set->EffectsVolume), [Set](int32 D) { Set->EffectsVolume = Step(Set->EffectsVolume, D, 0.05f, 0.f, 1.f); });
		Row(TEXT("Radio / Funk"), Percent(Set->RadioVolume), [Set](int32 D) { Set->RadioVolume = Step(Set->RadioVolume, D, 0.05f, 0.f, 1.f); });
		Row(TEXT("Stimmen / Voice-Chat"), Percent(Set->VoiceVolume), [Set](int32 D) { Set->VoiceVolume = Step(Set->VoiceVolume, D, 0.05f, 0.f, 1.f); });
		Row(TEXT("Funkfilter / Rauschen"), Percent(Set->RadioFilterStrength), [Set](int32 D) { Set->RadioFilterStrength = Step(Set->RadioFilterStrength, D, 0.1f, 0.f, 1.f); });
		Row(TEXT("Voice-Chat"), OnOff(Set->bVoiceChatEnabled), [Set](int32) { Set->bVoiceChatEnabled = !Set->bVoiceChatEnabled; });
		Row(TEXT("Push-to-Talk [T]"), Set->bPushToTalk ? TEXT("An") : TEXT("Aus (offenes Mikrofon)"), [Set](int32) { Set->bPushToTalk = !Set->bPushToTalk; });
		Y += S(10.f);
		Text(TEXT("Voice-Chat nutzt die Engine-VoIP (OnlineSubsystem). Stimmen sind räumlich und nur in der Nähe hörbar."), X, Y, 14.f, CDim);
		break;
	default:
		Row(TEXT("Untertitel"), OnOff(Set->bSubtitles), [Set](int32) { Set->bSubtitles = !Set->bSubtitles; });
		Row(TEXT("Untertitelgröße"), Percent(Set->SubtitleScale), [Set](int32 D) { Set->SubtitleScale = Step(Set->SubtitleScale, D, 0.1f, 0.7f, 1.8f); });
		Row(TEXT("Kompass"), OnOff(Set->bShowCompass), [Set](int32) { Set->bShowCompass = !Set->bShowCompass; });
		Row(TEXT("Horror-Intensität"), Set->HorrorIntensity <= 0.f ? FString(TEXT("Aus")) : Percent(Set->HorrorIntensity), [Set](int32 D) { Set->HorrorIntensity = Step(Set->HorrorIntensity, D, 0.25f, 0.f, 2.f); });
		Y += S(10.f);
		Text(TEXT("Die Horror-Intensität des Hosts gilt für die ganze Koop-Sitzung."), X, Y, 14.f, CDim);
		break;
	}

	Button(TEXT("Übernehmen und zurück"), X, H() - S(110.f), S(320.f), S(44.f), [PC, Set]()
	{
		Set->ApplySettings(false);
		PC->OpenScreen(PC->IsInMainMenu() ? ESUScreen::MainMenu : ESUScreen::Pause);
	});
	Button(TEXT("Standardwerte"), X + S(340.f), H() - S(110.f), S(220.f), S(44.f), [Set]()
	{
		Set->SetToDefaults();
		Set->ApplySettings(false);
	});
}

// ====================================================================================== join

void ASUHUD::DrawJoin()
{
	ASUPlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.f, 0.f, 0.f, 0.7f));
	DrawScreenTitle(TEXT("Koop beitreten"), TEXT("Adresse des Hosts eingeben (IPv4, optional :Port) · [Esc] zurück"));
	const float X = S(70.f);
	float Y = S(130.f);
	Panel(X, Y, S(460.f), S(60.f));
	const bool bCursor = FMath::Fmod(RealTime, 1.f) < 0.5f;
	Text(JoinAddress + (bCursor ? TEXT("_") : TEXT(" ")), X + S(16.f), Y + S(14.f), 26.f, CText, 0, false, false, true);
	Y += S(84.f);
	static const TCHAR* Keys[] = {TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("."), TEXT("0"), TEXT(":")};
	const float K = S(70.f);
	for (int32 I = 0; I < 12; ++I)
	{
		const FString Key = Keys[I];
		Button(Key, X + (I % 3) * (K + S(10.f)), Y + (I / 3) * (K + S(10.f)), K, K, [this, Key]()
		{
			if (JoinAddress.Len() < 40)
			{
				JoinAddress += Key;
			}
		}, true, 24.f);
	}
	const float SX = X + 3 * (K + S(10.f)) + S(20.f);
	Button(TEXT("⌫ Löschen"), SX, Y, S(200.f), K, [this]() { JoinAddress.LeftChopInline(1); });
	Button(TEXT("Leeren"), SX, Y + K + S(10.f), S(200.f), K, [this]() { JoinAddress.Reset(); });
	Button(TEXT("localhost"), SX, Y + 2 * (K + S(10.f)), S(200.f), K, [this]() { JoinAddress = TEXT("127.0.0.1"); });
	Button(TEXT("Verbinden"), SX, Y + 3 * (K + S(10.f)), S(200.f), K, [this, PC]()
	{
		if (!JoinAddress.IsEmpty())
		{
			PC->MenuJoin(JoinAddress);
		}
	}, !JoinAddress.IsEmpty());
	Text(TEXT("Der Host wählt im Hauptmenü „Koop hosten“. Im selben Netzwerk die lokale IP des Hosts verwenden,"), X, Y + 4 * (K + S(10.f)) + S(20.f), 14.f, CDim);
	Text(TEXT("über das Internet muss beim Host UDP-Port 7777 weitergeleitet sein."), X, Y + 4 * (K + S(10.f)) + S(42.f), 14.f, CDim);
}

// ====================================================================================== help

void ASUHUD::DrawHelp()
{
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.f, 0.f, 0.f, 0.78f));
	DrawScreenTitle(TEXT("Steuerung"), TEXT("[Esc] zurück"));
	struct FEntry { const TCHAR* Key; const TCHAR* What; };
	static const FEntry OnFoot[] = {
		{TEXT("W A S D"), TEXT("Gehen")}, {TEXT("Shift links"), TEXT("Rennen")}, {TEXT("Strg links"), TEXT("Ducken")}, {TEXT("Leertaste"), TEXT("Springen")},
		{TEXT("E"), TEXT("Benutzen / Einsteigen / Lesen")}, {TEXT("Q"), TEXT("Alternative Aktion (Mitnehmen, Zapfpistole herausnehmen …)")},
		{TEXT("F"), TEXT("Taschenlampe")}, {TEXT("C"), TEXT("Kamera ausrüsten")}, {TEXT("Rechte Maustaste"), TEXT("Kamerasucher")},
		{TEXT("Linke Maustaste"), TEXT("Foto aufnehmen")}, {TEXT("Mausrad"), TEXT("Zoom (im Sucher)")}, {TEXT("G"), TEXT("Blitz an/aus")},
		{TEXT("Tab / I"), TEXT("Inventar")}, {TEXT("M"), TEXT("Karte")}, {TEXT("J"), TEXT("Aufgaben anheften")}, {TEXT("P"), TEXT("Fotogalerie")},
		{TEXT("T (halten)"), TEXT("Sprechen (Koop, Push-to-Talk)")}, {TEXT("F5 / F9"), TEXT("Schnellspeichern / -laden (Host)")},
		{TEXT("F1"), TEXT("Diese Hilfe")}, {TEXT("Esc"), TEXT("Pause")},
	};
	static const FEntry Driving[] = {
		{TEXT("W / S"), TEXT("Gas / Bremse, rückwärts")}, {TEXT("A / D"), TEXT("Lenken")}, {TEXT("Leertaste"), TEXT("Handbremse")},
		{TEXT("F"), TEXT("Motor an/aus")}, {TEXT("L"), TEXT("Licht")}, {TEXT("K"), TEXT("Fernlicht")}, {TEXT("H"), TEXT("Hupe")},
		{TEXT("Q / R"), TEXT("Blinker links / rechts")}, {TEXT("X"), TEXT("Warnblinker")}, {TEXT("V"), TEXT("Scheibenwischer")},
		{TEXT("N"), TEXT("Radio an/aus")}, {TEXT("Pfeil links / rechts"), TEXT("Radiosender wechseln")}, {TEXT("Mausrad"), TEXT("Radiolautstärke")},
		{TEXT("E"), TEXT("Aussteigen (nur im Stand)")},
	};
	auto Column = [this](const TCHAR* Title, const FEntry* Entries, int32 Num, float X)
	{
		float Y = S(110.f);
		Text(Title, X, Y, 20.f, CAccent, 0, true);
		Y += S(36.f);
		for (int32 I = 0; I < Num; ++I)
		{
			Text(Entries[I].Key, X, Y, 15.f, CText, 0, true);
			Text(Entries[I].What, X + S(190.f), Y, 15.f, CDim);
			Y += S(25.f);
		}
	};
	Column(TEXT("Zu Fuß"), OnFoot, UE_ARRAY_COUNT(OnFoot), S(60.f));
	Column(TEXT("Im Fahrzeug"), Driving, UE_ARRAY_COUNT(Driving), FMath::Max(S(760.f), W() * 0.5f));
	Text(TEXT("Gamepad: linker Stick gehen, rechter Stick schauen, X benutzen, Y alternative Aktion, LT Sucher, RT Foto. Belegung: SUInputConfig.cpp."), S(60.f), H() - S(80.f), 13.f, CDim);
}

// ====================================================================================== map

void ASUHUD::DrawMap(bool bFullScreen)
{
	const ASUWorldBuilder* Builder = ASUWorldBuilder::Get(this);
	const ASUCharacter* Char = GetChar();
	if (!Builder)
	{
		return;
	}
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.01f, 0.01f, 0.01f, 0.9f));
	DrawScreenTitle(TEXT("Karte – Nördliches Waldgebiet"), TEXT("Mausrad: Zoom · Pfeiltasten: verschieben · [Esc]/[M] schließen"));

	const float Size = FMath::Min(H() - S(160.f), W() - S(520.f));
	const float MX = S(60.f);
	const float MY = S(100.f);
	if (bMapFollowPlayer && Char)
	{
		MapCenter = Builder->WorldToMapUV(Char->GetActorLocation());
	}
	const float Half = 0.5f / MapZoom;
	MapCenter.X = FMath::Clamp(MapCenter.X, Half, 1.f - Half);
	MapCenter.Y = FMath::Clamp(MapCenter.Y, Half, 1.f - Half);
	const FVector2D UV0 = MapCenter - FVector2D(Half, Half);

	// the map image is a real top-down capture of the generated world
	if (UTextureRenderTarget2D* Tex = Builder->GetMapTexture())
	{
		DrawTexture(Tex, MX, MY, Size, Size, UV0.X, UV0.Y, Half * 2.f, Half * 2.f, FLinearColor(0.8f, 0.78f, 0.7f, 1.f), BLEND_Opaque);
	}
	else
	{
		Rect(MX, MY, Size, Size, FLinearColor(0.08f, 0.1f, 0.07f, 1.f));
	}
	auto ToScreen = [&](const FVector2D& UV, FVector2D& Out) -> bool
	{
		const FVector2D Local = (UV - UV0) / (Half * 2.f);
		Out = FVector2D(MX + Local.X * Size, MY + Local.Y * Size);
		return Local.X >= 0.f && Local.X <= 1.f && Local.Y >= 0.f && Local.Y <= 1.f;
	};
	// roads on top for readability
	for (const TArray<FVector2D>& Road : Builder->GetMapRoads())
	{
		for (int32 I = 1; I < Road.Num(); ++I)
		{
			FVector2D A, B;
			const bool bA = ToScreen(Road[I - 1], A);
			const bool bB = ToScreen(Road[I], B);
			if (bA && bB)
			{
				DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(0.85f, 0.8f, 0.6f, 0.75f), FMath::Max(1.f, S(2.f) * FMath::Sqrt(MapZoom)));
			}
		}
	}
	Frame(MX, MY, Size, Size, FLinearColor(0.5f, 0.45f, 0.35f, 1.f), 2.f);

	// places
	struct FPlace { const TCHAR* Point; const TCHAR* Label; };
	static const FPlace Places[] = {
		{TEXT("Tower"), TEXT("Aussichtsturm (Basis)")}, {TEXT("GasStation"), TEXT("Tankstelle Pine Ridge")},
		{TEXT("Forsthaus"), TEXT("Forsthaus Hallberg")}, {TEXT("House3"), TEXT("Siedlung")}, {TEXT("Town"), TEXT("Harrow Falls")},
	};
	for (const FPlace& P : Places)
	{
		FVector2D SP;
		if (ToScreen(Builder->WorldToMapUV(Builder->GetPointLocation(P.Point)), SP))
		{
			Rect(SP.X - S(4.f), SP.Y - S(4.f), S(8.f), S(8.f), FLinearColor(0.95f, 0.92f, 0.85f, 1.f));
			Text(P.Label, SP.X + S(8.f), SP.Y - S(9.f), 13.f, CText);
		}
	}
	// mission marker
	const ASUMissionManager* MM = ASUMissionManager::Get(this);
	FVector Target;
	FText Label;
	if (MM && MM->GetMarker(Target, Label))
	{
		FVector2D SP;
		if (ToScreen(Builder->WorldToMapUV(Target), SP))
		{
			const float Pulse = S(10.f) + S(4.f) * FMath::Sin(RealTime * 4.f);
			Frame(SP.X - Pulse, SP.Y - Pulse, Pulse * 2.f, Pulse * 2.f, CAccent, 2.f);
			Text(Label.ToString(), SP.X, SP.Y + Pulse + S(4.f), 14.f, CAccent, 1, true);
		}
	}
	// vehicle and players
	if (const ASUGameState* GS = ASUGameState::Get(this))
	{
		if (const ASUVehicle* V = GS->MainVehicle.Get())
		{
			FVector2D SP;
			if (ToScreen(Builder->WorldToMapUV(V->GetActorLocation()), SP))
			{
				Rect(SP.X - S(6.f), SP.Y - S(4.f), S(12.f), S(8.f), FLinearColor(0.4f, 0.75f, 1.f, 1.f));
				Text(TEXT("Wagen"), SP.X + S(9.f), SP.Y - S(8.f), 12.f, FLinearColor(0.6f, 0.85f, 1.f));
			}
		}
	}
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		FVector2D SP;
		if (!ToScreen(Builder->WorldToMapUV(It->GetActorLocation()), SP))
		{
			continue;
		}
		const bool bMe = *It == Char;
		const float Yaw = It->GetActorRotation().Yaw;
		// arrow: north (yaw 0) points up on the map
		const FVector2D Dir(FMath::Sin(FMath::DegreesToRadians(Yaw)), -FMath::Cos(FMath::DegreesToRadians(Yaw)));
		const FVector2D Perp(-Dir.Y, Dir.X);
		const float L = S(bMe ? 12.f : 9.f);
		const FLinearColor C = bMe ? FLinearColor(1.f, 0.3f, 0.25f) : CGood;
		const FVector2D Tip = SP + Dir * L;
		const FVector2D BL = SP - Dir * L * 0.6f + Perp * L * 0.6f;
		const FVector2D BR = SP - Dir * L * 0.6f - Perp * L * 0.6f;
		DrawLine(Tip.X, Tip.Y, BL.X, BL.Y, C, 2.f);
		DrawLine(Tip.X, Tip.Y, BR.X, BR.Y, C, 2.f);
		DrawLine(BL.X, BL.Y, BR.X, BR.Y, C, 2.f);
		if (!bMe && It->GetPlayerState())
		{
			Text(It->GetPlayerState()->GetPlayerName(), SP.X + S(10.f), SP.Y - S(8.f), 12.f, CGood);
		}
	}
	// north arrow + scale
	Text(TEXT("N ▲"), MX + Size - S(14.f), MY + S(10.f), 15.f, CText, 2, true);
	const float MetersPerPixel = (Builder->GetMapHalfSize() * 2.f / 100.f) * (Half * 2.f) / Size;
	const float BarMeters = MetersPerPixel * S(120.f) > 1000.f ? 1000.f : (MetersPerPixel * S(120.f) > 500.f ? 500.f : 200.f);
	const float BarPx = BarMeters / MetersPerPixel;
	Rect(MX + S(16.f), MY + Size - S(26.f), BarPx, S(3.f), CText);
	Text(BarMeters >= 1000.f ? TEXT("1 km") : FString::Printf(TEXT("%d m"), FMath::RoundToInt(BarMeters)), MX + S(16.f), MY + Size - S(50.f), 12.f, CText);

	// legend / objectives on the right
	const float LX = MX + Size + S(40.f);
	Text(TEXT("Legende"), LX, MY, 18.f, CAccent, 0, true);
	Text(TEXT("▲ rot: du   ▲ grün: Team   ■ blau: Wagen   □ orange: Einsatzziel"), LX, MY + S(30.f), 13.f, CDim);
	Button(bMapFollowPlayer ? TEXT("Ansicht folgt dir") : TEXT("Auf mich zentrieren"), LX, MY + S(60.f), S(260.f), S(38.f), [this]() { bMapFollowPlayer = true; });
	Button(TEXT("Ganze Karte"), LX, MY + S(106.f), S(260.f), S(38.f), [this]() { MapZoom = 1.f; MapCenter = FVector2D(0.5f, 0.5f); bMapFollowPlayer = false; });
}
