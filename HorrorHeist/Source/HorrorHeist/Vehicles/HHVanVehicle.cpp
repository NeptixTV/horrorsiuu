#include "Vehicles/HHVanVehicle.h"
#include "Vehicles/HHGarageDoor.h"
#include "Heist/HHHeistGameState.h"
#include "Heist/HHVanPoint.h"
#include "Interaction/HHReadyStation.h"
#include "Player/HHCharacter.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Core/HHGameData.h"
#include "Core/HHNoiseSubsystem.h"
#include "Settings/HHGameUserSettings.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "HHVan"

AHHVanVehicle::AHHVanVehicle()
{
	auto MakePart = [this](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(Body);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		return Component;
	};
	LiveryL = MakePart(TEXT("LiveryL"));
	LiveryR = MakePart(TEXT("LiveryR"));
	LiveryL->SetCastShadow(false);
	LiveryR->SetCastShadow(false);
	SlidingDoor = MakePart(TEXT("SlidingDoor"));

	ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
	ChaseArm->SetupAttachment(Collision);
	ChaseArm->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	ChaseArm->SetUsingAbsoluteRotation(true);
	ChaseArm->TargetArmLength = 820.f;
	ChaseArm->SocketOffset = FVector(0.f, 0.f, 110.f);
	ChaseArm->bDoCollisionTest = true;
	ChaseArm->ProbeChannel = ECC_Camera;
	ChaseArm->bEnableCameraLag = true;
	ChaseArm->CameraLagSpeed = 9.f;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(ChaseArm, USpringArmComponent::SocketName);
	ChaseCamera->SetFieldOfView(85.f);

	Seats.SetNum(NumSeats);
}

void AHHVanVehicle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Mesh-local placement of the extra parts (the van mesh is built around its ground centre).
	LiveryL->SetStaticMesh(LiveryMesh);
	LiveryR->SetStaticMesh(LiveryMesh);
	LiveryR->SetRelativeLocationAndRotation(FVector(101.2f, 138.f, 145.f), FRotator(0.f, -90.f, 0.f));
	LiveryL->SetRelativeLocationAndRotation(FVector(-101.2f, 20.f, 145.f), FRotator(0.f, 90.f, 0.f));
	SlidingDoor->SetStaticMesh(SlidingDoorMesh);
	SlidingDoor->SetRelativeLocation(FVector(99.f, -20.f, 120.f));
}

void AHHVanVehicle::BeginPlay()
{
	Super::BeginPlay();
	Seats.SetNum(NumSeats);
	AdoptAttachments();
}

void AHHVanVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHVanVehicle, Seats);
}

AHHVanVehicle* AHHVanVehicle::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AHHVanVehicle> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AHHVanVehicle::AdoptAttachments()
{
	// Runs on every machine: these are level actors that do not replicate their movement, so each
	// machine attaches its own copy (they all start in the same spot).
	const FVector Center = GetActorLocation();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this || (!Actor->IsA<AHHReadyStation>() && !Actor->IsA<AHHVanPoint>()))
		{
			continue;
		}
		if (FVector::DistSquared2D(Actor->GetActorLocation(), Center) > FMath::Square(AdoptRadius) || Actor->GetAttachParentActor())
		{
			continue;
		}
		Actor->AttachToComponent(Collision, FAttachmentTransformRules::KeepWorldTransform);
		Collision->IgnoreActorWhenMoving(Actor, true);
	}
}

bool AHHVanVehicle::IsOnJob() const
{
	return AHHHeistGameState::IsJobWorld(this);
}

// ---------------------------------------------------------------------------------------
// Seats

AHHCharacter* AHHVanVehicle::GetOccupant(int32 Seat) const
{
	return Seats.IsValidIndex(Seat) && IsValid(Seats[Seat]) ? Seats[Seat].Get() : nullptr;
}

int32 AHHVanVehicle::GetNumOccupants() const
{
	int32 Count = 0;
	for (int32 Seat = 0; Seat < Seats.Num(); ++Seat)
	{
		Count += GetOccupant(Seat) ? 1 : 0;
	}
	return Count;
}

bool AHHVanVehicle::IsOccupant(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	for (int32 Seat = 0; Seat < Seats.Num(); ++Seat)
	{
		if (GetOccupant(Seat) == Actor)
		{
			return true;
		}
	}
	return false;
}

