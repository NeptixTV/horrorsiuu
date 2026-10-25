#include "Player/SUCharacter.h"
#include "SignalUnknown.h"
#include "Player/SUInventoryComponent.h"
#include "Player/SUInteractionComponent.h"
#include "Player/SUPhotoCameraComponent.h"
#include "Core/SUPlayerController.h"
#include "Core/SUPlayerState.h"
#include "Core/SUGameMode.h"
#include "Core/SUGameState.h"
#include "Core/SUInputConfig.h"
#include "Core/SUItemDatabase.h"
#include "Core/SUEventSubsystem.h"
#include "Core/SUGameUserSettings.h"
#include "Core/SUDeveloperSettings.h"
#include "Audio/SUProceduralAudioComponent.h"
#include "Audio/SUVoice.h"
#include "Interaction/SUInteractable.h"
#include "Interaction/SUPickupItem.h"
#include "Mission/SUMissionManager.h"
#include "Vehicle/SUVehicle.h"
#include "GasStation/SUFuelPump.h"
#include "UI/SUHUD.h"
#include "World/SUAssetLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "Net/VoiceConfig.h"
#include "Engine/World.h"

ASUCharacter::ASUCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 92.f);
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	BaseEyeHeight = 68.f;
	CrouchedEyeHeight = 40.f;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = 170.f;
	Move->MaxWalkSpeedCrouched = 105.f;
	Move->JumpZVelocity = 360.f;
	Move->AirControl = 0.15f;
	Move->BrakingDecelerationWalking = 1500.f;
	Move->GroundFriction = 9.f;
	Move->MaxStepHeight = 45.f;
	Move->SetWalkableFloorAngle(46.f);
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->bCanWalkOffLedgesWhenCrouching = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 68.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(90.f);

	ViewModel = CreateDefaultSubobject<USceneComponent>(TEXT("ViewModel"));
	ViewModel->SetupAttachment(FirstPersonCamera);

	HandAttach = CreateDefaultSubobject<USceneComponent>(TEXT("HandAttach"));
	HandAttach->SetupAttachment(FirstPersonCamera);
	HandAttach->SetRelativeLocation(FVector(35.f, 20.f, -28.f));

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(FirstPersonCamera);
	Flashlight->SetRelativeLocation(FVector(12.f, -16.f, -14.f));
	Flashlight->IntensityUnits = ELightUnits::Candelas;
	Flashlight->Intensity = 700.f;
	Flashlight->AttenuationRadius = 3400.f;
	Flashlight->OuterConeAngle = 24.f;
	Flashlight->InnerConeAngle = 11.f;
	Flashlight->LightColor = FColor(255, 240, 215);
	Flashlight->CastShadows = true;
	Flashlight->SetVisibility(false);

	FlashBulb = CreateDefaultSubobject<USpotLightComponent>(TEXT("CameraFlash"));
	FlashBulb->SetupAttachment(FirstPersonCamera);
	FlashBulb->SetRelativeLocation(FVector(6.f, 0.f, 10.f));
	FlashBulb->IntensityUnits = ELightUnits::Candelas;
	FlashBulb->Intensity = 9000.f;
	FlashBulb->AttenuationRadius = 1600.f;
	FlashBulb->OuterConeAngle = 70.f;
	FlashBulb->InnerConeAngle = 45.f;
	FlashBulb->CastShadows = true;
	FlashBulb->SetVisibility(false);

	Inventory = CreateDefaultSubobject<USUInventoryComponent>(TEXT("Inventory"));
	Interaction = CreateDefaultSubobject<USUInteractionComponent>(TEXT("Interaction"));
	PhotoCamera = CreateDefaultSubobject<USUPhotoCameraComponent>(TEXT("PhotoCamera"));
}

void ASUCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUCharacter, Health);
	DOREPLIFETIME(ASUCharacter, bDead);
	DOREPLIFETIME(ASUCharacter, bFlashlightOn);
	DOREPLIFETIME(ASUCharacter, HeldItem);
	DOREPLIFETIME(ASUCharacter, Vehicle);
	DOREPLIFETIME(ASUCharacter, SeatIndex);
	DOREPLIFETIME(ASUCharacter, HeldNozzlePump);
}

// ------------------------------------------------------------------------------------ lifecycle

void ASUCharacter::BeginPlay()
{
	Super::BeginPlay();
	BuildBody();

	if (GetNetMode() != NM_DedicatedServer)
	{
		FootstepAudio = NewObject<USUProceduralAudioComponent>(this, TEXT("Footsteps"));
		FootstepAudio->SetupAttachment(GetCapsuleComponent());
		FootstepAudio->SetRelativeLocation(FVector(0.f, 0.f, -85.f));
		FootstepAudio->ConfigureSound(ESUSoundKind::Footstep, SUAudioCategory::Effects, IsLocallyControlled() ? 0.55f : 0.9f, 2200.f);
		FootstepAudio->RegisterComponent();
		FootstepAudio->StartSound();
	}

	OnRep_Flashlight();
	OnRep_HeldItem();
	if (Vehicle)
	{
		ApplySeatedState();
	}
	if (Controller)
	{
		LastViewRotation = Controller->GetControlRotation();
	}
	SetupVoice();
}

void ASUCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (Vehicle)
		{
			Vehicle->RemoveOccupant(this, false);
		}
		if (HeldNozzlePump)
		{
			HeldNozzlePump->ReturnNozzle();
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ASUCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ApplyMovementSpeed();
}

void ASUCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	SetupVoice();
}

