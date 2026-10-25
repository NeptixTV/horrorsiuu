#include "Base/SUBaseDevices.h"
#include "Core/SUGameState.h"
#include "Core/SUPlayerController.h"
#include "Core/SUEventSubsystem.h"
#include "Mission/SUMissionManager.h"
#include "Mission/SUMissionDatabase.h"
#include "Player/SUCharacter.h"
#include "World/SUWorldBuilder.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// ====================================================================================== radio station

ASURadioStation::ASURadioStation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
}

float ASURadioStation::SpeakerPitch(const FText& Speaker)
{
	const FString S = Speaker.ToString();
	if (S == TEXT("Zentrale")) { return 1.f; }
	if (S == TEXT("???")) { return 0.62f; }
	if (S.Contains(TEXT("12"))) { return 0.86f; }
	if (S.Contains(TEXT("Streife"))) { return 0.8f; }
	return 1.15f;
}

void ASURadioStation::BuildGeometry()
{
	// origin: desk surface, +X towards the user
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 13.f), FVector(32.f, 52.f, 26.f), ESUMat::MetalPainted);
	AddPart(SceneRoot, ESUShape::Cube, FVector(16.2f, -12.f, 15.f), FVector(1.f, 22.f, 14.f), ESUMat::Black, FRotator::ZeroRotator, false); // speaker grille
	for (int32 K = 0; K < 3; ++K)
	{
		AddPart(SceneRoot, ESUShape::Cylinder, FVector(17.f, 6.f + K * 7.f, 8.f), FVector(4.f, 4.f, 2.f), ESUMat::Chrome, FRotator(90.f, 0.f, 0.f), false);
	}
	AddPart(SceneRoot, ESUShape::Cube, FVector(16.5f, 14.f, 20.f), FVector(0.6f, 18.f, 6.f), ESUMat::Black, FRotator::ZeroRotator, false); // display
	AddPart(SceneRoot, ESUShape::Cylinder, FVector(-10.f, 22.f, 50.f), FVector(1.f, 1.f, 50.f), ESUMat::MetalDark, FRotator::ZeroRotator, false); // antenna
	AddPart(SceneRoot, ESUShape::Cube, FVector(22.f, 30.f, 3.f), FVector(6.f, 4.f, 6.f), ESUMat::Black); // microphone

	ClockText = AddText(SceneRoot, FVector(17.2f, 14.f, 20.f), FRotator::ZeroRotator, TEXT("--:--"), 4.f, FColor(255, 120, 40));
	Led = AddPointLight(SceneRoot, FVector(18.f, 24.f, 24.f), 0.6f, 60.f, FLinearColor(0.2f, 1.f, 0.2f), false);

	StaticSound = GetOrCreateSound(ESUSoundKind::RadioStatic, 0.55f, 1800.f);
	VoiceSound = GetOrCreateSound(ESUSoundKind::RadioVoice, 1.1f, 2200.f);
	ClickSound = GetOrCreateSound(ESUSoundKind::Click, 0.6f, 1200.f);
}

void ASURadioStation::MulticastInterference_Implementation(float Duration)
{
	InterferenceTime = FMath::Max(InterferenceTime, Duration);
}

void ASURadioStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ASUMissionManager* MM = ASUMissionManager::Get(this);
	const ASUGameState* GS = ASUGameState::Get(this);

	ClockTimer -= DeltaSeconds;
	if (ClockText && GS && ClockTimer <= 0.f)
	{
		ClockTimer = 1.f;
		ClockText->SetText(FText::FromString(SUFormatTime(GS->GetTimeOfDay())));
	}

	float StaticLevel = 0.04f;
	float Crackle = 0.1f;
	bool bSpeaking = false;
	float Pitch = 1.f;
	float Signal = 1.f;

	if (MM)
	{
		if (MM->GetPhase() == ESUMissionPhase::Incoming)
		{
			// a call is waiting: pulsing static and a beep every few seconds
			const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.2f);
			StaticLevel = 0.25f + 0.35f * Pulse;
			Crackle = 0.6f;
			BeepTimer -= DeltaSeconds;
			if (BeepTimer <= 0.f && ClickSound)
			{
				BeepTimer = 2.5f;
				ClickSound->TriggerSound(4, 1.f);
			}
			if (Led)
			{
				Led->SetLightColor(FLinearColor(1.f, 0.2f, 0.1f));
				Led->SetIntensity(Pulse > 0.5f ? 1.2f : 0.1f);
			}
		}
		else if (Led)
		{
			Led->SetLightColor(FLinearColor(0.2f, 1.f, 0.2f));
			Led->SetIntensity(0.6f);
		}

		FSUSpeechLine Line;
		float LineTime = 0.f;
		if (MM->GetTransmissionLine(Line, LineTime))
		{
			bSpeaking = LineTime > 0.25f && LineTime < Line.GetDuration() - 0.15f;
			Pitch = SpeakerPitch(Line.Speaker);
			const bool bStrange = MM->GetTransmission().Kind == ESUTransmissionKind::Strange;
			Signal = bStrange ? 0.45f : 0.95f;
			StaticLevel = bStrange ? 0.45f : 0.12f;
			if (MM->GetTransmission().Serial != LastSerial && ClickSound)
			{
				LastSerial = MM->GetTransmission().Serial;
				ClickSound->TriggerSound(4, 1.f); // squelch
			}
		}
	}

	if (InterferenceTime > 0.f)
	{
		InterferenceTime -= DeltaSeconds;
		StaticLevel = FMath::Max(StaticLevel, 0.55f + 0.4f * FMath::FRand());
		Crackle = 1.f;
		Signal = FMath::Min(Signal, 0.2f);
	}

	if (StaticSound)
	{
		StaticSound->SetSoundParam(SULogic::Dsp::Param::A, StaticLevel);
		StaticSound->SetSoundParam(SULogic::Dsp::Param::B, Crackle);
	}
	if (VoiceSound)
	{
		VoiceSound->SetSoundParam(SULogic::Dsp::Param::A, bSpeaking ? 1.f : 0.f);
		VoiceSound->SetSoundParam(SULogic::Dsp::Param::B, Pitch);
		VoiceSound->SetSoundParam(SULogic::Dsp::Param::C, Signal);
		VoiceSound->SetSoundParam(SULogic::Dsp::Param::D, StaticLevel);
	}
}

bool ASURadioStation::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	const ASUMissionManager* MM = ASUMissionManager::Get(this);
	if (!MM)
	{
		return false;
	}
	switch (MM->GetPhase())
	{
	case ESUMissionPhase::Incoming:
		OutPrompt.Primary = SUText(TEXT("Funkspruch entgegennehmen"));
		OutPrompt.bPrimary = true;
		break;
	case ESUMissionPhase::Briefing:
		OutPrompt.Info = SUText(TEXT("Die Zentrale spricht …"));
		break;
	case ESUMissionPhase::Offered:
		OutPrompt.Primary = SUText(TEXT("Auftrag bestätigen"));
		OutPrompt.bPrimary = !MM->IsTransmitting();
		OutPrompt.Secondary = SUText(TEXT("Funkspruch wiederholen"));
		OutPrompt.bSecondary = !MM->IsTransmitting();
		break;
	case ESUMissionPhase::Active:
		OutPrompt.Secondary = SUText(TEXT("Auftrag erneut anhören"));
		OutPrompt.bSecondary = !MM->IsTransmitting();
		OutPrompt.Info = SUText(TEXT("Funkgerät – Kanal Zentrale"));
		break;
	default:
		OutPrompt.Info = SUText(TEXT("Funkgerät – kein Funkverkehr"));
		break;
	}
	return true;
}

void ASURadioStation::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	ASUMissionManager* MM = ASUMissionManager::Get(this);
	if (!MM)
	{
		return;
	}
	if (Option == ESUInteractOption::Primary)
	{
		if (MM->CanAnswer())
		{
			MM->AnswerRadio(User);
		}
		else if (MM->CanAccept() && !MM->IsTransmitting())
		{
			MM->AcceptOfferedMission(User);
		}
	}
	else
	{
		MM->RepeatLastTransmission();
	}
	Super::Interact(User, Component, Option);
}

// ====================================================================================== computer

ASUComputerTerminal::ASUComputerTerminal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
}

