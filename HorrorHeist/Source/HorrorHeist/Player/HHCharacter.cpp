#include "Player/HHCharacter.h"
#include "Player/HHPlayerState.h"
#include "Player/HHCosmeticComponent.h"
#include "Player/HHInteractionComponent.h"
#include "Player/HHFootstepComponent.h"
#include "Player/HHCharacterAnimInstance.h"
#include "Player/HHPlayerController.h"
#include "Player/HHInventoryComponent.h"
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
}

void AHHCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AHHCharacter, bFlashlightOn, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AHHCharacter, bSprinting, COND_SkipOwner);
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

	bHeadHidden = bFirstPersonView;
	GetMesh()->SetOwnerNoSee(false);
	Cosmetics->SetHiddenForOwner(bFirstPersonView);
	GetMesh()->SetVisibility(!bLocallyHidden, true);
	FallbackBody->SetOwnerNoSee(bFirstPersonView);
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
	if (!Controller || Axis.IsNearlyZero())
	{
		return;
	}
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix Basis(YawRotation);
	AddMovementInput(Basis.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(Basis.GetUnitAxis(EAxis::Y), Axis.X);
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
	if (bIsCrouched)
	{
		UnCrouch();
		return;
	}
	Jump();
}

void AHHCharacter::Input_JumpCompleted()
{
	StopJumping();
}

void AHHCharacter::Input_SprintStarted()
{
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
	if (Settings && !Settings->bToggleCrouch)
	{
		UnCrouch();
	}
}

void AHHCharacter::Input_Interact()
{
	Interaction->TryInteract();
}

void AHHCharacter::Input_InteractReleased()
{
	Interaction->StopInteract();
}

void AHHCharacter::SelectSlot(int32 Index)
{
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
	const AHHPlayerState* State = BoundPlayerState.Get();
	UHHInventoryComponent* Inventory = State ? State->GetInventory() : nullptr;
	if (Inventory && Inventory->IsEnabled() && !Inventory->GetSelected().IsEmpty())
	{
		Interaction->StopInteract();
		Inventory->RequestDrop();
	}
}

void AHHCharacter::Input_Flashlight()
{
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
	SetThirdPerson(!bThirdPerson);
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
	// Bulky loot: slower, and no sprinting with a TV in your arms.
	const float CarryFactor = bCarryingHeavy ? HeavyCarrySpeedFactor : 1.f;
	GetCharacterMovement()->MaxWalkSpeed = (bSprinting && !bCarryingHeavy ? SprintSpeed : WalkSpeed) * CarryFactor;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed * CarryFactor;
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
	CameraTargetZ = CrouchedEyeZ;
}

void AHHCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CameraZ -= HalfHeightAdjust;
	CameraTargetZ = StandingEyeZ;
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
