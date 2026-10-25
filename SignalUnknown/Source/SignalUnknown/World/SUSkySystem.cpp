#include "World/SUSkySystem.h"
#include "Core/SUGameState.h"
#include "Core/SUGameUserSettings.h"
#include "World/SUAssetLibrary.h"
#include "Audio/SUProceduralAudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ASUSkySystem::ASUSkySystem()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(SceneRoot);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->bAtmosphereSunLight = true;
	Sun->AtmosphereSunLightIndex = 0;
	Sun->Intensity = 90000.f; // lux, physically based
	Sun->LightSourceAngle = 0.53f;
	Sun->CastShadows = true;

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(SceneRoot);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->bAtmosphereSunLight = true;
	Moon->AtmosphereSunLightIndex = 1;
	Moon->Intensity = 0.f;
	Moon->LightColor = FColor(150, 170, 255);
	Moon->CastShadows = true;

	Flash = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("LightningFlash"));
	Flash->SetupAttachment(SceneRoot);
	Flash->SetMobility(EComponentMobility::Movable);
	Flash->Intensity = 0.f;
	Flash->LightColor = FColor(200, 210, 255);
	Flash->CastShadows = false;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(SceneRoot);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(SceneRoot);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	SkyLight->bLowerHemisphereIsBlack = false;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(SceneRoot);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(SceneRoot);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(SceneRoot);
	PostProcess->bUnbound = true;
	FPostProcessSettings& PP = PostProcess->Settings;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = -1.2f; // EV100 – keeps nights properly dark
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = 15.f;
	PP.bOverride_AutoExposureSpeedUp = true;
	PP.AutoExposureSpeedUp = 2.f;
	PP.bOverride_AutoExposureSpeedDown = true;
	PP.AutoExposureSpeedDown = 1.f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.45f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.45f;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.12f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.f;
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = 0.25f;
}

void ASUSkySystem::BeginPlay()
{
	Super::BeginPlay();

	Fog->SetFogDensity(0.015f);
	Fog->SetFogHeightFalloff(0.08f);
	Fog->SetVolumetricFog(true);
	Fog->SetStartDistance(200.f);

	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildRain();
		ThunderAudio = NewObject<USUProceduralAudioComponent>(this, TEXT("Thunder"));
		ThunderAudio->SetupAttachment(SceneRoot);
		ThunderAudio->ConfigureSound(ESUSoundKind::Thunder, SUAudioCategory::Ambience, 1.f, 0.f);
		ThunderAudio->RegisterComponent();
		ThunderAudio->StartSound();
	}
}

void ASUSkySystem::BuildRain()
{
	USUAssetLibrary* Lib = USUAssetLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	RainStreaks = NewObject<UInstancedStaticMeshComponent>(this, TEXT("RainStreaks"));
	RainStreaks->SetupAttachment(SceneRoot);
	RainStreaks->SetMobility(EComponentMobility::Movable);
	RainStreaks->SetStaticMesh(Lib->GetMesh(ESUShape::Cube));
	RainStreaks->SetMaterial(0, Lib->GetColorMaterial(FLinearColor(0.32f, 0.35f, 0.38f)));
	RainStreaks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RainStreaks->SetCastShadow(false);
	RainStreaks->SetVisibility(false);
	RainStreaks->RegisterComponent();

	// PLACEHOLDER rain: two stacked layers of thin streaks scrolled downward around the camera.
	FRandomStream Rng(1234);
	TArray<FTransform> Instances;
	const float Half = 1400.f;
	const float LayerH = 1200.f;
	for (int32 Layer = 0; Layer < 2; ++Layer)
	{
		for (int32 I = 0; I < 900; ++I)
		{
			const FVector P(Rng.FRandRange(-Half, Half), Rng.FRandRange(-Half, Half), Rng.FRandRange(0.f, LayerH) + Layer * LayerH);
			Instances.Add(FTransform(FRotator(0.f, 0.f, 6.f), P, FVector(0.012f, 0.012f, 0.55f)));
		}
	}
	RainStreaks->AddInstances(Instances, false);
}

bool ASUSkySystem::GetViewLocation(FVector& OutLocation) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && PC->PlayerCameraManager)
	{
		OutLocation = PC->PlayerCameraManager->GetCameraLocation();
		return true;
	}
	return false;
}