void ASUComputerTerminal::BuildGeometry()
{
	// origin: desk surface, +X towards the user
	AddPart(SceneRoot, ESUShape::Cube, FVector(-5.f, 0.f, 26.f), FVector(8.f, 52.f, 36.f), ESUMat::Black);                   // monitor
	AddPart(SceneRoot, ESUShape::Cube, FVector(-1.f, 0.f, 26.f), FVector(0.5f, 46.f, 30.f), ESUMat::Screen, FRotator::ZeroRotator, false);
	AddPart(SceneRoot, ESUShape::Cube, FVector(-6.f, 0.f, 4.f), FVector(16.f, 14.f, 8.f), ESUMat::Black);                    // stand
	AddPart(SceneRoot, ESUShape::Cube, FVector(18.f, 0.f, 1.2f), FVector(16.f, 44.f, 2.4f), ESUMat::CarDark);                 // keyboard
	AddPart(SceneRoot, ESUShape::Cube, FVector(18.f, 32.f, 1.5f), FVector(9.f, 6.f, 3.f), ESUMat::CarDark, FRotator::ZeroRotator, false); // mouse
	AddPart(SceneRoot, ESUShape::Cube, FVector(-8.f, -44.f, 21.f), FVector(40.f, 18.f, 42.f), ESUMat::MetalWhite);           // PC tower

	ScreenTitle = AddText(SceneRoot, FVector(-0.5f, 0.f, 36.f), FRotator::ZeroRotator, TEXT("POSTEN 07 // EINSATZSYSTEM"), 3.f, FColor(120, 255, 180));
	ScreenStatus = AddText(SceneRoot, FVector(-0.5f, 0.f, 26.f), FRotator::ZeroRotator, TEXT(""), 2.6f, FColor(200, 255, 220));
	AddPointLight(SceneRoot, FVector(25.f, 0.f, 26.f), 3.f, 180.f, FLinearColor(0.5f, 1.f, 0.8f), false);
}

void ASUComputerTerminal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!ScreenStatus)
	{
		return;
	}
	const ASUMissionManager* MM = ASUMissionManager::Get(this);
	FString Status = TEXT("Kein aktiver Einsatz");
	if (MM)
	{
		switch (MM->GetPhase())
		{
		case ESUMissionPhase::Incoming: Status = TEXT("! Eingehender Funkspruch"); break;
		case ESUMissionPhase::Briefing: Status = TEXT("Funkverbindung aktiv …"); break;
		case ESUMissionPhase::Offered: Status = TEXT("Neuer Auftrag – Bestätigung ausstehend"); break;
		case ESUMissionPhase::Active: Status = MM->IsWaitingForReport() ? TEXT("Bericht ausstehend – Fotos auswerten") : TEXT("Einsatz läuft"); break;
		case ESUMissionPhase::Completed: Status = TEXT("Bericht übermittelt"); break;
		default: break;
		}
	}
	ScreenStatus->SetText(FText::FromString(Status));
}

bool ASUComputerTerminal::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	OutPrompt.Primary = SUText(TEXT("Einsatzcomputer benutzen"));
	OutPrompt.bPrimary = true;
	return true;
}

void ASUComputerTerminal::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	if (!User)
	{
		return;
	}
	USUEventSubsystem::Post(this, SUEvent::TerminalOpened, NAME_None, User);
	if (ASUPlayerController* PC = Cast<ASUPlayerController>(User->GetController()))
	{
		PC->ClientOpenScreen(ESUScreen::Terminal, this);
	}
	Super::Interact(User, Component, Option);
}

// ====================================================================================== wall map

ASUWallMap::ASUWallMap()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
}

FVector ASUWallMap::BoardPoint(const FVector2D& UV, float Out) const
{
	return FVector(Out, (UV.X - 0.5f) * BoardWidth, (0.5f - UV.Y) * BoardHeight);
}

void ASUWallMap::BuildGeometry()
{
	// origin: board centre, +X = facing the room
	AddPart(SceneRoot, ESUShape::Cube, FVector(-2.f, 0.f, 0.f), FVector(3.f, BoardWidth + 8.f, BoardHeight + 8.f), ESUMat::WoodDark);
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 0.f), FVector(0.6f, BoardWidth, BoardHeight), ESUMat::Paper, FRotator::ZeroRotator, false);
	AddText(SceneRoot, FVector(0.6f, 0.f, BoardHeight * 0.5f + 7.f), FRotator::ZeroRotator, TEXT("NÖRDLICHES WALDGEBIET  –  SEKTOR 07"), 4.f, FColor(40, 30, 20));
	TryBuildLayout();
}

