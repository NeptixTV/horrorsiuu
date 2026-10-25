#include "Interaction/SUDoor.h"
#include "Player/SUCharacter.h"
#include "Audio/SUSoundEmitter.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ASUDoor::ASUDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
}

void ASUDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUDoor, Width);
	DOREPLIFETIME(ASUDoor, Height);
	DOREPLIFETIME(ASUDoor, Thickness);
	DOREPLIFETIME(ASUDoor, MaxAngle);
	DOREPLIFETIME(ASUDoor, Material);
	DOREPLIFETIME(ASUDoor, bLocked);
	DOREPLIFETIME(ASUDoor, LockedText);
	DOREPLIFETIME(ASUDoor, bSmall);
	DOREPLIFETIME(ASUDoor, DisplayName);
	DOREPLIFETIME(ASUDoor, bOpen);
	DOREPLIFETIME(ASUDoor, OpenSign);
	DOREPLIFETIME(ASUDoor, SlamCounter);
}

void ASUDoor::BuildGeometry()
{
	Hinge = NewObject<USceneComponent>(this, TEXT("Hinge"));
	Hinge->SetMobility(EComponentMobility::Movable);
	Hinge->SetupAttachment(SceneRoot);
	Hinge->SetRelativeLocation(FVector(0.f, -Width * 0.5f, 0.f));
	Hinge->RegisterComponent();

	AddPart(Hinge, ESUShape::Cube, FVector(0.f, Width * 0.5f, Height * 0.5f), FVector(Thickness, Width - 2.f, Height - 1.f), Material);
	if (!bSmall)
	{
		// panels and handles on both sides
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			AddPart(Hinge, ESUShape::Cube, FVector(Side * (Thickness * 0.5f + 1.f), Width * 0.5f, Height * 0.72f), FVector(2.f, Width * 0.7f, Height * 0.35f), ESUMat::WoodLight, FRotator::ZeroRotator, false);
			AddPart(Hinge, ESUShape::Cube, FVector(Side * (Thickness * 0.5f + 1.f), Width * 0.5f, Height * 0.28f), FVector(2.f, Width * 0.7f, Height * 0.35f), ESUMat::WoodLight, FRotator::ZeroRotator, false);
			AddPart(Hinge, ESUShape::Cylinder, FVector(Side * (Thickness * 0.5f + 4.f), Width - 10.f, Height * 0.48f), FVector(3.f, 3.f, 12.f), ESUMat::Chrome, FRotator::ZeroRotator, false);
		}
	}
	else
	{
		AddPart(Hinge, ESUShape::Cube, FVector(Thickness * 0.5f + 2.f, Width - 6.f, Height * 0.5f), FVector(3.f, 2.f, FMath::Min(40.f, Height * 0.4f)), ESUMat::Chrome, FRotator::ZeroRotator, false);
	}

	CurrentAngle = bOpen ? OpenSign * MaxAngle : 0.f;
	Hinge->SetRelativeRotation(FRotator(0.f, CurrentAngle, 0.f));
}

void ASUDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Hinge)
	{
		return;
	}
	const float Target = bOpen ? OpenSign * MaxAngle : 0.f;
	if (!FMath::IsNearlyEqual(CurrentAngle, Target, 0.05f))
	{
		const float Diff = Target - CurrentAngle;
		const float Step = FMath::Sign(Diff) * FMath::Min(FMath::Abs(Diff), AngleSpeed * DeltaSeconds * (0.35f + 0.65f * FMath::Clamp(FMath::Abs(Diff) / 30.f, 0.f, 1.f)));
		CurrentAngle += Step;
		Hinge->SetRelativeRotation(FRotator(0.f, CurrentAngle, 0.f));
	}
	else if (AngleSpeed > 200.f)
	{
		AngleSpeed = 160.f;
	}
}

bool ASUDoor::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	if (bLocked)
	{
		OutPrompt.Info = LockedText.IsEmpty() ? SUText(TEXT("Verschlossen")) : LockedText;
		OutPrompt.Primary = SUText(TEXT("Rütteln"));
		OutPrompt.bPrimary = true;
		return true;
	}
	const FString Name = DisplayName.IsEmpty() ? FString() : FString(TEXT(" ")) + DisplayName.ToString();
	OutPrompt.Primary = FText::FromString((bOpen ? FString(TEXT("Schließen")) : FString(TEXT("Öffnen"))) + Name);
	OutPrompt.bPrimary = true;
	return true;
}

void ASUDoor::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	if (bLocked)
	{
		ASUSoundEmitter::PlayAt(this, GetActorLocation() + FVector(0.f, 0.f, Height * 0.5f), ESUSoundKind::Impact, 0, 0.5f, 0.8f, 1500.f, 2.f, 3, 0.12f);
		if (User)
		{
			User->NotifyOwner(LockedText.IsEmpty() ? SUText(TEXT("Verschlossen.")) : LockedText);
		}
		return;
	}
	SetOpen(!bOpen, User ? User->GetActorLocation() : GetActorLocation());
	Super::Interact(User, Component, Option);
}

void ASUDoor::SetOpen(bool bInOpen, const FVector& FromLocation, bool bSlam)
{
	if (!HasAuthority() || bInOpen == bOpen)
	{
		return;
	}
	if (bInOpen)
	{
		const FVector Local = GetActorTransform().InverseTransformPosition(FromLocation);
		OpenSign = Local.X >= 0.f ? 1.f : -1.f;
	}
	bOpen = bInOpen;
	if (bSlam)
	{
		++SlamCounter;
		OnRep_Slam();
	}
	OnRep_Open();
}

void ASUDoor::OnRep_Open()
{
	// doors configured as open while spawning start silently
	if (HasActorBegunPlay())
	{
		PlayMoveSound(bOpen);
	}
}

void ASUDoor::OnRep_Slam()
{
	AngleSpeed = 650.f;
}

void ASUDoor::PlayMoveSound(bool bOpening)
{
	if (USUProceduralAudioComponent* Sound = GetOrCreateSound(ESUSoundKind::DoorCreak, bSmall ? 0.35f : 0.9f, 2500.f))
	{
		Sound->TriggerSound(bOpening ? 0 : 1, AngleSpeed > 200.f ? 1.4f : 1.f);
	}
}
