#include "Audio/SUAmbienceComponent.h"
#include "Audio/SUProceduralAudioComponent.h"
#include "Core/SUGameState.h"
#include "Core/SUGameUserSettings.h"
#include "Player/SUCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Sound/ReverbEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

namespace
{
	const FName ReverbTag(TEXT("SU_Environment"));
}

USUAmbienceComponent::USUAmbienceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

USUProceduralAudioComponent* USUAmbienceComponent::MakeLayer(const TCHAR* Name, int32 KindIndex, float Gain)
{
	AActor* Owner = GetOwner();
	USUProceduralAudioComponent* Layer = NewObject<USUProceduralAudioComponent>(Owner, FName(Name));
	Layer->SetupAttachment(Owner->GetRootComponent());
	Layer->ConfigureSound(static_cast<ESUSoundKind>(KindIndex), SUAudioCategory::Ambience, Gain, 0.f);
	Layer->RegisterComponent();
	Layer->StartSound();
	return Layer;
}

UReverbEffect* USUAmbienceComponent::MakeReverb(float Decay, float Density, float Diffusion, float Gain, float GainHF, float Reflections, float LateGain)
{
	UReverbEffect* R = NewObject<UReverbEffect>(this);
	R->DecayTime = Decay;
	R->Density = Density;
	R->Diffusion = Diffusion;
	R->Gain = Gain;
	R->GainHF = GainHF;
	R->ReflectionsGain = Reflections;
	R->ReflectionsDelay = 0.01f;
	R->LateGain = LateGain;
	R->LateDelay = 0.02f;
	R->DecayHFRatio = 0.6f;
	return R;
}

void USUAmbienceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld()->GetNetMode() == NM_DedicatedServer || !GetOwner() || !GetOwner()->GetRootComponent())
	{
		return;
	}
	Wind = MakeLayer(TEXT("AmbWind"), static_cast<int32>(ESUSoundKind::Wind), 0.55f);
	Birds = MakeLayer(TEXT("AmbBirds"), static_cast<int32>(ESUSoundKind::Birds), 0.5f);
	Crickets = MakeLayer(TEXT("AmbCrickets"), static_cast<int32>(ESUSoundKind::Crickets), 0.35f);
	Rain = MakeLayer(TEXT("AmbRain"), static_cast<int32>(ESUSoundKind::Rain), 0.6f);

	SmallRoomReverb = MakeReverb(0.45f, 0.9f, 0.8f, 0.4f, 0.7f, 0.5f, 0.6f);
	LargeRoomReverb = MakeReverb(1.6f, 1.f, 1.f, 0.45f, 0.8f, 0.3f, 1.1f);
	VehicleReverb = MakeReverb(0.25f, 1.f, 0.6f, 0.35f, 0.4f, 0.6f, 0.4f);
	ForestReverb = MakeReverb(0.9f, 0.4f, 0.4f, 0.2f, 0.55f, 0.1f, 0.25f);
}

void USUAmbienceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bReverbApplied)
	{
		UGameplayStatics::DeactivateReverbEffect(this, ReverbTag);
	}
	Super::EndPlay(EndPlayReason);
}

void USUAmbienceComponent::DetectAcoustic()
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const ASUCharacter* Char = Cast<ASUCharacter>(PC->GetPawn());
	if (Char && Char->IsSeated())
	{
		Acoustic = ESUAcoustic::Vehicle;
		return;
	}
	const FVector View = PC->PlayerCameraManager->GetCameraLocation();
	FCollisionQueryParams Params(FName(TEXT("SUAcoustic")), false, PC->GetPawn());
	FHitResult Hit;
	const bool bRoof = GetWorld()->LineTraceSingleByChannel(Hit, View, View + FVector(0.f, 0.f, 1500.f), ECC_Visibility, Params);
	if (!bRoof)
	{
		Acoustic = ESUAcoustic::Outdoors;
		return;
	}
	float Sum = 0.f;
	const int32 Dirs = 8;
	for (int32 I = 0; I < Dirs; ++I)
	{
		const float A = (2.f * UE_PI * I) / Dirs;
		const FVector End = View + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 1500.f;
		Sum += GetWorld()->LineTraceSingleByChannel(Hit, View, End, ECC_Visibility, Params) ? Hit.Distance : 1500.f;
	}
	Acoustic = (Sum / Dirs) < 380.f ? ESUAcoustic::SmallRoom : ESUAcoustic::LargeRoom;
}

void USUAmbienceComponent::ApplyReverb()
{
	if (Acoustic == AppliedAcoustic && bReverbApplied)
	{
		return;
	}
	UReverbEffect* Effect = ForestReverb;
	switch (Acoustic)
	{
	case ESUAcoustic::SmallRoom: Effect = SmallRoomReverb; break;
	case ESUAcoustic::LargeRoom: Effect = LargeRoomReverb; break;
	case ESUAcoustic::Vehicle: Effect = VehicleReverb; break;
	default: break;
	}
	if (Effect)
	{
		UGameplayStatics::ActivateReverbEffect(this, Effect, ReverbTag, 1.f, 0.6f, 0.6f);
		bReverbApplied = true;
	}
	AppliedAcoustic = Acoustic;
}

void USUAmbienceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Wind)
	{
		return;
	}
	CheckTimer -= DeltaTime;
	if (CheckTimer <= 0.f)
	{
		CheckTimer = 0.5f;
		DetectAcoustic();
		ApplyReverb();
	}

	const ASUGameState* GS = ASUGameState::Get(this);
	const float Dark = GS ? GS->GetDarkness() : 0.f;
	const float RainAmount = GS ? GS->GetRain() : 0.f;
	const float WindAmount = GS ? GS->GetWind() : 0.3f;
	const bool bIndoors = IsIndoors();
	const bool bVehicle = Acoustic == ESUAcoustic::Vehicle;
	const float Interior = bVehicle ? 0.85f : (bIndoors ? 0.7f : 0.f);

	float Height = 0.f;
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		if (PC->PlayerCameraManager)
		{
			Height = PC->PlayerCameraManager->GetCameraLocation().Z;
		}
	}
	// more wind high up on the tower
	const float Altitude = FMath::Clamp((Height - 4500.f) / 2500.f, 0.f, 1.f);

	Wind->SetSoundParam(SULogic::Dsp::Param::A, FMath::Clamp(0.25f + WindAmount * 0.6f + Altitude * 0.25f + RainAmount * 0.2f, 0.f, 1.f));
	Wind->SetSoundParam(SULogic::Dsp::Param::B, 0.4f + WindAmount * 0.6f);
	Wind->SetInterior(Interior);

	Birds->SetSoundParam(SULogic::Dsp::Param::A, FMath::Clamp((1.f - Dark * 1.4f) * (1.f - RainAmount) * (bIndoors || bVehicle ? 0.3f : 1.f), 0.f, 1.f));
	Birds->SetInterior(Interior);
	Crickets->SetSoundParam(SULogic::Dsp::Param::A, FMath::Clamp((Dark - 0.4f) * 1.6f * (1.f - RainAmount * 0.8f), 0.f, 1.f));
	Crickets->SetInterior(Interior);
	Rain->SetSoundParam(SULogic::Dsp::Param::A, RainAmount);
	Rain->SetInterior(bIndoors ? 0.5f : Interior);
}