void ASUCharacter::BuildBody()
{
	USUAssetLibrary* Lib = USUAssetLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	// PLACEHOLDER body for other players (no skeletal mesh / animations yet)
	static const ESUMat Jackets[4] = {ESUMat::GasRed, ESUMat::FabricBlue, ESUMat::MetalPainted, ESUMat::Yellow};
	const int32 PlayerIndex = GetPlayerState() ? (GetPlayerState()->GetPlayerId() & 3) : 0;
	auto Part = [this, Lib](USceneComponent* Parent, ESUShape Shape, const FVector& Loc, const FVector& Size, ESUMat Mat, const FRotator& Rot) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(Parent);
		C->SetStaticMesh(Lib->GetMesh(Shape));
		C->SetMaterial(0, Lib->GetMaterial(Mat));
		C->SetRelativeTransform(FTransform(Rot, Loc, Size / 100.f));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetOwnerNoSee(true);
		C->RegisterComponent();
		return C;
	};
	USceneComponent* Root = GetCapsuleComponent();
	BodyParts.Add(Part(Root, ESUShape::Cylinder, FVector(0.f, 0.f, 22.f), FVector(48.f, 30.f, 70.f), Jackets[PlayerIndex], FRotator::ZeroRotator));
	BodyParts.Add(Part(Root, ESUShape::Sphere, FVector(0.f, 0.f, 72.f), FVector(22.f, 20.f, 25.f), ESUMat::Skin, FRotator::ZeroRotator));
	BodyParts.Add(Part(Root, ESUShape::Cylinder, FVector(0.f, 0.f, 84.f), FVector(23.f, 23.f, 8.f), ESUMat::Cloth, FRotator::ZeroRotator)); // cap
	BodyParts.Add(Part(Root, ESUShape::Cylinder, FVector(0.f, -28.f, 18.f), FVector(11.f, 11.f, 62.f), Jackets[PlayerIndex], FRotator(0.f, 0.f, 8.f)));
	BodyParts.Add(Part(Root, ESUShape::Cylinder, FVector(0.f, 28.f, 18.f), FVector(11.f, 11.f, 62.f), Jackets[PlayerIndex], FRotator(0.f, 0.f, -8.f)));
	LegParts.Add(Part(Root, ESUShape::Cylinder, FVector(0.f, -11.f, -52.f), FVector(15.f, 15.f, 82.f), ESUMat::Cloth, FRotator::ZeroRotator));
	LegParts.Add(Part(Root, ESUShape::Cylinder, FVector(0.f, 11.f, -52.f), FVector(15.f, 15.f, 82.f), ESUMat::Cloth, FRotator::ZeroRotator));
	for (UStaticMeshComponent* L : LegParts)
	{
		BodyParts.Add(L);
	}

	// first person hand-held models (only the owner sees them)
	auto ViewPart = [this, Lib](ESUShape Shape, const FVector& Loc, const FVector& Size, ESUMat Mat, const FRotator& Rot) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(ViewModel);
		C->SetStaticMesh(Lib->GetMesh(Shape));
		C->SetMaterial(0, Lib->GetMaterial(Mat));
		C->SetRelativeTransform(FTransform(Rot, Loc, Size / 100.f));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetOnlyOwnerSee(true);
		C->SetCastShadow(false);
		C->RegisterComponent();
		return C;
	};
	HeldFlashlightModel = ViewPart(ESUShape::Cylinder, FVector(30.f, -18.f, -20.f), FVector(4.5f, 4.5f, 24.f), ESUMat::Black, FRotator(-90.f, 0.f, 0.f));
	HeldCameraModel = ViewPart(ESUShape::Cube, FVector(32.f, 10.f, -18.f), FVector(10.f, 14.f, 9.f), ESUMat::CarDark, FRotator(0.f, -10.f, 0.f));
	HeldFlashlightModel->SetVisibility(false);
	HeldCameraModel->SetVisibility(false);
}

// ------------------------------------------------------------------------------------ input

void ASUCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	ASUPlayerController* PC = Cast<ASUPlayerController>(GetController());
	if (!EIC || !PC)
	{
		return;
	}
	USUInputConfig* C = PC->GetInputConfig();
	EIC->BindAction(C->Move, ETriggerEvent::Triggered, this, &ASUCharacter::OnMove);
	EIC->BindAction(C->Look, ETriggerEvent::Triggered, this, &ASUCharacter::OnLook);
	EIC->BindAction(C->LookGamepad, ETriggerEvent::Triggered, this, &ASUCharacter::OnLookGamepad);
	EIC->BindAction(C->Jump, ETriggerEvent::Started, this, &ASUCharacter::OnJump);
	EIC->BindAction(C->Sprint, ETriggerEvent::Started, this, &ASUCharacter::OnSprintStart);
	EIC->BindAction(C->Sprint, ETriggerEvent::Completed, this, &ASUCharacter::OnSprintStop);
	EIC->BindAction(C->Crouch, ETriggerEvent::Started, this, &ASUCharacter::OnCrouchStart);
	EIC->BindAction(C->Crouch, ETriggerEvent::Completed, this, &ASUCharacter::OnCrouchStop);
	EIC->BindAction(C->Interact, ETriggerEvent::Started, this, &ASUCharacter::OnInteract);
	EIC->BindAction(C->InteractAlt, ETriggerEvent::Started, this, &ASUCharacter::OnInteractAlt);
	EIC->BindAction(C->Flashlight, ETriggerEvent::Started, this, &ASUCharacter::OnFlashlight);
	EIC->BindAction(C->CameraToggle, ETriggerEvent::Started, this, &ASUCharacter::OnCameraToggle);
	EIC->BindAction(C->Aim, ETriggerEvent::Started, this, &ASUCharacter::OnAimStart);
	EIC->BindAction(C->Aim, ETriggerEvent::Completed, this, &ASUCharacter::OnAimStop);
	EIC->BindAction(C->Shutter, ETriggerEvent::Started, this, &ASUCharacter::OnShutter);
	EIC->BindAction(C->Zoom, ETriggerEvent::Triggered, this, &ASUCharacter::OnZoom);
	EIC->BindAction(C->CameraFlash, ETriggerEvent::Started, this, &ASUCharacter::OnCameraFlash);

	EIC->BindAction(C->Throttle, ETriggerEvent::Triggered, this, &ASUCharacter::OnThrottle);
	EIC->BindAction(C->Throttle, ETriggerEvent::Completed, this, &ASUCharacter::OnThrottleStop);
	EIC->BindAction(C->Brake, ETriggerEvent::Triggered, this, &ASUCharacter::OnBrake);
	EIC->BindAction(C->Brake, ETriggerEvent::Completed, this, &ASUCharacter::OnBrakeStop);
	EIC->BindAction(C->Steer, ETriggerEvent::Triggered, this, &ASUCharacter::OnSteer);
	EIC->BindAction(C->Steer, ETriggerEvent::Completed, this, &ASUCharacter::OnSteerStop);
	EIC->BindAction(C->Handbrake, ETriggerEvent::Started, this, &ASUCharacter::OnHandbrakeStart);
	EIC->BindAction(C->Handbrake, ETriggerEvent::Completed, this, &ASUCharacter::OnHandbrakeStop);
	EIC->BindAction(C->Engine, ETriggerEvent::Started, this, &ASUCharacter::OnEngine);
	EIC->BindAction(C->Lights, ETriggerEvent::Started, this, &ASUCharacter::OnLights);
	EIC->BindAction(C->HighBeam, ETriggerEvent::Started, this, &ASUCharacter::OnHighBeam);
	EIC->BindAction(C->Horn, ETriggerEvent::Started, this, &ASUCharacter::OnHornStart);
	EIC->BindAction(C->Horn, ETriggerEvent::Completed, this, &ASUCharacter::OnHornStop);
	EIC->BindAction(C->BlinkLeft, ETriggerEvent::Started, this, &ASUCharacter::OnBlinkLeft);
	EIC->BindAction(C->BlinkRight, ETriggerEvent::Started, this, &ASUCharacter::OnBlinkRight);
	EIC->BindAction(C->Hazard, ETriggerEvent::Started, this, &ASUCharacter::OnHazard);
	EIC->BindAction(C->Wipers, ETriggerEvent::Started, this, &ASUCharacter::OnWipers);
	EIC->BindAction(C->RadioPower, ETriggerEvent::Started, this, &ASUCharacter::OnRadioPower);
	EIC->BindAction(C->RadioNext, ETriggerEvent::Started, this, &ASUCharacter::OnRadioNext);
	EIC->BindAction(C->RadioPrev, ETriggerEvent::Started, this, &ASUCharacter::OnRadioPrev);
	EIC->BindAction(C->RadioVolume, ETriggerEvent::Triggered, this, &ASUCharacter::OnRadioVolume);
}

void ASUCharacter::OnMove(const FInputActionValue& Value)
{
	if (bDead || IsSeated())
	{
		return;
	}
	MoveInput = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), MoveInput.Y);
	AddMovementInput(GetActorRightVector(), MoveInput.X);
}

void ASUCharacter::OnLook(const FInputActionValue& Value)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || bDead)
	{
		return;
	}
	const FVector2D V = Value.Get<FVector2D>();
	const USUGameUserSettings* S = USUGameUserSettings::Get();
	float Sens = 0.075f * (S ? S->MouseSensitivity : 1.f);
	if (PhotoCamera && PhotoCamera->IsAiming())
	{
		Sens /= FMath::Max(1.f, PhotoCamera->GetZoom());
	}
	const float Invert = (S && S->bInvertY) ? -1.f : 1.f;
	FRotator R = PC->GetControlRotation();
	R.Yaw += V.X * Sens;
	R.Pitch = FMath::ClampAngle(static_cast<float>(FRotator::NormalizeAxis(R.Pitch) + V.Y * Sens * Invert), -85.f, 85.f);
	R.Roll = 0.f;
	PC->SetControlRotation(R);
}

void ASUCharacter::OnLookGamepad(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	const float Dt = GetWorld()->GetDeltaSeconds();
	// 140 deg/s at full deflection, converted to the mouse scale used in OnLook
	const float Scale = 140.f * Dt / 0.075f;
	OnLook(FInputActionValue(FVector2D(V.X * Scale, V.Y * Scale)));
}

void ASUCharacter::OnJump(const FInputActionValue& Value)
{
	if (!bDead && !IsSeated())
	{
		Jump();
	}
}

void ASUCharacter::ApplyMovementSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = bSprinting ? 390.f : 170.f;
}

void ASUCharacter::OnSprintStart(const FInputActionValue& Value)
{
	bSprinting = true;
	ApplyMovementSpeed();
	ServerSetSprint(true);
}

void ASUCharacter::OnSprintStop(const FInputActionValue& Value)
{
	bSprinting = false;
	ApplyMovementSpeed();
	ServerSetSprint(false);
}

void ASUCharacter::ServerSetSprint_Implementation(bool bInSprint)
{
	bSprinting = bInSprint;
	ApplyMovementSpeed();
}

void ASUCharacter::OnCrouchStart(const FInputActionValue& Value)
{
	if (IsSeated() || bDead)
	{
		return;
	}
	const USUGameUserSettings* S = USUGameUserSettings::Get();
	if (S && S->bToggleCrouch)
	{
		if (bIsCrouched)
		{
			UnCrouch();
		}
		else
		{
			Crouch();
		}
	}
	else
	{
		Crouch();
	}
}

void ASUCharacter::OnCrouchStop(const FInputActionValue& Value)
{
	const USUGameUserSettings* S = USUGameUserSettings::Get();
	if (!S || !S->bToggleCrouch)
	{
		UnCrouch();
	}
}

void ASUCharacter::OnInteract(const FInputActionValue& Value)
{
	if (bDead)
	{
		return;
	}
	if (IsSeated())
	{
		ServerExitVehicle();
		return;
	}
	if (Interaction)
	{
		Interaction->TryInteract(ESUInteractOption::Primary);
	}
}

