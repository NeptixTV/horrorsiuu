#include "Player/HHCharacter.h"
#include "Player/HHPlayerState.h"
#include "Player/HHCosmeticComponent.h"
#include "Player/HHInteractionComponent.h"
#include "Player/HHFootstepComponent.h"
#include "Player/HHCharacterAnimInstance.h"
#include "Player/HHPlayerController.h"
#include "Player/HHInventoryComponent.h"
#include "Vehicles/HHVanVehicle.h"
#include "Heist/HHHeistGameMode.h"
#include "Audio/HHAudioSubsystem.h"
#include "Core/HHGameData.h"
#include "Data/HHCharacterDefinition.h"
#include "Data/HHEquipmentDefinition.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Settings/HHGameUserSettings.h"
#include "Settings/HHInputSubsystem.h"
#include "HorrorHeist.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/HHLootDefinition.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HHCharacter"

AHHCharacter::AHHCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed;
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	Movement->SetCrouchedHalfHeight(58.f);
	Movement->JumpZVelocity = 360.f;
	Movement->AirControl = 0.25f;
	Movement->MaxAcceleration = 1600.f;
	Movement->BrakingDecelerationWalking = 1900.f;
	Movement->GroundFriction = 7.f;
	Movement->bOrientRotationToMovement = false;
	Movement->bCanWalkOffLedgesWhenCrouching = true;

	// Mesh pivot at the feet, facing +Y in asset space (Blender -Y forward) -> rotate to +X.
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	GetMesh()->bCastHiddenShadow = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(EyeForward, 0.f, StandingEyeZ));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(90.f);

	ThirdPersonBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonBoom"));
	ThirdPersonBoom->SetupAttachment(GetCapsuleComponent());
	ThirdPersonBoom->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
	ThirdPersonBoom->TargetArmLength = 230.f;
	ThirdPersonBoom->SocketOffset = FVector(0.f, 42.f, 8.f);
	ThirdPersonBoom->bUsePawnControlRotation = true;
	ThirdPersonBoom->bDoCollisionTest = true;
	ThirdPersonBoom->ProbeSize = 12.f;
	ThirdPersonBoom->bEnableCameraLag = true;
	ThirdPersonBoom->CameraLagSpeed = 14.f;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(ThirdPersonBoom, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;
	ThirdPersonCamera->SetAutoActivate(false);

	FlashlightPivot = CreateDefaultSubobject<USceneComponent>(TEXT("FlashlightPivot"));
	FlashlightPivot->SetupAttachment(GetCapsuleComponent());
	FlashlightPivot->SetRelativeLocation(FVector(14.f, 16.f, 50.f));

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(FlashlightPivot);
	Flashlight->IntensityUnits = ELightUnits::Candelas;
	Flashlight->Intensity = 1600.f;
	Flashlight->AttenuationRadius = 2000.f;
	Flashlight->InnerConeAngle = 11.f;
	Flashlight->OuterConeAngle = 25.f;
	Flashlight->LightColor = FColor(255, 232, 200);
	Flashlight->VolumetricScatteringIntensity = 0.7f;
	Flashlight->SourceRadius = 1.5f;
	Flashlight->SetVisibility(false);

	HeldItem = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldItem"));
	HeldItem->SetupAttachment(FlashlightPivot);
	HeldItem->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldItem->SetGenerateOverlapEvents(false);
	HeldItem->SetCastShadow(true);
	HeldItem->SetVisibility(false);

	// Only used if the body mesh is missing (content not built): a plain stand-in so the
	// player is never invisible. The engine's basic shapes always exist.
	FallbackBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackBody"));
	FallbackBody->SetupAttachment(GetCapsuleComponent());
	FallbackBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallbackBody->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.75f));
	FallbackBody->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		FallbackBody->SetStaticMesh(Cylinder.Object);
	}

	Cosmetics = CreateDefaultSubobject<UHHCosmeticComponent>(TEXT("Cosmetics"));
	Interaction = CreateDefaultSubobject<UHHInteractionComponent>(TEXT("Interaction"));
	Footsteps = CreateDefaultSubobject<UHHFootstepComponent>(TEXT("Footsteps"));

	// Low on the floor around the hips of a body lying there; only the interaction trace hits it.
	ReviveZone = CreateDefaultSubobject<USphereComponent>(TEXT("ReviveZone"));
	ReviveZone->SetupAttachment(GetCapsuleComponent());
	ReviveZone->InitSphereRadius(60.f);
	ReviveZone->SetRelativeLocation(FVector(0.f, 0.f, -30.f));
	ReviveZone->SetCollisionObjectType(ECC_WorldDynamic);
	ReviveZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	ReviveZone->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ReviveZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReviveZone->SetGenerateOverlapEvents(false);
	ReviveZone->SetCanEverAffectNavigation(false);
	ReviveZone->CanCharacterStepUpOn = ECB_No;
}

void AHHCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AHHCharacter, bFlashlightOn, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AHHCharacter, bSprinting, COND_SkipOwner);
	DOREPLIFETIME(AHHCharacter, Vehicle);
	DOREPLIFETIME(AHHCharacter, SeatIndex);
	DOREPLIFETIME(AHHCharacter, bArrested);
	DOREPLIFETIME(AHHCharacter, BodyPose);
	DOREPLIFETIME(AHHCharacter, BleedOutAt);
}

void AHHCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyCharacterDefinition();
	BindToPlayerState();
	UpdateLocalVisibility();

	CameraZ = StandingEyeZ;
	CameraTargetZ = StandingEyeZ;

	SettingsHandle = UHHGameUserSettings::OnSettingsApplied().AddUObject(this, &AHHCharacter::HandleSettingsApplied);
	HandleSettingsApplied();
}

void AHHCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UHHGameUserSettings::OnSettingsApplied().Remove(SettingsHandle);
	UnbindFromPlayerState();
	Super::EndPlay(EndPlayReason);
}

void AHHCharacter::HandleSettingsApplied()
{
	if (const UHHGameUserSettings* Settings = UHHGameUserSettings::Get())
	{
		const float Fov = FMath::Clamp(Settings->FieldOfView, 70.f, 110.f);
		FirstPersonCamera->SetFieldOfView(Fov);
		ThirdPersonCamera->SetFieldOfView(Fov);
	}
}

void AHHCharacter::ReportMissingBody(const FString& Reason)
{
	FallbackBody->SetVisibility(true);
	if (bReportedMissingBody)
	{
		return;
	}
	bReportedMissingBody = true;
	UE_LOG(LogHorrorHeist, Error, TEXT("%s has no body: %s Showing a stand-in. Fix: 'The Quiet Job > Build / Rebuild All Content' in the editor."), *GetName(), *Reason);
	if (GEngine && IsLocallyControlled())
	{
		GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Orange, FString::Printf(TEXT("The Quiet Job: character body missing (%s). Run 'The Quiet Job > Build / Rebuild All Content'."), *Reason));
	}
}

void AHHCharacter::ApplyCharacterDefinition()
{
	const UHHGameData* GameData = UHHGameData::Get(this);
	if (!GameData)
	{
		ReportMissingBody(TEXT("DA_GameData was not found (Project Settings > Horror Heist > Game Data)."));
		return;
	}
	const UHHCharacterDefinition* Definition = GameData->DefaultCharacter.LoadSynchronous();
	if (!Definition)
	{
		ReportMissingBody(TEXT("DA_GameData.DefaultCharacter is empty."));
		return;
	}

	USkeletalMeshComponent* Body = GetMesh();
	if (USkeletalMesh* BodyMesh = Definition->BodyMesh.LoadSynchronous())
	{
		Body->SetSkeletalMeshAsset(BodyMesh);
		FallbackBody->SetVisibility(false);
	}
	else
	{
		ReportMissingBody(TEXT("DA_Character_Default.BodyMesh (SK_HH_Body) could not be loaded."));
	}

	Body->SetRelativeRotation(FRotator(0.f, Definition->MeshYawOffset, 0.f));
	BaseRotationOffset = Body->GetRelativeRotation().Quaternion();
	BaseTranslationOffset = Body->GetRelativeLocation();

	if (UClass* AnimOverride = Definition->AnimClassOverride.LoadSynchronous())
	{
		Body->SetAnimInstanceClass(AnimOverride);
	}
	else
	{
		Body->SetAnimInstanceClass(UHHCharacterAnimInstance::StaticClass());
		if (UHHCharacterAnimInstance* AnimInstance = Cast<UHHCharacterAnimInstance>(Body->GetAnimInstance()))
		{
			AnimInstance->InitializeFromDefinition(Definition);
		}
	}

	Cosmetics->SetBodyMesh(Body);
}

// ---------------------------------------------------------------------------------------
// Player state binding (appearance)

void AHHCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindToPlayerState();
	UpdateLocalVisibility();
}

void AHHCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindToPlayerState();
	UpdateLocalVisibility();
}

void AHHCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	UpdateLocalVisibility();
}

void AHHCharacter::BindToPlayerState()
{
	AHHPlayerState* State = GetPlayerState<AHHPlayerState>();
	if (State == BoundPlayerState.Get())
	{
		return;
	}
	UnbindFromPlayerState();
	if (!State)
	{
		return;
	}

	BoundPlayerState = State;
	CosmeticsHandle = State->OnCosmeticsChanged.AddUObject(this, &AHHCharacter::HandleCosmeticsChanged);
	EquipmentHandle = State->OnEquipmentChanged.AddUObject(this, &AHHCharacter::HandleEquipmentChanged);
	if (UHHInventoryComponent* Inventory = State->GetInventory())
	{
		InventoryHandle = Inventory->OnInventoryChanged.AddUObject(this, &AHHCharacter::HandleInventoryChanged);
	}
	HandleCosmeticsChanged(State);
	HandleEquipmentChanged(State);
	HandleInventoryChanged(State->GetInventory());
}

void AHHCharacter::UnbindFromPlayerState()
{
	if (AHHPlayerState* State = BoundPlayerState.Get())
	{
		State->OnCosmeticsChanged.Remove(CosmeticsHandle);
		State->OnEquipmentChanged.Remove(EquipmentHandle);
		if (UHHInventoryComponent* Inventory = State->GetInventory())
		{
			Inventory->OnInventoryChanged.Remove(InventoryHandle);
		}
	}
	BoundPlayerState.Reset();
}

void AHHCharacter::HandleCosmeticsChanged(AHHPlayerState* State)
{
	if (State)
	{
		Cosmetics->ApplyLoadout(State->GetCosmetics());
		UpdateLocalVisibility();
	}
}

void AHHCharacter::HandleEquipmentChanged(AHHPlayerState* State)
{
	UpdateFlashlightFromLoadout();
	UpdateHeldItem();
}

