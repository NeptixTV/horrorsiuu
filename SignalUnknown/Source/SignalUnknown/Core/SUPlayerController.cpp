#include "Core/SUPlayerController.h"
#include "SignalUnknown.h"
#include "Core/SUInputConfig.h"
#include "Core/SUGameInstance.h"
#include "Core/SUGameMode.h"
#include "Core/SUGameState.h"
#include "Core/SUPlayerState.h"
#include "Core/SUGameUserSettings.h"
#include "Core/SUEventSubsystem.h"
#include "Player/SUCharacter.h"
#include "UI/SUHUD.h"
#include "Audio/SUAmbienceComponent.h"
#include "Mission/SUMissionManager.h"
#include "GasStation/SUShopCounter.h"
#include "Vehicle/SUVehicle.h"
#include "Evidence/SUPhotoStorage.h"
#include "World/SUWorldBuilder.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Camera/CameraActor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	enum ESUContextBits : int32
	{
		Ctx_Global = 1 << 0,
		Ctx_OnFoot = 1 << 1,
		Ctx_Driver = 1 << 2,
		Ctx_Passenger = 1 << 3,
		Ctx_UI = 1 << 4
	};
}

ASUPlayerController::ASUPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
}

USUInputConfig* ASUPlayerController::GetInputConfig()
{
	if (!InputConfig)
	{
		InputConfig = NewObject<USUInputConfig>(this, TEXT("SUInputConfig"));
		InputConfig->Build();
		// menus and the pause screen must react while the game is paused
		for (UInputAction* Action : {InputConfig->Pause.Get(), InputConfig->UINavigate.Get(), InputConfig->UIAccept.Get(),
			InputConfig->UIBack.Get(), InputConfig->UIClick.Get(), InputConfig->UIScroll.Get(), InputConfig->UITab.Get()})
		{
			if (Action)
			{
				Action->bTriggerWhenPaused = true;
			}
		}
	}
	return InputConfig;
}

ASUHUD* ASUPlayerController::GetSUHUD() const
{
	return Cast<ASUHUD>(GetHUD());
}

ASUCharacter* ASUPlayerController::GetSUCharacter() const
{
	return Cast<ASUCharacter>(GetPawn());
}

void ASUPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	Ambience = NewObject<USUAmbienceComponent>(this, TEXT("Ambience"));
	Ambience->RegisterComponent();

	if (USUGameUserSettings* Settings = USUGameUserSettings::Get())
	{
		Settings->ApplySettings(false);
	}

	const USUGameInstance* GI = GetGameInstance<USUGameInstance>();
	if (GI && GI->GetStartMode() == ESUStartMode::Menu && GetNetMode() != NM_Client)
	{
		bMenuMode = true;
		OpenScreen(ESUScreen::MainMenu);
	}
	RefreshInputContexts();
}

void ASUPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		UE_LOG(LogSignalUnknown, Error, TEXT("Enhanced Input is not configured (DefaultInput.ini)."));
		return;
	}
	USUInputConfig* C = GetInputConfig();
	EIC->BindAction(C->Pause, ETriggerEvent::Started, this, &ASUPlayerController::OnPause);
	EIC->BindAction(C->Map, ETriggerEvent::Started, this, &ASUPlayerController::OnMap);
	EIC->BindAction(C->Inventory, ETriggerEvent::Started, this, &ASUPlayerController::OnInventory);
	EIC->BindAction(C->Journal, ETriggerEvent::Started, this, &ASUPlayerController::OnJournal);
	EIC->BindAction(C->Gallery, ETriggerEvent::Started, this, &ASUPlayerController::OnGallery);
	EIC->BindAction(C->Help, ETriggerEvent::Started, this, &ASUPlayerController::OnHelp);
	EIC->BindAction(C->QuickSave, ETriggerEvent::Started, this, &ASUPlayerController::OnQuickSave);
	EIC->BindAction(C->QuickLoad, ETriggerEvent::Started, this, &ASUPlayerController::OnQuickLoad);
	EIC->BindAction(C->PushToTalk, ETriggerEvent::Started, this, &ASUPlayerController::OnPushToTalkStart);
	EIC->BindAction(C->PushToTalk, ETriggerEvent::Completed, this, &ASUPlayerController::OnPushToTalkStop);
	EIC->BindAction(C->UINavigate, ETriggerEvent::Started, this, &ASUPlayerController::OnUINavigate);
	EIC->BindAction(C->UIAccept, ETriggerEvent::Started, this, &ASUPlayerController::OnUIAccept);
	EIC->BindAction(C->UIBack, ETriggerEvent::Started, this, &ASUPlayerController::OnUIBack);
	EIC->BindAction(C->UIClick, ETriggerEvent::Started, this, &ASUPlayerController::OnUIClick);
	EIC->BindAction(C->UIScroll, ETriggerEvent::Triggered, this, &ASUPlayerController::OnUIScroll);
	EIC->BindAction(C->UITab, ETriggerEvent::Started, this, &ASUPlayerController::OnUITab);
}

void ASUPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController())
	{
		return;
	}
	RefreshInputContexts();
	UpdateMenuCamera();
	UpdateVoice();
}

// ------------------------------------------------------------------------------------ input contexts

void ASUPlayerController::RefreshInputContexts()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}
	int32 Wanted = 0;
	if (IsScreenOpen())
	{
		Wanted = Ctx_UI;
	}
	else
	{
		Wanted = Ctx_Global;
		const ASUCharacter* Char = GetSUCharacter();
		if (Char && !Char->IsDead())
		{
			if (Char->IsSeated())
			{
				Wanted |= Ctx_Passenger;
				if (Char->IsDriver())
				{
					Wanted |= Ctx_Driver;
				}
			}
			else
			{
				Wanted |= Ctx_OnFoot;
			}
		}
	}
	if (Wanted == AppliedContexts)
	{
		return;
	}
	AppliedContexts = Wanted;

	USUInputConfig* C = GetInputConfig();
	Subsystem->ClearAllMappings();
	if (Wanted & Ctx_Global) { Subsystem->AddMappingContext(C->GlobalContext, 0); }
	if (Wanted & Ctx_OnFoot) { Subsystem->AddMappingContext(C->OnFootContext, 1); }
	if (Wanted & Ctx_Passenger) { Subsystem->AddMappingContext(C->PassengerContext, 1); }
	if (Wanted & Ctx_Driver) { Subsystem->AddMappingContext(C->DriverContext, 2); }
	if (Wanted & Ctx_UI) { Subsystem->AddMappingContext(C->UIContext, 10); }
}

void ASUPlayerController::ApplyInputMode()
{
	if (IsScreenOpen())
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		bShowMouseCursor = true;
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
	// Single player: menus pause the world. Co-op sessions never pause.
	const bool bPauseScreen = Screen == ESUScreen::Pause || Screen == ESUScreen::Settings || Screen == ESUScreen::Help;
	if (GetNetMode() == NM_Standalone && !bMenuMode)
	{
		SetPause(bPauseScreen);
	}
}

// ------------------------------------------------------------------------------------ screens

void ASUPlayerController::OpenScreen(ESUScreen NewScreen, AActor* Context)
{
	if (!IsLocalController())
	{
		ClientOpenScreen(NewScreen, Context);
		return;
	}
	const ESUScreen Old = Screen;
	Screen = NewScreen;
	ScreenContext = Context;
	if (ASUHUD* HUD = GetSUHUD())
	{
		HUD->OnScreenChanged(Old, NewScreen);
	}
	if (NewScreen == ESUScreen::Map)
	{
		// counts for "look at the map" objectives
		if (ASUCharacter* Char = GetSUCharacter())
		{
			Char->ServerReportUIEvent(SUEvent::MapViewed, NAME_None);
		}
	}
	ApplyInputMode();
	RefreshInputContexts();
}

