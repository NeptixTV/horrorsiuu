// Mock of the Unreal Engine gameplay framework (syntax checks only, see UEMockCore.h).
// All engine headers used by the project map to this one file (missing engine #includes are
// therefore NOT detected by the syntax check; Tools/SyntaxCheck/lint.py has a heuristic for that).
#pragma once
#include "UEMockObject.h"

class AActor;
class APawn;
class ACharacter;
class AController;
class APlayerController;
class APlayerState;
class AGameModeBase;
class AGameStateBase;
class AHUD;
class AWorldSettings;
class APlayerCameraManager;
class UActorComponent;
class USceneComponent;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture;
class UTexture2D;
class UTextureRenderTarget2D;
class UGameInstance;
class ULocalPlayer;
class UGameViewportClient;
class UInputComponent;
class UNetDriver;
class ULevel;
class UFont;
class UCanvas;
class USoundBase;
class USoundAttenuation;
class UReverbEffect;
class UPhysicalMaterial;
class FTimerManager;
class FConstPlayerControllerIterator;
class FViewport;
class FTexture;
class UEngine;
class UDamageType;
struct FPropertyChangedEvent;
struct FHitResult;

// ================================================================== engine types
enum ENetMode { NM_Standalone, NM_DedicatedServer, NM_ListenServer, NM_Client, NM_MAX };
enum ENetRole { ROLE_None, ROLE_SimulatedProxy, ROLE_AutonomousProxy, ROLE_Authority };
enum ECollisionChannel : int32
{
	ECC_WorldStatic, ECC_WorldDynamic, ECC_Pawn, ECC_Visibility, ECC_Camera, ECC_PhysicsBody, ECC_Vehicle, ECC_Destructible,
	ECC_GameTraceChannel1, ECC_GameTraceChannel2, ECC_GameTraceChannel3, ECC_MAX
};
enum ECollisionResponse : int32 { ECR_Ignore, ECR_Overlap, ECR_Block, ECR_MAX };
namespace ECollisionEnabled { enum Type { NoCollision, QueryOnly, PhysicsOnly, QueryAndPhysics, ProbeOnly, QueryAndProbe }; }
namespace EComponentMobility { enum Type { Static, Stationary, Movable }; }
namespace EEndPlayReason { enum Type { Destroyed, LevelTransition, EndPlayInEditor, RemovedFromWorld, Quit }; }
enum class ETeleportType : uint8 { None, TeleportPhysics, ResetPhysics };
enum ELevelTick { LEVELTICK_TimeOnly, LEVELTICK_ViewportsOnly, LEVELTICK_All, LEVELTICK_PauseTick };
enum ETickingGroup { TG_PrePhysics, TG_StartPhysics, TG_DuringPhysics, TG_EndPhysics, TG_PostPhysics, TG_PostUpdateWork, TG_LastDemotable };
enum EMovementMode { MOVE_None, MOVE_Walking, MOVE_NavWalking, MOVE_Falling, MOVE_Swimming, MOVE_Flying, MOVE_Custom };
enum class ESpawnActorCollisionHandlingMethod : uint8 { Undefined, AlwaysSpawn, AdjustIfPossibleButAlwaysSpawn, AdjustIfPossibleButDontSpawnIfColliding, DontSpawnIfColliding };
enum class EAutoPossessAI : uint8 { Disabled, PlacedInWorld, Spawned, PlacedInWorldOrSpawned };
enum class ELightUnits : uint8 { Unitless, Candelas, Lumens, EV };
enum EBlendMode { BLEND_Opaque, BLEND_Masked, BLEND_Translucent, BLEND_Additive, BLEND_Modulate, BLEND_AlphaComposite };
enum ESimpleElementBlendMode { SE_BLEND_Opaque, SE_BLEND_Masked, SE_BLEND_Translucent, SE_BLEND_Additive };
namespace EWindowMode { enum Type { Fullscreen, WindowedFullscreen, Windowed, NumWindowModes }; }
enum ETravelType { TRAVEL_Absolute, TRAVEL_Partial, TRAVEL_Relative };
enum EViewTargetBlendFunction { VTBlend_Linear, VTBlend_Cubic, VTBlend_EaseIn, VTBlend_EaseOut, VTBlend_EaseInOut };
enum class EMouseLockMode : uint8 { DoNotLock, LockOnCapture, LockAlways, LockInFullscreen };
namespace EQuitPreference { enum Type { Quit, Background }; }
namespace ENetworkFailure { enum Type { NetDriverAlreadyExists, NetDriverCreateFailure, NetDriverListenFailure, ConnectionLost, ConnectionTimeout, FailureReceived, OutdatedClient, OutdatedServer, PendingConnectionFailure, NetGuidMismatch, NetChecksumMismatch }; }
enum ELifetimeCondition { COND_None, COND_InitialOnly, COND_OwnerOnly, COND_SkipOwner, COND_SimulatedOnly, COND_AutonomousOnly, COND_SimulatedOrPhysics, COND_InitialOrOwner, COND_Custom, COND_ReplayOrOwner, COND_ReplayOnly };
enum EPixelFormat { PF_Unknown, PF_B8G8R8A8, PF_R8G8B8A8, PF_FloatRGBA, PF_G8 };
enum ETextureRenderTargetFormat { RTF_R8, RTF_RG8, RTF_RGBA8, RTF_RGBA8_SRGB, RTF_R16f, RTF_RG16f, RTF_RGBA16f, RTF_R32f, RTF_RG32f, RTF_RGBA32f, RTF_RGB10A2 };
enum TextureFilter { TF_Nearest, TF_Bilinear, TF_Trilinear, TF_Default };
enum TextureCompressionSettings { TC_Default, TC_VectorDisplacementmap, TC_HDR, TC_EditorIcon };
enum TextureAddress { TA_Wrap, TA_Clamp, TA_Mirror };
enum EHorizTextAligment { EHTA_Left, EHTA_Center, EHTA_Right };
enum EVerticalTextAligment { EVRTA_TextTop, EVRTA_TextCenter, EVRTA_TextBottom, EVRTA_QuadTop };
enum ESceneCaptureSource { SCS_SceneColorHDR, SCS_SceneColorHDRNoAlpha, SCS_FinalColorLDR, SCS_SceneColorSceneDepth, SCS_SceneDepth, SCS_DeviceDepth, SCS_Normal, SCS_BaseColor, SCS_FinalColorHDR, SCS_FinalToneCurveHDR };
namespace ECameraProjectionMode { enum Type { Perspective, Orthographic }; }
enum ESkyLightSourceType { SLS_CapturedScene, SLS_SpecifiedCubemap };
enum class EAttenuationDistanceModel : uint8 { Linear, Logarithmic, Inverse, LogReverse, NaturalSound, Custom };
namespace EAttenuationShape { enum Type { Sphere, Capsule, Box, Cone }; }
enum EAutoExposureMethod { AEM_Histogram, AEM_Basic, AEM_Manual };
enum class EImageFormat : int8 { Invalid = -1, PNG = 0, JPEG, GrayscaleJPEG, BMP, ICO, EXR, ICNS, TGA, HDR, TIFF, DDS };
enum class ERGBFormat : int8 { Invalid = -1, RGBA = 0, BGRA = 1, Gray = 2, RGBAF = 3, BGRE = 4, GrayF = 5 };
enum class EUpdateTransformFlags : int32 { None = 0 };
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

struct FHitResult
{
	int32 FaceIndex = 0;
	float Time = 1.f;
	float Distance = 0.f;
	FVector Location;
	FVector ImpactPoint;
	FVector Normal;
	FVector ImpactNormal;
	FVector TraceStart;
	FVector TraceEnd;
	float PenetrationDepth = 0.f;
	int32 Item = 0;
	FName BoneName;
	uint8 bBlockingHit : 1;
	uint8 bStartPenetrating : 1;
	TWeakObjectPtr<UPhysicalMaterial> PhysMaterial;
	FHitResult();
	explicit FHitResult(float InTime);
	AActor* GetActor() const;
	UPrimitiveComponent* GetComponent() const;
	bool IsValidBlockingHit() const;
	void Reset(float InTime = 1.f, bool bPreserveTraceData = true);
};

struct FCollisionQueryParams
{
	FName TraceTag;
	bool bTraceComplex = false;
	bool bReturnPhysicalMaterial = false;
	bool bReturnFaceIndex = false;
	bool bFindInitialOverlaps = true;
	bool bIgnoreTouches = false;
	static FCollisionQueryParams DefaultQueryParam;
	FCollisionQueryParams();
	FCollisionQueryParams(FName InTraceTag, bool bInTraceComplex = false, const AActor* InIgnoreActor = nullptr);
	void AddIgnoredActor(const AActor* InIgnoreActor);
	void AddIgnoredActors(const TArray<AActor*>& InIgnoreActors);
	void AddIgnoredComponent(const UPrimitiveComponent* InIgnoreComponent);
	void ClearIgnoredActors();
};
struct FCollisionResponseParams
{
	static FCollisionResponseParams DefaultResponseParam;
};
struct FCollisionObjectQueryParams
{
	FCollisionObjectQueryParams();
	explicit FCollisionObjectQueryParams(ECollisionChannel Channel);
	void AddObjectTypesToQuery(ECollisionChannel Channel);
	static FCollisionObjectQueryParams DefaultObjectQueryParam;
};
struct FCollisionShape
{
	static FCollisionShape MakeSphere(float Radius);
	static FCollisionShape MakeBox(const FVector& HalfExtent);
	static FCollisionShape MakeCapsule(float Radius, float HalfHeight);
	static FCollisionShape MakeCapsule(const FVector& Extent);
	bool IsNearlyZero() const;
};
struct FOverlapResult
{
	AActor* GetActor() const;
	UPrimitiveComponent* GetComponent() const;
};

enum class EAttachmentRule : uint8 { KeepRelative, KeepWorld, SnapToTarget };
enum class EDetachmentRule : uint8 { KeepRelative, KeepWorld };
struct FAttachmentTransformRules
{
	static FAttachmentTransformRules KeepRelativeTransform;
	static FAttachmentTransformRules KeepWorldTransform;
	static FAttachmentTransformRules SnapToTargetNotIncludingScale;
	static FAttachmentTransformRules SnapToTargetIncludingScale;
	FAttachmentTransformRules(EAttachmentRule InRule, bool bInWeldSimulatedBodies);
	FAttachmentTransformRules(EAttachmentRule InLocationRule, EAttachmentRule InRotationRule, EAttachmentRule InScaleRule, bool bInWeldSimulatedBodies);
};
struct FDetachmentTransformRules
{
	static FDetachmentTransformRules KeepRelativeTransform;
	static FDetachmentTransformRules KeepWorldTransform;
	FDetachmentTransformRules(EDetachmentRule InRule, bool bInCallModify);
};

struct FActorSpawnParameters
{
	FName Name;
	AActor* Template = nullptr;
	AActor* Owner = nullptr;
	APawn* Instigator = nullptr;
	ULevel* OverrideLevel = nullptr;
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::Undefined;
	uint8 bNoFail : 1;
	uint8 bDeferConstruction : 1;
	uint8 bAllowDuringConstructionScript : 1;
	EObjectFlags ObjectFlags = RF_Transactional;
	FActorSpawnParameters();
};

struct FLifetimeProperty
{
	int32 RepIndex = 0;
	ELifetimeCondition Condition = COND_None;
};
struct FDoRepLifetimeParams
{
	ELifetimeCondition Condition = COND_None;
	bool bIsPushBased = false;
};

struct FTickFunction
{
	ETickingGroup TickGroup = TG_PrePhysics;
	uint8 bCanEverTick : 1;
	uint8 bStartWithTickEnabled : 1;
	uint8 bTickEvenWhenPaused : 1;
	uint8 bAllowTickOnDedicatedServer : 1;
	uint8 bHighPriority : 1;
	float TickInterval = 0.f;
	FTickFunction();
	void SetTickFunctionEnable(bool bInEnabled);
	bool IsTickFunctionEnabled() const;
	void AddPrerequisite(UObject* TargetObject, FTickFunction& TargetTickFunction);
};
struct FActorTickFunction : FTickFunction {};
struct FActorComponentTickFunction : FTickFunction {};

struct FTimerHandle
{
	bool IsValid() const;
	void Invalidate();
};
using FTimerDelegate = TDelegate<void()>;
class FTimerManager
{
public:
	template <class U> void SetTimer(FTimerHandle& InOutHandle, U* InObj, void (U::*InTimerMethod)(), float InRate, bool InbLoop = false, float InFirstDelay = -1.f);
	template <class U> void SetTimer(FTimerHandle& InOutHandle, U* InObj, void (U::*InTimerMethod)() const, float InRate, bool InbLoop = false, float InFirstDelay = -1.f);
	void SetTimer(FTimerHandle& InOutHandle, const FTimerDelegate& InDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f);
	void SetTimer(FTimerHandle& InOutHandle, TFunction<void()>&& Callback, float InRate, bool InbLoop, float InFirstDelay = -1.f);
	FTimerHandle SetTimerForNextTick(TFunction<void()>&& Callback);
	template <class U> FTimerHandle SetTimerForNextTick(U* InObj, void (U::*InTimerMethod)());
	void ClearTimer(FTimerHandle& InHandle);
	void ClearAllTimersForObject(const void* Object);
	bool IsTimerActive(FTimerHandle InHandle) const;
	bool IsTimerPending(FTimerHandle InHandle) const;
	float GetTimerRemaining(FTimerHandle InHandle) const;
	float GetTimerElapsed(FTimerHandle InHandle) const;
	void PauseTimer(FTimerHandle InHandle);
	void UnPauseTimer(FTimerHandle InHandle);
};

struct FBodyInstance
{
	uint8 bOverrideMass : 1;
	uint8 bAutoWeld : 1;
	uint8 bUseCCD : 1;
	uint8 bLockXRotation : 1;
	uint8 bLockYRotation : 1;
	uint8 bLockZRotation : 1;
	uint8 bNotifyRigidBodyCollision : 1;
	uint8 bSimulatePhysics : 1;
	uint8 bEnableGravity : 1;
	float MassScale = 1.f;
	FVector COMNudge;
	float LinearDamping = 0.01f;
	float AngularDamping = 0.f;
	float MaxAngularVelocity = 3600.f;
	int32 PositionSolverIterationCount = 8;
	int32 VelocitySolverIterationCount = 1;
	FBodyInstance();
	void SetMassOverride(float MassInKG, bool bNewOverrideMass = true);
	float GetMassOverride() const;
	float GetBodyMass() const;
	void SetMassScale(float InMassScale = 1.f);
	void UpdateMassProperties();
	void SetInstanceSimulatePhysics(bool bSimulate, bool bMaintainPhysicsBlending = false, bool bPreserveExistingAttachments = false);
	void SetEnableGravity(bool bGravityEnabled);
	void SetUseCCD(bool bInUseCCD);
	void SetLinearDamping(float InDamping);
	void SetAngularDamping(float InDamping);
	void SetCollisionEnabled(ECollisionEnabled::Type NewType, bool bUpdatePhysicsFilterData = true);
	void SetResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse);
	void SetObjectType(ECollisionChannel Channel);
	void SetMaxDepenetrationVelocity(float MaxVelocity);
};