void AHHCharacter::HandleInventoryChanged(UHHInventoryComponent* Inventory)
{
	UpdateFlashlightFromLoadout();
	UpdateHeldItem();
	// Dropped the only light: it goes dark.
	if (bFlashlightOn && GetUsableLightId().IsNone() && HasAuthority())
	{
		bFlashlightOn = false;
		OnRep_Flashlight();
	}
}

FName AHHCharacter::GetUsableLightId() const
{
	const AHHPlayerState* State = BoundPlayerState.Get();
	if (!State)
	{
		return NAME_None;
	}
	const UHHInventoryComponent* Inventory = State->GetInventory();
	if (Inventory && Inventory->IsEnabled())
	{
		const UHHEquipmentDefinition* Light = Inventory->FindTool(TEXT("Light"));
		return Light ? Light->GetItemId() : NAME_None;
	}
	return State->GetEquipment().Get(EHHEquipmentSlot::Light);
}

void AHHCharacter::UpdateHeldItem()
{
	UStaticMesh* MeshToHold = nullptr;
	bool bLoot = false;
	bool bHeavy = false;
	bCarryingHeavy = false;

	const AHHPlayerState* State = BoundPlayerState.Get();
	const UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	if (Inventory && Inventory->IsEnabled())
	{
		// On a job: whatever is selected in the hotbar.
		const FHHInventorySlot& Slot = Inventory->GetSelected();
		MeshToHold = Inventory->GetSlotMesh(Inventory->GetSelectedIndex());
		bLoot = Slot.IsLoot();
		const UHHLootDefinition* Loot = bLoot && Registry ? Registry->FindLoot(Slot.ItemId) : nullptr;
		bHeavy = Loot && Loot->bHeavy;
		bCarryingHeavy = Inventory->IsCarryingHeavy();
	}
	else if (bFlashlightOn && Registry)
	{
		// Hideout / town: the torch is in the hand while it is on.
		if (const UHHEquipmentDefinition* Light = Registry->FindEquipment(GetUsableLightId()))
		{
			MeshToHold = Light->Mesh.LoadSynchronous();
		}
	}

	HeldItem->SetStaticMesh(MeshToHold);
	HeldItem->SetVisibility(MeshToHold != nullptr);
	if (bLoot)
	{
		// Loot is modelled facing -Y (Blender); turn its front towards the player. Bulky
		// things are carried low and centred, small ones sit in the hand.
		HeldItem->SetRelativeLocationAndRotation(bHeavy ? FVector(26.f, -16.f, -34.f) : FVector(14.f, -1.f, -7.f), FRotator(0.f, 90.f, 0.f));
	}
	else
	{
		// Equipment is modelled along +X (torch lens forward), its axis ~2.5 cm above the origin.
		HeldItem->SetRelativeLocationAndRotation(FVector(4.f, 0.f, -2.5f), FRotator::ZeroRotator);
	}
	if (MeshToHold)
	{
		UpdateFlashlightAim();
	}
	ApplyMovementSpeed();
}

bool AHHCharacter::GetHandTarget(FVector& OutWorldLocation, float& OutAlpha) const
{
	OutWorldLocation = HeldItem->GetComponentLocation();
	OutAlpha = HandAlpha;
	return HandAlpha > 0.01f;
}

void AHHCharacter::UpdateFlashlightFromLoadout()
{
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(this);
	const UHHEquipmentDefinition* Light = Registry ? Registry->FindEquipment(GetUsableLightId()) : nullptr;
	if (!Light)
	{
		return;
	}

	// Beam is data driven: tweak the item asset, not this code.
	Flashlight->SetIntensity(Light->GetStat(TEXT("Intensity"), 1600.f));
	Flashlight->SetAttenuationRadius(Light->GetStat(TEXT("Range"), 2000.f));
	const float Cone = Light->GetStat(TEXT("Cone"), 25.f);
	Flashlight->SetOuterConeAngle(Cone);
	Flashlight->SetInnerConeAngle(Cone * 0.45f);
	const float Warmth = FMath::Clamp(Light->GetStat(TEXT("Warmth"), 0.6f), 0.f, 1.f);
	Flashlight->SetLightColor(FMath::Lerp(FLinearColor(0.86f, 0.92f, 1.f), FLinearColor(1.f, 0.78f, 0.55f), Warmth));
}

void AHHCharacter::UpdateLocalVisibility()
{
	// True first person: while the local player looks through these eyes, only the head is
	// collapsed (anim instance) and head-worn cosmetics are hidden; the rest of the body stays
	// visible. Looking through a station camera or in third person shows everything.
	bool bFirstPersonView = false;
	if (IsLocallyControlled() && !bThirdPerson)
	{
		const APlayerController* PC = Cast<APlayerController>(GetController());
		const APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager.Get() : nullptr;
		const AActor* ViewTarget = CameraManager ? CameraManager->GetViewTarget() : this;
		const AActor* Pending = CameraManager ? CameraManager->PendingViewTarget.Target.Get() : nullptr;
		bFirstPersonView = ViewTarget == this && (Pending == nullptr || Pending == this);
	}
	else if (!IsLocallyControlled())
	{
		// A dead crewmate on this machine watches through our eyes.
		bFirstPersonView = IsViewedByLocalSpectator();
	}

	bHeadHidden = bFirstPersonView;
	GetMesh()->SetOwnerNoSee(false);
	Cosmetics->SetHiddenForOwner(bFirstPersonView);
	GetMesh()->SetVisibility(!bLocallyHidden, true);
	FallbackBody->SetOwnerNoSee(bFirstPersonView);
}