void ASUCharacter::OnInteractAlt(const FInputActionValue& Value)
{
	if (!bDead && Interaction && !IsSeated())
	{
		Interaction->TryInteract(ESUInteractOption::Secondary);
	}
}

void ASUCharacter::OnFlashlight(const FInputActionValue& Value)
{
	if (bDead || !Inventory)
	{
		return;
	}
	ASUPlayerController* PC = Cast<ASUPlayerController>(GetController());
	if (!Inventory->HasItem(SUItem::Flashlight))
	{
		if (PC)
		{
			PC->Notify(SUText(TEXT("Keine Taschenlampe dabei.")), FLinearColor(1.f, 0.8f, 0.5f));
		}
		return;
	}
	if (!bFlashlightOn && Inventory->GetCharge(SUItem::Flashlight) <= 0.f)
	{
		if (PC)
		{
			PC->Notify(SUText(TEXT("Batterie leer – Batterien im Inventar benutzen.")), FLinearColor(1.f, 0.6f, 0.4f));
		}
		return;
	}
	bFlashlightOn = !bFlashlightOn;
	OnRep_Flashlight();
	ServerSetFlashlight(bFlashlightOn);
}

void ASUCharacter::OnCameraToggle(const FInputActionValue& Value)
{
	if (bDead || !Inventory)
	{
		return;
	}
	if (!Inventory->HasItem(SUItem::Camera))
	{
		if (ASUPlayerController* PC = Cast<ASUPlayerController>(GetController()))
		{
			PC->Notify(SUText(TEXT("Keine Kamera dabei.")), FLinearColor(1.f, 0.8f, 0.5f));
		}
		return;
	}
	if (HeldItem == ESUHeldItem::Nozzle)
	{
		return;
	}
	const ESUHeldItem NewItem = HeldItem == ESUHeldItem::Camera ? ESUHeldItem::None : ESUHeldItem::Camera;
	HeldItem = NewItem;
	OnRep_HeldItem();
	ServerSetHeldItem(NewItem);
}

void ASUCharacter::OnAimStart(const FInputActionValue& Value)
{
	if (PhotoCamera && HeldItem == ESUHeldItem::Camera && !bDead)
	{
		PhotoCamera->SetAiming(true);
	}
}

void ASUCharacter::OnAimStop(const FInputActionValue& Value)
{
	if (PhotoCamera)
	{
		PhotoCamera->SetAiming(false);
	}
}

void ASUCharacter::OnShutter(const FInputActionValue& Value)
{
	if (PhotoCamera && HeldItem == ESUHeldItem::Camera && !bDead)
	{
		PhotoCamera->TakePhoto();
	}
}

void ASUCharacter::OnZoom(const FInputActionValue& Value)
{
	if (PhotoCamera && PhotoCamera->IsAiming())
	{
		PhotoCamera->AdjustZoom(Value.Get<float>());
	}
}

void ASUCharacter::OnCameraFlash(const FInputActionValue& Value)
{
	if (PhotoCamera && HeldItem == ESUHeldItem::Camera)
	{
		PhotoCamera->ToggleFlash();
	}
}

// ---- vehicle input (driver)

void ASUCharacter::OnThrottle(const FInputActionValue& Value) { InThrottleValue = FMath::Clamp(Value.Get<float>(), 0.f, 1.f); PushVehicleInput(false); }
void ASUCharacter::OnThrottleStop(const FInputActionValue& Value) { InThrottleValue = 0.f; PushVehicleInput(true); }
void ASUCharacter::OnBrake(const FInputActionValue& Value) { InBrakeValue = FMath::Clamp(Value.Get<float>(), 0.f, 1.f); PushVehicleInput(false); }
void ASUCharacter::OnBrakeStop(const FInputActionValue& Value) { InBrakeValue = 0.f; PushVehicleInput(true); }
void ASUCharacter::OnSteer(const FInputActionValue& Value) { InSteerValue = FMath::Clamp(Value.Get<float>(), -1.f, 1.f); PushVehicleInput(false); }
void ASUCharacter::OnSteerStop(const FInputActionValue& Value) { InSteerValue = 0.f; PushVehicleInput(true); }
void ASUCharacter::OnHandbrakeStart(const FInputActionValue& Value) { bInHandbrake = true; PushVehicleInput(true); }
void ASUCharacter::OnHandbrakeStop(const FInputActionValue& Value) { bInHandbrake = false; PushVehicleInput(true); }
void ASUCharacter::OnEngine(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::ToggleEngine, 0.f); }
void ASUCharacter::OnLights(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::ToggleLights, 0.f); }
void ASUCharacter::OnHighBeam(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::ToggleHighBeam, 0.f); }
void ASUCharacter::OnHornStart(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::HornOn, 0.f); }
void ASUCharacter::OnHornStop(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::HornOff, 0.f); }
void ASUCharacter::OnBlinkLeft(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::BlinkLeft, 0.f); }
void ASUCharacter::OnBlinkRight(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::BlinkRight, 0.f); }
void ASUCharacter::OnHazard(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::Hazard, 0.f); }
void ASUCharacter::OnWipers(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::Wipers, 0.f); }
void ASUCharacter::OnRadioPower(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::RadioPower, 0.f); }
void ASUCharacter::OnRadioNext(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::RadioNext, 0.f); }
void ASUCharacter::OnRadioPrev(const FInputActionValue& Value) { ServerVehicleCommand(ESUVehicleCommand::RadioPrev, 0.f); }

void ASUCharacter::OnRadioVolume(const FInputActionValue& Value)
{
	const float V = Value.Get<float>();
	if (FMath::Abs(V) > 0.01f)
	{
		ServerVehicleCommand(ESUVehicleCommand::RadioVolume, V > 0.f ? 0.1f : -0.1f);
	}
}

