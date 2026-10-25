#include "Interaction/SUFurniture.h"
#include "Player/SUCharacter.h"
#include "Core/SUGameState.h"
#include "Core/SUGameMode.h"
#include "Core/SUEventSubsystem.h"
#include "Core/SUPlayerController.h"
#include "Mission/SUMissionManager.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// ====================================================================================== drawer

ASUDrawer::ASUDrawer()
{
	PrimaryActorTick.bCanEverTick = true;
	DrawerRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DrawerRoot"));
	DrawerRoot->SetMobility(EComponentMobility::Movable);
	DrawerRoot->SetupAttachment(SceneRoot);
}

void ASUDrawer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUDrawer, Width);
	DOREPLIFETIME(ASUDrawer, Depth);
	DOREPLIFETIME(ASUDrawer, CabinetHeight);
	DOREPLIFETIME(ASUDrawer, Material);
	DOREPLIFETIME(ASUDrawer, bOpen);
}

void ASUDrawer::BuildGeometry()
{
	const float T = 2.f;
	const float H = CabinetHeight;
	// carcass (open front at the drawer height)
	AddPart(SceneRoot, ESUShape::Cube, FVector(-Depth * 0.5f, 0.f, H - T * 0.5f), FVector(Depth, Width, T), Material);
	AddPart(SceneRoot, ESUShape::Cube, FVector(-Depth * 0.5f, 0.f, T * 0.5f + 8.f), FVector(Depth, Width, T), Material);
	AddPart(SceneRoot, ESUShape::Cube, FVector(-Depth * 0.5f, -Width * 0.5f + T * 0.5f, H * 0.5f), FVector(Depth, T, H), Material);
	AddPart(SceneRoot, ESUShape::Cube, FVector(-Depth * 0.5f, Width * 0.5f - T * 0.5f, H * 0.5f), FVector(Depth, T, H), Material);
	AddPart(SceneRoot, ESUShape::Cube, FVector(-Depth + T * 0.5f, 0.f, H * 0.5f), FVector(T, Width, H), Material);
	// lower fixed front panel
	AddPart(SceneRoot, ESUShape::Cube, FVector(-T * 0.5f, 0.f, H * 0.3f), FVector(T, Width - 2.f, H * 0.58f), Material);

	DrawerRoot->SetRelativeLocation(FVector(0.f, 0.f, H * 0.62f));

	const float DH = H * 0.3f;
	const float DW = Width - 6.f;
	const float DD = Depth - 6.f;
	AddPart(DrawerRoot, ESUShape::Cube, FVector(-T * 0.5f, 0.f, DH * 0.5f), FVector(T, Width - 2.f, DH), Material);            // front
	AddPart(DrawerRoot, ESUShape::Cube, FVector(-DD * 0.5f, 0.f, 0.5f), FVector(DD, DW, 1.f), ESUMat::WoodDark);               // bottom
	AddPart(DrawerRoot, ESUShape::Cube, FVector(-DD * 0.5f, -DW * 0.5f, DH * 0.4f), FVector(DD, 1.f, DH * 0.8f), ESUMat::WoodDark);
	AddPart(DrawerRoot, ESUShape::Cube, FVector(-DD * 0.5f, DW * 0.5f, DH * 0.4f), FVector(DD, 1.f, DH * 0.8f), ESUMat::WoodDark);
	AddPart(DrawerRoot, ESUShape::Cube, FVector(-DD, 0.f, DH * 0.4f), FVector(1.f, DW, DH * 0.8f), ESUMat::WoodDark);
	AddPart(DrawerRoot, ESUShape::Cube, FVector(2.f, 0.f, DH * 0.55f), FVector(2.f, 12.f, 2.f), ESUMat::Chrome, FRotator::ZeroRotator, false); // handle
	Slide = bOpen ? Depth * 0.65f : 0.f;
	DrawerRoot->SetRelativeLocation(FVector(Slide, 0.f, H * 0.62f));
}

FVector ASUDrawer::GetItemSlot(int32 Index) const
{
	const float DD = Depth - 6.f;
	return FVector(-DD * 0.35f - (Index / 2) * 12.f, (Index % 2 == 0 ? -1.f : 1.f) * (Width - 6.f) * 0.2f, 1.5f);
}

void ASUDrawer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!DrawerRoot)
	{
		return;
	}
	const float Target = bOpen ? Depth * 0.65f : 0.f;
	if (!FMath::IsNearlyEqual(Slide, Target, 0.1f))
	{
		Slide = FMath::FInterpConstantTo(Slide, Target, DeltaSeconds, 90.f);
		DrawerRoot->SetRelativeLocation(FVector(Slide, 0.f, CabinetHeight * 0.62f));
	}
}