void ASUPlayerController::CloseScreen()
{
	if (Screen == ESUScreen::MainMenu)
	{
		return; // the main menu is left via its buttons only
	}
	if (Screen == ESUScreen::Settings && bMenuMode)
	{
		OpenScreen(ESUScreen::MainMenu);
		return;
	}
	if (Screen == ESUScreen::Join)
	{
		OpenScreen(ESUScreen::MainMenu);
		return;
	}
	OpenScreen(ESUScreen::None);
}

void ASUPlayerController::ClientOpenScreen_Implementation(ESUScreen NewScreen, AActor* Context)
{
	if (bMenuMode)
	{
		return;
	}
	OpenScreen(NewScreen, Context);
}

void ASUPlayerController::ToggleScreen(ESUScreen Target)
{
	if (bMenuMode)
	{
		return;
	}
	if (Screen == Target)
	{
		CloseScreen();
	}
	else if (!IsScreenOpen())
	{
		OpenScreen(Target);
	}
}

// ------------------------------------------------------------------------------------ feedback

void ASUPlayerController::Notify(const FText& Text, FLinearColor Color, float Duration)
{
	if (ASUHUD* HUD = GetSUHUD())
	{
		FSUNotification N;
		N.Text = Text;
		N.Color = Color;
		N.Duration = Duration;
		HUD->AddNotification(N);
	}
}

void ASUPlayerController::ClientNotify_Implementation(const FText& Text, FLinearColor Color)
{
	Notify(Text, Color, 4.5f);
}

void ASUPlayerController::ClientSpeech_Implementation(const FSUSpeechLine& Line, bool bRadio)
{
	if (ASUHUD* HUD = GetSUHUD())
	{
		HUD->AddSpeech(Line, bRadio);
	}
}

void ASUPlayerController::ClientReportResult_Implementation(const FSUReportResult& Result)
{
	LastReport = Result;
	bReportToShow = true;
	if (Result.bAccepted)
	{
		Notify(FText::FromString(FString::Printf(TEXT("Bericht angenommen: +%s"), *SUFormatMoney(Result.TotalCents))), FLinearColor(0.5f, 1.f, 0.6f), 6.f);
	}
	else
	{
		Notify(Result.Message, FLinearColor(1.f, 0.6f, 0.4f), 6.f);
	}
}

// ------------------------------------------------------------------------------------ server requests

void ASUPlayerController::ServerSubmitReport_Implementation(const TArray<FGuid>& PhotoIds)
{
	ASUGameState* GS = ASUGameState::Get(this);
	if (!GS || !GS->MissionManager)
	{
		return;
	}
	// must stand at the terminal: the mission manager validates the distance
	const FSUReportResult Result = GS->MissionManager->SubmitReport(this, PhotoIds);
	ClientReportResult(Result);
}

void ASUPlayerController::ServerAcceptMission_Implementation()
{
	ASUGameState* GS = ASUGameState::Get(this);
	if (GS && GS->MissionManager)
	{
		GS->MissionManager->AcceptOfferedMission(GetPawn());
	}
}

void ASUPlayerController::ServerShopBuy_Implementation(AActor* Shop, FName ItemId)
{
	if (ASUShopCounter* Counter = Cast<ASUShopCounter>(Shop))
	{
		Counter->Buy(this, ItemId);
	}
}

void ASUPlayerController::ServerShopPayFuel_Implementation(AActor* Shop)
{
	if (ASUShopCounter* Counter = Cast<ASUShopCounter>(Shop))
	{
		Counter->PayFuel(this);
	}
}

void ASUPlayerController::ServerShopRepair_Implementation(AActor* Shop)
{
	if (ASUShopCounter* Counter = Cast<ASUShopCounter>(Shop))
	{
		Counter->RepairVehicle(this);
	}
}

