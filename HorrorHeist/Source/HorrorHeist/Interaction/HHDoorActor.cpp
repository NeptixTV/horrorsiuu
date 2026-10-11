#include "Interaction/HHDoorActor.h"
#include "Core/HHNoiseSubsystem.h"
#include "Heist/HHHeistGameMode.h"
#include "Heist/HHHeistGameState.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Player/HHInventoryComponent.h"
#include "Data/HHEquipmentDefinition.h"
#include "GameFramework/Pawn.h"
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
	DOREPLIFETIME(AHHDoorActor, bLocked);
	DOREPLIFETIME(AHHDoorActor, bBroken);
}

void AHHDoorActor::BeginPlay()
{
	Super::BeginPlay();
	ReplicatedSpeed = SwingSpeed;
	CurrentAngle = TargetAngle;
	PreviousTarget = TargetAngle;
	Hinge->SetRelativeRotation(FRotator(0.f, CurrentAngle, 0.f));
}

bool AHHDoorActor::IsOnJob() const
{
	return GetWorld() && GetWorld()->GetGameState<AHHHeistGameState>() != nullptr;
}

float AHHDoorActor::ToolSeconds(AActor* Interactor, FName ToolTag, float BaseSeconds, float ReferenceSpeed) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
	const UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	const UHHEquipmentDefinition* Tool = Inventory ? Inventory->FindTool(ToolTag) : nullptr;
	const float Speed = Tool ? FMath::Max(Tool->GetStat(TEXT("Speed"), ReferenceSpeed), 5.f) : ReferenceSpeed;
	return BaseSeconds * ReferenceSpeed / Speed;
}

EHHDoorAction AHHDoorActor::ResolveAction(AActor* Interactor) const
{
	if (!bLocked)
	{
		return IsOpen() ? EHHDoorAction::Close : EHHDoorAction::Open;
	}
	if (!IsOnJob())
	{
		return EHHDoorAction::LockedOutsideJob;
	}
	if (bUnlockFromInside && Interactor)
	{
		const FVector ToUser = Interactor->GetActorLocation() - Hinge->GetComponentLocation();
		const bool bOnPositiveY = FVector::DotProduct(ToUser, GetActorRightVector()) > 0.f;
		if (bOnPositiveY == bInsideIsPositiveY)
		{
			return EHHDoorAction::UnlockInside;
		}
	}
	const AHHHeistGameState* HeistState = GetWorld()->GetGameState<AHHHeistGameState>();
	if (!KeyId.IsNone() && HeistState && HeistState->HasKey(KeyId))
	{
		return EHHDoorAction::UnlockKey;
	}
	const APawn* Pawn = Cast<APawn>(Interactor);
	const AHHPlayerState* State = Pawn ? Pawn->GetPlayerState<AHHPlayerState>() : nullptr;
	const UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	if (Inventory)
	{
		if (bCanPick && Inventory->FindTool(TEXT("Lockpick")))
		{
			return EHHDoorAction::Pick;
		}
		if (bCanCutGlass && Inventory->FindTool(TEXT("GlassCutter")))
		{
			return EHHDoorAction::CutGlass;
		}
		if (bCanPry && Inventory->FindTool(TEXT("Crowbar")))
		{
			return EHHDoorAction::Pry;
		}
	}
	return EHHDoorAction::Rattle;
}

FText AHHDoorActor::GetInteractionPrompt(AActor* Interactor) const
{
	switch (ResolveAction(Interactor))
	{
	case EHHDoorAction::Close:				return ClosePrompt;
	case EHHDoorAction::UnlockInside:		return LOCTEXT("UnlockInside", "Unlock");
	case EHHDoorAction::UnlockKey:			return LOCTEXT("UnlockKey", "Unlock with the key");
	case EHHDoorAction::Pick:				return LOCTEXT("Pick", "Pick the lock");
	case EHHDoorAction::CutGlass:			return LOCTEXT("CutGlass", "Cut the glass and reach in");
	case EHHDoorAction::Pry:				return LOCTEXT("Pry", "Force the door");
	case EHHDoorAction::Rattle:
	case EHHDoorAction::LockedOutsideJob:	return LOCTEXT("Locked", "Locked");
	default:								return OpenPrompt;
	}
}

FText AHHDoorActor::GetInteractionDetail(AActor* Interactor) const
{
	switch (ResolveAction(Interactor))
	{
	case EHHDoorAction::Pick:		return LOCTEXT("PickDetail", "Hold  -  quiet");
	case EHHDoorAction::CutGlass:	return LOCTEXT("CutDetail", "Hold  -  quiet");
	case EHHDoorAction::Pry:		return LOCTEXT("PryDetail", "Hold  -  LOUD, the neighbours will hear it");
	case EHHDoorAction::LockedOutsideJob:
		return LOCTEXT("ComeBack", "Nobody lets you in. Come back with the crew after dark.");
	case EHHDoorAction::Rattle:
	{
		TArray<FText> Ways;
		if (!KeyId.IsNone())		{ Ways.Add(LOCTEXT("WayKey", "the key")); }
		if (bCanPick)				{ Ways.Add(LOCTEXT("WayPick", "lockpicks")); }
		if (bCanCutGlass)			{ Ways.Add(LOCTEXT("WayGlass", "a glass cutter")); }
		if (bCanPry)				{ Ways.Add(LOCTEXT("WayPry", "a crowbar")); }
		if (Ways.Num() == 0)
		{
			return LOCTEXT("NoWay", "Not from this side.");
		}
		return FText::Format(LOCTEXT("Needs", "Needs {0}"), FText::Join(LOCTEXT("Or", " or "), Ways));
	}
	default:
		return FText::GetEmpty();
	}
}