void ASUCharacter::PushVehicleInput(bool bForce)
{
	if (!IsDriver() || !Vehicle)
	{
		return;
	}
	// apply immediately on this machine (prediction), the server receives it below
	Vehicle->SetDriverInput(InThrottleValue, InBrakeValue, InSteerValue, bInHandbrake);
	bVehicleInputDirty = true;
	if (bForce)
	{
		VehicleInputSendTimer = 0.f;
	}
}

// ------------------------------------------------------------------------------------ tick

void ASUCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if ((Vehicle != nullptr) != bAppliedSeated)
	{
		ApplySeatedState();
	}

	if (IsLocallyControlled())
	{
		UpdateCameraEffects(DeltaSeconds);
		UpdateSeatedLook(DeltaSeconds);

		// angular look speed (camera shake rating for photos)
		const FRotator Now = GetControlRotation();
		const FRotator Delta = (Now - LastViewRotation).GetNormalized();
		LookAngularSpeed = FMath::Lerp(LookAngularSpeed, static_cast<float>(FMath::Sqrt(Delta.Yaw * Delta.Yaw + Delta.Pitch * Delta.Pitch)) / FMath::Max(DeltaSeconds, 1e-3f), 0.35f);
		LastViewRotation = Now;

		if (IsDriver())
		{
			// changes are sent at most 20x per second, and resent 4x per second (unreliable RPC)
			VehicleInputSendTimer -= DeltaSeconds;
			VehicleInputResendTimer -= DeltaSeconds;
			if ((bVehicleInputDirty && VehicleInputSendTimer <= 0.f) || VehicleInputResendTimer <= 0.f)
			{
				VehicleInputSendTimer = 0.05f;
				VehicleInputResendTimer = 0.25f;
				bVehicleInputDirty = false;
				if (!HasAuthority())
				{
					ServerVehicleInput(InThrottleValue, InBrakeValue, InSteerValue, bInHandbrake);
				}
			}
		}
	}
	else
	{
		// remote players: the camera (and the flashlight attached to it) follows their aim pitch
		FirstPersonCamera->SetWorldRotation(GetBaseAimRotation());
	}

	UpdateFootsteps(DeltaSeconds);
	UpdateFlashlight(DeltaSeconds);

	if (FlashBulbTime > 0.f)
	{
		FlashBulbTime -= DeltaSeconds;
		if (FlashBulbTime <= 0.f)
		{
			FlashBulb->SetVisibility(false);
		}
	}
}

void ASUCharacter::UpdateCameraEffects(float DeltaSeconds)
{
	const USUGameUserSettings* S = USUGameUserSettings::Get();
	const bool bAiming = PhotoCamera && PhotoCamera->IsAiming();
	if (!bAiming)
	{
		FirstPersonCamera->SetFieldOfView(FMath::FInterpTo(FirstPersonCamera->FieldOfView, S ? S->FieldOfView : 90.f, DeltaSeconds, 10.f));
	}
	const float BaseZ = IsSeated() ? 62.f : 68.f;
	FVector Offset(0.f, 0.f, BaseZ);
	const float Speed = GetVelocity().Size2D();
	if (S && S->bHeadBob && !IsSeated() && GetCharacterMovement()->IsMovingOnGround() && Speed > 30.f && !bAiming)
	{
		HeadBobTime += DeltaSeconds * (Speed > 300.f ? 13.f : 9.f);
		const float Amp = FMath::Clamp(Speed / 400.f, 0.f, 1.f);
		Offset.Y += FMath::Sin(HeadBobTime * 0.5f) * 0.9f * Amp;
		Offset.Z += FMath::Abs(FMath::Sin(HeadBobTime)) * 1.6f * Amp;
	}
	FirstPersonCamera->SetRelativeLocation(FMath::VInterpTo(FirstPersonCamera->GetRelativeLocation(), Offset, DeltaSeconds, 12.f));

	// held models
	if (HeldCameraModel)
	{
		HeldCameraModel->SetVisibility(HeldItem == ESUHeldItem::Camera && !bAiming);
	}
	if (HeldFlashlightModel)
	{
		HeldFlashlightModel->SetVisibility(bFlashlightOn && !bAiming && !IsSeated());
	}
}

void ASUCharacter::UpdateSeatedLook(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !Vehicle)
	{
		return;
	}
	// the view turns with the car; looking around is limited to what the head can do
	const float VehicleYaw = Vehicle->GetActorRotation().Yaw;
	const float Delta = FRotator::NormalizeAxis(VehicleYaw - LastVehicleYaw);
	LastVehicleYaw = VehicleYaw;
	FRotator R = PC->GetControlRotation();
	R.Yaw += Delta;
	const float Rel = FMath::Clamp(static_cast<float>(FRotator::NormalizeAxis(R.Yaw - VehicleYaw)), -140.f, 140.f);
	R.Yaw = VehicleYaw + Rel;
	R.Pitch = FMath::ClampAngle(static_cast<float>(FRotator::NormalizeAxis(R.Pitch)), -60.f, 50.f);
	PC->SetControlRotation(R);
}

ESUSurface ASUCharacter::DetectSurface() const
{
	FHitResult Hit;
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 40.f);
	FCollisionQueryParams Params(FName(TEXT("SUFootstep")), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.GetComponent())
	{
		const UPrimitiveComponent* C = Hit.GetComponent();
		if (C->ComponentHasTag(SUSurfaceTag::Asphalt)) { return ESUSurface::Asphalt; }
		if (C->ComponentHasTag(SUSurfaceTag::Gravel)) { return ESUSurface::Gravel; }
		if (C->ComponentHasTag(SUSurfaceTag::Wood)) { return ESUSurface::Wood; }
		if (C->ComponentHasTag(SUSurfaceTag::Concrete)) { return ESUSurface::Concrete; }
		if (C->ComponentHasTag(SUSurfaceTag::Metal)) { return ESUSurface::Metal; }
		if (C->ComponentHasTag(SUSurfaceTag::Dirt)) { return ESUSurface::Dirt; }
	}
	return ESUSurface::Grass;
}

