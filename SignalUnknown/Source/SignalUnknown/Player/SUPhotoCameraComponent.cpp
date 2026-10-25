#include "Player/SUPhotoCameraComponent.h"
#include "SignalUnknown.h"
#include "Player/SUCharacter.h"
#include "Player/SUInventoryComponent.h"
#include "Core/SUPlayerController.h"
#include "Core/SUPlayerState.h"
#include "Core/SUGameState.h"
#include "Core/SUItemDatabase.h"
#include "Core/SUDeveloperSettings.h"
#include "Core/SUGameUserSettings.h"
#include "Evidence/SUEvidenceSubsystem.h"
#include "Evidence/SUPhotoStorage.h"
#include "Audio/SUProceduralAudioComponent.h"
#include "Logic/SUEvidenceLogic.h"
#include "UI/SUHUD.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "UnrealClient.h"

USUPhotoCameraComponent::USUPhotoCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

ASUCharacter* USUPhotoCameraComponent::GetCharacter() const
{
	return Cast<ASUCharacter>(GetOwner());
}

void USUPhotoCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ScreenshotHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
		ScreenshotHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void USUPhotoCameraComponent::NotifyLocal(const TCHAR* Text) const
{
	const ASUCharacter* C = GetCharacter();
	if (ASUPlayerController* PC = C ? Cast<ASUPlayerController>(C->GetController()) : nullptr)
	{
		PC->Notify(SUText(Text), FLinearColor(1.f, 0.75f, 0.45f));
	}
}

void USUPhotoCameraComponent::SetAiming(bool bInAiming)
{
	bAiming = bInAiming;
	if (!bAiming)
	{
		TargetZoom = FMath::Max(1.f, TargetZoom);
	}
}

void USUPhotoCameraComponent::AdjustZoom(float Delta)
{
	TargetZoom = FMath::Clamp(TargetZoom + Delta * 0.35f, 1.f, 4.f);
}

void USUPhotoCameraComponent::ToggleFlash()
{
	bFlash = !bFlash;
	NotifyLocal(bFlash ? TEXT("Blitz: an") : TEXT("Blitz: aus"));
}

float USUPhotoCameraComponent::GetBattery() const
{
	const ASUCharacter* C = GetCharacter();
	return (C && C->GetInventory()) ? C->GetInventory()->GetCharge(SUItem::Camera) : 0.f;
}

int32 USUPhotoCameraComponent::GetPhotoCount() const
{
	const ASUCharacter* C = GetCharacter();
	const ASUPlayerState* PS = C ? C->GetPlayerState<ASUPlayerState>() : nullptr;
	return PS ? PS->CountPhotosOnCamera() : 0;
}

int32 USUPhotoCameraComponent::GetCapacity() const
{
	return GetDefault<USUGameplaySettings>()->CameraCapacity;
}

bool USUPhotoCameraComponent::IsShaky() const
{
	const ASUCharacter* C = GetCharacter();
	return C && SULogic::IsShotBlurred(C->GetLookAngularSpeed(), C->GetVelocity().Size(), Zoom);
}

FString USUPhotoCameraComponent::GetExposureText() const
{
	const ASUGameState* GS = ASUGameState::Get(this);
	const float Dark = GS ? GS->GetDarkness() : 0.f;
	static const TCHAR* Shutters[] = {TEXT("1/250"), TEXT("1/125"), TEXT("1/60"), TEXT("1/30"), TEXT("1/15")};
	static const int32 Isos[] = {100, 200, 800, 1600, 3200};
	const int32 Index = FMath::Clamp(FMath::FloorToInt(Dark * 4.99f), 0, 4);
	return FString::Printf(TEXT("%s   F%.1f   ISO %d"), Shutters[Index], 2.8f + (Zoom - 1.f) * 0.9f, Isos[Index]);
}

void USUPhotoCameraComponent::UpdateFocus(float DeltaTime)
{
	const ASUCharacter* C = GetCharacter();
	if (!C)
	{
		return;
	}
	const FVector Start = C->GetEyeLocation();
	const FVector End = Start + C->GetControlRotation().Vector() * 6000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(FName(TEXT("SUFocus")), false, C);
	TargetFocus = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ? FMath::Max(30.f, Hit.Distance) : 6000.f;
	// autofocus motor: fast for small changes, ~0.4 s for big ones
	FocusDistance = FMath::FInterpTo(FocusDistance, TargetFocus, DeltaTime, 6.f);
	FocusError = FMath::Clamp(FMath::Abs(FocusDistance - TargetFocus) / FMath::Max(TargetFocus, 60.f), 0.f, 1.f);
}