bool AHHDoorActor::CanInteract(AActor* Interactor) const
{
	return bInteractionEnabled;
}

float AHHDoorActor::GetInteractDuration(AActor* Interactor) const
{
	switch (ResolveAction(Interactor))
	{
	case EHHDoorAction::Pick:		return ToolSeconds(Interactor, TEXT("Lockpick"), PickSeconds, 40.f);
	case EHHDoorAction::CutGlass:	return ToolSeconds(Interactor, TEXT("GlassCutter"), CutSeconds, 60.f);
	case EHHDoorAction::Pry:		return ToolSeconds(Interactor, TEXT("Crowbar"), PrySeconds, 85.f);
	default:						return 0.f;
	}
}

USoundBase* AHHDoorActor::GetHoldSound(AActor* Interactor) const
{
	switch (ResolveAction(Interactor))
	{
	case EHHDoorAction::Pick:		return PickLoopSound;
	case EHHDoorAction::CutGlass:	return GlassCutSound;
	default:						return nullptr;
	}
}

void AHHDoorActor::SetLocked(bool bNewLocked)
{
	if (HasAuthority() && bLocked != bNewLocked && !(bNewLocked && bBroken))
	{
		bLocked = bNewLocked;
		ForceNetUpdate();
	}
}

void AHHDoorActor::OnRep_Lock()
{
}

void AHHDoorActor::PlayForEveryone(USoundBase* Sound) const
{
	if (!Sound)
	{
		return;
	}
	if (AHHHeistGameState* HeistState = GetWorld()->GetGameState<AHHHeistGameState>())
	{
		HeistState->MulticastSoundAt(Sound, DoorMesh->GetComponentLocation(), 1.f);
	}
	else
	{
		const_cast<AHHDoorActor*>(this)->MulticastPlaySound(Sound, DoorMesh->GetComponentLocation(), 1.f);
	}
}

void AHHDoorActor::OpenAwayFrom(AActor* Interactor)
{
	float Sign = 1.f;
	if (!bOneWay && Interactor)
	{
		// Swing away from the user.
		const FVector ToUser = (Interactor->GetActorLocation() - Hinge->GetComponentLocation()).GetSafeNormal2D();
		Sign = FVector::DotProduct(ToUser, GetActorRightVector()) > 0.f ? -1.f : 1.f;
	}
	SetTargetAngle(OpenAngle * Sign, SwingSpeed, false);
}

void AHHDoorActor::Interact(AActor* Interactor)
{
	BP_OnInteracted(Interactor);
	if (!HasAuthority())
	{
		return;
	}

	const EHHDoorAction Action = ResolveAction(Interactor);
	const APawn* Pawn = Cast<APawn>(Interactor);
	AController* User = Pawn ? Pawn->GetController() : nullptr;
	AHHPlayerController* PC = Cast<AHHPlayerController>(User);
	UHHNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UHHNoiseSubsystem>();
	AHHHeistGameMode* HeistMode = GetWorld()->GetAuthGameMode<AHHHeistGameMode>();

	switch (Action)
	{
	case EHHDoorAction::Open:
		OpenAwayFrom(Interactor);
		break;

	case EHHDoorAction::Close:
		SetTargetAngle(0.f, SwingSpeed, false);
		break;

	case EHHDoorAction::Rattle:
	case EHHDoorAction::LockedOutsideJob:
		PlayForEveryone(LockedSound);
		if (PC)
		{
			PC->ClientNotify(Action == EHHDoorAction::LockedOutsideJob
				? LOCTEXT("LockedHub", "Locked. Nobody's home to let you in - come back on a job.")
				: FText::Format(LOCTEXT("LockedJob", "Locked. {0}."), GetInteractionDetail(Interactor)), EHHNotifyType::Info);
		}
		if (Noise)
		{
			Noise->ReportNoise(GetActorLocation(), 0.12f, Interactor, TEXT("DoorRattle"));
		}
		return;

	default:
		// Every way of getting through a locked door ends with it unlocked and swinging open.
		bLocked = false;
		if (Action == EHHDoorAction::Pry)
		{
			bBroken = true;
			PlayForEveryone(PrySound);
		}
		else
		{
			PlayForEveryone(UnlockSound);
		}
		ForceNetUpdate();
		OpenAwayFrom(Interactor);
		if (HeistMode)
		{
			HeistMode->HandleDoorUnlocked(User, this, Action == EHHDoorAction::Pry, Action == EHHDoorAction::UnlockInside);
		}
		if (Noise)
		{
			const float Loudness = Action == EHHDoorAction::Pry ? 0.9f : (Action == EHHDoorAction::CutGlass ? 0.3f : 0.15f);
			Noise->ReportNoise(GetActorLocation(), Loudness, Interactor, Action == EHHDoorAction::Pry ? TEXT("ForcedEntry") : TEXT("Unlock"));
		}
		return;
	}

	if (Noise)
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