bool ASUDrawer::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	OutPrompt.Primary = bOpen ? SUText(TEXT("Schublade schließen")) : SUText(TEXT("Schublade öffnen"));
	OutPrompt.bPrimary = true;
	return true;
}

void ASUDrawer::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	bOpen = !bOpen;
	OnRep_Open();
	Super::Interact(User, Component, Option);
}

void ASUDrawer::OnRep_Open()
{
	if (USUProceduralAudioComponent* Sound = GetOrCreateSound(ESUSoundKind::Impact, 0.25f, 1200.f))
	{
		Sound->TriggerSound(0, 0.4f);
	}
}

// ====================================================================================== bed

ASUBed::ASUBed()
{
}

void ASUBed::BuildGeometry()
{
	// frame, mattress, pillow, blanket (origin: floor centre, length along X)
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 20.f), FVector(200.f, 92.f, 20.f), ESUMat::WoodDark);
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 38.f), FVector(194.f, 86.f, 16.f), ESUMat::White);
	AddPart(SceneRoot, ESUShape::Cube, FVector(80.f, 0.f, 50.f), FVector(30.f, 60.f, 10.f), ESUMat::Paper, FRotator::ZeroRotator, false);
	AddPart(SceneRoot, ESUShape::Cube, FVector(-25.f, 0.f, 47.5f), FVector(140.f, 90.f, 4.f), ESUMat::FabricBlue, FRotator::ZeroRotator, false);
	AddPart(SceneRoot, ESUShape::Cube, FVector(102.f, 0.f, 45.f), FVector(6.f, 92.f, 70.f), ESUMat::WoodDark);
	for (int32 I = 0; I < 4; ++I)
	{
		const float X = (I < 2) ? -95.f : 95.f;
		const float Y = (I % 2 == 0) ? -42.f : 42.f;
		AddPart(SceneRoot, ESUShape::Cube, FVector(X, Y, 10.f), FVector(6.f, 6.f, 20.f), ESUMat::WoodDark, FRotator::ZeroRotator, false);
	}
}

FText ASUBed::GetSleepBlocker() const
{
	const ASUGameState* GS = ASUGameState::Get(this);
	if (!GS)
	{
		return SUText(TEXT("Nicht möglich."));
	}
	if (GS->MissionManager && GS->MissionManager->GetPhase() == ESUMissionPhase::Active && !GS->MissionManager->IsWaitingForReport())
	{
		return SUText(TEXT("Erst den laufenden Einsatz erledigen."));
	}
	if (GS->MissionManager && (GS->MissionManager->GetPhase() == ESUMissionPhase::Incoming || GS->MissionManager->GetPhase() == ESUMissionPhase::Briefing))
	{
		return SUText(TEXT("Das Funkgerät meldet sich gerade."));
	}
	const float Hours = GS->GetTimeOfDay();
	if (Hours > 7.f && Hours < 19.f)
	{
		return SUText(TEXT("Du bist nicht müde (Schlafen ab 19:00)."));
	}
	// all players must be at the base
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), GetActorLocation()) > 4000.f)
		{
			return SUText(TEXT("Alle Spieler müssen im Turm sein."));
		}
	}
	return FText::GetEmpty();
}

bool ASUBed::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	OutPrompt.Primary = SUText(TEXT("Schlafen (bis 7:00, speichert)"));
	OutPrompt.bPrimary = true;
	OutPrompt.Info = GetSleepBlocker();
	return true;
}

void ASUBed::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	const FText Blocker = GetSleepBlocker();
	if (!Blocker.IsEmpty())
	{
		if (User)
		{
			User->NotifyOwner(Blocker);
		}
		return;
	}
	ASUGameState* GS = ASUGameState::Get(this);
	if (!GS)
	{
		return;
	}
	const float Now = GS->GetTimeOfDay();
	const float Hours = Now < 7.f ? 7.f - Now : 24.f - Now + 7.f;
	GS->AdvanceTime(Hours);
	GS->SetWeather(0.f, 0.25f, 0.45f, 0.25f, 5.f); // fresh morning
	for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
	{
		It->HealFull();
		It->ClientFadeFromBlack(4.f);
	}
	USUEventSubsystem::Post(this, SUEvent::Slept, NAME_None, User);
	if (ASUGameMode* GM = GetWorld()->GetAuthGameMode<ASUGameMode>())
	{
		FString Message;
		GM->SaveGameNow(Message);
		if (User)
		{
			User->NotifyOwner(FText::FromString(FString::Printf(TEXT("Ausgeschlafen. %s"), *Message)));
		}
	}
	Super::Interact(User, Component, Option);
}
