// SIGNAL UNKNOWN – Canvas HUD with all screens.
//
// Immediate-mode UI: every frame the current screen draws its widgets and registers clickable
// buttons. Mouse clicks and keyboard / gamepad navigation (focus index) act on the buttons of the
// last frame. No asset (UMG widget blueprint) is needed, fonts come from the engine's core style.
//
// In-game HUD: interaction prompt, objectives, compass, subtitles (radio only within earshot of a
// radio), notifications, vehicle display + rear mirror, camera viewfinder, vitals, fades.
// Screens: main menu, pause, settings, join, inventory, terminal, map, shop, document, gallery,
// help, trunk.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Core/SUTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "SUHUD.generated.h"

class ASUPlayerController;
class ASUCharacter;
class ASUGameState;
class ASUMissionManager;
class UTexture;
struct FSUPhotoMeta;

UCLASS()
class SIGNALUNKNOWN_API ASUHUD : public AHUD
{
	GENERATED_BODY()

public:
	ASUHUD();

	virtual void DrawHUD() override;

	// ---------------------------------------------------------------- API used by gameplay code
	void AddNotification(const FSUNotification& Notification);
	void AddSpeech(const FSUSpeechLine& Line, bool bRadio);
	void OnScreenChanged(ESUScreen OldScreen, ESUScreen NewScreen);
	void FlashDamage(float Strength);
	void StartFade(float From, float To, float Seconds);
	void SetHiddenForScreenshot(bool bHidden) { bHiddenForScreenshot = bHidden; }

	// ---------------------------------------------------------------- UI input (from the controller)
	void UINavigate(FIntPoint Direction);
	void UIAccept();
	/** Returns true if the screen handled "back" itself (e.g. closed a sub page). */
	bool UIBack();
	void UIClick();
	void UIScroll(float Delta);
	void UITab(int32 Direction);

private:
	// ---------------------------------------------------------------- immediate mode widgets
	struct FButton
	{
		FBox2D Rect;
		TFunction<void()> OnClick;
		TFunction<void(int32)> OnAdjust; // left / right on setting rows
		bool bEnabled = true;
	};
	TArray<FButton> Buttons;
	TArray<FButton> LastButtons;
	int32 FocusIndex = 0;
	bool bMouseMovedSinceNav = true;
	FVector2D LastMouse = FVector2D::ZeroVector;

	struct FNotice
	{
		FSUNotification Data;
		float Age = 0.f;
	};
	TArray<FNotice> Notices;

	struct FSubtitle
	{
		FSUSpeechLine Line;
		bool bRadio = false;
		float Age = 0.f;
	};
	TArray<FSubtitle> SpeechQueue;

	// ---------------------------------------------------------------- screen state
	ESUScreen Screen = ESUScreen::None;
	int32 Tab = 0;
	float Scroll = 0.f;
	int32 SelectedIndex = INDEX_NONE;
	FString JoinAddress;
	TSet<FGuid> ReportSelection;
	bool bShowReportResult = false;
	float MapZoom = 1.f;
	FVector2D MapCenter = FVector2D(0.5f, 0.5f);
	bool bMapFollowPlayer = true;
	bool bConfirmQuit = false;

	// ---------------------------------------------------------------- effects
	float DamageFlash = 0.f;
	float FadeFrom = 0.f;
	float FadeTo = 0.f;
	float FadeDuration = 0.f;
	float FadeTime = 0.f;
	bool bHiddenForScreenshot = false;
	float ObjectiveFlash = 0.f;
	int32 LastObjectiveHash = 0;
	float UIScale = 1.f;
	float RealTime = 0.f;

	// ---------------------------------------------------------------- drawing helpers
	FSlateFontInfo Font(float Size, bool bBold = false, bool bMono = false) const;
	FVector2D Measure(const FString& Text, const FSlateFontInfo& InFont) const;
	/** Draws text. Align: 0 left, 1 centre, 2 right. Returns the size. */
	FVector2D Text(const FString& InText, float X, float Y, float Size, const FLinearColor& Color, int32 Align = 0, bool bBold = false, bool bShadow = true, bool bMono = false);
	/** Word-wrapped text block, returns the height. */
	float TextBlock(const FString& InText, float X, float Y, float Width, float Size, const FLinearColor& Color, bool bMono = false, int32 MaxLines = 0);
	TArray<FString> WrapLines(const FString& InText, float Width, const FSlateFontInfo& InFont) const;
	void Rect(float X, float Y, float W, float H, const FLinearColor& Color);
	void Frame(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness = 1.f);
	void Panel(float X, float Y, float W, float H, float Alpha = 0.82f);
	void Image(UTexture* Texture, float X, float Y, float W, float H, const FLinearColor& Tint = FLinearColor::White, bool bFlipX = false);
	/** Clickable button. */
	bool Button(const FString& Label, float X, float Y, float W, float H, TFunction<void()> OnClick, bool bEnabled = true, float FontSize = 17.f);
	/** Settings row: label, value, arrows. */
	void SettingRow(const FString& Label, const FString& Value, float X, float Y, float W, TFunction<void(int32)> OnAdjust);
	bool IsFocused(int32 ButtonIndex) const;
	float S(float V) const { return V * UIScale; }
	float W() const;
	float H() const;

	// ---------------------------------------------------------------- in-game HUD
	void DrawGameHUD(float DeltaSeconds);
	void DrawPrompt(const ASUCharacter* Char);
	void DrawObjectives(const ASUMissionManager* MM, bool bCompact);
	void DrawCompass(const ASUCharacter* Char, const ASUMissionManager* MM);
	void DrawSubtitles(const ASUCharacter* Char, const ASUMissionManager* MM, float DeltaSeconds);
	void DrawNotifications(float DeltaSeconds);
	void DrawVitals(const ASUCharacter* Char, const ASUGameState* GS);
	void DrawVehicleHUD(const ASUCharacter* Char);
	void DrawViewfinder(const ASUCharacter* Char);
	void DrawEffects(const ASUCharacter* Char, float DeltaSeconds);

	// ---------------------------------------------------------------- screens
	void DrawScreen();
	void DrawMainMenu();
	void DrawPause();
	void DrawSettings();
	void DrawJoin();
	void DrawInventory();
	void DrawTerminal();
	void DrawTerminalMission(float X, float Y, float Width, float Height);
	void DrawTerminalReport(float X, float Y, float Width, float Height);
	void DrawTerminalAccount(float X, float Y, float Width, float Height);
	void DrawTerminalArchive(float X, float Y, float Width, float Height);
	void DrawReportResult(float X, float Y, float Width, float Height);
	void DrawMap(bool bFullScreen);
	void DrawShop();
	void DrawDocument();
	void DrawGallery();
	void DrawPhotoDetails(const FSUPhotoMeta& Meta, float X, float Y, float Width, float Height, bool bAllowDelete);
	void DrawHelp();
	void DrawTrunk();
	void DrawScreenTitle(const FString& Title, const FString& Hint);

	ASUPlayerController* GetPC() const;
	ASUCharacter* GetChar() const;
	void ResetScreenState();
};