struct FKey
{
	FKey();
	FKey(const FName InName);
	FKey(const TCHAR* InName);
	FName GetFName() const;
	FText GetDisplayName(bool bLongDisplayName = true) const;
	bool IsValid() const;
	bool IsGamepadKey() const;
	bool IsMouseButton() const;
	bool operator==(const FKey& O) const;
};
struct EKeys
{
	static const FKey AnyKey, MouseX, MouseY, Mouse2D, MouseScrollUp, MouseScrollDown, MouseWheelAxis, LeftMouseButton, RightMouseButton, MiddleMouseButton,
		ThumbMouseButton, ThumbMouseButton2, BackSpace, Tab, Enter, Pause, CapsLock, Escape, SpaceBar, PageUp, PageDown, End, Home, Left, Up, Right, Down,
		Insert, Delete, Zero, One, Two, Three, Four, Five, Six, Seven, Eight, Nine, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		NumPadZero, NumPadOne, NumPadTwo, NumPadThree, NumPadFour, NumPadFive, NumPadSix, NumPadSeven, NumPadEight, NumPadNine, Multiply, Add, Subtract,
		Decimal, Divide, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, NumLock, ScrollLock, LeftShift, RightShift, LeftControl, RightControl, LeftAlt,
		RightAlt, LeftCommand, RightCommand, Semicolon, Equals, Comma, Underscore, Hyphen, Period, Slash, Tilde, LeftBracket, Backslash, RightBracket,
		Apostrophe, Gamepad_Left2D, Gamepad_LeftX, Gamepad_LeftY, Gamepad_Right2D, Gamepad_RightX, Gamepad_RightY, Gamepad_LeftTriggerAxis,
		Gamepad_RightTriggerAxis, Gamepad_LeftThumbstick, Gamepad_RightThumbstick, Gamepad_Special_Left, Gamepad_Special_Right, Gamepad_FaceButton_Bottom,
		Gamepad_FaceButton_Right, Gamepad_FaceButton_Left, Gamepad_FaceButton_Top, Gamepad_LeftShoulder, Gamepad_RightShoulder, Gamepad_LeftTrigger,
		Gamepad_RightTrigger, Gamepad_DPad_Up, Gamepad_DPad_Down, Gamepad_DPad_Right, Gamepad_DPad_Left;
};

struct FPostProcessSettings
{
	uint8 bOverride_AutoExposureMethod : 1;
	uint8 bOverride_AutoExposureMinBrightness : 1;
	uint8 bOverride_AutoExposureMaxBrightness : 1;
	uint8 bOverride_AutoExposureBias : 1;
	uint8 bOverride_AutoExposureSpeedUp : 1;
	uint8 bOverride_AutoExposureSpeedDown : 1;
	uint8 bOverride_AutoExposureApplyPhysicalCameraExposure : 1;
	uint8 bOverride_MotionBlurAmount : 1;
	uint8 bOverride_MotionBlurMax : 1;
	uint8 bOverride_FilmGrainIntensity : 1;
	uint8 bOverride_FilmGrainIntensityShadows : 1;
	uint8 bOverride_VignetteIntensity : 1;
	uint8 bOverride_BloomIntensity : 1;
	uint8 bOverride_ColorSaturation : 1;
	uint8 bOverride_ColorContrast : 1;
	uint8 bOverride_ColorGain : 1;
	uint8 bOverride_SceneColorTint : 1;
	uint8 bOverride_DepthOfFieldFocalDistance : 1;
	uint8 bOverride_DepthOfFieldFstop : 1;
	uint8 bOverride_DepthOfFieldSensorWidth : 1;
	uint8 bOverride_DepthOfFieldDepthBlurAmount : 1;
	uint8 bOverride_DepthOfFieldDepthBlurRadius : 1;
	uint8 bOverride_LocalExposureHighlightContrastScale : 1;
	uint8 bOverride_LocalExposureShadowContrastScale : 1;
	uint8 bOverride_AmbientOcclusionIntensity : 1;
	uint8 bOverride_ChromaticAberrationIntensity : 1;
	uint8 bOverride_SceneFringeIntensity : 1;
	uint8 bOverride_WhiteTemp : 1;
	uint8 bOverride_AutoExposureLowPercent : 1;
	uint8 bOverride_AutoExposureHighPercent : 1;
	uint8 bOverride_ExpandGamut : 1;
	uint8 bOverride_ToneCurveAmount : 1;
	TEnumAsByte<EAutoExposureMethod> AutoExposureMethod;
	float AutoExposureMinBrightness = 0.f;
	float AutoExposureMaxBrightness = 0.f;
	float AutoExposureBias = 0.f;
	float AutoExposureSpeedUp = 3.f;
	float AutoExposureSpeedDown = 1.f;
	float AutoExposureApplyPhysicalCameraExposure = 1.f;
	float AutoExposureLowPercent = 10.f;
	float AutoExposureHighPercent = 90.f;
	float MotionBlurAmount = 0.5f;
	float MotionBlurMax = 5.f;
	float FilmGrainIntensity = 0.f;
	float FilmGrainIntensityShadows = 1.f;
	float VignetteIntensity = 0.4f;
	float BloomIntensity = 0.675f;
	FVector4 ColorSaturation;
	FVector4 ColorContrast;
	FVector4 ColorGain;
	FLinearColor SceneColorTint;
	float DepthOfFieldFocalDistance = 0.f;
	float DepthOfFieldFstop = 4.f;
	float DepthOfFieldSensorWidth = 24.576f;
	float DepthOfFieldDepthBlurAmount = 1.f;
	float DepthOfFieldDepthBlurRadius = 0.f;
	float LocalExposureHighlightContrastScale = 1.f;
	float LocalExposureShadowContrastScale = 1.f;
	float AmbientOcclusionIntensity = 0.5f;
	float ChromaticAberrationIntensity = 0.f;
	float SceneFringeIntensity = 0.f;
	float WhiteTemp = 6500.f;
	float ExpandGamut = 1.f;
	float ToneCurveAmount = 1.f;
	FPostProcessSettings();
};



struct FProcMeshTangent
{
	FVector TangentX;
	bool bFlipTangentY = false;
	FProcMeshTangent();
	FProcMeshTangent(float X, float Y, float Z);
	FProcMeshTangent(const FVector& InTangentX, bool bInFlipTangentY);
};

struct FRotationMatrix : FMatrix
{
	explicit FRotationMatrix(const FRotator& Rot);
	FVector GetScaledAxis(EAxis::Type Axis) const;
	FVector GetUnitAxis(EAxis::Type Axis) const;
	static FMatrix MakeFromX(const FVector& X);
	static FMatrix MakeFromXZ(const FVector& X, const FVector& Z);
	static FMatrix MakeFromXY(const FVector& X, const FVector& Y);
	static FMatrix MakeFromZ(const FVector& Z);
	FRotator Rotator() const;
};
struct FRotationTranslationMatrix : FMatrix
{
	FRotationTranslationMatrix(const FRotator& Rot, const FVector& Origin);
};

class UCollisionProfile : public UObject
{
public:
	static UCollisionProfile* Get();
	static const FName NoCollision_ProfileName;
	static const FName BlockAll_ProfileName;
	static const FName BlockAllDynamic_ProfileName;
	static const FName PhysicsActor_ProfileName;
	static const FName Pawn_ProfileName;
	static const FName CustomCollisionProfileName;
	static const FName Vehicle_ProfileName;
	static const FName DefaultProjectile_ProfileName;
};

// ================================================================== components
class UActorComponent : public UObject
{
public:
	typedef UObject Super;
	UActorComponent();
	UActorComponent(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	FActorComponentTickFunction PrimaryComponentTick;
	TArray<FName> ComponentTags;
	uint8 bAutoActivate : 1;
	uint8 bWantsInitializeComponent : 1;
	AActor* GetOwner() const;
	template <class T> T* GetOwner() const;
	UWorld* GetWorld() const override;
	ENetMode GetNetMode() const;
	bool IsNetMode(ENetMode Mode) const;
	ENetRole GetOwnerRole() const;
	void RegisterComponent();
	void UnregisterComponent();
	void DestroyComponent(bool bPromoteChildren = false);
	bool IsRegistered() const;
	void SetComponentTickEnabled(bool bEnabled);
	bool IsComponentTickEnabled() const;
	void SetComponentTickInterval(float TickInterval);
	void SetIsReplicated(bool ShouldReplicate);
	bool GetIsReplicated() const;
	void SetIsReplicatedByDefault(const bool bNewReplicates);
	bool ComponentHasTag(FName Tag) const;
	virtual void Activate(bool bReset = false);
	virtual void Deactivate();
	bool IsActive() const;
	void SetActive(bool bNewActive, bool bReset = false);
	void SetAutoActivate(bool bNewAutoActivate);
	virtual void InitializeComponent();
	virtual void UninitializeComponent();
	virtual void BeginPlay();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction);
	virtual void OnComponentCreated();
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy);
	bool HasBegunPlay() const;
	void MarkRenderStateDirty();
protected:
	virtual void OnRegister();
	virtual void OnUnregister();
};