void USUPhotoCameraComponent::ApplyView(float DeltaTime)
{
	ASUCharacter* C = GetCharacter();
	UCameraComponent* Cam = C ? C->GetFirstPersonCamera() : nullptr;
	if (!Cam)
	{
		return;
	}
	Zoom = FMath::FInterpTo(Zoom, bAiming ? TargetZoom : 1.f, DeltaTime, 8.f);
	if (bAiming)
	{
		const USUGameUserSettings* S = USUGameUserSettings::Get();
		const float BaseFov = S ? S->FieldOfView : 90.f;
		Cam->SetFieldOfView(FMath::FInterpTo(Cam->FieldOfView, FMath::Min(BaseFov, 75.f) / Zoom, DeltaTime, 14.f));
		FPostProcessSettings& PP = Cam->PostProcessSettings;
		PP.bOverride_DepthOfFieldFocalDistance = true;
		PP.DepthOfFieldFocalDistance = FocusDistance;
		PP.bOverride_DepthOfFieldFstop = true;
		PP.DepthOfFieldFstop = 2.8f + (Zoom - 1.f) * 0.9f;
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.75f;
		Cam->PostProcessBlendWeight = 1.f;
	}
	else
	{
		Cam->PostProcessBlendWeight = 0.f;
	}
}

void USUPhotoCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ASUCharacter* C = GetCharacter();
	if (!C || !C->IsLocallyControlled())
	{
		return;
	}
	if (bAiming)
	{
		UpdateFocus(DeltaTime);
	}
	ApplyView(DeltaTime);

	if (bCapturing && GetWorld()->GetTimeSeconds() - CaptureStartTime > 2.0)
	{
		UE_LOG(LogSignalUnknown, Warning, TEXT("Screenshot did not arrive – photo stored without image."));
		FinishCapture(false);
	}
}

void USUPhotoCameraComponent::TakePhoto()
{
	ASUCharacter* C = GetCharacter();
	ASUPlayerController* PC = C ? Cast<ASUPlayerController>(C->GetController()) : nullptr;
	if (!C || !PC || !C->IsLocallyControlled() || bCapturing)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < CooldownUntil)
	{
		return;
	}
	if (!C->GetInventory()->HasItem(SUItem::Camera))
	{
		return;
	}
	if (GetBattery() <= 0.005f)
	{
		NotifyLocal(TEXT("Kamera-Akku leer – Batterien benutzen."));
		return;
	}
	if (GetPhotoCount() >= GetCapacity())
	{
		NotifyLocal(TEXT("Speicherkarte voll – alte Fotos in der Galerie [P] löschen."));
		return;
	}
	USUEvidenceSubsystem* Evidence = GetWorld()->GetSubsystem<USUEvidenceSubsystem>();
	if (!Evidence || !PC->PlayerCameraManager)
	{
		return;
	}

	// ---- what is in the picture?
	FSUPhotoShot Shot;
	Shot.CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	Shot.CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
	Shot.HorizontalFovDeg = PC->PlayerCameraManager->GetFOVAngle();
	int32 VX = 16;
	int32 VY = 9;
	PC->GetViewportSize(VX, VY);
	Shot.AspectRatio = VY > 0 ? static_cast<float>(VX) / VY : 16.f / 9.f;
	Shot.bFlash = bFlash;
	Shot.bFlashlightOn = C->IsFlashlightOn();
	Shot.FlashlightDirection = C->GetFlashlightDirection();
	Shot.bBlurred = IsShaky();
	Shot.FocusError = bAiming ? FocusError : 0.35f; // shooting from the hip: no autofocus
	Shot.IgnoredActors.Add(C);

	PendingMeta = FSUPhotoMeta();
	PendingMeta.PhotoId = FGuid::NewGuid();
	PendingMeta.Hits = Evidence->EvaluatePhoto(Shot);
	PendingMeta.TakenAt = FDateTime::Now();
	PendingMeta.Location = Shot.CameraLocation;
	PendingMeta.ZoomFactor = Zoom;
	PendingMeta.bFlash = bFlash;
	PendingMeta.bBlurred = Shot.bBlurred;
	PendingMeta.FileName = PendingMeta.PhotoId.ToString(EGuidFormats::Digits) + TEXT(".png");
	if (const ASUGameState* GS = ASUGameState::Get(this))
	{
		PendingMeta.DayTimeHours = GS->GetTimeOfDay();
	}
	PendingBlur = (Shot.bBlurred ? 1.f : 0.f) + FMath::Max(0.f, Shot.FocusError - 0.2f) * 2.f;

	// ---- flash and shutter
	if (bFlash)
	{
		C->TriggerFlashBulb();
		C->ServerCameraFlash();
	}
	if (!ShutterSound && GetNetMode() != NM_DedicatedServer)
	{
		ShutterSound = NewObject<USUProceduralAudioComponent>(C);
		ShutterSound->SetupAttachment(C->GetRootComponent());
		ShutterSound->ConfigureSound(ESUSoundKind::Click, SUAudioCategory::Effects, 0.9f, 0.f);
		ShutterSound->RegisterComponent();
		ShutterSound->StartSound();
	}
	if (ShutterSound)
	{
		ShutterSound->TriggerSound(3, 1.f);
	}

	// ---- capture the real frame (the HUD hides itself for this frame)
	bCapturing = true;
	CaptureStartTime = Now;
	LastShotTime = Now;
	if (ASUHUD* HUD = PC->GetSUHUD())
	{
		HUD->SetHiddenForScreenshot(true);
	}
	ScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &USUPhotoCameraComponent::HandleScreenshot);
	FScreenshotRequest::RequestScreenshot(false);
}

