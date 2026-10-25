// SIGNAL UNKNOWN – player controller: input contexts, screens/menus, client notifications,
// server requests coming from UI screens (report, shop, trunk, save).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/SUTypes.h"
#include "Mission/SUMissionTypes.h"
#include "SUPlayerController.generated.h"

class USUInputConfig;
class ASUHUD;
class ASUCharacter;
class ACameraActor;
class USUAmbienceComponent;
struct FInputActionValue;

UCLASS()
class SIGNALUNKNOWN_API ASUPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASUPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	USUInputConfig* GetInputConfig();
	ASUHUD* GetSUHUD() const;
	ASUCharacter* GetSUCharacter() const;

	// ---------------------------------------------------------------- screens
	ESUScreen GetScreen() const { return Screen; }
	AActor* GetScreenContext() const { return ScreenContext.Get(); }
	bool IsScreenOpen() const { return Screen != ESUScreen::None; }
	void OpenScreen(ESUScreen NewScreen, AActor* Context = nullptr);
	void CloseScreen();
	bool IsInMainMenu() const { return Screen == ESUScreen::MainMenu || bMenuMode; }

	UFUNCTION(Client, Reliable)
	void ClientOpenScreen(ESUScreen NewScreen, AActor* Context);

	// ---------------------------------------------------------------- feedback
	/** Local HUD notification. */
	void Notify(const FText& Text, FLinearColor Color = FLinearColor::White, float Duration = 4.f);

	UFUNCTION(Client, Reliable)
	void ClientNotify(const FText& Text, FLinearColor Color);

	/** Subtitle (radio, NPC). */
	UFUNCTION(Client, Reliable)
	void ClientSpeech(const FSUSpeechLine& Line, bool bRadio);

	UFUNCTION(Client, Reliable)
	void ClientReportResult(const FSUReportResult& Result);

	const FSUReportResult& GetLastReport() const { return LastReport; }
	bool HasReportToShow() const { return bReportToShow; }
	void ClearReportToShow() { bReportToShow = false; }

	// ---------------------------------------------------------------- requests from UI screens
	UFUNCTION(Server, Reliable)
	void ServerSubmitReport(const TArray<FGuid>& PhotoIds);

	UFUNCTION(Server, Reliable)
	void ServerAcceptMission();

	UFUNCTION(Server, Reliable)
	void ServerShopBuy(AActor* Shop, FName ItemId);

	UFUNCTION(Server, Reliable)
	void ServerShopPayFuel(AActor* Shop);

	UFUNCTION(Server, Reliable)
	void ServerShopRepair(AActor* Shop);

	UFUNCTION(Server, Reliable)
	void ServerDeletePhoto(FGuid PhotoId);

	UFUNCTION(Server, Reliable)
	void ServerTrunkTransfer(AActor* VehicleActor, FName ItemId, bool bToTrunk);

	UFUNCTION(Server, Reliable)
	void ServerRequestSave();

	UFUNCTION(Server, Reliable)
	void ServerRequestLoad();

	UFUNCTION(Server, Reliable)
	void ServerBeginSession(bool bContinue);

	// ---------------------------------------------------------------- main menu
	void MenuStartSolo(bool bContinue);
	void MenuHost(bool bContinue);
	void MenuJoin(const FString& Address);
	void MenuQuit();
	void LeaveToMainMenu();

	/** Shows / hides the persistent objective list (J). */
	bool IsJournalPinned() const { return bJournalPinned; }

protected:
	void OnPause(const FInputActionValue& Value);
	void OnMap(const FInputActionValue& Value);
	void OnInventory(const FInputActionValue& Value);
	void OnJournal(const FInputActionValue& Value);
	void OnGallery(const FInputActionValue& Value);
	void OnHelp(const FInputActionValue& Value);
	void OnQuickSave(const FInputActionValue& Value);
	void OnQuickLoad(const FInputActionValue& Value);
	void OnPushToTalkStart(const FInputActionValue& Value);
	void OnPushToTalkStop(const FInputActionValue& Value);
	void OnUINavigate(const FInputActionValue& Value);
	void OnUIAccept(const FInputActionValue& Value);
	void OnUIBack(const FInputActionValue& Value);
	void OnUIClick(const FInputActionValue& Value);
	void OnUIScroll(const FInputActionValue& Value);
	void OnUITab(const FInputActionValue& Value);

private:
	UPROPERTY(Transient)
	TObjectPtr<USUInputConfig> InputConfig;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> MenuCamera;

	UPROPERTY(Transient)
	TObjectPtr<USUAmbienceComponent> Ambience;

	ESUScreen Screen = ESUScreen::None;
	TWeakObjectPtr<AActor> ScreenContext;
	FSUReportResult LastReport;
	bool bReportToShow = false;
	bool bJournalPinned = false;
	bool bMenuMode = false;
	bool bTalking = false;
	bool bVoiceTransmitting = false;

	/** Bit mask of the currently applied input contexts. */
	int32 AppliedContexts = -1;

	void RefreshInputContexts();
	void ApplyInputMode();
	void UpdateMenuCamera();
	void UpdateVoice();
	void ToggleScreen(ESUScreen Target);
};