struct FBoxSphereBounds
{
	FVector Origin;
	FVector BoxExtent;
	double SphereRadius = 0;
	FBox GetBox() const;
	FSphere GetSphere() const;
};
class USceneComponent : public UActorComponent
{
public:
	FBoxSphereBounds Bounds;
	typedef UActorComponent Super;
	USceneComponent();
	USceneComponent(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	TEnumAsByte<EComponentMobility::Type> Mobility;
	void SetupAttachment(USceneComponent* InParent, FName InSocketName = NAME_None);
	bool AttachToComponent(USceneComponent* Parent, const FAttachmentTransformRules& AttachmentRules, FName SocketName = NAME_None);
	void DetachFromComponent(const FDetachmentTransformRules& DetachmentRules);
	USceneComponent* GetAttachParent() const;
	AActor* GetAttachParentActor() const;
	const TArray<USceneComponent*>& GetAttachChildren() const;
	void GetChildrenComponents(bool bIncludeAllDescendants, TArray<USceneComponent*>& Children) const;
	bool IsAttachedTo(const USceneComponent* TestComp) const;
	void SetRelativeLocation(FVector NewLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeRotation(FRotator NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeRotation(const FQuat& NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeTransform(const FTransform& NewTransform, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeScale3D(FVector NewScale3D);
	void AddRelativeLocation(FVector DeltaLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddRelativeRotation(FRotator DeltaRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddLocalRotation(FRotator DeltaRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddLocalOffset(FVector DeltaLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddWorldOffset(FVector DeltaLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddWorldRotation(FRotator DeltaRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldLocation(FVector NewLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldRotation(FRotator NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldRotation(const FQuat& NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldTransform(const FTransform& NewTransform, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldScale3D(FVector NewScale);
	FVector GetComponentLocation() const;
	FRotator GetComponentRotation() const;
	FQuat GetComponentQuat() const;
	FVector GetComponentScale() const;
	const FTransform& GetComponentTransform() const;
	FVector GetComponentVelocity() const;
	FVector GetForwardVector() const;
	FVector GetRightVector() const;
	FVector GetUpVector() const;
	FVector GetRelativeLocation() const;
	FRotator GetRelativeRotation() const;
	FVector GetRelativeScale3D() const;
	FTransform GetRelativeTransform() const;
	FVector GetSocketLocation(FName InSocketName) const;
	FRotator GetSocketRotation(FName InSocketName) const;
	FTransform GetSocketTransform(FName InSocketName) const;
	void SetVisibility(bool bNewVisibility, bool bPropagateToChildren = false);
	void ToggleVisibility(bool bPropagateToChildren = false);
	bool IsVisible() const;
	void SetHiddenInGame(bool NewHidden, bool bPropagateToChildren = false);
	void SetMobility(EComponentMobility::Type NewMobility);
	void SetUsingAbsoluteLocation(bool bInAbsoluteLocation);
	void SetUsingAbsoluteRotation(bool bInAbsoluteRotation);
	void SetUsingAbsoluteScale(bool bInAbsoluteScale);
	void SetAbsolute(bool bNewAbsoluteLocation = false, bool bNewAbsoluteRotation = false, bool bNewAbsoluteScale = false);
	void UpdateComponentToWorld(EUpdateTransformFlags UpdateTransformFlags = EUpdateTransformFlags::None, ETeleportType Teleport = ETeleportType::None);
};

class UPrimitiveComponent;
using FComponentHitSignature = TDynamicMulticastDelegate<UPrimitiveComponent*, AActor*, UPrimitiveComponent*, FVector, const FHitResult&>;
using FComponentBeginOverlapSignature = TDynamicMulticastDelegate<UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32, bool, const FHitResult&>;
using FComponentEndOverlapSignature = TDynamicMulticastDelegate<UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32>;

class UPrimitiveComponent : public USceneComponent
{
public:
	typedef USceneComponent Super;
	UPrimitiveComponent();
	UPrimitiveComponent(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	FBodyInstance BodyInstance;
	FComponentHitSignature OnComponentHit;
	FComponentBeginOverlapSignature OnComponentBeginOverlap;
	FComponentEndOverlapSignature OnComponentEndOverlap;
	uint8 CastShadow : 1;
	uint8 bCastDynamicShadow : 1;
	uint8 bCastStaticShadow : 1;
	uint8 bCastHiddenShadow : 1;
	uint8 bOwnerNoSee : 1;
	uint8 bOnlyOwnerSee : 1;
	uint8 bReceivesDecals : 1;
	uint8 bRenderCustomDepth : 1;
	uint8 bVisibleInReflectionCaptures : 1;
	uint8 bVisibleInRealTimeSkyCaptures : 1;
	uint8 bAffectDistanceFieldLighting : 1;
	uint8 bAffectDynamicIndirectLighting : 1;
	uint8 bUseAsOccluder : 1;
	uint8 bRenderInMainPass : 1;
	uint8 bReturnMaterialOnMove : 1;
	float LDMaxDrawDistance = 0.f;
	float CachedMaxDrawDistance = 0.f;
	float BoundsScale = 1.f;
	int32 TranslucencySortPriority = 0;
	void SetCollisionEnabled(ECollisionEnabled::Type NewType);
	ECollisionEnabled::Type GetCollisionEnabled() const;
	void SetCollisionProfileName(FName InCollisionProfileName, bool bUpdateOverlaps = true);
	FName GetCollisionProfileName() const;
	void SetCollisionResponseToAllChannels(ECollisionResponse NewResponse);
	void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse);
	ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const;
	void SetCollisionObjectType(ECollisionChannel Channel);
	ECollisionChannel GetCollisionObjectType() const;
	void SetGenerateOverlapEvents(bool bInGenerateOverlapEvents);
	bool GetGenerateOverlapEvents() const;
	void SetNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision);
	void SetSimulatePhysics(bool bSimulate);
	bool IsSimulatingPhysics(FName BoneName = NAME_None) const;
	void SetEnableGravity(bool bGravityEnabled);
	void SetLinearDamping(float InDamping);
	void SetAngularDamping(float InDamping);
	void SetMassOverrideInKg(FName BoneName = NAME_None, float MassInKg = 1.f, bool bOverrideMass = true);
	float GetMass() const;
	void SetCenterOfMass(FVector CenterOfMassOffset, FName BoneName = NAME_None);
	FVector GetCenterOfMass(FName BoneName = NAME_None) const;
	void AddForce(FVector Force, FName BoneName = NAME_None, bool bAccelChange = false);
	void AddForceAtLocation(FVector Force, FVector Location, FName BoneName = NAME_None);
	void AddForceAtLocationLocal(FVector Force, FVector Location, FName BoneName = NAME_None);
	void AddImpulse(FVector Impulse, FName BoneName = NAME_None, bool bVelChange = false);
	void AddImpulseAtLocation(FVector Impulse, FVector Location, FName BoneName = NAME_None);
	void AddTorqueInRadians(FVector Torque, FName BoneName = NAME_None, bool bAccelChange = false);
	void AddTorqueInDegrees(FVector Torque, FName BoneName = NAME_None, bool bAccelChange = false);
	void AddAngularImpulseInDegrees(FVector Impulse, FName BoneName = NAME_None, bool bVelChange = false);
	FVector GetPhysicsLinearVelocity(FName BoneName = NAME_None);
	FVector GetPhysicsLinearVelocityAtPoint(FVector Point, FName BoneName = NAME_None);
	void SetPhysicsLinearVelocity(FVector NewVel, bool bAddToCurrent = false, FName BoneName = NAME_None);
	FVector GetPhysicsAngularVelocityInDegrees(FName BoneName = NAME_None) const;
	FVector GetPhysicsAngularVelocityInRadians(FName BoneName = NAME_None) const;
	void SetPhysicsAngularVelocityInDegrees(FVector NewAngVel, bool bAddToCurrent = false, FName BoneName = NAME_None);
	void SetPhysicsAngularVelocityInRadians(FVector NewAngVel, bool bAddToCurrent = false, FName BoneName = NAME_None);
	void SetPhysicsMaxAngularVelocityInDegrees(float NewMaxAngVel, bool bAddToCurrent = false, FName BoneName = NAME_None);
	void WakeRigidBody(FName BoneName = NAME_None);
	void PutRigidBodyToSleep(FName BoneName = NAME_None);
	void SetUseCCD(bool InUseCCD, FName BoneName = NAME_None);
	void SetCastShadow(bool NewCastShadow);
	void SetCastHiddenShadow(bool NewCastHiddenShadow);
	void SetOwnerNoSee(bool bNewOwnerNoSee);
	void SetOnlyOwnerSee(bool bNewOnlyOwnerSee);
	void SetRenderCustomDepth(bool bValue);
	void SetCustomDepthStencilValue(int32 Value);
	void SetReceivesDecals(bool bNewReceivesDecals);
	void SetCanEverAffectNavigation(bool bRelevant);
	void SetCullDistance(float NewCullDistance);
	void SetVisibleInSceneCaptureOnly(bool bValue);
	void SetHiddenInSceneCapture(bool bValue);
	void SetTranslucentSortPriority(int32 NewTranslucentSortPriority);
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material);
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const;
	virtual int32 GetNumMaterials() const;
	UMaterialInstanceDynamic* CreateDynamicMaterialInstance(int32 ElementIndex, UMaterialInterface* SourceMaterial = nullptr, FName OptionalName = NAME_None);
	UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamic(int32 ElementIndex);
	void GetOverlappingActors(TArray<AActor*>& OverlappingActors, TSubclassOf<AActor> ClassFilter = nullptr) const;
	void GetOverlappingComponents(TArray<UPrimitiveComponent*>& OutOverlappingComponents) const;
	bool IsOverlappingActor(const AActor* Other) const;
	bool IsOverlappingComponent(const UPrimitiveComponent* OtherComp) const;
	void IgnoreActorWhenMoving(AActor* Actor, bool bShouldIgnore);
	void IgnoreComponentWhenMoving(UPrimitiveComponent* Component, bool bShouldIgnore);
	bool LineTraceComponent(FHitResult& OutHit, const FVector Start, const FVector End, const FCollisionQueryParams& Params);
	void SetLightingChannels(bool bChannel0, bool bChannel1, bool bChannel2);
	void SetBoundsScale(float NewBoundsScale = 1.f);
	void SetDefaultCustomPrimitiveDataFloat(int32 DataIndex, float Value);
	void SetCustomPrimitiveDataFloat(int32 DataIndex, float Value);
	void SetCustomPrimitiveDataVector4(int32 DataIndex, FVector4 Value);
};

class UShapeComponent : public UPrimitiveComponent
{
public:
	typedef UPrimitiveComponent Super;
	static UClass* StaticClass();
	FColor ShapeColor;
	uint8 bDrawOnlyIfSelected : 1;
	uint8 bDynamicObstacle : 1;
	void SetLineThickness(float Thickness);
};
class UBoxComponent : public UShapeComponent
{
public:
	typedef UShapeComponent Super;
	static UClass* StaticClass();
	void SetBoxExtent(FVector InBoxExtent, bool bUpdateOverlaps = true);
	FVector GetScaledBoxExtent() const;
	FVector GetUnscaledBoxExtent() const;
	void InitBoxExtent(const FVector& InBoxExtent);
};
class UCapsuleComponent : public UShapeComponent
{
public:
	typedef UShapeComponent Super;
	static UClass* StaticClass();
	void InitCapsuleSize(float InRadius, float InHalfHeight);
	void SetCapsuleSize(float InRadius, float InHalfHeight, bool bUpdateOverlaps = true);
	void SetCapsuleRadius(float Radius, bool bUpdateOverlaps = true);
	void SetCapsuleHalfHeight(float HalfHeight, bool bUpdateOverlaps = true);
	float GetScaledCapsuleRadius() const;
	float GetScaledCapsuleHalfHeight() const;
	float GetUnscaledCapsuleRadius() const;
	float GetUnscaledCapsuleHalfHeight() const;
	void GetScaledCapsuleSize(float& OutRadius, float& OutHalfHeight) const;
};
class USphereComponent : public UShapeComponent
{
public:
	typedef UShapeComponent Super;
	static UClass* StaticClass();
	void InitSphereRadius(float InSphereRadius);
	void SetSphereRadius(float InSphereRadius, bool bUpdateOverlaps = true);
	float GetScaledSphereRadius() const;
};

class UMeshComponent : public UPrimitiveComponent
{
public:
	typedef UPrimitiveComponent Super;
	static UClass* StaticClass();
	void SetScalarParameterValueOnMaterials(const FName ParameterName, const float ParameterValue);
	void SetVectorParameterValueOnMaterials(const FName ParameterName, const FVector ParameterValue);
};
class UStaticMeshComponent : public UMeshComponent
{
public:
	typedef UMeshComponent Super;
	UStaticMeshComponent();
	UStaticMeshComponent(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	virtual bool SetStaticMesh(UStaticMesh* NewMesh);
	UStaticMesh* GetStaticMesh() const;
	int32 ForcedLodModel = 0;
	uint8 bOverrideMinLOD : 1;
	int32 MinLOD = 0;
	void SetForcedLodModel(int32 NewForcedLodModel);
	void SetEvaluateWorldPositionOffset(bool NewValue);
	void SetWorldPositionOffsetDisableDistance(int32 NewValue);
};
class UInstancedStaticMeshComponent : public UStaticMeshComponent
{
public:
	typedef UStaticMeshComponent Super;
	UInstancedStaticMeshComponent();
	UInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	int32 InstanceStartCullDistance = 0;
	int32 InstanceEndCullDistance = 0;
	int32 NumCustomDataFloats = 0;
	int32 InstanceLODDistanceScale = 1;
	virtual int32 AddInstance(const FTransform& InstanceTransform, bool bWorldSpace = false);
	virtual TArray<int32> AddInstances(const TArray<FTransform>& InstanceTransforms, bool bShouldReturnIndices, bool bWorldSpace = false, bool bUpdateNavigation = true);
	virtual bool UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false);
	virtual bool RemoveInstance(int32 InstanceIndex);
	virtual void ClearInstances();
	int32 GetInstanceCount() const;
	bool GetInstanceTransform(int32 InstanceIndex, FTransform& OutInstanceTransform, bool bWorldSpace = false) const;
	void SetCullDistances(int32 StartCullDistance, int32 EndCullDistance);
	void SetNumCustomDataFloats(int32 InNumCustomDataFloats);
	bool SetCustomDataValue(int32 InstanceIndex, int32 CustomDataIndex, float CustomDataValue, bool bMarkRenderStateDirty = false);
	void PreAllocateInstancesMemory(int32 AddedInstanceCount);
};
class UHierarchicalInstancedStaticMeshComponent : public UInstancedStaticMeshComponent
{
public:
	typedef UInstancedStaticMeshComponent Super;
	UHierarchicalInstancedStaticMeshComponent();
	UHierarchicalInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	uint8 bEnableDensityScaling : 1;
	uint8 bDisableCollision : 1;
	void BuildTreeIfOutdated(bool Async, bool ForceUpdate);
};
class UProceduralMeshComponent : public UMeshComponent
{
public:
	typedef UMeshComponent Super;
	static UClass* StaticClass();
	uint8 bUseComplexAsSimpleCollision : 1;
	uint8 bUseAsyncCooking : 1;
	void CreateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FLinearColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision, bool bSRGBConversion = false);
	void CreateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);
	void ClearMeshSection(int32 SectionIndex);
	void ClearAllMeshSections();
	void SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility);
	int32 GetNumSections() const;
};
class UCableComponent : public UMeshComponent
{
public:
	typedef UMeshComponent Super;
	static UClass* StaticClass();
	bool bAttachStart = true;
	bool bAttachEnd = true;
	FVector EndLocation;
	float CableLength = 100.f;
	int32 NumSegments = 10;
	float SubstepTime = 0.02f;
	int32 SolverIterations = 1;
	bool bEnableStiffness = false;
	bool bUseSubstepping = false;
	bool bSkipCableUpdateWhenNotVisible = false;
	bool bSkipCableUpdateWhenNotOwnerRecentlyRendered = false;
	bool bEnableCollision = false;
	float CollisionFriction = 0.2f;
	FVector CableForce;
	float CableGravityScale = 1.f;
	float CableWidth = 10.f;
	int32 NumSides = 4;
	float TileMaterial = 1.f;
	void SetAttachEndTo(AActor* Actor, FName ComponentProperty, FName SocketName = NAME_None);
	void SetAttachEndToComponent(USceneComponent* Component, FName SocketName = NAME_None);
	AActor* GetAttachedActor() const;
	USceneComponent* GetAttachedComponent() const;
	void GetCableParticleLocations(TArray<FVector>& Locations) const;
};
class UTextRenderComponent : public UPrimitiveComponent
{
public:
	typedef UPrimitiveComponent Super;
	static UClass* StaticClass();
	FText Text;
	FColor TextRenderColor;
	float XScale = 1.f;
	float YScale = 1.f;
	float WorldSize = 26.f;
	void SetText(const FText& Value);
	void SetTextRenderColor(FColor Value);
	void SetWorldSize(float Value);
	void SetXScale(float Value);
	void SetYScale(float Value);
	void SetHorizontalAlignment(EHorizTextAligment Value);
	void SetVerticalAlignment(EVerticalTextAligment Value);
	void SetHorizSpacingAdjust(float Value);
	void SetFont(UFont* Value);
	void SetTextMaterial(UMaterialInterface* Material);
};

class ULightComponentBase : public USceneComponent
{
public:
	typedef USceneComponent Super;
	static UClass* StaticClass();
	float Intensity = 0.f;
	FColor LightColor;
	uint32 bAffectsWorld : 1;
	uint32 CastShadows : 1;
	uint32 CastStaticShadows : 1;
	uint32 CastDynamicShadows : 1;
	uint32 bAffectTranslucentLighting : 1;
	uint32 bCastVolumetricShadow : 1;
	float IndirectLightingIntensity = 1.f;
	float VolumetricScatteringIntensity = 1.f;
	void SetCastShadows(bool bNewValue);
	void SetCastVolumetricShadow(bool bNewValue);
	void SetAffectReflection(bool bNewValue);
	void SetAffectGlobalIllumination(bool bNewValue);
	FLinearColor GetLightColor() const;
};
class ULightComponent : public ULightComponentBase
{
public:
	typedef ULightComponentBase Super;
	static UClass* StaticClass();
	float Temperature = 6500.f;
	uint32 bUseTemperature : 1;
	float SpecularScale = 1.f;
	float ShadowBias = 0.5f;
	float ShadowSlopeBias = 0.5f;
	float ShadowResolutionScale = 1.f;
	float MaxDrawDistance = 0.f;
	float MaxDistanceFadeRange = 0.f;
	float ContactShadowLength = 0.f;
	void SetIntensity(float NewIntensity);
	void SetLightColor(FLinearColor NewLightColor, bool bSRGB = true);
	void SetTemperature(float NewTemperature);
	void SetUseTemperature(bool bNewValue);
	void SetVolumetricScatteringIntensity(float NewIntensity);
	void SetIndirectLightingIntensity(float NewIntensity);
	void SetSpecularScale(float NewValue);
	void SetShadowBias(float NewValue);
	void SetBloomScale(float NewValue);
	void SetLightFunctionMaterial(UMaterialInterface* NewLightFunctionMaterial);
};
class ULocalLightComponent : public ULightComponent
{
public:
	typedef ULightComponent Super;
	static UClass* StaticClass();
	ELightUnits IntensityUnits = ELightUnits::Unitless;
	float AttenuationRadius = 1000.f;
	float InverseExposureBlend = 0.f;
	void SetAttenuationRadius(float NewRadius);
	void SetIntensityUnits(ELightUnits NewIntensityUnits);
};
class UPointLightComponent : public ULocalLightComponent
{
public:
	typedef ULocalLightComponent Super;
	UPointLightComponent();
	UPointLightComponent(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	uint32 bUseInverseSquaredFalloff : 1;
	float LightFalloffExponent = 8.f;
	float SourceRadius = 0.f;
	float SoftSourceRadius = 0.f;
	float SourceLength = 0.f;
	void SetLightFalloffExponent(float NewLightFalloffExponent);
	void SetSourceRadius(float bNewValue);
	void SetSoftSourceRadius(float bNewValue);
	void SetSourceLength(float NewValue);
	void SetUseInverseSquaredFalloff(bool bNewValue);
};
class USpotLightComponent : public UPointLightComponent
{
public:
	typedef UPointLightComponent Super;
	USpotLightComponent();
	USpotLightComponent(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	float InnerConeAngle = 0.f;
	float OuterConeAngle = 44.f;
	void SetInnerConeAngle(float NewInnerConeAngle);
	void SetOuterConeAngle(float NewOuterConeAngle);
};
class UDirectionalLightComponent : public ULightComponent
{
public:
	typedef ULightComponent Super;
	static UClass* StaticClass();
	uint32 bAtmosphereSunLight : 1;
	int32 AtmosphereSunLightIndex = 0;
	FLinearColor AtmosphereSunDiskColorScale;
	uint32 bCastCloudShadows : 1;
	uint32 bCastShadowsOnClouds : 1;
	uint32 bCastShadowsOnAtmosphere : 1;
	uint32 bPerPixelAtmosphereTransmittance : 1;
	float LightSourceAngle = 0.5357f;
	float LightSourceSoftAngle = 0.f;
	float DynamicShadowDistanceMovableLight = 20000.f;
	float DynamicShadowDistanceStationaryLight = 0.f;
	int32 DynamicShadowCascades = 3;
	float CascadeDistributionExponent = 3.f;
	float CascadeTransitionFraction = 0.1f;
	float ShadowDistanceFadeoutFraction = 0.1f;
	float CloudShadowStrength = 1.f;
	void SetDynamicShadowDistanceMovableLight(float NewValue);
	void SetDynamicShadowCascades(int32 NewValue);
	void SetCascadeDistributionExponent(float NewValue);
	void SetLightSourceAngle(float NewValue);
	void SetAtmosphereSunLight(bool bNewValue);
	void SetAtmosphereSunLightIndex(int32 NewValue);
	void SetCastCloudShadows(bool bNewValue);
};
class USkyLightComponent : public ULightComponentBase
{
public:
	typedef ULightComponentBase Super;
	static UClass* StaticClass();
	uint8 bRealTimeCapture : 1;
	TEnumAsByte<ESkyLightSourceType> SourceType;
	float SkyDistanceThreshold = 150000.f;
	uint8 bCaptureEmissiveOnly : 1;
	uint8 bLowerHemisphereIsBlack : 1;
	FLinearColor LowerHemisphereColor;
	float OcclusionMaxDistance = 1000.f;
	float Contrast = 0.f;
	float MinOcclusion = 0.f;
	FColor OcclusionTint;
	void SetIntensity(float NewIntensity);
	void SetLightColor(FLinearColor NewLightColor);
	void SetLowerHemisphereColor(const FLinearColor& InLowerHemisphereColor);
	void SetVolumetricScatteringIntensity(float NewIntensity);
	void SetRealTimeCapture(bool bInRealTimeCapture);
	void RecaptureSky();
};
class USkyAtmosphereComponent : public USceneComponent
{
public:
	typedef USceneComponent Super;
	static UClass* StaticClass();
	float BottomRadius = 6360.f;
	FColor GroundAlbedo;
	float AtmosphereHeight = 100.f;
	float MultiScatteringFactor = 1.f;
	float RayleighScatteringScale = 0.0331f;
	FLinearColor RayleighScattering;
	float RayleighExponentialDistribution = 8.f;
	float MieScatteringScale = 0.003996f;
	FLinearColor MieScattering;
	float MieAnisotropy = 0.8f;
	float MieExponentialDistribution = 1.2f;
	FLinearColor SkyLuminanceFactor;
	float AerialPespectiveViewDistanceScale = 1.f;
	float HeightFogContribution = 1.f;
	void SetRayleighScatteringScale(float NewValue);
	void SetRayleighScattering(FLinearColor NewValue);
	void SetMieScatteringScale(float NewValue);
	void SetMieAnisotropy(float NewValue);
	void SetSkyLuminanceFactor(FLinearColor NewValue);
	void SetAerialPespectiveViewDistanceScale(float NewValue);
	void SetHeightFogContribution(float NewValue);
	void SetMultiScatteringFactor(float NewValue);
};
class UExponentialHeightFogComponent : public USceneComponent
{
public:
	typedef USceneComponent Super;
	static UClass* StaticClass();
	float FogDensity = 0.02f;
	float FogHeightFalloff = 0.2f;
	float FogMaxOpacity = 1.f;
	float StartDistance = 0.f;
	FLinearColor FogInscatteringLuminance;
	FLinearColor DirectionalInscatteringLuminance;
	bool bEnableVolumetricFog = false;
	float VolumetricFogScatteringDistribution = 0.2f;
	FColor VolumetricFogAlbedo;
	FLinearColor VolumetricFogEmissive;
	float VolumetricFogExtinctionScale = 1.f;
	float VolumetricFogDistance = 6000.f;
	void SetFogDensity(float Value);
	void SetFogHeightFalloff(float Value);
	void SetFogMaxOpacity(float Value);
	void SetStartDistance(float Value);
	void SetFogInscatteringColor(FLinearColor Value);
	void SetDirectionalInscatteringColor(FLinearColor Value);
	void SetVolumetricFog(bool bNewValue);
	void SetVolumetricFogScatteringDistribution(float NewValue);
	void SetVolumetricFogExtinctionScale(float NewValue);
	void SetVolumetricFogAlbedo(FColor NewValue);
	void SetVolumetricFogEmissive(FLinearColor NewValue);
	void SetVolumetricFogDistance(float NewValue);
	void SetSecondFogDensity(float Value);
	void SetSecondFogHeightFalloff(float Value);
	void SetSecondFogHeightOffset(float Value);
};
class UVolumetricCloudComponent : public USceneComponent
{
public:
	typedef USceneComponent Super;
	static UClass* StaticClass();
	float LayerBottomAltitude = 5.f;
	float LayerHeight = 10.f;
	float TracingStartMaxDistance = 350.f;
	float TracingMaxDistance = 50.f;
	float PlanetRadius = 6360.f;
	FColor GroundAlbedo;
	TObjectPtr<UMaterialInterface> Material;
	bool bUsePerSampleAtmosphericLightTransmittance = false;
	float SkyLightCloudBottomOcclusion = 0.5f;
	float ViewSampleCountScale = 1.f;
	float ShadowViewSampleCountScale = 1.f;
	void SetLayerBottomAltitude(float NewValue);
	void SetLayerHeight(float NewValue);
	void SetPlanetRadius(float NewValue);
	void SetMaterial(UMaterialInterface* NewValue);
	void SetSkyLightCloudBottomOcclusion(float NewValue);
	void SetViewSampleCountScale(float NewValue);
};
class UPostProcessComponent : public USceneComponent
{
public:
	typedef USceneComponent Super;
	static UClass* StaticClass();
	FPostProcessSettings Settings;
	float Priority = 0.f;
	float BlendRadius = 100.f;
	float BlendWeight = 1.f;
	uint32 bEnabled : 1;
	uint32 bUnbound : 1;
};

struct FEngineShowFlags
{
	void SetFog(bool b);
	void SetAtmosphere(bool b);
	void SetCloud(bool b);
	void SetDynamicShadows(bool b);
	void SetPostProcessing(bool b);
	void SetSkyLighting(bool b);
	void SetLighting(bool b);
	void SetBloom(bool b);
	void SetMotionBlur(bool b);
	void SetAntiAliasing(bool b);
	void SetEyeAdaptation(bool b);
	void SetGlobalIllumination(bool b);
	void SetLumenGlobalIllumination(bool b);
	void SetLumenReflections(bool b);
	void SetVolumetricFog(bool b);
	void SetTemporalAA(bool b);
	void SetAmbientOcclusion(bool b);
	void SetScreenSpaceReflections(bool b);
	void SetInstancedFoliage(bool b);
	void SetInstancedStaticMeshes(bool b);
	void SetTranslucency(bool b);
};
class USceneCaptureComponent : public USceneComponent
{
public:
	typedef USceneComponent Super;
	static UClass* StaticClass();
	uint8 bCaptureEveryFrame : 1;
	uint8 bCaptureOnMovement : 1;
	uint8 bAlwaysPersistRenderingState : 1;
	TArray<TWeakObjectPtr<AActor>> HiddenActors;
	TArray<TObjectPtr<AActor>> ShowOnlyActors;
	FEngineShowFlags ShowFlags;
	float LODDistanceFactor = 1.f;
	float MaxViewDistanceOverride = -1.f;
	int32 CaptureSortPriority = 0;
	void HideActorComponents(AActor* InActor, const bool bIncludeFromChildActors = false);
	void HideComponent(UPrimitiveComponent* InComponent);
	void ClearHiddenComponents();
};
class USceneCaptureComponent2D : public USceneCaptureComponent
{
public:
	typedef USceneCaptureComponent Super;
	static UClass* StaticClass();
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionType;
	float FOVAngle = 90.f;
	float OrthoWidth = 512.f;
	TObjectPtr<UTextureRenderTarget2D> TextureTarget;
	TEnumAsByte<ESceneCaptureSource> CaptureSource;
	FPostProcessSettings PostProcessSettings;
	float PostProcessBlendWeight = 1.f;
	uint8 bUseCustomProjectionMatrix : 1;
	uint8 bEnableClipPlane : 1;
	void CaptureScene();
};
class UCameraComponent : public USceneComponent
{
public:
	typedef USceneComponent Super;
	static UClass* StaticClass();
	float FieldOfView = 90.f;
	float OrthoWidth = 512.f;
	float AspectRatio = 1.777778f;
	uint8 bConstrainAspectRatio : 1;
	uint8 bUsePawnControlRotation : 1;
	uint8 bLockToHmd : 1;
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode;
	float PostProcessBlendWeight = 1.f;
	FPostProcessSettings PostProcessSettings;
	virtual void SetFieldOfView(float InFieldOfView);
	void SetAspectRatio(float InAspectRatio);
	void SetConstraintAspectRatio(bool bInConstrainAspectRatio);
	void SetPostProcessBlendWeight(float InPostProcessBlendWeight);
	void SetProjectionMode(ECameraProjectionMode::Type InProjectionMode);
};

class UMovementComponent : public UActorComponent
{
public:
	typedef UActorComponent Super;
	static UClass* StaticClass();
	FVector Velocity;
	uint8 bConstrainToPlane : 1;
	uint8 bSnapToPlaneAtStart : 1;
	USceneComponent* UpdatedComponent = nullptr;
	virtual float GetMaxSpeed() const;
	virtual void StopMovementImmediately();
	bool IsMovingOnGround() const;
	bool IsFalling() const;
};
struct FNavAgentProperties
{
	uint32 bCanCrouch : 1;
	uint32 bCanJump : 1;
	uint32 bCanWalk : 1;
	uint32 bCanSwim : 1;
	uint32 bCanFly : 1;
	float AgentRadius = -1.f;
	float AgentHeight = -1.f;
};
struct FMovementProperties
{
	uint8 bCanCrouch : 1;
	uint8 bCanJump : 1;
	uint8 bCanWalk : 1;
};
class UNavMovementComponent : public UMovementComponent
{
public:
	typedef UMovementComponent Super;
	FNavAgentProperties NavAgentProps;
	FNavAgentProperties& GetNavAgentPropertiesRef();
	bool CanEverCrouch() const;
	void SetCanEverCrouch(bool bValue);
};
class UPawnMovementComponent : public UNavMovementComponent
{
public:
	typedef UNavMovementComponent Super;
	APawn* GetPawnOwner() const;
	FVector GetPendingInputVector() const;
	FVector ConsumeInputVector();
};
struct FFindFloorResult
{
	uint32 bBlockingHit : 1;
	uint32 bWalkableFloor : 1;
	float FloorDist = 0.f;
	FHitResult HitResult;
	bool IsWalkableFloor() const;
};
class UCharacterMovementComponent : public UPawnMovementComponent
{
public:
	typedef UPawnMovementComponent Super;
	static UClass* StaticClass();
	TEnumAsByte<EMovementMode> MovementMode;
	uint8 CustomMovementMode = 0;
	float GravityScale = 1.f;
	float MaxStepHeight = 45.f;
	float JumpZVelocity = 420.f;
	float JumpOffJumpZFactor = 0.5f;
	float GroundFriction = 8.f;
	float MaxWalkSpeed = 600.f;
	float MaxWalkSpeedCrouched = 300.f;
	float MaxSwimSpeed = 300.f;
	float MaxFlySpeed = 600.f;
	float MaxCustomMovementSpeed = 600.f;
	float MaxAcceleration = 2048.f;
	float MinAnalogWalkSpeed = 0.f;
	float BrakingFrictionFactor = 2.f;
	float BrakingFriction = 0.f;
	float BrakingSubStepTime = 1.f / 33.f;
	float BrakingDecelerationWalking = 2048.f;
	float BrakingDecelerationFalling = 0.f;
	float AirControl = 0.05f;
	float AirControlBoostMultiplier = 2.f;
	float FallingLateralFriction = 0.f;
	float CrouchedHalfHeight = 40.f;
	float Buoyancy = 1.f;
	float PerchRadiusThreshold = 0.f;
	float PerchAdditionalHeight = 40.f;
	FRotator RotationRate;
	uint8 bUseSeparateBrakingFriction : 1;
	uint8 bApplyGravityWhileJumping : 1;
	uint8 bUseControllerDesiredRotation : 1;
	uint8 bOrientRotationToMovement : 1;
	uint8 bSweepWhileNavWalking : 1;
	uint8 bCanWalkOffLedges : 1;
	uint8 bCanWalkOffLedgesWhenCrouching : 1;
	uint8 bRunPhysicsWithNoController : 1;
	uint8 bUseRVOAvoidance : 1;
	uint8 bRequestedMoveUseAcceleration : 1;
	uint8 bWantsToCrouch : 1;
	uint8 bIgnoreBaseRotation : 1;
	uint8 bMaintainHorizontalGroundVelocity : 1;
	uint8 bImpartBaseVelocityX : 1;
	uint8 bImpartBaseVelocityY : 1;
	uint8 bImpartBaseVelocityZ : 1;
	uint8 bEnablePhysicsInteraction : 1;
	uint8 bPushForceUsingZOffset : 1;
	float StandingDownwardForceScale = 1.f;
	float InitialPushForceFactor = 500.f;
	float PushForceFactor = 750000.f;
	FFindFloorResult CurrentFloor;
	float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const;
	void SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode = 0);
	void SetWalkableFloorAngle(float InWalkableFloorAngle);
	float GetWalkableFloorAngle() const;
	void SetWalkableFloorZ(float InWalkableFloorZ);
	void DisableMovement();
	void StopMovementImmediately() override;
	bool IsMovingOnGround() const;
	bool IsFalling() const;
	bool IsCrouching() const;
	bool IsSwimming() const;
	bool IsFlying() const;
	bool IsWalking() const;
	void AddImpulse(FVector Impulse, bool bVelocityChange = false);
	void AddForce(FVector Force);
	void Launch(FVector const& LaunchVel);
	FVector GetCurrentAcceleration() const;
	float GetGravityZ() const;
	void SetAvoidanceEnabled(bool bEnable);
};

class USoundBase : public UObject
{
public:
	static UClass* StaticClass();
	virtual float GetDuration() const;
};
struct FSoundAttenuationSettings
{
	uint8 bAttenuate : 1;
	uint8 bSpatialize : 1;
	uint8 bAttenuateWithLPF : 1;
	uint8 bEnableListenerFocus : 1;
	uint8 bEnableOcclusion : 1;
	uint8 bUseComplexCollisionForOcclusion : 1;
	uint8 bEnableReverbSend : 1;
	uint8 bEnablePriorityAttenuation : 1;
	uint8 bApplyNormalizationToStereoSounds : 1;
	uint8 bEnableLogFrequencyScaling : 1;
	uint8 bEnableSubmixSends : 1;
	EAttenuationDistanceModel DistanceAlgorithm = EAttenuationDistanceModel::Linear;
	TEnumAsByte<EAttenuationShape::Type> AttenuationShape;
	float dBAttenuationAtMax = -60.f;
	FVector AttenuationShapeExtents;
	float ConeOffset = 0.f;
	float FalloffDistance = 3600.f;
	float LPFRadiusMin = 3000.f;
	float LPFRadiusMax = 6000.f;
	float LPFFrequencyAtMin = 20000.f;
	float LPFFrequencyAtMax = 20000.f;
	float HPFFrequencyAtMin = 0.f;
	float HPFFrequencyAtMax = 0.f;
	float StereoSpread = 200.f;
	TEnumAsByte<ECollisionChannel> OcclusionTraceChannel;
	float OcclusionLowPassFilterFrequency = 20000.f;
	float OcclusionVolumeAttenuation = 0.f;
	float OcclusionInterpolationTime = 0.1f;
	float ReverbWetLevelMin = 0.3f;
	float ReverbWetLevelMax = 0.95f;
	float ReverbDistanceMin = 0.f;
	float ReverbDistanceMax = 0.f;
	float ManualReverbSendLevel = 0.2f;
	float NonSpatializedRadiusStart = 0.f;
	float NonSpatializedRadiusEnd = 0.f;
	FSoundAttenuationSettings();
};
class USoundAttenuation : public UObject
{
public:
	static UClass* StaticClass();
	FSoundAttenuationSettings Attenuation;
};
class USoundClass : public UObject {};
class USoundSubmixBase : public UObject {};
class USoundEffectSourcePresetChain : public UObject {};
class UReverbEffect : public UObject
{
public:
	static UClass* StaticClass();
	float Density = 1.f;
	float Diffusion = 1.f;
	float Gain = 0.32f;
	float GainHF = 0.89f;
	float DecayTime = 1.49f;
	float DecayHFRatio = 0.83f;
	float ReflectionsGain = 0.05f;
	float ReflectionsDelay = 0.007f;
	float LateGain = 1.26f;
	float LateDelay = 0.011f;
	float AirAbsorptionGainHF = 0.994f;
	float RoomRolloffFactor = 0.f;
	bool bBypassEarlyReflections = false;
	bool bBypassLateReflections = false;
};
class USynthComponent : public USceneComponent
{
public:
	typedef USceneComponent Super;
	USynthComponent(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	uint8 bAutoDestroy : 1;
	uint8 bStopWhenOwnerDestroyed : 1;
	uint8 bAllowSpatialization : 1;
	uint8 bOverrideAttenuation : 1;
	uint8 bOutputToBusOnly : 1;
	uint8 bIsUISound : 1;
	uint8 bIsPreviewSound : 1;
	TObjectPtr<USoundAttenuation> AttenuationSettings;
	FSoundAttenuationSettings AttenuationOverrides;
	TObjectPtr<USoundClass> SoundClass;
	TObjectPtr<USoundEffectSourcePresetChain> SourceEffectChain;
	int32 PreferredBufferLength = 512;
	int32 NumChannels = 1;
	virtual void Start();
	virtual void Stop();
	bool IsPlaying() const;
	void SetVolumeMultiplier(float VolumeMultiplier);
	void SetLowPassFilterEnabled(bool InLowPassFilterEnabled);
	void SetLowPassFilterFrequency(float InLowPassFilterFrequency);
	void SetSubmixSend(USoundSubmixBase* Submix, float SendLevel);
	void FadeIn(float FadeInDuration, float FadeVolumeLevel = 1.f, float StartTime = 0.f);
	void FadeOut(float FadeOutDuration, float FadeVolumeLevel);
protected:
	virtual bool Init(int32& SampleRate);
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) = 0;
	virtual void OnBeginGenerate();
	virtual void OnEndGenerate();
};
struct FVoiceSettings
{
	TObjectPtr<USceneComponent> ComponentToAttachTo;
	TObjectPtr<USoundAttenuation> AttenuationSettings;
	TObjectPtr<USoundEffectSourcePresetChain> SourceEffectChain;
};
class UVOIPTalker : public UActorComponent
{
public:
	typedef UActorComponent Super;
	static UClass* StaticClass();
	FVoiceSettings Settings;
	static UVOIPTalker* CreateTalkerForPlayer(APlayerState* OwningState);
	void RegisterWithPlayerState(APlayerState* OwningState);
	float GetVoiceLevel();
};
class UVOIPStatics : public UObject
{
public:
	static void SetMicThreshold(float InThreshold);
	static float GetVoiceSampleRate();
};

// ================================================================== assets
class UStaticMesh : public UObject
{
public:
	static UClass* StaticClass();
	FBox GetBoundingBox() const;
};
class UMaterialInterface : public UObject
{
public:
	static UClass* StaticClass();
};
class UMaterial : public UMaterialInterface
{
public:
	static UClass* StaticClass();
	static UMaterial* GetDefaultMaterial(int32 Domain);
};
class UMaterialInstance : public UMaterialInterface {};
class UMaterialInstanceDynamic : public UMaterialInstance
{
public:
	static UClass* StaticClass();
	static UMaterialInstanceDynamic* Create(UMaterialInterface* ParentMaterial, UObject* InOuter);
	static UMaterialInstanceDynamic* Create(UMaterialInterface* ParentMaterial, UObject* InOuter, FName Name);
	void SetScalarParameterValue(FName ParameterName, float Value);
	void SetVectorParameterValue(FName ParameterName, const FLinearColor& Value);
	void SetTextureParameterValue(FName ParameterName, UTexture* Value);
	bool GetVectorParameterValue(FName ParameterName, FLinearColor& OutValue) const;
};
struct FTexture2DMipMap
{
	int32 SizeX = 0;
	int32 SizeY = 0;
	struct FBulk
	{
		void* Lock(uint32 LockFlags);
		const void* LockReadOnly() const;
		void Unlock() const;
		int64 GetBulkDataSize() const;
		void* Realloc(int64 InElementCount);
	} BulkData;
};
#define LOCK_READ_ONLY 1
#define LOCK_READ_WRITE 2
struct FTexturePlatformData
{
	int32 SizeX = 0;
	int32 SizeY = 0;
	TArray<FTexture2DMipMap> Mips;
};
class FTextureResource {};
class FTextureRenderTargetResource : public FTextureResource {};
class UTexture : public UObject
{
public:
	static UClass* StaticClass();
	TEnumAsByte<TextureFilter> Filter;
	TEnumAsByte<TextureCompressionSettings> CompressionSettings;
	uint8 SRGB : 1;
	uint8 NeverStream : 1;
	int32 LODGroup = 0;
	FTextureResource* GetResource();
	const FTextureResource* GetResource() const;
	virtual void UpdateResource();
};
class UTexture2D : public UTexture
{
public:
	static UClass* StaticClass();
	TEnumAsByte<TextureAddress> AddressX;
	TEnumAsByte<TextureAddress> AddressY;
	static UTexture2D* CreateTransient(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat = PF_B8G8R8A8, const FName InName = NAME_None);
	FTexturePlatformData* GetPlatformData();
	const FTexturePlatformData* GetPlatformData() const;
	int32 GetSizeX() const;
	int32 GetSizeY() const;
	void UpdateResource() override;
};
class UTextureRenderTarget : public UTexture {};
class UTextureRenderTarget2D : public UTextureRenderTarget
{
public:
	static UClass* StaticClass();
	int32 SizeX = 0;
	int32 SizeY = 0;
	FLinearColor ClearColor;
	TEnumAsByte<ETextureRenderTargetFormat> RenderTargetFormat;
	uint8 bAutoGenerateMips : 1;
	void InitAutoFormat(uint32 InSizeX, uint32 InSizeY);
	void InitCustomFormat(uint32 InSizeX, uint32 InSizeY, EPixelFormat InOverrideFormat, bool bInForceLinearGamma);
	void ResizeTarget(uint32 InSizeX, uint32 InSizeY);
	void UpdateResourceImmediate(bool bClearRenderTarget = true);
	FTextureRenderTargetResource* GameThread_GetRenderTargetResource();
};
class UFont : public UObject {};
class UDataTable : public UObject
{
public:
	template <class T> T* FindRow(FName RowName, const TCHAR* ContextString, bool bWarnIfRowMissing = true) const;
	template <class T> void GetAllRows(const TCHAR* ContextString, TArray<T*>& OutRowArray) const;
	TArray<FName> GetRowNames() const;
};

// ================================================================== actors
class UInputComponent : public UActorComponent
{
public:
	static UClass* StaticClass();
};
class AActor : public UObject
{
public:
	typedef UObject Super;
	AActor();
	AActor(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	FActorTickFunction PrimaryActorTick;
	uint8 bAlwaysRelevant : 1;
	uint8 bOnlyRelevantToOwner : 1;
	uint8 bNetLoadOnClient : 1;
	uint8 bNetUseOwnerRelevancy : 1;
	uint8 bReplayRewindable : 1;
	uint8 bCanBeDamaged : 1;
	uint8 bFindCameraComponentWhenViewTarget : 1;
	uint8 bBlockInput : 1;
	uint8 bEnableAutoLODGeneration : 1;
	uint8 bIsEditorOnlyActor : 1;
	TArray<FName> Tags;
	TObjectPtr<UInputComponent> InputComponent;
	float InitialLifeSpan = 0.f;
	float CustomTimeDilation = 1.f;

	UWorld* GetWorld() const override;
	UGameInstance* GetGameInstance() const;
	template <class T> T* GetGameInstance() const;
	FTimerManager& GetWorldTimerManager() const;
	FBox GetComponentsBoundingBox(bool bNonColliding = false, bool bIncludeFromChildActors = false) const;
	ENetMode GetNetMode() const;
	bool IsNetMode(ENetMode Mode) const;
	bool HasAuthority() const;
	ENetRole GetLocalRole() const;
	ENetRole GetRemoteRole() const;
	void SetReplicates(bool bInReplicates);
	bool GetIsReplicated() const;
	void SetReplicatingMovement(bool bInReplicateMovement);
	bool IsReplicatingMovement() const;
	void SetNetUpdateFrequency(float Frequency);
	float GetNetUpdateFrequency() const;
	void SetNetCullDistanceSquared(float DistanceSq);
	void SetMinNetUpdateFrequency(float Frequency);
	void ForceNetUpdate();
	void FlushNetDormancy();
	bool IsActorInitialized() const;
	bool HasActorBegunPlay() const;
	bool IsActorBeingDestroyed() const;
	virtual bool Destroy(bool bNetForce = false, bool bShouldModifyLevel = true);
	virtual void SetLifeSpan(float InLifespan);
	float GetLifeSpan() const;

	FVector GetActorLocation() const;
	FRotator GetActorRotation() const;
	FQuat GetActorQuat() const;
	FVector GetActorScale3D() const;
	const FTransform& GetActorTransform() const;
	FVector GetActorForwardVector() const;
	FVector GetActorRightVector() const;
	FVector GetActorUpVector() const;
	bool SetActorLocation(const FVector& NewLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	bool SetActorRotation(FRotator NewRotation, ETeleportType Teleport = ETeleportType::None);
	bool SetActorRotation(const FQuat& NewRotation, ETeleportType Teleport = ETeleportType::None);
	bool SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	bool SetActorLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	bool SetActorTransform(const FTransform& NewTransform, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetActorScale3D(FVector NewScale3D);
	void SetActorRelativeLocation(FVector NewRelativeLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddActorWorldOffset(FVector DeltaLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddActorWorldRotation(FRotator DeltaRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddActorLocalOffset(FVector DeltaLocation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddActorLocalRotation(FRotator DeltaRotation, bool bSweep = false, FHitResult* OutSweepHitResult = nullptr, ETeleportType Teleport = ETeleportType::None);
	virtual FVector GetVelocity() const;
	float GetDistanceTo(const AActor* OtherActor) const;
	float GetSquaredDistanceTo(const AActor* OtherActor) const;
	float GetHorizontalDistanceTo(const AActor* OtherActor) const;
	void GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent, bool bIncludeFromChildActors = false) const;
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	USceneComponent* GetRootComponent() const;
	bool SetRootComponent(USceneComponent* NewRootComponent);
	void AttachToComponent(USceneComponent* Parent, const FAttachmentTransformRules& AttachmentRules, FName SocketName = NAME_None);
	void AttachToActor(AActor* ParentActor, const FAttachmentTransformRules& AttachmentRules, FName SocketName = NAME_None);
	void DetachFromActor(const FDetachmentTransformRules& DetachmentRules);
	AActor* GetAttachParentActor() const;
	void GetAttachedActors(TArray<AActor*>& OutActors, bool bResetArray = true, bool bRecursivelyIncludeAttachedActors = false) const;
	template <class T> void GetComponents(TArray<T*>& OutComponents, bool bIncludeFromChildActors = false) const;
	template <class T> T* FindComponentByClass() const;
	UActorComponent* GetComponentByClass(TSubclassOf<UActorComponent> ComponentClass) const;
	void AddInstanceComponent(UActorComponent* Component);
	void AddOwnedComponent(UActorComponent* Component);

	void SetActorHiddenInGame(bool bNewHidden);
	bool IsHidden() const;
	void SetHidden(bool bInHidden);
	void SetActorEnableCollision(bool bNewActorEnableCollision);
	bool GetActorEnableCollision() const;
	void SetActorTickEnabled(bool bEnabled);
	bool IsActorTickEnabled() const;
	void SetActorTickInterval(float TickInterval);
	float GetActorTickInterval() const;
	void SetTickableWhenPaused(bool bTickableWhenPaused);
	bool ActorHasTag(FName Tag) const;
	AActor* GetOwner() const;
	template <class T> T* GetOwner() const;
	virtual void SetOwner(AActor* NewOwner);
	APawn* GetInstigator() const;
	template <class T> T* GetInstigator() const;
	AController* GetInstigatorController() const;
	void SetInstigator(APawn* InInstigator);
	void FinishSpawning(const FTransform& Transform, bool bIsDefaultTransform = false, const struct FComponentInstanceDataCache* InstanceDataCache = nullptr);
	ULevel* GetLevel() const;
	void EnableInput(APlayerController* PlayerController);
	void DisableInput(APlayerController* PlayerController);
	float GetGameTimeSinceCreation() const;

	virtual void BeginPlay();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	virtual void Tick(float DeltaSeconds);
	virtual void PostInitializeComponents();
	virtual void PreInitializeComponents();
	virtual void OnConstruction(const FTransform& Transform);
	virtual void PostActorCreated();
	virtual void Destroyed();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor);
	virtual void NotifyActorEndOverlap(AActor* OtherActor);
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit);
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser);
	virtual void PostNetInit();
	virtual void OnRep_ReplicatedMovement();
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const;

protected:
	uint8 bReplicates : 1;
	TObjectPtr<USceneComponent> RootComponent;
};

class AInfo : public AActor
{
public:
	typedef AActor Super;
	AInfo();
	AInfo(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
};

class AController;
class APawn : public AActor
{
public:
	typedef AActor Super;
	APawn();
	APawn(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	uint32 bUseControllerRotationPitch : 1;
	uint32 bUseControllerRotationYaw : 1;
	uint32 bUseControllerRotationRoll : 1;
	uint32 bCanAffectNavigationGeneration : 1;
	float BaseEyeHeight = 64.f;
	EAutoPossessAI AutoPossessAI = EAutoPossessAI::PlacedInWorld;
	TSubclassOf<AController> AIControllerClass;
	TObjectPtr<AController> Controller;
	AController* GetController() const;
	template <class T> T* GetController() const;
	APlayerState* GetPlayerState() const;
	template <class T> T* GetPlayerState() const;
	bool IsLocallyControlled() const;
	bool IsPlayerControlled() const;
	bool IsBotControlled() const;
	virtual FRotator GetBaseAimRotation() const;
	FRotator GetControlRotation() const;
	virtual FRotator GetViewRotation() const;
	virtual FVector GetPawnViewLocation() const;
	APlayerController* GetLocalViewingPlayerController() const;
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.f, bool bForce = false);
	FVector GetPendingMovementInputVector() const;
	FVector GetLastMovementInputVector() const;
	FVector ConsumeMovementInputVector();
	virtual void AddControllerPitchInput(float Val);
	virtual void AddControllerYawInput(float Val);
	virtual void AddControllerRollInput(float Val);
	UPawnMovementComponent* GetMovementComponent() const;
	virtual void PossessedBy(AController* NewController);
	virtual void UnPossessed();
	virtual void OnRep_PlayerState();
	virtual void OnRep_Controller();
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);
	virtual void PawnClientRestart();
	virtual void Restart();
	virtual void NotifyControllerChanged();
	virtual void DetachFromControllerPendingDestroy();
	virtual void TurnOff();
	virtual FVector GetNavAgentLocation() const;
	void SpawnDefaultController();
	FVector GetVelocity() const override;
};

class UCharacterMovementComponent;
class ACharacter : public APawn
{
public:
	typedef APawn Super;
	ACharacter();
	ACharacter(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	uint32 bIsCrouched : 1;
	uint32 bPressedJump : 1;
	float CrouchedEyeHeight = 32.f;
	float JumpMaxHoldTime = 0.f;
	int32 JumpMaxCount = 1;
	int32 JumpCurrentCount = 0;
	UCharacterMovementComponent* GetCharacterMovement() const;
	UCapsuleComponent* GetCapsuleComponent() const;
	class USkeletalMeshComponent* GetMesh() const;
	virtual void Jump();
	virtual void StopJumping();
	virtual bool CanJump() const;
	virtual void Crouch(bool bClientSimulation = false);
	virtual void UnCrouch(bool bClientSimulation = false);
	virtual bool CanCrouch() const;
	virtual void Landed(const FHitResult& Hit);
	virtual void OnJumped();
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);
	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride);
	virtual void FellOutOfWorld(const class UDamageType& DmgType);
};

class APlayerState;
class AController : public AActor
{
public:
	typedef AActor Super;
	static UClass* StaticClass();
	TObjectPtr<APlayerState> PlayerState;
	APawn* GetPawn() const;
	template <class T> T* GetPawn() const;
	template <class T> T* GetPlayerState() const;
	virtual FRotator GetControlRotation() const;
	virtual void SetControlRotation(const FRotator& NewRotation);
	virtual void SetInitialLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation);
	bool IsLocalController() const;
	bool IsLocalPlayerController() const;
	bool IsPlayerController() const;
	virtual void Possess(APawn* InPawn);
	virtual void UnPossess();
	virtual void StopMovement();
	virtual void OnPossess(APawn* InPawn);
	virtual void OnUnPossess();
	virtual void GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const;
	virtual bool LineOfSightTo(const AActor* Other, FVector ViewPoint = FVector(0, 0, 0), bool bAlternateChecks = false) const;
	void SetIgnoreMoveInput(bool bNewMoveInput);
	void SetIgnoreLookInput(bool bNewLookInput);
};
namespace EPathFollowingRequestResult { enum Type { Failed, AlreadyAtGoal, RequestSuccessful }; }
class UNavigationQueryFilter : public UObject {};
class AAIController : public AController
{
public:
	typedef AController Super;
	static UClass* StaticClass();
	EPathFollowingRequestResult::Type MoveToLocation(const FVector& Dest, float AcceptanceRadius = -1, bool bStopOnOverlap = true, bool bUsePathfinding = true, bool bProjectDestinationToNavigation = false, bool bCanStrafe = true, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr, bool bAllowPartialPath = true);
	EPathFollowingRequestResult::Type MoveToActor(AActor* Goal, float AcceptanceRadius = -1, bool bStopOnOverlap = true, bool bUsePathfinding = true, bool bCanStrafe = true, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr, bool bAllowPartialPath = true);
	void StopMovement() override;
	void SetFocus(AActor* NewFocus, uint8 InPriority = 0);
	void SetFocalPoint(FVector NewFocus, uint8 InPriority = 0);
	void ClearFocus(uint8 InPriority);
};

struct FInputModeDataBase
{
	virtual ~FInputModeDataBase() = default;
};
struct FInputModeGameOnly : FInputModeDataBase
{
	FInputModeGameOnly& SetConsumeCaptureMouseDown(bool InConsumeCaptureMouseDown);
};
struct FInputModeGameAndUI : FInputModeDataBase
{
	FInputModeGameAndUI& SetLockMouseToViewportBehavior(EMouseLockMode InMouseLockMode);
	FInputModeGameAndUI& SetHideCursorDuringCapture(bool InHideCursorDuringCapture);
};
struct FInputModeUIOnly : FInputModeDataBase
{
	FInputModeUIOnly& SetLockMouseToViewportBehavior(EMouseLockMode InMouseLockMode);
};
struct FViewTargetTransitionParams
{
	float BlendTime = 0.f;
	TEnumAsByte<EViewTargetBlendFunction> BlendFunction;
	float BlendExp = 2.f;
	uint32 bLockOutgoing : 1;
};

class APlayerCameraManager : public AActor
{
public:
	typedef AActor Super;
	static UClass* StaticClass();
	float ViewPitchMin = -89.9f;
	float ViewPitchMax = 89.9f;
	float ViewYawMin = 0.f;
	float ViewYawMax = 359.999f;
	float ViewRollMin = -89.9f;
	float ViewRollMax = 89.9f;
	FVector GetCameraLocation() const;
	FRotator GetCameraRotation() const;
	float GetFOVAngle() const;
	void StartCameraFade(float FromAlpha, float ToAlpha, float Duration, FLinearColor Color, bool bShouldFadeAudio = false, bool bHoldWhenFinished = false);
	void StopCameraFade();
	void SetManualCameraFade(float InFadeAmount, FLinearColor Color, bool bInFadeAudio);
	AActor* GetViewTarget() const;
};
class ACameraActor : public AActor
{
public:
	typedef AActor Super;
	static UClass* StaticClass();
	UCameraComponent* GetCameraComponent() const;
};

class UPlayer : public UObject
{
public:
	TObjectPtr<APlayerController> PlayerController;
};
class ULocalPlayer : public UPlayer
{
public:
	static UClass* StaticClass();
	template <class T> T* GetSubsystem() const;
	template <class T> static T* GetSubsystem(const ULocalPlayer* LocalPlayer);
	UGameInstance* GetGameInstance() const;
	int32 GetControllerId() const;
	TObjectPtr<UGameViewportClient> ViewportClient;
};

class AHUD;
class APlayerController : public AController
{
public:
	typedef AController Super;
	APlayerController();
	APlayerController(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	uint32 bShowMouseCursor : 1;
	uint32 bEnableClickEvents : 1;
	uint32 bEnableTouchEvents : 1;
	uint32 bEnableMouseOverEvents : 1;
	uint32 bAutoManageActiveCameraTarget : 1;
	uint32 bInputEnabled : 1;
	TObjectPtr<APlayerCameraManager> PlayerCameraManager;
	TObjectPtr<UPlayer> Player;
	AHUD* GetHUD() const;
	template <class T> T* GetHUD() const;
	ULocalPlayer* GetLocalPlayer() const;
	void SetInputMode(const FInputModeDataBase& InData);
	virtual bool SetPause(bool bPause, TDelegate<bool()> CanUnpauseDelegate = TDelegate<bool()>());
	bool IsPaused() const;
	bool GetMousePosition(float& LocationX, float& LocationY) const;
	void GetViewportSize(int32& SizeX, int32& SizeY) const;
	bool ProjectWorldLocationToScreen(FVector WorldLocation, FVector2D& ScreenLocation, bool bPlayerViewportRelative = false) const;
	bool DeprojectScreenPositionToWorld(float ScreenX, float ScreenY, FVector& WorldLocation, FVector& WorldDirection) const;
	void ClientTravel(const FString& URL, enum ETravelType TravelType, bool bSeamless = false, FGuid MapPackageGuid = FGuid());
	virtual void SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());
	void SetViewTargetWithBlend(AActor* NewViewTarget, float BlendTime = 0, enum EViewTargetBlendFunction BlendFunc = VTBlend_Linear, float BlendExp = 0, bool bLockOutgoing = false);
	AActor* GetViewTarget() const;
	virtual void ToggleSpeaking(bool bInSpeaking);
	virtual void PlayerTick(float DeltaTime);
	virtual void SetupInputComponent();
	virtual void ClientSetRotation(FRotator NewRotation, bool bResetCamera = false);
	void ConsoleCommand(const FString& Command, bool bWriteToLog = true);
	virtual void ClientReturnToMainMenuWithTextReason(const FText& ReturnReason);
	bool IsInputKeyDown(FKey Key) const;
	bool WasInputKeyJustPressed(FKey Key) const;
	void FlushPressedKeys();
	virtual void ReceivedPlayer();
	virtual void OnPossess(APawn* InPawn) override;
	virtual void GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const override;
	void SetMouseLocation(const int X, const int Y);
	virtual void StartSpectatingOnly();
	void ClientMessage(const FString& S, FName Type = NAME_None, float MsgLifeTime = 0.f);
};

class APlayerState : public AInfo
{
public:
	typedef AInfo Super;
	static UClass* StaticClass();
	FString GetPlayerName() const;
	virtual void SetPlayerName(const FString& S);
	float GetPingInMilliseconds() const;
	uint8 GetCompressedPing() const;
	int32 GetPlayerId() const;
	APawn* GetPawn() const;
	template <class T> T* GetPawn() const;
	APlayerController* GetPlayerController() const;
	AController* GetOwningController() const;
	bool IsABot() const;
	bool IsSpectator() const;
	virtual void CopyProperties(APlayerState* PlayerState);
	virtual void OverrideWith(APlayerState* PlayerState);
};

class AGameStateBase : public AInfo
{
public:
	typedef AInfo Super;
	AGameStateBase();
	AGameStateBase(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	TArray<TObjectPtr<APlayerState>> PlayerArray;
	virtual double GetServerWorldTimeSeconds() const;
	float GetPlayerStartTime(AController* Controller) const;
	bool HasBegunPlay() const;
	bool HasMatchStarted() const;
	const AGameModeBase* GetDefaultGameMode() const;
	virtual void AddPlayerState(APlayerState* PlayerState);
	virtual void RemovePlayerState(APlayerState* PlayerState);
};

class AGameSession : public AInfo
{
public:
	int32 MaxPlayers = 16;
	virtual bool KickPlayer(APlayerController* KickedPlayer, const FText& KickReason);
};
class AGameModeBase : public AInfo
{
public:
	typedef AInfo Super;
	AGameModeBase();
	AGameModeBase(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	TSubclassOf<AGameStateBase> GameStateClass;
	TSubclassOf<APlayerController> PlayerControllerClass;
	TSubclassOf<APlayerState> PlayerStateClass;
	TSubclassOf<AHUD> HUDClass;
	TSubclassOf<APawn> DefaultPawnClass;
	TSubclassOf<APawn> SpectatorClass;
	TObjectPtr<AGameStateBase> GameState;
	TObjectPtr<AGameSession> GameSession;
	uint32 bUseSeamlessTravel : 1;
	uint32 bStartPlayersAsSpectators : 1;
	uint32 bPauseable : 1;
	template <class T> T* GetGameState() const;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage);
	virtual void InitGameState();
	virtual void PreLogin(const FString& Options, const FString& Address, const struct FUniqueNetIdRepl& UniqueId, FString& ErrorMessage);
	virtual void PostLogin(APlayerController* NewPlayer);
	virtual void Logout(AController* Exiting);
	virtual void HandleStartingNewPlayer(APlayerController* NewPlayer);
	virtual void RestartPlayer(AController* NewPlayer);
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot);
	virtual void RestartPlayerAtTransform(AController* NewPlayer, const FTransform& SpawnTransform);
	virtual APawn* SpawnDefaultPawnAtTransform(AController* NewPlayer, const FTransform& SpawnTransform);
	virtual AActor* ChoosePlayerStart(AController* Player);
	virtual AActor* FindPlayerStart(AController* Player, const FString& IncomingName = TEXT(""));
	virtual bool ShouldSpawnAtStartSpot(AController* Player);
	virtual UClass* GetDefaultPawnClassForController(AController* InController);
	virtual void StartPlay();
	virtual bool AllowPausing(APlayerController* PC = nullptr);
	virtual void ResetLevel();
	int32 GetNumPlayers();
	virtual void GenericPlayerInitialization(AController* C);
	virtual void SwapPlayerControllers(APlayerController* OldPC, APlayerController* NewPC);
};

class UCanvas;
class FCanvas;
class AHUD : public AActor
{
public:
	typedef AActor Super;
	AHUD();
	AHUD(const FObjectInitializer& ObjectInitializer);
	static UClass* StaticClass();
	TObjectPtr<APlayerController> PlayerOwner;
	TObjectPtr<UCanvas> Canvas;
	TObjectPtr<UCanvas> DebugCanvas;
	uint8 bShowHUD : 1;
	uint8 bShowDebugInfo : 1;
	virtual void DrawHUD();
	virtual void PostRender();
	virtual void ShowHUD();
	APlayerController* GetOwningPlayerController() const;
	APawn* GetOwningPawn() const;
	void DrawRect(FLinearColor RectColor, float ScreenX, float ScreenY, float ScreenW, float ScreenH);
	void DrawLine(float StartScreenX, float StartScreenY, float EndScreenX, float EndScreenY, FLinearColor LineColor, float LineThickness = 0.f);
	void DrawText(const FString& Text, FLinearColor TextColor, float ScreenX, float ScreenY, UFont* Font = nullptr, float Scale = 1.f, bool bScalePosition = false);
	void DrawTexture(UTexture* Texture, float ScreenX, float ScreenY, float ScreenW, float ScreenH, float TextureU, float TextureV, float TextureUWidth, float TextureVHeight, FLinearColor TintColor = FLinearColor::White, EBlendMode BlendMode = BLEND_Translucent, float Scale = 1.f, bool bScalePosition = false, float Rotation = 0.f, FVector2D RotPivot = FVector2D::ZeroVector);
	void DrawTextureSimple(UTexture* Texture, float ScreenX, float ScreenY, float Scale = 1.f, bool bScalePosition = false);
	void DrawMaterialSimple(UMaterialInterface* Material, float ScreenX, float ScreenY, float ScreenW, float ScreenH, float Scale = 1.f, bool bScalePosition = false);
	void GetTextSize(const FString& Text, float& OutWidth, float& OutHeight, UFont* Font = nullptr, float Scale = 1.f) const;
	FVector Project(FVector Location, bool bClampToZeroPlane = false) const;
};

// ------------------------------------------------------------------ canvas
struct FTextEffect;
struct FCanvasItem
{
	FVector2D Position;
	FLinearColor Color;
	ESimpleElementBlendMode BlendMode = SE_BLEND_Translucent;
	void SetColor(const FLinearColor& InColor);
	virtual ~FCanvasItem() = default;
};
struct FSlateFontInfo;
struct FCanvasTextItem : FCanvasItem
{
	FCanvasTextItem(const FVector2D& InPosition, const FText& InText, const UFont* InFont, const FLinearColor& InColor, const FTextEffect* InEffect = nullptr);
	FCanvasTextItem(const FVector2D& InPosition, const FText& InText, const FSlateFontInfo& InFontInfo, const FLinearColor& InColor, const FTextEffect* InEffect = nullptr);
	void EnableShadow(const FLinearColor& InColor, const FVector2D& InOffset = FVector2D(1.0f, 1.0f));
	void DisableShadow();
	FVector2D Scale;
	uint32 bCentreX : 1;
	uint32 bCentreY : 1;
	uint32 bOutlined : 1;
	FLinearColor OutlineColor;
	FVector2D ShadowOffset;
	FLinearColor ShadowColor;
	FText Text;
};
struct FCanvasTileItem : FCanvasItem
{
	FCanvasTileItem(const FVector2D& InPosition, const FTexture* InTexture, const FVector2D& InSize, const FLinearColor& InColor);
	FCanvasTileItem(const FVector2D& InPosition, const FVector2D& InSize, const FLinearColor& InColor);
	FCanvasTileItem(const FVector2D& InPosition, const FTexture* InTexture, const FVector2D& InSize, const FVector2D& InUV0, const FVector2D& InUV1, const FLinearColor& InColor);
	FVector2D Size;
	FVector2D UV0;
	FVector2D UV1;
	float Rotation = 0.f;
	FVector2D PivotPoint;
};
struct FCanvasLineItem : FCanvasItem
{
	FCanvasLineItem(const FVector2D& InPosition, const FVector2D& InEndPos);
	float LineThickness = 0.f;
};
class UCanvas : public UObject
{
public:
	float ClipX = 0.f;
	float ClipY = 0.f;
	float OrgX = 0.f;
	float OrgY = 0.f;
	FCanvas* Canvas = nullptr;
	FColor DrawColor;
	void DrawItem(FCanvasItem& Item);
	void DrawItem(FCanvasItem& Item, const FVector2D& InPosition);
	void DrawItem(FCanvasItem& Item, float X, float Y);
	float DrawText(const UFont* InFont, const FString& InText, float X, float Y, float XScale = 1.f, float YScale = 1.f);
	void TextSize(const UFont* InFont, const FString& InText, float& XL, float& YL, float ScaleX = 1.f, float ScaleY = 1.f);
	void SetDrawColor(FColor const& C);
	void SetDrawColor(uint8 R, uint8 G, uint8 B, uint8 A = 255);
	FVector Project(FVector Location, bool bClampToNearPlane = true) const;
	void Deproject(FVector2D ScreenPosition, FVector& WorldOrigin, FVector& WorldDirection) const;
	void K2_DrawLine(FVector2D ScreenPositionA, FVector2D ScreenPositionB, float Thickness = 1.f, FLinearColor RenderColor = FLinearColor::White);
};

// ------------------------------------------------------------------ slate (fonts only)
struct FFontOutlineSettings
{
	int32 OutlineSize = 0;
	FLinearColor OutlineColor;
	static FFontOutlineSettings NoOutline;
};
struct FSlateFontInfo
{
	TObjectPtr<const UObject> FontObject;
	FName TypefaceFontName;
	float Size = 24.f;
	FFontOutlineSettings OutlineSettings;
	int32 LetterSpacing = 0;
	FSlateFontInfo();
	FSlateFontInfo(const UObject* InFontObject, const float InSize, const FName& InTypefaceFontName = NAME_None, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings());
};
class FCoreStyle
{
public:
	static FSlateFontInfo GetDefaultFontStyle(const FName InTypefaceFontName, const float InSize, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings());
	static bool IsInitialized();
};
class FSlateFontMeasure
{
public:
	FVector2D Measure(const FString& Text, const FSlateFontInfo& InFontInfo, const float FontScale = 1.0f) const;
	FVector2D Measure(const FText& Text, const FSlateFontInfo& InFontInfo, const float FontScale = 1.0f) const;
	uint16 GetMaxCharacterHeight(const FSlateFontInfo& InFontInfo, float FontScale = 1.0f) const;
};
class FSlateRenderer
{
public:
	TSharedRef<FSlateFontMeasure> GetFontMeasureService() const;
};
class FSlateApplication
{
public:
	static bool IsInitialized();
	static FSlateApplication& Get();
	FSlateRenderer* GetRenderer() const;
};

// ================================================================== world / game instance
class AWorldSettings : public AInfo
{
public:
	float KillZ = -1048575.f;
	float WorldGravityZ = 0.f;
	uint32 bEnableWorldBoundsChecks : 1;
};
class UWorld : public UObject
{
public:
	static UClass* StaticClass();
	uint8 bIsTearingDown : 1;
	FTimerManager& GetTimerManager() const;
	double GetTimeSeconds() const;
	double GetRealTimeSeconds() const;
	double GetAudioTimeSeconds() const;
	float GetDeltaSeconds() const;
	double GetUnpausedTimeSeconds() const;
	ENetMode GetNetMode() const;
	bool IsNetMode(ENetMode Mode) const;
	bool IsPaused() const;
	bool IsGameWorld() const;
	AGameStateBase* GetGameState() const;
	template <class T> T* GetGameState() const;
	AGameModeBase* GetAuthGameMode() const;
	template <class T> T* GetAuthGameMode() const;
	APlayerController* GetFirstPlayerController() const;
	template <class T> T* GetFirstPlayerController() const;
	UGameInstance* GetGameInstance() const;
	template <class T> T* GetGameInstance() const;
	template <class T> T* GetSubsystem() const;
	UGameViewportClient* GetGameViewport() const;
	AWorldSettings* GetWorldSettings(bool bCheckStreamingPersistent = false, bool bChecked = true) const;
	FString GetMapName() const;
	UNetDriver* GetNetDriver() const;
	FConstPlayerControllerIterator GetPlayerControllerIterator() const;
	bool ServerTravel(const FString& InURL, bool bAbsolute = false, bool bShouldSkipGameNotify = false);
	AActor* SpawnActor(UClass* Class, FTransform const* Transform, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	AActor* SpawnActor(UClass* InClass, FVector const* Location = nullptr, FRotator const* Rotation = nullptr, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	template <class T> T* SpawnActor(const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	template <class T> T* SpawnActor(FVector const& Location, FRotator const& Rotation, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	template <class T> T* SpawnActor(UClass* Class, FVector const& Location, FRotator const& Rotation, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	template <class T> T* SpawnActor(UClass* Class, FTransform const& Transform, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	template <class T> T* SpawnActorDeferred(UClass* Class, FTransform const& Transform, AActor* Owner = nullptr, APawn* Instigator = nullptr, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::Undefined);
	bool DestroyActor(AActor* Actor, bool bNetForce = false, bool bShouldModifyLevel = true);
	bool LineTraceSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;
	bool LineTraceMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;
	bool LineTraceTestByChannel(const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;
	bool LineTraceSingleByObjectType(FHitResult& OutHit, const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam) const;
	bool SweepSingleByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;
	bool SweepMultiByChannel(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;
	bool SweepTestByChannel(const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;
	bool OverlapBlockingTestByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;
	bool OverlapAnyTestByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;
	bool OverlapMultiByChannel(TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam) const;
};

class FConstPlayerControllerIterator
{
public:
	explicit operator bool() const;
	FConstPlayerControllerIterator& operator++();
	const TWeakObjectPtr<APlayerController>& operator*() const;
	const TWeakObjectPtr<APlayerController>* operator->() const;
};
// UWorld::GetPlayerControllerIterator
FConstPlayerControllerIterator SUMockPlayerControllerIterator();

enum class EActorIteratorFlags { AllActors = 0, OnlyActiveLevels = 1, SkipPendingKill = 2 };
template <class T>
class TActorIterator
{
public:
	explicit TActorIterator(const UWorld* InWorld, TSubclassOf<T> InClass = T::StaticClass(), EActorIteratorFlags InFlags = EActorIteratorFlags::OnlyActiveLevels);
	explicit operator bool() const;
	bool operator!() const;
	TActorIterator& operator++();
	T* operator*() const;
	T* operator->() const;
};
template <class T>
class TActorRange
{
public:
	explicit TActorRange(const UWorld* InWorld);
	T** begin() const;
	T** end() const;
};

class FSubsystemCollectionBase
{
public:
	template <class T> T* InitializeDependency();
};
class USubsystem : public UObject
{
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const;
	virtual void Initialize(FSubsystemCollectionBase& Collection);
	virtual void Deinitialize();
};
class UGameInstanceSubsystem : public USubsystem
{
public:
	typedef USubsystem Super;
	static UClass* StaticClass();
	UGameInstance* GetGameInstance() const;
};
class UWorldSubsystem : public USubsystem
{
public:
	typedef USubsystem Super;
	static UClass* StaticClass();
	UWorld* GetWorld() const override;
	template <class T> T* GetWorldRef() const;
	virtual void OnWorldBeginPlay(UWorld& InWorld);
	virtual void PostInitialize();
protected:
	virtual bool DoesSupportWorldType(int32 WorldType) const;
};
class ULocalPlayerSubsystem : public USubsystem
{
public:
	ULocalPlayer* GetLocalPlayer() const;
};

class UGameInstance : public UObject
{
public:
	typedef UObject Super;
	static UClass* StaticClass();
	virtual void Init();
	virtual void Shutdown();
	virtual void OnStart();
	UWorld* GetWorld() const override;
	template <class T> T* GetSubsystem() const;
	template <class T> static T* GetSubsystem(const UGameInstance* GameInstance);
	UEngine* GetEngine() const;
	APlayerController* GetFirstLocalPlayerController(const UWorld* World = nullptr) const;
	ULocalPlayer* GetFirstGamePlayer() const;
	FTimerManager& GetTimerManager() const;
	UGameViewportClient* GetGameViewportClient() const;
	virtual void ReturnToMainMenu();
	bool IsDedicatedServerInstance() const;
};

using FOnScreenshotCaptured = TMulticastDelegate<int32, int32, const TArray<FColor>&>;
class FViewport
{
public:
	FIntPoint GetSizeXY() const;
};
class UGameViewportClient : public UObject
{
public:
	static FOnScreenshotCaptured& OnScreenshotCaptured();
	FViewport* Viewport = nullptr;
	void GetViewportSize(FVector2D& OutViewportSize) const;
	UWorld* GetWorld() const override;
	bool IsFocused(FViewport* InViewport);
	void SetSuppressTransitionMessage(bool bSuppress);
};
class FScreenshotRequest
{
public:
	static void RequestScreenshot(bool bInShowUI);
	static void RequestScreenshot(const FString& InFilename, bool bInShowUI, bool bAddFilenameSuffix, bool bHdrScreenshot = false);
	static bool IsScreenshotRequested();
	static void Reset();
};

using FOnNetworkFailure = TMulticastDelegate<UWorld*, UNetDriver*, ENetworkFailure::Type, const FString&>;
using FOnTravelFailure = TMulticastDelegate<UWorld*, int32, const FString&>;
class UGameUserSettings;
class UEngine : public UObject
{
public:
	TObjectPtr<UGameViewportClient> GameViewport;
	UFont* GetTinyFont() const;
	UFont* GetSmallFont() const;
	UFont* GetMediumFont() const;
	UFont* GetLargeFont() const;
	UFont* GetSubtitleFont() const;
	FOnNetworkFailure& OnNetworkFailure();
	FOnTravelFailure& OnTravelFailure();
	UGameUserSettings* GetGameUserSettings();
	void AddOnScreenDebugMessage(int32 Key, float TimeToDisplay, FColor DisplayColor, const FString& DebugMessage, bool bNewerOnTop = true, const FVector2D& TextScale = FVector2D(1, 1));
	UWorld* GetWorldFromContextObject(const UObject* Object, int32 Behavior) const;
	APlayerController* GetFirstLocalPlayerController(const UWorld* InWorld);
	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);
};
extern UEngine* GEngine;
class UNetDriver : public UObject {};
class UNetConnection : public UObject {};

class UDeveloperSettings : public UObject
{
public:
	typedef UObject Super;
	static UClass* StaticClass();
	virtual FName GetContainerName() const;
	virtual FName GetCategoryName() const;
	virtual FName GetSectionName() const;
};
class UGameUserSettings : public UObject
{
public:
	typedef UObject Super;
	static UClass* StaticClass();
	static UGameUserSettings* GetGameUserSettings();
	virtual void ApplySettings(bool bCheckForCommandLineOverrides);
	virtual void ApplyNonResolutionSettings();
	void ApplyResolutionSettings(bool bCheckForCommandLineOverrides);
	virtual void SaveSettings();
	virtual void LoadSettings(bool bForceReload = false);
	virtual void SetToDefaults();
	virtual void ValidateSettings();
	virtual void ResetToCurrentSettings();
	virtual void ConfirmVideoMode();
	void SetOverallScalabilityLevel(int32 Value);
	int32 GetOverallScalabilityLevel() const;
	void SetScreenResolution(FIntPoint Resolution);
	FIntPoint GetScreenResolution() const;
	FIntPoint GetDesktopResolution() const;
	void SetFullscreenMode(EWindowMode::Type InFullscreenMode);
	EWindowMode::Type GetFullscreenMode() const;
	void SetResolutionScaleNormalized(float NewScaleNormalized);
	float GetResolutionScaleNormalized() const;
	void SetResolutionScaleValueEx(float NewScaleValue);
	float GetResolutionScaleValueEx() const;
	void SetFrameRateLimit(float NewLimit);
	float GetFrameRateLimit() const;
	void SetVSyncEnabled(bool bEnable);
	bool IsVSyncEnabled() const;
	void SetViewDistanceQuality(int32 Value);
	void SetShadowQuality(int32 Value);
	void SetPostProcessingQuality(int32 Value);
	void SetTextureQuality(int32 Value);
	void SetVisualEffectQuality(int32 Value);
	void SetFoliageQuality(int32 Value);
	void SetGlobalIlluminationQuality(int32 Value);
	void SetReflectionQuality(int32 Value);
	void SetAntiAliasingQuality(int32 Value);
	void SetShadingQuality(int32 Value);
	int32 GetShadowQuality() const;
	int32 GetTextureQuality() const;
	void RunHardwareBenchmark(int32 WorkScale = 10, float CPUMultiplier = 1.f, float GPUMultiplier = 1.f);
	void ApplyHardwareBenchmarkResults();
	bool IsDirty() const;
};
class USaveGame : public UObject
{
public:
	typedef UObject Super;
	static UClass* StaticClass();
};

class UGameplayStatics : public UObject
{
public:
	static void OpenLevel(const UObject* WorldContextObject, FName LevelName, bool bAbsolute = true, FString Options = FString(TEXT("")));
	static APlayerController* GetPlayerController(const UObject* WorldContextObject, int32 PlayerIndex);
	static APawn* GetPlayerPawn(const UObject* WorldContextObject, int32 PlayerIndex);
	static ACharacter* GetPlayerCharacter(const UObject* WorldContextObject, int32 PlayerIndex);
	static AGameModeBase* GetGameMode(const UObject* WorldContextObject);
	static AGameStateBase* GetGameState(const UObject* WorldContextObject);
	static UGameInstance* GetGameInstance(const UObject* WorldContextObject);
	static bool DoesSaveGameExist(const FString& SlotName, const int32 UserIndex);
	static bool SaveGameToSlot(USaveGame* SaveGameObject, const FString& SlotName, const int32 UserIndex);
	static USaveGame* LoadGameFromSlot(const FString& SlotName, const int32 UserIndex);
	static bool DeleteGameInSlot(const FString& SlotName, const int32 UserIndex);
	static USaveGame* CreateSaveGameObject(TSubclassOf<USaveGame> SaveGameClass);
	static bool SaveDataToSlot(const TArray<uint8>& InSaveData, const FString& SlotName, const int32 UserIndex);
	static bool LoadDataFromSlot(TArray<uint8>& OutSaveData, const FString& SlotName, const int32 UserIndex);
	static bool SaveGameToMemory(USaveGame* SaveGameObject, TArray<uint8>& OutSaveData);
	static USaveGame* LoadGameFromMemory(const TArray<uint8>& InSaveData);
	static void GetAllActorsOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors);
	static AActor* GetActorOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass);
	static void SetGlobalTimeDilation(const UObject* WorldContextObject, float TimeDilation);
	static bool SetGamePaused(const UObject* WorldContextObject, bool bPaused);
	static double GetTimeSeconds(const UObject* WorldContextObject);
	static double GetRealTimeSeconds(const UObject* WorldContextObject);
	static void ActivateReverbEffect(const UObject* WorldContextObject, UReverbEffect* ReverbEffect, FName TagName, float Priority = 0.f, float Volume = 0.5f, float FadeTime = 2.f);
	static void DeactivateReverbEffect(const UObject* WorldContextObject, FName TagName);
	static void PlaySound2D(const UObject* WorldContextObject, USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f);
	static void PlaySoundAtLocation(const UObject* WorldContextObject, USoundBase* Sound, FVector Location, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, USoundAttenuation* AttenuationSettings = nullptr);
	static FString GetPlatformName();
	static FString GetCurrentLevelName(const UObject* WorldContextObject, bool bRemovePrefixString = true);
};
class UKismetSystemLibrary : public UObject
{
public:
	static void QuitGame(const UObject* WorldContextObject, class APlayerController* SpecificPlayer, TEnumAsByte<EQuitPreference::Type> QuitPreference, bool bIgnorePlatformRestrictions);
	static bool GetSupportedFullscreenResolutions(TArray<FIntPoint>& Resolutions);
	static bool GetConvenientWindowedResolutions(TArray<FIntPoint>& Resolutions);
	static void ExecuteConsoleCommand(const UObject* WorldContextObject, const FString& Command, class APlayerController* SpecificPlayer = nullptr);
	static FString GetGameName();
	static FString GetProjectSavedDirectory();
	static bool IsDedicatedServer(const UObject* WorldContextObject);
	static bool IsServer(const UObject* WorldContextObject);
	static bool IsStandalone(const UObject* WorldContextObject);
};
class UKismetRenderingLibrary : public UObject
{
public:
	static UTextureRenderTarget2D* CreateRenderTarget2D(UObject* WorldContextObject, int32 Width = 256, int32 Height = 256, ETextureRenderTargetFormat Format = RTF_RGBA16f, FLinearColor ClearColor = FLinearColor::Black, bool bAutoGenerateMipMaps = false, bool bSupportUAVs = false);
	static void ClearRenderTarget2D(UObject* WorldContextObject, UTextureRenderTarget2D* TextureRenderTarget, FLinearColor ClearColor = FLinearColor(0, 0, 0, 1));
	static void ReleaseRenderTarget2D(UTextureRenderTarget2D* TextureRenderTarget);
};
class UKismetMathLibrary : public UObject
{
public:
	static FRotator FindLookAtRotation(const FVector& Start, const FVector& Target);
};

// ------------------------------------------------------------------ enhanced input
enum class ETriggerEvent : uint8 { None = 0, Triggered = 1, Started = 2, Ongoing = 4, Canceled = 8, Completed = 16 };
enum class EInputActionValueType : uint8 { Boolean, Axis1D, Axis2D, Axis3D };
enum class EInputAxisSwizzle : uint8 { YXZ, ZYX, XZY, YZX, ZXY };
struct FInputActionValue
{
	using Axis1D = float;
	using Axis2D = FVector2D;
	using Axis3D = FVector;
	FInputActionValue();
	FInputActionValue(bool bInValue);
	FInputActionValue(float InValue);
	FInputActionValue(const FVector2D& InValue);
	FInputActionValue(const FVector& InValue);
	FInputActionValue(EInputActionValueType InValueType, FVector InValue);
	template <typename T> T Get() const;
	EInputActionValueType GetValueType() const;
	float GetMagnitude() const;
	bool IsNonZero(float Tolerance = UE_KINDA_SMALL_NUMBER) const;
};
class UInputModifier : public UObject {};
class UInputModifierNegate : public UInputModifier
{
public:
	static UClass* StaticClass();
	bool bX = true;
	bool bY = true;
	bool bZ = true;
};
class UInputModifierSwizzleAxis : public UInputModifier
{
public:
	static UClass* StaticClass();
	EInputAxisSwizzle Order = EInputAxisSwizzle::YXZ;
};
class UInputModifierScalar : public UInputModifier
{
public:
	static UClass* StaticClass();
	FVector Scalar;
};
class UInputModifierDeadZone : public UInputModifier
{
public:
	static UClass* StaticClass();
	float LowerThreshold = 0.2f;
	float UpperThreshold = 1.f;
};
class UInputTrigger : public UObject {};
struct FEnhancedActionKeyMapping
{
	TObjectPtr<const class UInputAction> Action;
	FKey Key;
	TArray<TObjectPtr<UInputTrigger>> Triggers;
	TArray<TObjectPtr<UInputModifier>> Modifiers;
};
class UInputAction : public UObject
{
public:
	static UClass* StaticClass();
	bool bConsumeInput = true;
	bool bTriggerWhenPaused = false;
	bool bReserveAllMappings = false;
	EInputActionValueType ValueType = EInputActionValueType::Boolean;
	TArray<TObjectPtr<UInputTrigger>> Triggers;
	TArray<TObjectPtr<UInputModifier>> Modifiers;
};
class UInputMappingContext : public UObject
{
public:
	static UClass* StaticClass();
	FEnhancedActionKeyMapping& MapKey(const UInputAction* Action, FKey ToKey);
	void UnmapKey(const UInputAction* Action, FKey Key);
	void UnmapAll();
	const TArray<FEnhancedActionKeyMapping>& GetMappings() const;
};
struct FModifyContextOptions
{
	uint8 bIgnoreAllPressedKeysUntilRelease : 1;
	uint8 bForceImmediately : 1;
	FModifyContextOptions();
};
class UEnhancedInputLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
public:
	static UClass* StaticClass();
	void AddMappingContext(const UInputMappingContext* MappingContext, int32 Priority, const FModifyContextOptions& Options = FModifyContextOptions());
	void RemoveMappingContext(const UInputMappingContext* MappingContext, const FModifyContextOptions& Options = FModifyContextOptions());
	void ClearAllMappings();
	bool HasMappingContext(const UInputMappingContext* MappingContext) const;
};
struct FEnhancedInputActionEventBinding
{
	ETriggerEvent GetTriggerEvent() const;
	uint32 GetHandle() const;
};
class UEnhancedInputComponent : public UInputComponent
{
public:
	static UClass* StaticClass();
	template <class U> FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent, U* Object, void (U::*Func)(const FInputActionValue&));
	template <class U> FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent, U* Object, void (U::*Func)());
	template <class U> FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent, const TObjectPtr<U>& Object, void (U::*Func)(const FInputActionValue&));
	void ClearActionBindings();
	bool RemoveBindingByHandle(const uint32 Handle);
};

// ------------------------------------------------------------------ image wrapper / modules / console
class IImageWrapper
{
public:
	virtual ~IImageWrapper() = default;
	virtual bool SetRaw(const void* InRawData, int64 InRawSize, const int32 InWidth, const int32 InHeight, const ERGBFormat InFormat, const int32 InBitDepth, const int32 InBytesPerRow = 0) = 0;
	virtual bool SetCompressed(const void* InCompressedData, int64 InCompressedSize) = 0;
	virtual TArray64<uint8> GetCompressed(int32 Quality = 0) = 0;
	virtual bool GetRaw(const ERGBFormat InFormat, int32 InBitDepth, TArray64<uint8>& OutRawData) = 0;
	virtual int64 GetWidth() const = 0;
	virtual int64 GetHeight() const = 0;
	virtual int32 GetBitDepth() const = 0;
	virtual ERGBFormat GetFormat() const = 0;
};
class IModuleInterface
{
public:
	virtual ~IModuleInterface() = default;
	virtual void StartupModule() {}
	virtual void ShutdownModule() {}
};
class IImageWrapperModule : public IModuleInterface
{
public:
	virtual TSharedPtr<IImageWrapper> CreateImageWrapper(const EImageFormat InFormat, const TCHAR* DebugImageName = nullptr) = 0;
	virtual EImageFormat DetectImageFormat(const void* CompressedData, int64 CompressedSize) = 0;
};
class FModuleManager
{
public:
	static FModuleManager& Get();
	template <typename T> static T& LoadModuleChecked(const FName ModuleName);
	template <typename T> static T* LoadModulePtr(const FName ModuleName);
	template <typename T> static T& GetModuleChecked(const FName ModuleName);
	bool IsModuleLoaded(const FName InModuleName) const;
};
class FDefaultGameModuleImpl : public IModuleInterface {};
class FDefaultModuleImpl : public IModuleInterface {};
#define IMPLEMENT_PRIMARY_GAME_MODULE(ModuleImplClass, ModuleName, GameName) [[maybe_unused]] static ModuleImplClass SUMockModule_##ModuleName;
#define IMPLEMENT_MODULE(ModuleImplClass, ModuleName) static ModuleImplClass SUMockModule_##ModuleName;
#define IMPLEMENT_GAME_MODULE(ModuleImplClass, ModuleName) static ModuleImplClass SUMockModule_##ModuleName;

struct FImageUtils
{
	static void ImageResize(int32 SrcWidth, int32 SrcHeight, const TArray<FColor>& SrcData, int32 DstWidth, int32 DstHeight, TArray<FColor>& DstData, bool bLinearSpace, bool bForceOpaqueOutput = true);
	static bool CompressImage(TArray64<uint8>& OutData, const TCHAR* ToFormatExtension, const struct FImageView& InImage, int32 Quality = 0);
	static UTexture2D* ImportBufferAsTexture2D(const TArray<uint8>& Buffer);
	static UTexture2D* ImportFileAsTexture2D(const FString& Filename);
	static void PNGCompressImageArray(int32 ImageWidth, int32 ImageHeight, const TArray<FColor>& SrcData, TArray64<uint8>& DstData);
};

enum EConsoleVariableFlags { ECVF_Default = 0, ECVF_Cheat = 1, ECVF_ReadOnly = 4, ECVF_Scalability = 0x100, ECVF_SetByCode = 0x0B000000, ECVF_SetByGameSetting = 0x06000000 };
class IConsoleVariable
{
public:
	virtual ~IConsoleVariable() = default;
	virtual void Set(const TCHAR* InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode) = 0;
	void Set(int32 InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode);
	void Set(float InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode);
	void Set(bool InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode);
	virtual int32 GetInt() const = 0;
	virtual float GetFloat() const = 0;
	virtual bool GetBool() const = 0;
	virtual FString GetString() const = 0;
};
class IConsoleManager
{
public:
	static IConsoleManager& Get();
	virtual ~IConsoleManager() = default;
	virtual IConsoleVariable* FindConsoleVariable(const TCHAR* Name, bool bTrackFrequentCalls = true) const = 0;
};
using FConsoleCommandDelegate = TDelegate<void()>;
using FConsoleCommandWithArgsDelegate = TDelegate<void(const TArray<FString>&)>;
using FConsoleCommandWithWorldDelegate = TDelegate<void(UWorld*)>;
using FConsoleCommandWithWorldAndArgsDelegate = TDelegate<void(const TArray<FString>&, UWorld*)>;
class FAutoConsoleObject {};
class FAutoConsoleVariableRef : public FAutoConsoleObject
{
public:
	FAutoConsoleVariableRef(const TCHAR* Name, int32& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	FAutoConsoleVariableRef(const TCHAR* Name, float& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	FAutoConsoleVariableRef(const TCHAR* Name, bool& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	FAutoConsoleVariableRef(const TCHAR* Name, FString& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
};
template <typename T> class TAutoConsoleVariable : public FAutoConsoleObject
{
public:
	TAutoConsoleVariable(const TCHAR* Name, const T& DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	T GetValueOnGameThread() const;
	IConsoleVariable* AsVariable();
};
class FAutoConsoleCommand : public FAutoConsoleObject
{
public:
	FAutoConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandDelegate& Command, uint32 Flags = ECVF_Default);
	FAutoConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Command, uint32 Flags = ECVF_Default);
};
class FAutoConsoleCommandWithWorld : public FAutoConsoleObject
{
public:
	FAutoConsoleCommandWithWorld(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldDelegate& Command, uint32 Flags = ECVF_Default);
};
class FAutoConsoleCommandWithWorldAndArgs : public FAutoConsoleObject
{
public:
	FAutoConsoleCommandWithWorldAndArgs(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldAndArgsDelegate& Command, uint32 Flags = ECVF_Default);
};

// ------------------------------------------------------------------ networking helpers
#define DOREPLIFETIME(c, v) { (void)static_cast<decltype(((c*)nullptr)->v)*>(nullptr); OutLifetimeProps.AddDefaulted(); }
#define DOREPLIFETIME_CONDITION(c, v, cond) { (void)static_cast<decltype(((c*)nullptr)->v)*>(nullptr); ELifetimeCondition SUCond = cond; (void)SUCond; OutLifetimeProps.AddDefaulted(); }
#define DOREPLIFETIME_WITH_PARAMS(c, v, params) { (void)static_cast<decltype(((c*)nullptr)->v)*>(nullptr); OutLifetimeProps.AddDefaulted(); }
#define DOREPLIFETIME_WITH_PARAMS_FAST(c, v, params) DOREPLIFETIME_WITH_PARAMS(c, v, params)
#define DOREPLIFETIME_CONDITION_NOTIFY(c, v, cond, rncond) DOREPLIFETIME_CONDITION(c, v, cond)
enum ELifetimeRepNotifyCondition { REPNOTIFY_OnChanged, REPNOTIFY_Always };
#define GET_MEMBER_NAME_CHECKED(ClassName, MemberName) ((void)sizeof(&ClassName::MemberName), FName(TEXT(#MemberName)))