bool AHHCharacter::IsViewedByLocalSpectator() const
{
	const UWorld* World = GetWorld();
	const APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
	if (!LocalPC || !LocalPC->IsLocalController() || LocalPC->GetPawn() == this || !LocalPC->PlayerCameraManager)
	{
		return false;
	}
	return LocalPC->PlayerCameraManager->GetViewTarget() == this;
}

void AHHCharacter::SetLocallyHidden(bool bInHidden)
{
	bLocallyHidden = bInHidden;
	UpdateLocalVisibility();
}

void AHHCharacter::SetThirdPerson(bool bEnable)
{
	bThirdPerson = bEnable;
	FirstPersonCamera->SetActive(!bEnable);
	ThirdPersonCamera->SetActive(bEnable);
	UpdateLocalVisibility();
}

void AHHCharacter::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = FirstPersonCamera->GetComponentLocation();
	OutRotation = GetBaseAimRotation();
}

// ---------------------------------------------------------------------------------------
// Input

void AHHCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const UHHInputSubsystem* Actions = UHHInputSubsystem::Get(Cast<APlayerController>(GetController()));
	if (!Input || !Actions)
	{
		return;
	}

	Input->BindAction(Actions->GetAction(EHHInputAction::Move), ETriggerEvent::Triggered, this, &AHHCharacter::Input_Move);
	Input->BindAction(Actions->GetAction(EHHInputAction::Move), ETriggerEvent::Completed, this, &AHHCharacter::Input_MoveCompleted);
	Input->BindAction(Actions->GetAction(EHHInputAction::Horn), ETriggerEvent::Started, this, &AHHCharacter::Input_Horn);
	Input->BindAction(Actions->GetAction(EHHInputAction::Look), ETriggerEvent::Triggered, this, &AHHCharacter::Input_Look);
	Input->BindAction(Actions->GetAction(EHHInputAction::LookGamepad), ETriggerEvent::Triggered, this, &AHHCharacter::Input_LookGamepad);
	Input->BindAction(Actions->GetAction(EHHInputAction::Jump), ETriggerEvent::Started, this, &AHHCharacter::Input_JumpStarted);
	Input->BindAction(Actions->GetAction(EHHInputAction::Jump), ETriggerEvent::Completed, this, &AHHCharacter::Input_JumpCompleted);
	Input->BindAction(Actions->GetAction(EHHInputAction::Sprint), ETriggerEvent::Started, this, &AHHCharacter::Input_SprintStarted);
	Input->BindAction(Actions->GetAction(EHHInputAction::Sprint), ETriggerEvent::Completed, this, &AHHCharacter::Input_SprintCompleted);
	Input->BindAction(Actions->GetAction(EHHInputAction::Crouch), ETriggerEvent::Started, this, &AHHCharacter::Input_CrouchStarted);
	Input->BindAction(Actions->GetAction(EHHInputAction::Crouch), ETriggerEvent::Completed, this, &AHHCharacter::Input_CrouchCompleted);
	Input->BindAction(Actions->GetAction(EHHInputAction::Interact), ETriggerEvent::Started, this, &AHHCharacter::Input_Interact);
	Input->BindAction(Actions->GetAction(EHHInputAction::Interact), ETriggerEvent::Completed, this, &AHHCharacter::Input_InteractReleased);
	Input->BindAction(Actions->GetAction(EHHInputAction::Slot1), ETriggerEvent::Started, this, &AHHCharacter::Input_Slot1);
	Input->BindAction(Actions->GetAction(EHHInputAction::Slot2), ETriggerEvent::Started, this, &AHHCharacter::Input_Slot2);
	Input->BindAction(Actions->GetAction(EHHInputAction::Slot3), ETriggerEvent::Started, this, &AHHCharacter::Input_Slot3);
	Input->BindAction(Actions->GetAction(EHHInputAction::Slot4), ETriggerEvent::Started, this, &AHHCharacter::Input_Slot4);
	Input->BindAction(Actions->GetAction(EHHInputAction::Slot5), ETriggerEvent::Started, this, &AHHCharacter::Input_Slot5);
	Input->BindAction(Actions->GetAction(EHHInputAction::SlotNext), ETriggerEvent::Started, this, &AHHCharacter::Input_SlotNext);
	Input->BindAction(Actions->GetAction(EHHInputAction::SlotPrev), ETriggerEvent::Started, this, &AHHCharacter::Input_SlotPrev);
	Input->BindAction(Actions->GetAction(EHHInputAction::Drop), ETriggerEvent::Started, this, &AHHCharacter::Input_Drop);
	Input->BindAction(Actions->GetAction(EHHInputAction::Flashlight), ETriggerEvent::Started, this, &AHHCharacter::Input_Flashlight);
	Input->BindAction(Actions->GetAction(EHHInputAction::ToggleView), ETriggerEvent::Started, this, &AHHCharacter::Input_ToggleView);
}

void AHHCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (IsSeated())
	{
		// In the van the movement keys are the pedals and the wheel (driver only).
		if (IsDriving())
		{
			Vehicle->SetDriverInput(Axis);
		}
		return;
	}
	if (!Controller || Axis.IsNearlyZero() || bArrested || IsDead() || BodyPose == EHHBodyPose::Kneeling)
	{
		return;
	}
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix Basis(YawRotation);
	AddMovementInput(Basis.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(Basis.GetUnitAxis(EAxis::Y), Axis.X);
}