FVector AHHVanVehicle::GetSeatOffset(int32 Seat) const
{
	switch (Seat)
	{
	case 0:		return FVector(95.f, -45.f, 20.f);
	case 1:		return FVector(95.f, 45.f, 20.f);
	case 2:		return FVector(-80.f, -45.f, 20.f);
	default:	return FVector(-80.f, 45.f, 20.f);
	}
}

int32 AHHVanVehicle::SeatFor(const AActor* Interactor) const
{
	if (!Interactor)
	{
		return 1;
	}
	const FVector Local = GetActorTransform().InverseTransformPosition(Interactor->GetActorLocation());
	if (Local.X > -20.f)
	{
		return Local.Y < 0.f ? 0 : 1;
	}
	return Local.Y < 0.f ? 2 : 3;
}

bool AHHVanVehicle::TryEnter(AHHCharacter* Character, int32 PreferredSeat)
{
	if (!HasAuthority() || !Character || Character->IsSeated() || IsOccupant(Character) || Character->IsIncapacitated())
	{
		return false;
	}
	if (!bDrivable && PreferredSeat == 0)
	{
		PreferredSeat = 1;
	}
	const int32 Order[] = { PreferredSeat, 1, 2, 3, 0 };
	for (const int32 Seat : Order)
	{
		if (!Seats.IsValidIndex(Seat) || GetOccupant(Seat) || (Seat == 0 && !bDrivable))
		{
			continue;
		}
		Seats[Seat] = Character;
		Character->SetSeat(this, Seat);
		if (Seat == 0)
		{
			// The driver's connection owns the van so its RPCs (state, horn, lights) are accepted.
			SetOwner(Character->GetController());
			MulticastSound(EngineStartSound, 0.8f);
		}
		MulticastSound(DoorSound, 0.8f);
		ForceNetUpdate();
		return true;
	}
	return false;
}

void AHHVanVehicle::Exit(AHHCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}
	const int32 Seat = Seats.IndexOfByKey(Character);
	if (Seat == INDEX_NONE)
	{
		return;
	}
	FVector Spot;
	const bool bFound = FindExitSpot(Character, Seat, Spot);
	Seats[Seat] = nullptr;
	if (Seat == 0)
	{
		SetOwner(nullptr);
		DriverInput = FVector2D::ZeroVector;
		bHandbrakeInput = false;
	}
	Character->SetSeat(nullptr, INDEX_NONE);
	const FRotator Facing(0.f, GetActorRotation().Yaw, 0.f);
	if (bFound)
	{
		Character->TeleportTo(Spot, Facing, false, true);
		Character->ClientExitedVehicle(Spot, Facing);
	}
	MulticastSound(DoorSound, 0.8f);
	ForceNetUpdate();
}

bool AHHVanVehicle::FindExitSpot(const AHHCharacter* Character, int32 Seat, FVector& OutLocation) const
{
	static const FVector2D DriverSide[] = { { 95.f, -200.f }, { -60.f, -200.f }, { -380.f, 0.f }, { 95.f, 200.f }, { 380.f, 0.f } };
	static const FVector2D PassengerSide[] = { { 95.f, 200.f }, { -60.f, 200.f }, { -380.f, 0.f }, { 95.f, -200.f }, { 380.f, 0.f } };
	static const FVector2D RearSide[] = { { -60.f, 200.f }, { -380.f, 0.f }, { -60.f, -200.f }, { 95.f, 200.f }, { 380.f, 0.f } };
	const FVector2D* Candidates = Seat == 0 ? DriverSide : (Seat == 1 ? PassengerSide : RearSide);

	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FTransform Flat(FRotator(0.f, GetActorRotation().Yaw, 0.f), GetGroundLocation());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HHVanExit), false, this);
	Params.AddIgnoredActor(Character);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FVector Point = Flat.TransformPosition(FVector(Candidates[Index].X, Candidates[Index].Y, 0.f));
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Point + FVector(0.f, 0.f, 220.f), Point - FVector(0.f, 0.f, 300.f), ECC_WorldStatic, Params))
		{
			continue;
		}
		FVector Location = Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 4.f);
		FRotator Rotation = FRotator::ZeroRotator;
		if (GetWorld()->FindTeleportSpot(Character, Location, Rotation))
		{
			OutLocation = Location;
			return true;
		}
	}
	return false;
}

void AHHVanVehicle::OnRep_Seats()
{
	// The local driver lost the seat: forget the pedals.
	const AHHCharacter* Driver = GetDriver();
	if (!Driver || !Driver->IsLocallyControlled())
	{
		DriverInput = FVector2D::ZeroVector;
		bHandbrakeInput = false;
	}
}

