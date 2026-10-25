#include "Interaction/SULighting.h"
#include "Player/SUCharacter.h"
#include "Core/SUGameState.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Net/UnrealNetwork.h"

// ====================================================================================== lamp

ASULamp::ASULamp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
}

void ASULamp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASULamp, Style);
	DOREPLIFETIME(ASULamp, Intensity);
	DOREPLIFETIME(ASULamp, Radius);
	DOREPLIFETIME(ASULamp, LightColor);
	DOREPLIFETIME(ASULamp, bCastShadows);
	DOREPLIFETIME(ASULamp, PowerSource);
	DOREPLIFETIME(ASULamp, bAutoNight);
	DOREPLIFETIME(ASULamp, bOn);
}

void ASULamp::BuildGeometry()
{
	switch (Style)
	{
	case ESULampStyle::Ceiling:
		AddPart(SceneRoot, ESUShape::Cylinder, FVector(0.f, 0.f, 4.f), FVector(28.f, 28.f, 6.f), ESUMat::MetalWhite, FRotator::ZeroRotator, false);
		AddPart(SceneRoot, ESUShape::Sphere, FVector(0.f, 0.f, -4.f), FVector(16.f, 16.f, 12.f), ESUMat::Lamp, FRotator::ZeroRotator, false);
		Light = AddPointLight(SceneRoot, FVector(0.f, 0.f, -18.f), Intensity, Radius, LightColor, bCastShadows);
		break;
	case ESULampStyle::Hanging:
		AddPart(SceneRoot, ESUShape::Cylinder, FVector(0.f, 0.f, 30.f), FVector(1.f, 1.f, 60.f), ESUMat::Black, FRotator::ZeroRotator, false);
		AddPart(SceneRoot, ESUShape::Cone, FVector(0.f, 0.f, -5.f), FVector(34.f, 34.f, 18.f), ESUMat::MetalPainted, FRotator::ZeroRotator, false);
		AddPart(SceneRoot, ESUShape::Sphere, FVector(0.f, 0.f, -14.f), FVector(10.f, 10.f, 10.f), ESUMat::Lamp, FRotator::ZeroRotator, false);
		Light = AddPointLight(SceneRoot, FVector(0.f, 0.f, -24.f), Intensity, Radius, LightColor, bCastShadows);
		break;
	case ESULampStyle::Desk:
		AddPart(SceneRoot, ESUShape::Cylinder, FVector(0.f, 0.f, 1.5f), FVector(16.f, 16.f, 3.f), ESUMat::MetalDark, FRotator::ZeroRotator, false);
		AddPart(SceneRoot, ESUShape::Cylinder, FVector(0.f, 0.f, 20.f), FVector(2.f, 2.f, 38.f), ESUMat::MetalDark, FRotator::ZeroRotator, false);
		AddPart(SceneRoot, ESUShape::Cone, FVector(6.f, 0.f, 38.f), FVector(18.f, 18.f, 14.f), ESUMat::GasRed, FRotator(-30.f, 0.f, 0.f), false);
		Light = AddSpotLight(SceneRoot, FVector(8.f, 0.f, 34.f), FRotator(-70.f, 0.f, 0.f), Intensity, Radius, 55.f, LightColor);
		break;
	case ESULampStyle::Wall:
		AddPart(SceneRoot, ESUShape::Cube, FVector(-4.f, 0.f, 0.f), FVector(6.f, 14.f, 20.f), ESUMat::MetalDark, FRotator::ZeroRotator, false);
		AddPart(SceneRoot, ESUShape::Sphere, FVector(6.f, 0.f, 0.f), FVector(14.f, 14.f, 16.f), ESUMat::Lamp, FRotator::ZeroRotator, false);
		Light = AddPointLight(SceneRoot, FVector(20.f, 0.f, -4.f), Intensity, Radius, LightColor, bCastShadows);
		break;
	case ESULampStyle::Post:
		AddPart(SceneRoot, ESUShape::Cylinder, FVector(0.f, 0.f, 250.f), FVector(14.f, 14.f, 500.f), ESUMat::WoodDark, FRotator::ZeroRotator, true);
		AddPart(SceneRoot, ESUShape::Cube, FVector(40.f, 0.f, 490.f), FVector(90.f, 6.f, 6.f), ESUMat::MetalDark, FRotator::ZeroRotator, false);
		AddPart(SceneRoot, ESUShape::Cone, FVector(80.f, 0.f, 478.f), FVector(40.f, 40.f, 16.f), ESUMat::MetalPainted, FRotator::ZeroRotator, false);
		AddPart(SceneRoot, ESUShape::Sphere, FVector(80.f, 0.f, 468.f), FVector(14.f, 14.f, 10.f), ESUMat::Lamp, FRotator::ZeroRotator, false);
		Light = AddSpotLight(SceneRoot, FVector(80.f, 0.f, 460.f), FRotator(-90.f, 0.f, 0.f), Intensity, Radius, 70.f, LightColor);
		break;
	case ESULampStyle::Neon:
		AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 2.f), FVector(120.f, 14.f, 5.f), ESUMat::MetalWhite, FRotator::ZeroRotator, false);
		AddPart(SceneRoot, ESUShape::Cylinder, FVector(0.f, 0.f, -2.f), FVector(4.f, 4.f, 110.f), ESUMat::Lamp, FRotator(90.f, 0.f, 0.f), false);
		Light = AddPointLight(SceneRoot, FVector(0.f, 0.f, -20.f), Intensity, Radius, LightColor, bCastShadows);
		break;
	}
	ApplyLight(1.f);
}