void AHHCharacter::Input_MoveCompleted(const FInputActionValue& Value)
{
	if (IsDriving())
	{
		Vehicle->SetDriverInput(FVector2D::ZeroVector);
	}
}

void AHHCharacter::Input_Horn()
{
	if (IsDriving())
	{
		Vehicle->ServerHorn();
	}
}

void AHHCharacter::ApplyLookDelta(float YawDegrees, float PitchDegrees)
{
	// Rotate the control rotation directly so the result does not depend on the legacy
	// input scale settings. The controller applies it in UpdateRotation() this same frame.
	if (!Controller)
	{
		return;
	}
	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Yaw = FRotator::NormalizeAxis(Rotation.Yaw + YawDegrees);
	Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch) + PitchDegrees, -84.f, 84.f);
	Rotation.Roll = 0.f;
	Controller->SetControlRotation(Rotation);
}

void AHHCharacter::Input_Look(const FInputActionValue& Value)
{
	// Mouse deltas arrive pre-scaled by the Mouse2D axis config (0.07), x2.5 matches the
	// feel of the engine templates at sensitivity 1.
	const FVector2D Delta = Value.Get<FVector2D>();
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	const float Sensitivity = 2.5f * (Settings ? FMath::Clamp(Settings->MouseSensitivity, 0.05f, 5.f) : 1.f);
	const float Invert = Settings && Settings->bInvertY ? -1.f : 1.f;
	ApplyLookDelta(Delta.X * Sensitivity, Delta.Y * Sensitivity * Invert);
}

void AHHCharacter::Input_LookGamepad(const FInputActionValue& Value)
{
	const FVector2D Stick = Value.Get<FVector2D>();
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	const float Sensitivity = Settings ? FMath::Clamp(Settings->ControllerSensitivity, 0.1f, 4.f) : 1.f;
	const float Invert = Settings && Settings->bInvertY ? -1.f : 1.f;
	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	// Square the stick for finer control near the centre.
	const FVector2D Shaped(Stick.X * FMath::Abs(Stick.X), Stick.Y * FMath::Abs(Stick.Y));
	ApplyLookDelta(Shaped.X * LookRateYaw * Sensitivity * DeltaTime, Shaped.Y * LookRatePitch * Sensitivity * DeltaTime * Invert);
}

void AHHCharacter::Input_JumpStarted()
{
	if (IsSeated() || bArrested)
	{
		if (IsDriving())
		{
			Vehicle->SetHandbrake(true);
		}
		return;
	}
	if (IsIncapacitated() || BodyPose == EHHBodyPose::Kneeling)
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
		return;
	}
	Jump();
}

void AHHCharacter::Input_JumpCompleted()
{
	if (IsDriving())
	{
		Vehicle->SetHandbrake(false);
	}
	StopJumping();
}

void AHHCharacter::Input_SprintStarted()
{
	if (IsSeated() || IsIncapacitated())
	{
		return;
	}
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (Settings && Settings->bToggleSprint)
	{
		SetSprinting(!bSprinting);
	}
	else
	{
		SetSprinting(true);
	}
}

void AHHCharacter::Input_SprintCompleted()
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (!Settings || !Settings->bToggleSprint)
	{
		SetSprinting(false);
	}
}

void AHHCharacter::Input_CrouchStarted()
{
	if (IsSeated() || IsIncapacitated())
	{
		return;
	}
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	const bool bToggle = !Settings || Settings->bToggleCrouch;
	if (bToggle && bIsCrouched)
	{
		UnCrouch();
		return;
	}
	if (bSprinting)
	{
		SetSprinting(false);
	}
	Crouch();
}

void AHHCharacter::Input_CrouchCompleted()
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (Settings && !Settings->bToggleCrouch && !IsIncapacitated())
	{
		UnCrouch();
	}
}

void AHHCharacter::Input_Interact()
{
	if (IsSeated())
	{
		ServerExitVehicle();
		return;
	}
	if (IsDead())
	{
		// Watching the crew: the interact key switches to the next one.
		if (AHHPlayerController* PC = Cast<AHHPlayerController>(GetController()))
		{
			PC->SpectateNext();
		}
		return;
	}
	if (IsDowned())
	{
		return;
	}
	Interaction->TryInteract();
}

void AHHCharacter::Input_InteractReleased()
{
	Interaction->StopInteract();
}

void AHHCharacter::SelectSlot(int32 Index)
{
	if (IsIncapacitated())
	{
		return;
	}
	const AHHPlayerState* State = BoundPlayerState.Get();
	UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	if (Inventory && Inventory->IsEnabled() && Index != Inventory->GetSelectedIndex())
	{
		Interaction->StopInteract();
		Inventory->RequestSelect(Index);
		if (UHHAudioSubsystem* Audio = UHHAudioSubsystem::Get(this))
		{
			Audio->PlayUI(EHHUISound::InventorySelect);
		}
	}
}

void AHHCharacter::Input_Slot1() { SelectSlot(0); }
void AHHCharacter::Input_Slot2() { SelectSlot(1); }
void AHHCharacter::Input_Slot3() { SelectSlot(2); }
void AHHCharacter::Input_Slot4() { SelectSlot(3); }
void AHHCharacter::Input_Slot5() { SelectSlot(4); }

void AHHCharacter::Input_SlotNext()
{
	const AHHPlayerState* State = BoundPlayerState.Get();
	const UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	if (Inventory)
	{
		SelectSlot((Inventory->GetSelectedIndex() + 1) % UHHInventoryComponent::NumSlots);
	}
}