// ---------------------------------------------------------------------------------------
// Interaction

FText AHHVanVehicle::GetInteractionPrompt(AActor* Interactor) const
{
	if (GetNumOccupants() >= NumSeats)
	{
		return LOCTEXT("Full", "The van is full");
	}
	const int32 Seat = SeatFor(Interactor);
	if (Seat == 0 && bDrivable && !GetDriver())
	{
		return LOCTEXT("Drive", "Get in and drive");
	}
	return LOCTEXT("GetIn", "Get in the van");
}

FText AHHVanVehicle::GetInteractionDetail(AActor* Interactor) const
{
	if (!bDrivable)
	{
		return LOCTEXT("Parked", "Not going anywhere now.");
	}
	if (IsOnJob())
	{
		const AHHHeistGameState* State = GetWorld()->GetGameState<AHHHeistGameState>();
		return FText::Format(LOCTEXT("JobDetail", "{0} of {1} crew inside  -  drive to the getaway spot to end the job"),
			FText::AsNumber(GetNumOccupants()), FText::AsNumber(State ? State->GetCrew().Num() : NumSeats));
	}
	return LOCTEXT("HubDetail", "WASD drive  -  Space brake  -  G garage door  -  F lights  -  H horn  -  E get out");
}

bool AHHVanVehicle::CanInteract(AActor* Interactor) const
{
	const AHHCharacter* Character = Cast<AHHCharacter>(Interactor);
	if (!Character || Character->IsSeated() || Character->IsArrested() || Character->IsIncapacitated() || GetNumOccupants() >= NumSeats)
	{
		return false;
	}
	const AHHHeistGameState* Heist = GetWorld()->GetGameState<AHHHeistGameState>();
	return !Heist || Heist->GetHeistPhase() != EHHHeistPhase::Ended;
}

void AHHVanVehicle::Interact(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}
	TryEnter(Cast<AHHCharacter>(Interactor), SeatFor(Interactor));
}

void AHHVanVehicle::SetInteractionFocus(bool bFocused)
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	UMaterialInterface* Overlay = nullptr;
	if (bFocused && (!Settings || Settings->bInteractionHighlight))
	{
		if (const UHHGameData* GameData = UHHGameData::Get(this))
		{
			Overlay = GameData->InteractionHighlight.LoadSynchronous();
		}
	}
	Body->SetOverlayMaterial(Overlay);
}

// ---------------------------------------------------------------------------------------
// Driving

bool AHHVanVehicle::IsLocallySimulated() const
{
	const AHHCharacter* Driver = GetDriver();
	return Driver && bDrivable ? Driver->IsLocallyControlled() : HasAuthority();
}

void AHHVanVehicle::GetDriveInput(float& OutThrottle, float& OutSteer, bool& bOutHandbrake) const
{
	const AHHCharacter* Driver = GetDriver();
	if (Driver && bDrivable && Driver->IsLocallyControlled())
	{
		OutThrottle = FMath::Clamp(DriverInput.Y, -1.f, 1.f);
		OutSteer = FMath::Clamp(DriverInput.X, -1.f, 1.f);
		bOutHandbrake = bHandbrakeInput;
		return;
	}
	// Nobody at the wheel: the handbrake is on.
	OutThrottle = 0.f;
	OutSteer = 0.f;
	bOutHandbrake = true;
}

bool AHHVanVehicle::IsEngineRunning() const
{
	return GetDriver() != nullptr && bDrivable;
}

void AHHVanVehicle::SetDriverInput(const FVector2D& Axis)
{
	DriverInput = Axis;
}

void AHHVanVehicle::SetHandbrake(bool bOn)
{
	bHandbrakeInput = bOn;
}

void AHHVanVehicle::PostSimulate(float DeltaSeconds)
{
	if (HasAuthority())
	{
		return;
	}
	SendTimer -= DeltaSeconds;
	if (SendTimer <= 0.f)
	{
		SendTimer = 1.f / 30.f;
		ServerDriveState(GetActorLocation(), GetActorRotation().Yaw, Speed, Steer);
	}
}