void ASUCharacter::UpdateFootsteps(float DeltaSeconds)
{
	if (IsSeated() || bDead || !GetCharacterMovement()->IsMovingOnGround())
	{
		StepAccumulator = 0.f;
		return;
	}
	const float Speed = GetVelocity().Size2D();
	if (Speed < 40.f)
	{
		return;
	}
	StepAccumulator += Speed * DeltaSeconds;
	const float Stride = bIsCrouched ? 55.f : (Speed > 300.f ? 100.f : 72.f);
	if (StepAccumulator < Stride)
	{
		return;
	}
	StepAccumulator -= Stride;
	const ESUSurface Surface = DetectSurface();
	int32 Kind = 0;
	switch (Surface)
	{
	case ESUSurface::Gravel: Kind = 1; break;
	case ESUSurface::Wood: Kind = 2; break;
	case ESUSurface::Asphalt:
	case ESUSurface::Concrete: Kind = 3; break;
	case ESUSurface::Metal: Kind = 4; break;
	default: Kind = 0; break;
	}
	const float Strength = FMath::Clamp(Speed / 300.f, 0.35f, 1.2f) * (bIsCrouched ? 0.4f : 1.f);
	if (FootstepAudio)
	{
		FootstepAudio->TriggerSound(Kind, Strength);
	}
	if (HasAuthority())
	{
		USUEventSubsystem::Noise(this, GetActorLocation(), bIsCrouched ? 0.05f : (Speed > 300.f ? 0.45f : 0.18f), this);
	}
}

void ASUCharacter::UpdateFlashlight(float DeltaSeconds)
{
	if (!HasAuthority() || !bFlashlightOn || !Inventory)
	{
		return;
	}
	FlashlightDrainTimer += DeltaSeconds;
	if (FlashlightDrainTimer >= 1.f)
	{
		// one battery lasts about 12 minutes
		const float Charge = Inventory->GetCharge(SUItem::Flashlight) - FlashlightDrainTimer / 720.f;
		FlashlightDrainTimer = 0.f;
		Inventory->SetCharge(SUItem::Flashlight, Charge);
		if (Charge <= 0.f || !Inventory->HasItem(SUItem::Flashlight))
		{
			bFlashlightOn = false;
			OnRep_Flashlight();
			NotifyOwner(SUText(TEXT("Die Taschenlampe geht aus – Batterie leer.")), FLinearColor(1.f, 0.6f, 0.4f));
		}
	}
}

// ------------------------------------------------------------------------------------ state

FVector ASUCharacter::GetEyeLocation() const
{
	return FirstPersonCamera ? FirstPersonCamera->GetComponentLocation() : GetActorLocation();
}

FRotator ASUCharacter::GetViewRotationReplicated() const
{
	return IsLocallyControlled() ? GetControlRotation() : GetBaseAimRotation();
}

float ASUCharacter::GetFlashlightCharge() const
{
	return Inventory ? Inventory->GetCharge(SUItem::Flashlight) : 0.f;
}

FVector ASUCharacter::GetFlashlightDirection() const
{
	return Flashlight ? Flashlight->GetForwardVector() : GetActorForwardVector();
}

void ASUCharacter::OnRep_Flashlight()
{
	Flashlight->SetVisibility(bFlashlightOn && !bDead);
}

void ASUCharacter::OnRep_HeldItem()
{
	if (PhotoCamera && HeldItem != ESUHeldItem::Camera)
	{
		PhotoCamera->SetAiming(false);
	}
}

void ASUCharacter::OnRep_Health(float OldHealth)
{
	if (Health < OldHealth)
	{
		LastDamageTime = GetWorld()->GetTimeSeconds();
		if (ASUPlayerController* PC = Cast<ASUPlayerController>(GetController()))
		{
			if (ASUHUD* HUD = PC->GetSUHUD())
			{
				HUD->FlashDamage(FMath::Clamp((OldHealth - Health) / 40.f, 0.2f, 1.f));
			}
		}
	}
}

void ASUCharacter::OnRep_Dead()
{
	if (bDead)
	{
		Flashlight->SetVisibility(false);
		GetCharacterMovement()->DisableMovement();
		for (UStaticMeshComponent* P : BodyParts)
		{
			if (P)
			{
				P->SetRelativeRotation(P->GetRelativeRotation() + FRotator(0.f, 0.f, 80.f));
			}
		}
		if (IsLocallyControlled())
		{
			if (ASUPlayerController* PC = Cast<ASUPlayerController>(GetController()))
			{
				if (ASUHUD* HUD = PC->GetSUHUD())
				{
					HUD->StartFade(0.f, 1.f, 2.5f);
				}
			}
		}
	}
}

void ASUCharacter::OnRep_Seat()
{
	ApplySeatedState();
}

void ASUCharacter::ApplySeatedState()
{
	const bool bSeated = Vehicle != nullptr;
	if (bSeated)
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		bUseControllerRotationYaw = false;
		if (USceneComponent* Seat = Vehicle->GetSeatComponent(SeatIndex))
		{
			AttachToComponent(Seat, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		LastVehicleYaw = Vehicle->GetActorRotation().Yaw;
		for (UStaticMeshComponent* L : LegParts)
		{
			if (L)
			{
				L->SetVisibility(false);
			}
		}
		if (PhotoCamera)
		{
			PhotoCamera->SetAiming(false);
		}
	}
	else if (bAppliedSeated)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (!bDead)
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		bUseControllerRotationYaw = true;
		SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
		for (UStaticMeshComponent* L : LegParts)
		{
			if (L)
			{
				L->SetVisibility(true);
			}
		}
		InThrottleValue = InBrakeValue = InSteerValue = 0.f;
		bInHandbrake = false;
	}
	bAppliedSeated = bSeated;
}

// ------------------------------------------------------------------------------------ server API