void AHHCharacter::Input_SlotPrev()
{
	const AHHPlayerState* State = BoundPlayerState.Get();
	const UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	if (Inventory)
	{
		SelectSlot((Inventory->GetSelectedIndex() + UHHInventoryComponent::NumSlots - 1) % UHHInventoryComponent::NumSlots);
	}
}

void AHHCharacter::Input_Drop()
{
	if (IsSeated())
	{
		// G in the van: the garage door remote.
		if (IsDriving())
		{
			Vehicle->ServerGarageRemote();
		}
		return;
	}
	const AHHPlayerState* State = BoundPlayerState.Get();
	UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	if (Inventory && Inventory->IsEnabled() && !Inventory->GetSelected().IsEmpty() && !IsIncapacitated())
	{
		Interaction->StopInteract();
		Inventory->RequestDrop();
	}
}

void AHHCharacter::Input_Flashlight()
{
	if (IsSeated())
	{
		if (IsDriving())
		{
			Vehicle->ServerToggleHeadlights();
		}
		return;
	}
	if (IsDead())
	{
		return;
	}
	if (GetUsableLightId().IsNone())
	{
		if (AHHPlayerController* PC = Cast<AHHPlayerController>(GetController()))
		{
			const AHHPlayerState* State = BoundPlayerState.Get();
			const bool bOnJob = State && State->GetInventory() && State->GetInventory()->IsEnabled();
			PC->ClientNotify(bOnJob
				? LOCTEXT("NoLightJob", "You are not carrying a light.")
				: LOCTEXT("NoLight", "No light in your loadout. Pick one at the equipment locker."), EHHNotifyType::Warning);
		}
		return;
	}

	const bool bNewOn = !bFlashlightOn;
	bFlashlightOn = bNewOn;
	OnRep_Flashlight();
	if (!HasAuthority())
	{
		ServerSetFlashlight(bNewOn);
	}
}

void AHHCharacter::Input_ToggleView()
{
	if (IsSeated())
	{
		return;
	}
	SetThirdPerson(!bThirdPerson);
}

// ---------------------------------------------------------------------------------------
// The van and the police