void ASUPlayerController::ServerDeletePhoto_Implementation(FGuid PhotoId)
{
	if (ASUPlayerState* PS = GetPlayerState<ASUPlayerState>())
	{
		const FSUPhotoMeta* Meta = PS->FindPhoto(PhotoId);
		if (Meta && !Meta->bSubmitted)
		{
			PS->RemovePhoto(PhotoId);
		}
	}
}

void ASUPlayerController::ServerTrunkTransfer_Implementation(AActor* VehicleActor, FName ItemId, bool bToTrunk)
{
	ASUVehicle* Vehicle = Cast<ASUVehicle>(VehicleActor);
	ASUCharacter* Char = GetSUCharacter();
	if (Vehicle && Char)
	{
		Vehicle->TransferTrunkItem(Char, ItemId, bToTrunk);
	}
}

void ASUPlayerController::ServerRequestSave_Implementation()
{
	ASUGameMode* GM = GetWorld()->GetAuthGameMode<ASUGameMode>();
	if (!GM)
	{
		return;
	}
	FString Message;
	const bool bOk = GM->SaveGameNow(Message);
	ClientNotify(FText::FromString(Message), bOk ? FLinearColor(0.6f, 1.f, 0.6f) : FLinearColor(1.f, 0.5f, 0.4f));
}

void ASUPlayerController::ServerRequestLoad_Implementation()
{
	ASUGameMode* GM = GetWorld()->GetAuthGameMode<ASUGameMode>();
	if (!GM)
	{
		return;
	}
	FString Message;
	const bool bOk = GM->LoadGameNow(Message);
	ClientNotify(FText::FromString(Message), bOk ? FLinearColor(0.6f, 1.f, 0.6f) : FLinearColor(1.f, 0.5f, 0.4f));
}

void ASUPlayerController::ServerBeginSession_Implementation(bool bContinue)
{
	if (ASUGameMode* GM = GetWorld()->GetAuthGameMode<ASUGameMode>())
	{
		GM->BeginSession(bContinue);
	}
}

// ------------------------------------------------------------------------------------ main menu

void ASUPlayerController::MenuStartSolo(bool bContinue)
{
	if (USUGameInstance* GI = GetGameInstance<USUGameInstance>())
	{
		GI->StartSolo(bContinue);
	}
	bMenuMode = false;
	OpenScreen(ESUScreen::None);
	ServerBeginSession(bContinue);
}

void ASUPlayerController::MenuHost(bool bContinue)
{
	if (USUGameInstance* GI = GetGameInstance<USUGameInstance>())
	{
		GI->HostCoop(bContinue);
	}
}

void ASUPlayerController::MenuJoin(const FString& Address)
{
	if (USUGameInstance* GI = GetGameInstance<USUGameInstance>())
	{
		GI->JoinCoop(Address);
	}
}

void ASUPlayerController::MenuQuit()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ASUPlayerController::LeaveToMainMenu()
{
	SetPause(false);
	if (USUGameInstance* GI = GetGameInstance<USUGameInstance>())
	{
		GI->ReturnToMenu();
	}
}

void ASUPlayerController::UpdateMenuCamera()
{
	if (bMenuMode)
	{
		if (!MenuCamera)
		{
			const ASUGameState* GS = ASUGameState::Get(this);
			const ASUWorldBuilder* Builder = GS ? GS->GetWorldBuilder() : nullptr;
			if (Builder && GS->IsWorldReady())
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				MenuCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Builder->GetMenuCameraTransform(), Params);
			}
		}
		if (MenuCamera && GetViewTarget() != MenuCamera)
		{
			bAutoManageActiveCameraTarget = false;
			SetViewTarget(MenuCamera);
		}
	}
	else if (MenuCamera)
	{
		bAutoManageActiveCameraTarget = true;
		if (APawn* P = GetPawn())
		{
			SetViewTargetWithBlend(P, 1.2f, VTBlend_EaseInOut, 2.f);
		}
		MenuCamera->Destroy();
		MenuCamera = nullptr;
	}
}