void ASUCharacter::SetSeated(ASUVehicle* InVehicle, int32 InSeat)
{
	if (!HasAuthority())
	{
		return;
	}
	Vehicle = InVehicle;
	SeatIndex = InVehicle ? InSeat : INDEX_NONE;
	if (InVehicle && HeldItem == ESUHeldItem::Camera)
	{
		HeldItem = ESUHeldItem::None;
		OnRep_HeldItem();
	}
	ApplySeatedState();
}

void ASUCharacter::NotifyOwner(const FText& Text, FLinearColor Color)
{
	if (ASUPlayerController* PC = Cast<ASUPlayerController>(GetController()))
	{
		PC->ClientNotify(Text, Color);
	}
}

void ASUCharacter::ApplyDamageSU(float Amount, AActor* Causer)
{
	if (!HasAuthority() || bDead || Amount <= 0.f)
	{
		return;
	}
	const float Old = Health;
	Health = FMath::Max(0.f, Health - Amount);
	OnRep_Health(Old);
	if (Health <= 0.f)
	{
		Die();
	}
}

void ASUCharacter::Die()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	if (Vehicle)
	{
		Vehicle->RemoveOccupant(this, true);
	}
	if (HeldNozzlePump)
	{
		HeldNozzlePump->ReturnNozzle();
	}
	bFlashlightOn = false;
	OnRep_Dead();
	NotifyOwner(SUText(TEXT("Du verlierst das Bewusstsein …")), FLinearColor(1.f, 0.3f, 0.3f));
	if (ASUGameMode* GM = GetWorld()->GetAuthGameMode<ASUGameMode>())
	{
		GM->RequestRespawn(GetController(), 7.f);
	}
}

void ASUCharacter::HealFull()
{
	if (HasAuthority() && !bDead)
	{
		Health = 100.f;
	}
}

void ASUCharacter::SetHealthDirect(float InHealth)
{
	if (HasAuthority())
	{
		Health = FMath::Clamp(InHealth, 1.f, 100.f);
	}
}

void ASUCharacter::TeleportToBase(const FTransform& Where)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Vehicle)
	{
		Vehicle->RemoveOccupant(this, false);
	}
	SetActorLocationAndRotation(Where.GetLocation(), FRotator(0.f, Where.Rotator().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	ClientSetViewRotation(FRotator(0.f, Where.Rotator().Yaw, 0.f));
}

void ASUCharacter::ClientSetViewRotation_Implementation(FRotator NewRotation)
{
	if (Controller)
	{
		Controller->SetControlRotation(NewRotation);
	}
}

void ASUCharacter::ClientFadeFromBlack_Implementation(float Seconds)
{
	if (ASUPlayerController* PC = Cast<ASUPlayerController>(GetController()))
	{
		if (ASUHUD* HUD = PC->GetSUHUD())
		{
			HUD->StartFade(1.f, 0.f, Seconds);
		}
	}
}

void ASUCharacter::SetHeldNozzlePump(ASUFuelPump* Pump)
{
	if (!HasAuthority())
	{
		return;
	}
	HeldNozzlePump = Pump;
	if (Pump)
	{
		HeldItem = ESUHeldItem::Nozzle;
	}
	else if (HeldItem == ESUHeldItem::Nozzle)
	{
		HeldItem = ESUHeldItem::None;
	}
	OnRep_HeldItem();
}

void ASUCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	const float FallSpeed = -GetVelocity().Z;
	if (FootstepAudio)
	{
		FootstepAudio->TriggerSound(static_cast<int32>(DetectSurface() == ESUSurface::Wood ? 2 : 0), FMath::Clamp(FallSpeed / 500.f, 0.4f, 1.5f));
	}
	if (HasAuthority() && FallSpeed > 1100.f)
	{
		ApplyDamageSU((FallSpeed - 1100.f) * 0.12f, nullptr);
	}
}

// ------------------------------------------------------------------------------------ RPCs

void ASUCharacter::ServerInteract_Implementation(AActor* Target, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	if (bDead || !IsValid(Target))
	{
		return;
	}
	ISUInteractable* Interactable = Cast<ISUInteractable>(Target);
	if (!Interactable)
	{
		return;
	}
	// validate distance (cheap anti-cheat / desync guard)
	const FVector Eye = GetPawnViewLocation();
	const FBox Bounds = Component ? Component->Bounds.GetBox() : Target->GetComponentsBoundingBox(true);
	const float Dist = FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(Eye));
	if (Dist > Interactable->GetInteractionRange() + 120.f)
	{
		UE_LOG(LogSignalUnknown, Verbose, TEXT("Interaction rejected: too far (%.0f)"), Dist);
		return;
	}
	Interactable->Interact(this, Component, Option);
}

void ASUCharacter::ServerReportUIEvent_Implementation(FName EventName, FName Param)
{
	if (EventName == SUEvent::MapViewed)
	{
		USUEventSubsystem::Post(this, SUEvent::MapViewed, NAME_None, this);
	}
}

void ASUCharacter::ServerSetFlashlight_Implementation(bool bOn)
{
	if (bOn && (!Inventory->HasItem(SUItem::Flashlight) || Inventory->GetCharge(SUItem::Flashlight) <= 0.f))
	{
		bOn = false;
	}
	bFlashlightOn = bOn && !bDead;
	OnRep_Flashlight();
}

void ASUCharacter::ServerSetHeldItem_Implementation(ESUHeldItem Item)
{
	if (Item == ESUHeldItem::Nozzle || HeldItem == ESUHeldItem::Nozzle)
	{
		return; // the nozzle is controlled by the pump
	}
	if (Item == ESUHeldItem::Camera && (!Inventory->HasItem(SUItem::Camera) || IsSeated()))
	{
		Item = ESUHeldItem::None;
	}
	HeldItem = Item;
	OnRep_HeldItem();
}