void AHHCharacter::SetSeat(AHHVanVehicle* InVehicle, int32 InSeat)
{
	if (!HasAuthority())
	{
		return;
	}
	Vehicle = InVehicle;
	SeatIndex = InVehicle ? InSeat : INDEX_NONE;
	if (Vehicle)
	{
		AttachToComponent(Vehicle->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		SetActorRelativeLocation(Vehicle->GetSeatOffset(SeatIndex));
		SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	}
	OnRep_Seat();
	ForceNetUpdate();
}

void AHHCharacter::SetArrested(bool bInArrested)
{
	if (HasAuthority() && bArrested != bInArrested)
	{
		bArrested = bInArrested;
		OnRep_Seat();
		ForceNetUpdate();
	}
}

void AHHCharacter::ServerExitVehicle_Implementation()
{
	if (Vehicle)
	{
		Vehicle->Exit(this);
	}
}

void AHHCharacter::ClientExitedVehicle_Implementation(FVector Location, FRotator Rotation)
{
	if (IsSeated())
	{
		// The seat replication may arrive after this RPC: let go of the van right away.
		Vehicle = nullptr;
		SeatIndex = INDEX_NONE;
		ApplyBodyState();
	}
	SetActorLocationAndRotation(Location, FRotator(0.f, Rotation.Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (Controller)
	{
		Controller->SetControlRotation(FRotator(0.f, Rotation.Yaw, 0.f));
	}
}

void AHHCharacter::OnRep_Seat()
{
	ApplyBodyState();
}

void AHHCharacter::ApplyBodyState()
{
	const bool bStow = Vehicle != nullptr || bArrested;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Vehicle && GetAttachParentActor() != Vehicle)
	{
		AttachToComponent(Vehicle->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		SetActorRelativeLocation(Vehicle->GetSeatOffset(SeatIndex));
		SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	else if (!Vehicle && Cast<AHHVanVehicle>(GetAttachParentActor()))
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	if (bStow == bBodyStowed)
	{
		return;
	}
	bBodyStowed = bStow;

	if (bStow)
	{
		Interaction->StopInteract();
		if (bFlashlightOn)
		{
			bFlashlightOn = false;
			OnRep_Flashlight();
		}
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		Movement->SetComponentTickEnabled(false);
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
	}
	else
	{
		SetActorHiddenInGame(false);
		SetActorEnableCollision(true);
		Movement->SetComponentTickEnabled(true);
		Movement->SetMovementMode(MOVE_Walking);
	}
	UpdateLocalVisibility();
}

// ---------------------------------------------------------------------------------------
// Down, dead, back up

float AHHCharacter::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : (World ? static_cast<float>(World->GetTimeSeconds()) : 0.f);
}

float AHHCharacter::GetBleedOutRemaining() const
{
	return IsDowned() ? FMath::Max(0.f, BleedOutAt - GetServerTime()) : 0.f;
}

void AHHCharacter::SetDowned(float BleedOutSeconds)
{
	if (!HasAuthority() || IsIncapacitated() || IsSeated() || bArrested)
	{
		return;
	}
	BleedOutAt = GetServerTime() + FMath::Max(5.f, BleedOutSeconds);
	BodyPose = EHHBodyPose::Downed;
	OnRep_BodyPose();
	ForceNetUpdate();
}

void AHHCharacter::SetDead()
{
	if (!HasAuthority() || IsDead())
	{
		return;
	}
	BodyPose = EHHBodyPose::Dead;
	BleedOutAt = 0.f;
	OnRep_BodyPose();
	ForceNetUpdate();
}

void AHHCharacter::ReviveBy(AHHCharacter* Helper)
{
	if (!HasAuthority() || !IsDowned())
	{
		return;
	}
	BodyPose = EHHBodyPose::Normal;
	BleedOutAt = 0.f;
	bWasRevived = true;
	if (AHHPlayerState* State = GetPlayerState<AHHPlayerState>())
	{
		State->SetPsyche(FMath::Max(State->GetPsyche(), RevivedPsyche));
	}
	if (Helper && Helper->GetBodyPose() == EHHBodyPose::Kneeling)
	{
		Helper->BodyPose = EHHBodyPose::Normal;
		Helper->OnRep_BodyPose();
	}
	OnRep_BodyPose();
	ForceNetUpdate();
	if (AHHHeistGameMode* HeistMode = GetWorld()->GetAuthGameMode<AHHHeistGameMode>())
	{
		HeistMode->HandleRevived(this, Helper);
	}
}

void AHHCharacter::SetKneeling(bool bKneel)
{
	const bool bCan = BodyPose == EHHBodyPose::Normal || BodyPose == EHHBodyPose::Kneeling;
	const EHHBodyPose Wanted = bKneel ? EHHBodyPose::Kneeling : EHHBodyPose::Normal;
	if (!bCan || BodyPose == Wanted)
	{
		return;
	}
	BodyPose = Wanted;
	OnRep_BodyPose();
	if (!HasAuthority())
	{
		ServerSetKneeling(bKneel);
	}
}

void AHHCharacter::ServerSetKneeling_Implementation(bool bKneel)
{
	SetKneeling(bKneel);
	ForceNetUpdate();
}

void AHHCharacter::OnRep_BodyPose()
{
	ApplyPose();
}

void AHHCharacter::ApplyPose()
{
	const EHHBodyPose Previous = AppliedPose;
	AppliedPose = BodyPose;
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	ReviveZone->SetCollisionEnabled(IsDowned() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	if (IsIncapacitated())
	{
		Interaction->StopInteract();
		bSprinting = false;
		// The capsule shrinks to the crouched size while on the floor (the owner drives crouching).
		if (IsLocallyControlled())
		{
			Crouch();
		}
		CameraTargetZ = DownedEyeZ;
	}
	if (IsDead())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		// The crew can step over the body.
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		if (bFlashlightOn)
		{
			bFlashlightOn = false;
			OnRep_Flashlight();
		}
		if (IsLocallyControlled())
		{
			if (AHHPlayerController* PC = Cast<AHHPlayerController>(GetController()))
			{
				PC->SpectateNext();
			}
		}
	}
	else if (Previous == EHHBodyPose::Dead)
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		if (!bBodyStowed)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
	if (BodyPose == EHHBodyPose::Kneeling)
	{
		Movement->StopMovementImmediately();
	}
	if (BodyPose == EHHBodyPose::Normal && (Previous == EHHBodyPose::Downed || Previous == EHHBodyPose::Dead))
	{
		if (IsLocallyControlled())
		{
			UnCrouch();
		}
		CameraTargetZ = bIsCrouched ? CrouchedEyeZ : StandingEyeZ;
	}
	ApplyMovementSpeed();
}

FText AHHCharacter::GetInteractionPrompt(AActor* Interactor) const
{
	if (!IsDowned())
	{
		return FText::GetEmpty();
	}
	const AHHPlayerState* State = GetPlayerState<AHHPlayerState>();
	return FText::Format(LOCTEXT("ReviveCrew", "Help {0} up"), FText::FromString(State ? State->GetPlayerName() : FString(TEXT("them"))));
}

FText AHHCharacter::GetInteractionDetail(AActor* Interactor) const
{
	if (!IsDowned())
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("ReviveTime", "{0} s before it is too late"), FText::AsNumber(FMath::CeilToInt(GetBleedOutRemaining())));
}

bool AHHCharacter::CanInteract(AActor* Interactor) const
{
	const AHHCharacter* Helper = Cast<AHHCharacter>(Interactor);
	return IsDowned() && Helper && Helper != this && !Helper->IsIncapacitated() && !Helper->IsSeated() && !Helper->IsArrested();
}

void AHHCharacter::Interact(AActor* Interactor)
{
	if (CanInteract(Interactor))
	{
		ReviveBy(Cast<AHHCharacter>(Interactor));
	}
}

float AHHCharacter::GetInteractDuration(AActor* Interactor) const
{
	return ReviveSeconds;
}

// ---------------------------------------------------------------------------------------
// Movement state

void AHHCharacter::SetSprinting(bool bNewSprinting)
{
	if (bNewSprinting && bIsCrouched)
	{
		UnCrouch();
	}
	if (bSprinting == bNewSprinting)
	{
		return;
	}
	bSprinting = bNewSprinting;
	ApplyMovementSpeed();
	if (!HasAuthority())
	{
		ServerSetSprinting(bNewSprinting);
	}
}

void AHHCharacter::ApplyMovementSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (IsDowned())
	{
		// Dragging yourself across the floor (the capsule is crouched while down).
		Movement->MaxWalkSpeed = CrawlSpeed;
		Movement->MaxWalkSpeedCrouched = CrawlSpeed;
		return;
	}
	if (BodyPose == EHHBodyPose::Kneeling || IsDead())
	{
		Movement->MaxWalkSpeed = 0.f;
		Movement->MaxWalkSpeedCrouched = 0.f;
		return;
	}
	// Bulky loot: slower, and no sprinting with a TV in your arms.
	const float CarryFactor = bCarryingHeavy ? HeavyCarrySpeedFactor : 1.f;
	Movement->MaxWalkSpeed = (bSprinting && !bCarryingHeavy ? SprintSpeed : WalkSpeed) * CarryFactor;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed * CarryFactor;
}

void AHHCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	bSprinting = bNewSprinting;
	ApplyMovementSpeed();
}