// ------------------------------------------------------------------------------------ voice

void ASUPlayerController::UpdateVoice()
{
	const USUGameUserSettings* Settings = USUGameUserSettings::Get();
	if (!Settings || GetNetMode() == NM_Standalone)
	{
		return;
	}
	// voice activation mode: transmit continuously, the engine's silence detection gates it
	const bool bWant = Settings->bVoiceChatEnabled && (!Settings->bPushToTalk || bTalking);
	if (bWant != bVoiceTransmitting)
	{
		bVoiceTransmitting = bWant;
		ToggleSpeaking(bWant);
	}
}

// ------------------------------------------------------------------------------------ input handlers

void ASUPlayerController::OnPause(const FInputActionValue& Value)
{
	if (bMenuMode)
	{
		return;
	}
	ToggleScreen(ESUScreen::Pause);
}

void ASUPlayerController::OnMap(const FInputActionValue& Value)
{
	ToggleScreen(ESUScreen::Map);
}

void ASUPlayerController::OnInventory(const FInputActionValue& Value)
{
	ToggleScreen(ESUScreen::Inventory);
}

void ASUPlayerController::OnJournal(const FInputActionValue& Value)
{
	bJournalPinned = !bJournalPinned;
}

void ASUPlayerController::OnGallery(const FInputActionValue& Value)
{
	ToggleScreen(ESUScreen::Gallery);
}

void ASUPlayerController::OnHelp(const FInputActionValue& Value)
{
	ToggleScreen(ESUScreen::Help);
}

void ASUPlayerController::OnQuickSave(const FInputActionValue& Value)
{
	if (GetNetMode() == NM_Client)
	{
		Notify(SUText(TEXT("Nur der Host kann speichern.")), FLinearColor(1.f, 0.7f, 0.4f));
		return;
	}
	ServerRequestSave();
}

void ASUPlayerController::OnQuickLoad(const FInputActionValue& Value)
{
	if (GetNetMode() == NM_Client)
	{
		Notify(SUText(TEXT("Nur der Host kann laden.")), FLinearColor(1.f, 0.7f, 0.4f));
		return;
	}
	ServerRequestLoad();
}

void ASUPlayerController::OnPushToTalkStart(const FInputActionValue& Value)
{
	bTalking = true;
}

void ASUPlayerController::OnPushToTalkStop(const FInputActionValue& Value)
{
	bTalking = false;
}

void ASUPlayerController::OnUINavigate(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	if (ASUHUD* HUD = GetSUHUD())
	{
		const int32 X = V.X > 0.5 ? 1 : (V.X < -0.5 ? -1 : 0);
		const int32 Y = V.Y > 0.5 ? -1 : (V.Y < -0.5 ? 1 : 0); // screen space: down = +1
		HUD->UINavigate(FIntPoint(X, Y));
	}
}

void ASUPlayerController::OnUIAccept(const FInputActionValue& Value)
{
	if (ASUHUD* HUD = GetSUHUD())
	{
		HUD->UIAccept();
	}
}

void ASUPlayerController::OnUIBack(const FInputActionValue& Value)
{
	ASUHUD* HUD = GetSUHUD();
	if (HUD && HUD->UIBack())
	{
		return; // the screen handled it (e.g. closed a sub page)
	}
	CloseScreen();
}

void ASUPlayerController::OnUIClick(const FInputActionValue& Value)
{
	if (ASUHUD* HUD = GetSUHUD())
	{
		HUD->UIClick();
	}
}

void ASUPlayerController::OnUIScroll(const FInputActionValue& Value)
{
	if (ASUHUD* HUD = GetSUHUD())
	{
		HUD->UIScroll(Value.Get<float>());
	}
}

void ASUPlayerController::OnUITab(const FInputActionValue& Value)
{
	if (ASUHUD* HUD = GetSUHUD())
	{
		HUD->UITab(Value.Get<float>() > 0.f ? 1 : -1);
	}
}
