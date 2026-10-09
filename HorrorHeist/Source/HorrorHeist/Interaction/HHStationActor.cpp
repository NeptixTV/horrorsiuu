#include "Interaction/HHStationActor.h"
#include "Player/HHPlayerController.h"
#include "Settings/HHGameUserSettings.h"
#include "UI/HHLobbyHUD.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

AHHStationActor::AHHStationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(Root);
	ViewCamera->SetRelativeLocation(FVector(-180.f, 0.f, 160.f));
	ViewCamera->SetFieldOfView(60.f);
	ViewCamera->bConstrainAspectRatio = false;
}

bool AHHStationActor::CanInteract(AActor* Interactor) const
{
	return bUsable && Super::CanInteract(Interactor);
}

void AHHStationActor::Interact(AActor* Interactor)
{
	Super::Interact(Interactor);

	const APawn* Pawn = Cast<APawn>(Interactor);
	AHHPlayerController* PC = Pawn ? Cast<AHHPlayerController>(Pawn->GetController()) : nullptr;
	if (PC && PC->IsLocalController())
	{
		if (AHHLobbyHUD* LobbyHUD = PC->GetLobbyHUD())
		{
			LobbyHUD->OpenScreen(Screen, this);
		}
	}
}

void AHHStationActor::OnViewActivated(APlayerController* Viewer)
{
	if (ActiveViewers++ == 0)
	{
		BaseCameraRotation = ViewCamera->GetRelativeRotation();
		DriftTime = FMath::FRandRange(0.f, 100.f);
		SetActorTickEnabled(CameraDrift > 0.f);
	}
}

void AHHStationActor::OnViewDeactivated(APlayerController* Viewer)
{
	ActiveViewers = FMath::Max(0, ActiveViewers - 1);
	if (ActiveViewers == 0)
	{
		SetActorTickEnabled(false);
		ViewCamera->SetRelativeRotation(BaseCameraRotation);
	}
}

void AHHStationActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (CameraDrift <= 0.f || (Settings && Settings->bReduceMotion))
	{
		ViewCamera->SetRelativeRotation(BaseCameraRotation);
		return;
	}

	// Two incommensurate noise curves read as a hand-held camera breathing.
	DriftTime += DeltaSeconds;
	const float Pitch = FMath::PerlinNoise1D(DriftTime * 0.21f) * CameraDrift;
	const float Yaw = FMath::PerlinNoise1D(DriftTime * 0.17f + 31.7f) * CameraDrift * 1.4f;
	const float Roll = FMath::PerlinNoise1D(DriftTime * 0.11f + 77.1f) * CameraDrift * 0.5f;
	ViewCamera->SetRelativeRotation(BaseCameraRotation + FRotator(Pitch, Yaw, Roll));
}

AHHStationActor* AHHStationActor::FindForScreen(const UWorld* World, EHHLobbyScreen InScreen)
{
	if (!World)
	{
		return nullptr;
	}
	AHHStationActor* Fallback = nullptr;
	for (TActorIterator<AHHStationActor> It(World); It; ++It)
	{
		if (It->Screen != InScreen)
		{
			continue;
		}
		if (It->bUsable)
		{
			return *It;
		}
		Fallback = Fallback ? Fallback : *It;
	}
	return Fallback;
}