void AHHCharacter::OnRep_Sprinting()
{
	ApplyMovementSpeed();
}

void AHHCharacter::ServerSetFlashlight_Implementation(bool bNewOn)
{
	if (bFlashlightOn != bNewOn)
	{
		bFlashlightOn = bNewOn;
		OnRep_Flashlight();
	}
}

void AHHCharacter::OnRep_Flashlight()
{
	Flashlight->SetVisibility(bFlashlightOn);
	UpdateFlashlightAim();
	UpdateHeldItem();

	if (const UHHGameData* GameData = UHHGameData::Get(this))
	{
		USoundBase* Click = (bFlashlightOn ? GameData->FlashlightOn : GameData->FlashlightOff).LoadSynchronous();
		if (Click)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Click, FlashlightPivot->GetComponentLocation(), 0.8f);
		}
	}
}

void AHHCharacter::ServerInteract_Implementation(AActor* Target)
{
	Interaction->ExecuteInteraction(Target);
}

void AHHCharacter::ServerBeginHold_Implementation(AActor* Target)
{
	Interaction->BeginServerHold(Target);
}

void AHHCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// Keep the eye where it was this frame, then ease down.
	CameraZ += HalfHeightAdjust;
	CameraTargetZ = IsIncapacitated() ? DownedEyeZ : CrouchedEyeZ;
}

void AHHCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CameraZ -= HalfHeightAdjust;
	CameraTargetZ = IsIncapacitated() ? DownedEyeZ : StandingEyeZ;
}

void AHHCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	Footsteps->PlayLanding(FMath::Abs(GetVelocity().Z));

	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (IsLocallyControlled() && (!Settings || Settings->AllowsCameraShake()))
	{
		LandingDip = FMath::Clamp(FMath::Abs(GetVelocity().Z) / 120.f, 0.f, 7.f);
	}
}

// ---------------------------------------------------------------------------------------
// Tick

void AHHCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && IsDowned() && GetServerTime() >= BleedOutAt)
	{
		if (AHHHeistGameMode* HeistMode = GetWorld()->GetAuthGameMode<AHHHeistGameMode>())
		{
			HeistMode->HandleBleedOut(this);
		}
		else
		{
			SetDead();
		}
	}

	if (IsLocallyControlled())
	{
		UpdateCamera(DeltaSeconds);
		// The HUD moves the camera between the eyes and station cameras: follow it.
		const bool bWasHidden = bHeadHidden;
		UpdateLocalVisibility();
		if (bWasHidden != bHeadHidden)
		{
			UpdateFlashlightAim();
		}
	}
	else if (bHeadHidden || IsViewedByLocalSpectator())
	{
		UpdateLocalVisibility();
	}

	const bool bHolding = HeldItem->IsVisible();
	HandAlpha = FMath::FInterpTo(HandAlpha, bHolding ? 1.f : 0.f, DeltaSeconds, 8.f);
	if (bFlashlightOn || bHolding)
	{
		UpdateFlashlightAim();
	}
}

void AHHCharacter::UpdateFlashlightAim()
{
	FlashlightPivot->SetWorldRotation(GetBaseAimRotation());
}

void AHHCharacter::UpdateCamera(float DeltaSeconds)
{
	const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	const bool bAllowBob = !Settings || Settings->AllowsHeadBob();

	CameraZ = FMath::FInterpTo(CameraZ, CameraTargetZ, DeltaSeconds, 9.f);
	LandingDip = FMath::FInterpTo(LandingDip, 0.f, DeltaSeconds, 7.f);

	const float Speed = GetVelocity().Size2D();
	const bool bGrounded = GetCharacterMovement()->IsMovingOnGround();
	const float MoveAlpha = bGrounded ? FMath::Clamp(Speed / SprintSpeed, 0.f, 1.f) : 0.f;
	BobBlend = FMath::FInterpTo(BobBlend, MoveAlpha, DeltaSeconds, 6.f);

	// One sine period = two steps. Cadence rises with speed.
	const float Cadence = FMath::Lerp(1.7f, 2.7f, MoveAlpha);
	BobPhase = FMath::Fmod(BobPhase + DeltaSeconds * Cadence * UE_PI, 2.f * UE_PI);

	FVector Offset(EyeForward, 0.f, CameraZ - LandingDip);
	if (bAllowBob)
	{
		const float Amp = BobBlend * (bIsCrouched ? 0.6f : 1.f);
		Offset.Z += FMath::Sin(BobPhase * 2.f) * 1.8f * Amp;
		Offset.Y += FMath::Sin(BobPhase) * 1.2f * Amp;
		// Slow breathing when standing still.
		const float Time = GetWorld()->GetTimeSeconds();
		Offset.Z += FMath::Sin(Time * 1.3f) * 0.35f * (1.f - BobBlend);
	}
	FirstPersonCamera->SetRelativeLocation(Offset);
}

#undef LOCTEXT_NAMESPACE
