#include "Interaction/HHPreviewStation.h"
#include "World/HHPreviewMannequin.h"
#include "Player/HHCharacter.h"
#include "Progression/HHProfileSubsystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"

AHHPreviewStation::AHHPreviewStation()
{
	Screen = EHHLobbyScreen::Customization;
	CameraDrift = 0.25f;

	PreviewSpot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewSpot"));
	PreviewSpot->SetupAttachment(Root);
	PreviewSpot->SetRelativeLocation(FVector(120.f, 0.f, 0.f));

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(PreviewSpot);
	Boom->SetRelativeLocation(FVector(0.f, 0.f, TargetHeight));
	// Look back at the mannequin from in front of it (station forward = +X).
	Boom->SetRelativeRotation(FRotator(-6.f, 180.f, 0.f));
	Boom->TargetArmLength = TargetArm;
	Boom->bDoCollisionTest = false;
	Boom->bUsePawnControlRotation = false;
	// Shift the camera right so the body sits left of the UI panel.
	Boom->SocketOffset = FVector(0.f, 45.f, 0.f);

	ViewCamera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	ViewCamera->SetRelativeLocation(FVector::ZeroVector);
	ViewCamera->SetRelativeRotation(FRotator::ZeroRotator);
	ViewCamera->SetFieldOfView(45.f);
}

void AHHPreviewStation::OnViewActivated(APlayerController* Viewer)
{
	Super::OnViewActivated(Viewer);
	SetActorTickEnabled(true);
	CurrentViewer = Viewer;

	if (!Mannequin)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Mannequin = GetWorld()->SpawnActor<AHHPreviewMannequin>(AHHPreviewMannequin::StaticClass(), PreviewSpot->GetComponentTransform(), Params);
	}

	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		ProfileHandle = Profile->OnProfileChanged.AddUObject(this, &AHHPreviewStation::HandleProfileChanged);
	}
	ShowEquipped();

	TargetYaw = 0.f;
	CurrentYaw = 0.f;
	FocusSlot(EHHCosmeticSlot::MAX);

	// The real pawn would otherwise stand in the shot next to its own mannequin.
	if (AHHCharacter* Character = Viewer ? Cast<AHHCharacter>(Viewer->GetPawn()) : nullptr)
	{
		Character->SetLocallyHidden(true);
	}
}

void AHHPreviewStation::OnViewDeactivated(APlayerController* Viewer)
{
	Super::OnViewDeactivated(Viewer);

	if (UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
	{
		Profile->OnProfileChanged.Remove(ProfileHandle);
	}
	if (Mannequin)
	{
		Mannequin->Destroy();
		Mannequin = nullptr;
	}
	if (AHHCharacter* Character = Viewer ? Cast<AHHCharacter>(Viewer->GetPawn()) : nullptr)
	{
		Character->SetLocallyHidden(false);
	}
	CurrentViewer.Reset();
	SetActorTickEnabled(false);
}

void AHHPreviewStation::HandleProfileChanged()
{
	if (!bShowingPreview)
	{
		ShowEquipped();
	}
}

void AHHPreviewStation::ShowEquipped()
{
	bShowingPreview = false;
	if (Mannequin)
	{
		if (const UHHProfileSubsystem* Profile = UHHProfileSubsystem::Get(this))
		{
			Mannequin->ShowLoadout(Profile->GetCosmetics());
		}
	}
}

void AHHPreviewStation::PreviewLoadout(const FHHCosmeticLoadout& Loadout)
{
	bShowingPreview = true;
	if (Mannequin)
	{
		Mannequin->ShowLoadout(Loadout);
	}
}

void AHHPreviewStation::AddYaw(float Degrees)
{
	TargetYaw += Degrees;
}

void AHHPreviewStation::AddZoom(float Steps)
{
	TargetArm = FMath::Clamp(TargetArm + Steps * 35.f, MinArmLength, MaxArmLength);
	// Closer shots favour the upper body.
	const float Alpha = (TargetArm - MinArmLength) / FMath::Max(MaxArmLength - MinArmLength, 1.f);
	TargetHeight = FMath::Lerp(150.f, 100.f, Alpha);
}

void AHHPreviewStation::FocusSlot(EHHCosmeticSlot Slot)
{
	switch (Slot)
	{
	case EHHCosmeticSlot::Hair:
	case EHHCosmeticSlot::Hat:
	case EHHCosmeticSlot::Mask:
		TargetArm = 110.f;
		TargetHeight = 160.f;
		break;
	case EHHCosmeticSlot::Top:
	case EHHCosmeticSlot::Jacket:
	case EHHCosmeticSlot::Accessory:
	case EHHCosmeticSlot::Gloves:
		TargetArm = 190.f;
		TargetHeight = 128.f;
		break;
	case EHHCosmeticSlot::Pants:
		TargetArm = 200.f;
		TargetHeight = 70.f;
		break;
	case EHHCosmeticSlot::Shoes:
		TargetArm = 150.f;
		TargetHeight = 30.f;
		break;
	case EHHCosmeticSlot::Backpack:
		TargetArm = 230.f;
		TargetHeight = 120.f;
		TargetYaw = 180.f;
		break;
	default:
		TargetArm = 280.f;
		TargetHeight = 100.f;
		break;
	}
}

void AHHPreviewStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	CurrentYaw = FMath::FInterpTo(CurrentYaw, TargetYaw, DeltaSeconds, 8.f);
	if (Mannequin)
	{
		const FRotator Base = PreviewSpot->GetComponentRotation();
		Mannequin->SetActorLocationAndRotation(PreviewSpot->GetComponentLocation(), Base + FRotator(0.f, CurrentYaw, 0.f));
	}

	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, TargetArm, DeltaSeconds, 6.f);
	FVector BoomLocation = Boom->GetRelativeLocation();
	BoomLocation.Z = FMath::FInterpTo(BoomLocation.Z, TargetHeight, DeltaSeconds, 6.f);
	Boom->SetRelativeLocation(BoomLocation);
}