bool ASULamp::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	return false; // controlled by switches
}

bool ASULamp::IsLit() const
{
	const bool bPowered = !PowerSource || PowerSource->IsRunning();
	return bOn && bPowered;
}

void ASULamp::SetOn(bool bInOn)
{
	if (HasAuthority())
	{
		bOn = bInOn;
		OnRep_On();
	}
}

void ASULamp::OnRep_On()
{
	ApplyLight(1.f);
}

void ASULamp::ApplyLight(float Factor)
{
	if (Light)
	{
		const bool bLit = IsLit();
		Light->SetVisibility(bLit && Factor > 0.02f);
		Light->SetIntensity(Intensity * Factor);
	}
}

void ASULamp::MulticastFlicker_Implementation(float Duration)
{
	FlickerTime = FMath::Max(FlickerTime, Duration);
}

void ASULamp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bAutoNight && HasAuthority())
	{
		NextAutoCheck -= DeltaSeconds;
		if (NextAutoCheck <= 0.f)
		{
			NextAutoCheck = 5.f;
			if (const ASUGameState* GS = ASUGameState::Get(this))
			{
				const bool bShouldBeOn = GS->GetDarkness() > 0.45f;
				if (bShouldBeOn != bOn)
				{
					SetOn(bShouldBeOn);
				}
			}
		}
	}

	if (FlickerTime > 0.f)
	{
		FlickerTime -= DeltaSeconds;
		const float Noise = FMath::FRand();
		ApplyLight(Noise < 0.35f ? 0.f : (Noise < 0.6f ? 0.25f : 1.f));
		if (FlickerTime <= 0.f)
		{
			ApplyLight(1.f);
		}
	}
	else if (PowerSource)
	{
		// follow generator state changes
		ApplyLight(1.f);
	}
}

// ====================================================================================== switch

ASULightSwitch::ASULightSwitch()
{
}

void ASULightSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASULightSwitch, bOn);
}

void ASULightSwitch::BuildGeometry()
{
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 0.f), FVector(2.f, 8.f, 12.f), ESUMat::White, FRotator::ZeroRotator, true);
	Lever = AddPart(SceneRoot, ESUShape::Cube, FVector(1.5f, 0.f, 0.f), FVector(2.f, 2.f, 4.f), ESUMat::MetalWhite, FRotator::ZeroRotator, false);
}

bool ASULightSwitch::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	OutPrompt.Primary = bOn ? SUText(TEXT("Licht ausschalten")) : SUText(TEXT("Licht einschalten"));
	OutPrompt.bPrimary = true;
	return true;
}

void ASULightSwitch::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	bOn = !bOn;
	for (ASULamp* Lamp : Lamps)
	{
		if (Lamp)
		{
			Lamp->SetOn(bOn);
		}
	}
	if (Lever)
	{
		Lever->SetRelativeLocation(FVector(1.5f, 0.f, bOn ? 2.f : -2.f));
	}
	Super::Interact(User, Component, Option);
}

// ====================================================================================== generator

ASUGenerator::ASUGenerator()
{
}

void ASUGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUGenerator, bRunning);
}

void ASUGenerator::BuildGeometry()
{
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 40.f), FVector(110.f, 70.f, 70.f), ESUMat::MetalPainted);
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 4.f), FVector(130.f, 80.f, 8.f), ESUMat::MetalDark);
	AddPart(SceneRoot, ESUShape::Cylinder, FVector(-30.f, 20.f, 90.f), FVector(10.f, 10.f, 40.f), ESUMat::Rust, FRotator::ZeroRotator, false);
	AddPart(SceneRoot, ESUShape::Cube, FVector(56.f, 0.f, 50.f), FVector(4.f, 30.f, 20.f), ESUMat::Yellow, FRotator::ZeroRotator, false);
	OnRep_Running();
}

bool ASUGenerator::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	OutPrompt.Primary = bRunning ? SUText(TEXT("Generator abschalten")) : SUText(TEXT("Generator starten"));
	OutPrompt.bPrimary = true;
	OutPrompt.Info = bRunning ? SUText(TEXT("Versorgt den Turm mit Strom")) : SUText(TEXT("Kein Strom im Turm"));
	return true;
}

void ASUGenerator::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	bRunning = !bRunning;
	OnRep_Running();
	Super::Interact(User, Component, Option);
}

void ASUGenerator::OnRep_Running()
{
	if (USUProceduralAudioComponent* Hum = GetOrCreateSound(ESUSoundKind::Hum, 0.8f, 2500.f))
	{
		Hum->SetSoundParam(SULogic::Dsp::Param::A, bRunning ? 0.8f : 0.f);
		Hum->SetSoundParam(SULogic::Dsp::Param::B, 30.f);
	}
}