void ASUWallMap::TryBuildLayout()
{
	// On clients the replicated board can begin play before the local world is generated.
	const ASUWorldBuilder* Builder = ASUWorldBuilder::Get(this);
	if (bLayoutBuilt || !Builder || !Builder->IsBuilt() || !Builder->GetLayout())
	{
		return;
	}
	bLayoutBuilt = true;
	auto InBoard = [](const FVector2D& UV) { return UV.X > 0.02f && UV.X < 0.98f && UV.Y > 0.02f && UV.Y < 0.98f; };

	// forest tint patches (coarse) to make clearings readable
	const SULogic::WorldLayout& L = *Builder->GetLayout();
	const int32 Grid = 18;
	for (int32 J = 0; J < Grid; ++J)
	{
		for (int32 I = 0; I < Grid; ++I)
		{
			const FVector2D UV((I + 0.5f) / Grid, (J + 0.5f) / Grid);
			const FVector W = Builder->MapUVToWorld(UV);
			if (L.ForestFactor(W.X / 100.0, W.Y / 100.0) > 0.5)
			{
				AddBox(BoardPoint(UV, 0.35f), FVector(0.1f, BoardWidth / Grid - 0.6f, BoardHeight / Grid - 0.6f), ESUMat::FoliageLight, FRotator::ZeroRotator, false);
			}
		}
	}
	// lake
	const FVector2D LakeUV = Builder->WorldToMapUV(FVector(L.TheLake.Center.X * 100.0, L.TheLake.Center.Y * 100.0, 0.0));
	const float LakeR = static_cast<float>(L.TheLake.Radius * 100.0 / (Builder->GetMapHalfSize() * 2.f)) * BoardWidth;
	AddPart(SceneRoot, ESUShape::Cylinder, BoardPoint(LakeUV, 0.5f), FVector(LakeR * 2.f, LakeR * 2.f * BoardHeight / BoardWidth, 0.2f), ESUMat::FabricBlue, FRotator(90.f, 0.f, 0.f), false);
	// roads
	for (const TArray<FVector2D>& Road : Builder->GetMapRoads())
	{
		for (int32 I = 0; I + 1 < Road.Num(); ++I)
		{
			if (InBoard(Road[I]) && InBoard(Road[I + 1]))
			{
				AddBeam(BoardPoint(Road[I], 0.6f), BoardPoint(Road[I + 1], 0.6f), 0.7f, ESUMat::GasRed, false);
			}
		}
	}
	// sites
	struct FSiteMark { const TCHAR* Point; const TCHAR* Label; ESUMat Mat; };
	const FSiteMark Marks[] = {
		{TEXT("Tower"), TEXT("Turm 07"), ESUMat::White},
		{TEXT("GasStation"), TEXT("Tankstelle"), ESUMat::Yellow},
		{TEXT("Forsthaus"), TEXT("Forsthaus"), ESUMat::WoodPainted},
		{TEXT("House3"), TEXT("Pine Ridge"), ESUMat::Concrete},
		{TEXT("M02Car"), TEXT("Nordstraße"), ESUMat::Rust},
	};
	for (const FSiteMark& M : Marks)
	{
		FTransform T;
		if (Builder->GetPoint(FName(M.Point), T))
		{
			const FVector2D UV = Builder->WorldToMapUV(T.GetLocation());
			if (InBoard(UV))
			{
				AddBox(BoardPoint(UV, 0.9f), FVector(0.8f, 2.4f, 2.4f), M.Mat, FRotator::ZeroRotator, false);
				AddText(SceneRoot, BoardPoint(UV, 1.f) + FVector(0.f, 0.f, 3.f), FRotator::ZeroRotator, M.Label, 2.2f, FColor(30, 25, 20));
			}
		}
	}
	TargetPin = AddPart(SceneRoot, ESUShape::Sphere, BoardPoint(FVector2D(0.5f, 0.5f), 2.f), FVector(3.f, 3.f, 3.f), ESUMat::Red, FRotator::ZeroRotator, false);
	if (TargetPin)
	{
		TargetPin->SetVisibility(false);
	}
}