void ASUSkySystem::UpdateRain(float DeltaSeconds, float Rain, const FVector& ViewLocation)
{
	if (!RainStreaks)
	{
		return;
	}
	IndoorCheckTimer -= DeltaSeconds;
	if (IndoorCheckTimer <= 0.f)
	{
		IndoorCheckTimer = 0.4f;
		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("SURainRoof")), false);
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			Params.AddIgnoredActor(PC->GetPawn());
		}
		bLocalIndoors = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + FVector(0.f, 0.f, 2500.f), ECC_Visibility, Params);
	}
	const bool bVisible = Rain > 0.05f && !bLocalIndoors;
	RainStreaks->SetVisibility(bVisible);
	if (!bVisible)
	{
		return;
	}
	const float LayerH = 1200.f;
	RainScroll = FMath::Fmod(RainScroll + DeltaSeconds * 950.f, LayerH);
	RainStreaks->SetWorldLocation(ViewLocation + FVector(0.f, 0.f, -LayerH * 0.8f - RainScroll));
}

void ASUSkySystem::TriggerLightning(float Strength, float ThunderDelay)
{
	FlashTime = 0.f;
	FlashStrength = Strength;
	ThunderTime = FMath::Max(0.1f, ThunderDelay);
	ThunderStrength = Strength;
}

void ASUSkySystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const ASUGameState* GS = ASUGameState::Get(this);
	const float Hours = GS ? GS->GetTimeOfDay() : 17.6f;
	const float Rain = GS ? GS->GetRain() : 0.f;
	const float Cloud = GS ? GS->GetCloud() : 0.3f;
	const float FogAmount = GS ? GS->GetFog() : 0.3f;
	CurrentDarkness = GS ? GS->GetDarkness() : 0.f;

	// ---- sun: rises in the east (90°), south at noon, sets in the west (270°)
	const float Elevation = ASUGameState::SunElevationForHours(Hours);
	const float DayT = (Hours - 5.83f) / 14.83f;
	const float Azimuth = 90.f + 180.f * DayT;
	Sun->SetWorldRotation(FRotator(-Elevation, Azimuth + 180.f, 0.f));
	const float SunVisible = FMath::SmoothStep(-4.f, 2.f, Elevation);
	Sun->SetIntensity(90000.f * SunVisible * (1.f - 0.75f * Cloud) * (1.f - 0.5f * Rain));
	Sun->SetCastShadows(SunVisible > 0.01f);

	// ---- moon: opposite the sun, fixed elevation at night
	const float MoonVisible = 1.f - FMath::SmoothStep(-6.f, 4.f, Elevation);
	Moon->SetWorldRotation(FRotator(-35.f, Azimuth, 0.f));
	Moon->SetIntensity(0.35f * MoonVisible * (1.f - 0.8f * Cloud));
	Moon->SetCastShadows(MoonVisible > 0.05f);

	// ---- fog & weather
	Fog->SetFogDensity(0.008f + 0.035f * FogAmount + 0.02f * Rain);
	Fog->SetFogHeightFalloff(FMath::Lerp(0.12f, 0.03f, FogAmount));

	// ---- lightning
	if (FlashTime >= 0.f)
	{
		FlashTime += DeltaSeconds;
		const bool bOn = (FlashTime < 0.07f) || (FlashTime > 0.13f && FlashTime < 0.19f);
		Flash->SetIntensity(bOn ? 4.f * FlashStrength : 0.f);
		Flash->SetWorldRotation(FRotator(-60.f, 30.f, 0.f));
		if (FlashTime > 0.3f)
		{
			FlashTime = -1.f;
			Flash->SetIntensity(0.f);
		}
	}
	if (ThunderTime > 0.f)
	{
		ThunderTime -= DeltaSeconds;
		if (ThunderTime <= 0.f && ThunderAudio)
		{
			ThunderAudio->TriggerSound(0, ThunderStrength);
		}
	}

	// ---- player settings that live in the post process
	if (const USUGameUserSettings* Settings = USUGameUserSettings::Get())
	{
		FPostProcessSettings& PP = PostProcess->Settings;
		PP.FilmGrainIntensity = Settings->bFilmGrain ? 0.12f + 0.1f * CurrentDarkness : 0.f;
		PP.MotionBlurAmount = Settings->bMotionBlur ? 0.35f : 0.f;
	}

	FVector ViewLocation;
	if (GetViewLocation(ViewLocation))
	{
		UpdateRain(DeltaSeconds, Rain, ViewLocation);
	}
}