void ASUCharacter::ServerUseItem_Implementation(FName ItemId)
{
	const FSUItemDef* Def = FSUItemDatabase::Find(ItemId);
	if (!Def || !Inventory->HasItem(ItemId) || bDead)
	{
		return;
	}
	if (ItemId == SUItem::Battery)
	{
		bool bUsed = false;
		for (const FName Device : {SUItem::Flashlight, SUItem::Camera})
		{
			if (Inventory->HasItem(Device) && Inventory->GetCharge(Device) < 0.98f)
			{
				Inventory->SetCharge(Device, 1.f);
				bUsed = true;
			}
		}
		if (bUsed)
		{
			Inventory->RemoveItem(ItemId, 1);
			NotifyOwner(SUText(TEXT("Batterien gewechselt.")));
		}
		else
		{
			NotifyOwner(SUText(TEXT("Alle Geräte sind voll geladen.")));
		}
		return;
	}
	if (Def->HealthRestore > 0.f)
	{
		Health = FMath::Min(100.f, Health + Def->HealthRestore);
		Inventory->RemoveItem(ItemId, 1);
		NotifyOwner(FText::FromString(FString::Printf(TEXT("%s benutzt."), *Def->Name.ToString())));
		return;
	}
	if (ItemId == SUItem::Toolkit)
	{
		NotifyOwner(SUText(TEXT("Den Werkzeugkasten an der Motorhaube des Wagens benutzen [Q].")));
		return;
	}
	NotifyOwner(SUText(TEXT("Das kann man nicht benutzen.")));
}

void ASUCharacter::ServerDropItem_Implementation(FName ItemId)
{
	const FSUItemDef* Def = FSUItemDatabase::Find(ItemId);
	if (!Def || Def->bQuestItem || IsSeated() || !Inventory->HasItem(ItemId))
	{
		return;
	}
	const float Charge = Inventory->GetCharge(ItemId);
	if (Inventory->RemoveItem(ItemId, 1) <= 0)
	{
		return;
	}
	if (ItemId == SUItem::Flashlight && !Inventory->HasItem(SUItem::Flashlight))
	{
		bFlashlightOn = false;
		OnRep_Flashlight();
	}
	if (ItemId == SUItem::Camera && HeldItem == ESUHeldItem::Camera)
	{
		HeldItem = ESUHeldItem::None;
		OnRep_HeldItem();
	}
	FVector Where = GetActorLocation() + GetActorForwardVector() * 70.f;
	FHitResult Hit;
	FCollisionQueryParams Params(FName(TEXT("SUDrop")), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Where + FVector(0.f, 0.f, 50.f), Where - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
	{
		Where = Hit.Location;
	}
	ASUPickupItem::SpawnItem(GetWorld(), ItemId, 1, Charge, FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), Where));
}

void ASUCharacter::ServerRegisterPhoto_Implementation(const FSUPhotoMeta& Meta)
{
	ASUPlayerState* PS = GetPlayerState<ASUPlayerState>();
	if (!PS || !Inventory->HasItem(SUItem::Camera))
	{
		return;
	}
	if (PS->CountPhotosOnCamera() >= GetDefault<USUGameplaySettings>()->CameraCapacity)
	{
		NotifyOwner(SUText(TEXT("Speicherkarte voll – Fotos in der Galerie [P] löschen.")), FLinearColor(1.f, 0.6f, 0.4f));
		return;
	}
	FSUPhotoMeta Stored = Meta;
	Stored.bSubmitted = false;
	if (const ASUMissionManager* MM = ASUMissionManager::Get(this))
	{
		Stored.MissionId = MM->GetPhase() == ESUMissionPhase::Active ? MM->GetActiveMissionId() : NAME_None;
	}
	PS->AddPhoto(Stored);
	Inventory->SetCharge(SUItem::Camera, Inventory->GetCharge(SUItem::Camera) - 0.012f);
	if (ASUMissionManager* MM = ASUMissionManager::Get(this))
	{
		MM->OnPhotoRegistered(PS, Stored);
	}
}

void ASUCharacter::ServerCameraFlash_Implementation()
{
	MulticastCameraFlash();
}

void ASUCharacter::MulticastCameraFlash_Implementation()
{
	if (!IsLocallyControlled()) // the photographer already flashed locally (same frame as the capture)
	{
		TriggerFlashBulb();
	}
}

void ASUCharacter::TriggerFlashBulb()
{
	FlashBulb->SetVisibility(true);
	FlashBulbTime = 0.09f;
}

void ASUCharacter::ServerExitVehicle_Implementation()
{
	if (Vehicle)
	{
		Vehicle->RequestExit(this);
	}
}

void ASUCharacter::ServerVehicleInput_Implementation(float InThrottle, float InBrake, float InSteer, bool bInHandbrakeValue)
{
	if (IsDriver() && Vehicle)
	{
		Vehicle->SetDriverInput(FMath::Clamp(InThrottle, 0.f, 1.f), FMath::Clamp(InBrake, 0.f, 1.f), FMath::Clamp(InSteer, -1.f, 1.f), bInHandbrakeValue);
	}
}

void ASUCharacter::ServerVehicleCommand_Implementation(ESUVehicleCommand Command, float Value)
{
	if (Vehicle)
	{
		Vehicle->HandleCommand(this, Command, Value);
	}
}

// ------------------------------------------------------------------------------------ voice

void ASUCharacter::SetupVoice()
{
	if (VoiceTalker || IsLocallyControlled() || GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_Standalone)
	{
		return;
	}
	APlayerState* PS = GetPlayerState();
	if (!PS)
	{
		return;
	}
	// Proximity voice: the engine VoIP stream of this player is played at their head with
	// distance attenuation, occlusion and the current reverb (see Audio/SUVoice).
	VoiceTalker = UVOIPTalker::CreateTalkerForPlayer(PS);
	if (VoiceTalker)
	{
		VoiceTalker->Settings.ComponentToAttachTo = FirstPersonCamera;
		VoiceTalker->Settings.AttenuationSettings = USUVoice::GetProximityAttenuation();
	}
}