void USUPhotoCameraComponent::BoxBlur(TArray<FColor>& Pixels, int32 Width, int32 Height, int32 Radius)
{
	if (Radius <= 0 || Width <= 0 || Height <= 0)
	{
		return;
	}
	TArray<FColor> Tmp;
	Tmp.SetNumUninitialized(Pixels.Num());
	// horizontal, then vertical
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const bool bHorizontal = Pass == 0;
		const TArray<FColor>& Src = bHorizontal ? Pixels : Tmp;
		TArray<FColor>& Dst = bHorizontal ? Tmp : Pixels;
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				int32 R = 0, G = 0, B = 0, N = 0;
				for (int32 K = -Radius; K <= Radius; ++K)
				{
					const int32 SX = bHorizontal ? FMath::Clamp(X + K, 0, Width - 1) : X;
					const int32 SY = bHorizontal ? Y : FMath::Clamp(Y + K, 0, Height - 1);
					const FColor& C = Src[SY * Width + SX];
					R += C.R;
					G += C.G;
					B += C.B;
					++N;
				}
				Dst[Y * Width + X] = FColor(static_cast<uint8>(R / N), static_cast<uint8>(G / N), static_cast<uint8>(B / N), 255);
			}
		}
	}
}

void USUPhotoCameraComponent::HandleScreenshot(int32 Width, int32 Height, const TArray<FColor>& Colors)
{
	if (!bCapturing)
	{
		return;
	}
	if (ScreenshotHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
		ScreenshotHandle.Reset();
	}
	if (Width <= 0 || Height <= 0 || Colors.Num() < Width * Height)
	{
		FinishCapture(false);
		return;
	}

	// scale down to the configured photo size (box filter)
	const int32 MaxWidth = GetDefault<USUGameplaySettings>()->PhotoMaxWidth;
	const int32 Factor = FMath::Max(1, FMath::DivideAndRoundUp(Width, MaxWidth));
	const int32 W = Width / Factor;
	const int32 H = Height / Factor;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(W * H);
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			int32 R = 0, G = 0, B = 0;
			for (int32 SY = 0; SY < Factor; ++SY)
			{
				for (int32 SX = 0; SX < Factor; ++SX)
				{
					const FColor& C = Colors[(Y * Factor + SY) * Width + (X * Factor + SX)];
					R += C.R;
					G += C.G;
					B += C.B;
				}
			}
			const int32 N = Factor * Factor;
			Pixels[Y * W + X] = FColor(static_cast<uint8>(R / N), static_cast<uint8>(G / N), static_cast<uint8>(B / N), 255);
		}
	}
	// shaky / out of focus shots really are blurry
	if (PendingBlur > 0.05f)
	{
		BoxBlur(Pixels, W, H, FMath::Clamp(FMath::RoundToInt(PendingBlur * 4.f), 1, 8));
	}
	if (USUPhotoStorage* Storage = USUPhotoStorage::Get(this))
	{
		Storage->StorePhoto(PendingMeta.PhotoId, MoveTemp(Pixels), W, H);
	}
	FinishCapture(true);
}

void USUPhotoCameraComponent::FinishCapture(bool bWithImage)
{
	bCapturing = false;
	CooldownUntil = GetWorld()->GetTimeSeconds() + 0.6;
	if (ScreenshotHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
		ScreenshotHandle.Reset();
	}
	ASUCharacter* C = GetCharacter();
	if (ASUPlayerController* PC = C ? Cast<ASUPlayerController>(C->GetController()) : nullptr)
	{
		if (ASUHUD* HUD = PC->GetSUHUD())
		{
			HUD->SetHiddenForScreenshot(false);
		}
	}
	if (!bWithImage)
	{
		PendingMeta.FileName.Empty();
	}
	if (C)
	{
		C->ServerRegisterPhoto(PendingMeta);
	}
}