void AHHVanVehicle::ServerDriveState_Implementation(FVector_NetQuantize10 InLocation, float InYaw, float InSpeed, float InSteer)
{
	const AHHCharacter* Driver = GetDriver();
	if (!Driver || !bDrivable || Driver->GetController() != GetOwner())
	{
		return;
	}
	// Co-op trust with a sanity check: no teleporting across town.
	if (FVector::DistSquared(GetActorLocation(), InLocation) > FMath::Square(2500.f))
	{
		ClientCorrect(GetActorLocation(), GetActorRotation().Yaw);
		return;
	}
	Speed = FMath::Clamp(InSpeed, -MaxReverseSpeed, MaxForwardSpeed);
	Steer = FMath::Clamp(InSteer, -1.f, 1.f);
	const FRotator Current = GetActorRotation();
	SetActorLocationAndRotation(InLocation, FRotator(Current.Pitch, InYaw, Current.Roll));
	FollowGround(1.f / 30.f, false);
	PublishState();
}

void AHHVanVehicle::ClientCorrect_Implementation(FVector InLocation, float InYaw)
{
	SetActorLocationAndRotation(InLocation, FRotator(0.f, InYaw, 0.f));
	Speed = 0.f;
}

void AHHVanVehicle::OnBumped(const FHitResult& Hit, float ImpactSpeed)
{
	Super::OnBumped(Hit, ImpactSpeed);
	// A crash in a quiet street at night is heard (the heist mode listens to noise).
	if (ImpactSpeed > 600.f && IsOnJob())
	{
		if (UHHNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UHHNoiseSubsystem>())
		{
			Noise->ReportNoise(Hit.ImpactPoint, FMath::Clamp(ImpactSpeed / 1500.f, 0.6f, 1.f), GetDriver(), TEXT("Crash"));
		}
	}
}

void AHHVanVehicle::ServerToggleHeadlights_Implementation()
{
	SetHeadlights(!AreHeadlightsOn());
}

void AHHVanVehicle::ServerHorn_Implementation()
{
	MulticastSound(HornSound, 1.f);
	if (IsOnJob())
	{
		if (UHHNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UHHNoiseSubsystem>())
		{
			Noise->ReportNoise(GetActorLocation(), 1.f, GetDriver(), TEXT("Horn"));
		}
	}
}

void AHHVanVehicle::ServerGarageRemote_Implementation()
{
	AHHCharacter* Driver = GetDriver();
	AHHGarageDoor* Door = AHHGarageDoor::FindNearest(this, GetActorLocation(), RemoteRange);
	AHHPlayerController* PC = Driver ? Cast<AHHPlayerController>(Driver->GetController()) : nullptr;
	if (!Door)
	{
		if (PC)
		{
			PC->ClientNotify(LOCTEXT("NoGarage", "No garage door in range of the remote."), EHHNotifyType::Info);
		}
		return;
	}
	FText Reason;
	if (!Door->CanOperate(Reason))
	{
		if (PC)
		{
			PC->ClientNotify(Reason, EHHNotifyType::Warning);
		}
		return;
	}
	Door->Toggle();
}

void AHHVanVehicle::MulticastSound_Implementation(USoundBase* Sound, float Volume)
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), Volume);
	}
}

// ---------------------------------------------------------------------------------------
// Every frame

void AHHVanVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		// Somebody left the game while seated.
		bool bChanged = false;
		for (int32 Seat = 0; Seat < Seats.Num(); ++Seat)
		{
			if (Seats[Seat] && !IsValid(Seats[Seat]))
			{
				Seats[Seat] = nullptr;
				bChanged = true;
				if (Seat == 0)
				{
					SetOwner(nullptr);
				}
			}
		}
		if (bChanged)
		{
			ForceNetUpdate();
		}
		StoppedSeconds = FMath::Abs(Speed) < 30.f ? StoppedSeconds + DeltaSeconds : 0.f;
	}
	UpdateChaseCamera(DeltaSeconds);
}

void AHHVanVehicle::UpdateChaseCamera(float DeltaSeconds)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const float VanYaw = GetActorRotation().Yaw;
	if (!PC || !IsOccupant(Pawn))
	{
		bHasLastViewYaw = false;
		return;
	}
	// The view turns with the van; the mouse looks around on top of that.
	FRotator Control = PC->GetControlRotation();
	if (bHasLastViewYaw)
	{
		Control.Yaw = FRotator::NormalizeAxis(Control.Yaw + FMath::FindDeltaAngleDegrees(LastViewYaw, VanYaw));
		PC->SetControlRotation(Control);
	}
	LastViewYaw = VanYaw;
	bHasLastViewYaw = true;
	const float Pitch = FMath::Clamp(FRotator::NormalizeAxis(Control.Pitch), -50.f, 20.f) - 12.f;
	ChaseArm->SetWorldRotation(FRotator(Pitch, Control.Yaw, 0.f));
}

#undef LOCTEXT_NAMESPACE