void ASUWallMap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLayoutBuilt)
	{
		TryBuildLayout();
	}
	if (!TargetPin)
	{
		return;
	}
	const ASUMissionManager* MM = ASUMissionManager::Get(this);
	const ASUWorldBuilder* Builder = ASUWorldBuilder::Get(this);
	FVector Marker;
	FText Label;
	if (MM && Builder && MM->GetPhase() == ESUMissionPhase::Active && MM->GetMarker(Marker, Label))
	{
		TargetPin->SetRelativeLocation(BoardPoint(Builder->WorldToMapUV(Marker), 2.f));
		TargetPin->SetVisibility(true);
	}
	else
	{
		TargetPin->SetVisibility(false);
	}
}

bool ASUWallMap::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	OutPrompt.Primary = SUText(TEXT("Karte ansehen"));
	OutPrompt.bPrimary = true;
	return true;
}

void ASUWallMap::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	USUEventSubsystem::Post(this, SUEvent::MapViewed, NAME_None, User);
	if (User)
	{
		if (ASUPlayerController* PC = Cast<ASUPlayerController>(User->GetController()))
		{
			PC->ClientOpenScreen(ESUScreen::Map, this);
		}
	}
	Super::Interact(User, Component, Option);
}

// ====================================================================================== weather radio

ASUWeatherRadio::ASUWeatherRadio()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
}

void ASUWeatherRadio::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUWeatherRadio, ReportStartTime);
}

void ASUWeatherRadio::BuildGeometry()
{
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 9.f), FVector(12.f, 26.f, 18.f), ESUMat::WoodPainted);
	AddPart(SceneRoot, ESUShape::Cube, FVector(6.2f, -5.f, 9.f), FVector(0.5f, 12.f, 12.f), ESUMat::Black, FRotator::ZeroRotator, false);
	AddPart(SceneRoot, ESUShape::Cylinder, FVector(6.5f, 8.f, 9.f), FVector(4.f, 4.f, 2.f), ESUMat::Chrome, FRotator(90.f, 0.f, 0.f), false);
	Voice = GetOrCreateSound(ESUSoundKind::RadioVoice, 0.8f, 1200.f);
}

void ASUWeatherRadio::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AGameStateBase* GS = GetWorld()->GetGameState();
	if (!Voice || !GS)
	{
		return;
	}
	const double T = GS->GetServerWorldTimeSeconds() - ReportStartTime;
	Voice->SetSoundParam(SULogic::Dsp::Param::A, (T > 0.3 && T < 11.0) ? 1.f : 0.f);
	Voice->SetSoundParam(SULogic::Dsp::Param::B, 1.2f);
	Voice->SetSoundParam(SULogic::Dsp::Param::C, 0.9f);
	Voice->SetSoundParam(SULogic::Dsp::Param::D, 0.1f);
}

bool ASUWeatherRadio::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	OutPrompt.Primary = SUText(TEXT("Wetterbericht hören"));
	OutPrompt.bPrimary = true;
	return true;
}

void ASUWeatherRadio::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	const ASUGameState* GS = ASUGameState::Get(this);
	if (!GS)
	{
		return;
	}
	ReportStartTime = GS->GetServerWorldTimeSeconds();
	FSUSpeechLine Now;
	Now.Speaker = SUText(TEXT("Wetterdienst"));
	Now.Text = FText::FromString(FString::Printf(TEXT("Nordwald, %s Uhr: %s"), *SUFormatTime(GS->GetTimeOfDay()), *GS->DescribeWeather(false).ToString()));
	Now.Duration = 5.5f;
	FSUSpeechLine Forecast;
	Forecast.Speaker = SUText(TEXT("Wetterdienst"));
	Forecast.Text = FText::FromString(FString::Printf(TEXT("Aussichten: %s"), *GS->DescribeWeather(true).ToString()));
	Forecast.Duration = 5.5f;
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 1200.f)
		{
			if (ASUPlayerController* PC = Cast<ASUPlayerController>(It->GetController()))
			{
				PC->ClientSpeech(Now, true);
				PC->ClientSpeech(Forecast, true);
			}
		}
	}
	Super::Interact(User, Component, Option);
}
