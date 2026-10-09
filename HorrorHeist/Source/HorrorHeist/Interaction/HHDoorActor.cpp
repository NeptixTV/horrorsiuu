#include "Interaction/HHDoorActor.h"
#include "Core/HHNoiseSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "HHDoor"

AHHDoorActor::AHHDoorActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetReplicatingMovement(false);

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Hinge);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
	DoorMesh->SetMobility(EComponentMobility::Movable);

	OpenPrompt = LOCTEXT("Open", "Open");
	ClosePrompt = LOCTEXT("Close", "Close");
}

void AHHDoorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHDoorActor, TargetAngle);
	DOREPLIFETIME(AHHDoorActor, ReplicatedSpeed);
	DOREPLIFETIME(AHHDoorActor, bReplicatedCreak);
}

void AHHDoorActor::BeginPlay()
{
	Super::BeginPlay();
	ReplicatedSpeed = SwingSpeed;
	CurrentAngle = TargetAngle;
	PreviousTarget = TargetAngle;
	Hinge->SetRelativeRotation(FRotator(0.f, CurrentAngle, 0.f));
}

FText AHHDoorActor::GetInteractionPrompt(AActor* Interactor) const
{
	return IsOpen() ? ClosePrompt : OpenPrompt;
}

void AHHDoorActor::Interact(AActor* Interactor)
{
	BP_OnInteracted(Interactor);
	if (!HasAuthority())
	{
		return;
	}

	float NewTarget = 0.f;
	if (!IsOpen())
	{
		float Sign = 1.f;
		if (!bOneWay && Interactor)
		{
			// Swing away from the user.
			const FVector ToUser = (Interactor->GetActorLocation() - Hinge->GetComponentLocation()).GetSafeNormal2D();
			Sign = FVector::DotProduct(ToUser, GetActorRightVector()) > 0.f ? -1.f : 1.f;
		}
		NewTarget = OpenAngle * Sign;
	}
	SetTargetAngle(NewTarget, SwingSpeed, false);

	if (UHHNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UHHNoiseSubsystem>())
	{
		Noise->ReportNoise(GetActorLocation(), 0.45f, Interactor, TEXT("Door"));
	}
}

void AHHDoorActor::SetTargetAngle(float Angle, float Speed, bool bPlayCreak)
{
	if (!HasAuthority())
	{
		return;
	}
	ReplicatedSpeed = FMath::Max(Speed, 1.f);
	bReplicatedCreak = bPlayCreak;
	TargetAngle = Angle;
	OnRep_TargetAngle();
	ForceNetUpdate();
}

void AHHDoorActor::OnRep_TargetAngle()
{
	const bool bOpening = FMath::Abs(TargetAngle) > FMath::Abs(PreviousTarget);
	USoundBase* Sound = bReplicatedCreak ? CreakSound.Get() : (bOpening ? OpenSound.Get() : nullptr);
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, DoorMesh->GetComponentLocation());
	}
	PreviousTarget = TargetAngle;
	SetActorTickEnabled(true);
}

void AHHDoorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Previous = CurrentAngle;
	CurrentAngle = FMath::FInterpConstantTo(CurrentAngle, TargetAngle, DeltaSeconds, ReplicatedSpeed);
	// Ease the last few degrees so the door settles instead of stopping dead.
	if (FMath::Abs(TargetAngle - CurrentAngle) < 8.f)
	{
		CurrentAngle = FMath::FInterpTo(CurrentAngle, TargetAngle, DeltaSeconds, 10.f);
	}
	Hinge->SetRelativeRotation(FRotator(0.f, CurrentAngle, 0.f));

	if (FMath::IsNearlyEqual(CurrentAngle, TargetAngle, 0.05f))
	{
		CurrentAngle = TargetAngle;
		Hinge->SetRelativeRotation(FRotator(0.f, CurrentAngle, 0.f));
		SetActorTickEnabled(false);
		// Closing thud when the door lands in its frame.
		if (FMath::IsNearlyZero(TargetAngle) && !FMath::IsNearlyZero(Previous) && CloseSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, CloseSound, DoorMesh->GetComponentLocation());
		}
	}
}

#undef LOCTEXT_NAMESPACE
