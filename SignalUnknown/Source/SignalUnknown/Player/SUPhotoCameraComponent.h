// SIGNAL UNKNOWN – the in-game photo camera.
//
// Taking a photo:
//  1. the evidence subsystem rates every registered evidence object for this exact view
//     (frame coverage, occlusion traces, distance, light, shake, focus),
//  2. the real rendered frame is captured (viewport screenshot without HUD), scaled, blurred if
//     the shot was shaky, and stored as PNG,
//  3. the metadata (with the evidence hits) is sent to the server and counts for the mission.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Evidence/SUPhotoTypes.h"
#include "SUPhotoCameraComponent.generated.h"

class ASUCharacter;
class USUProceduralAudioComponent;

UCLASS(ClassGroup = (SignalUnknown), meta = (BlueprintSpawnableComponent))
class SIGNALUNKNOWN_API USUPhotoCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USUPhotoCameraComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SetAiming(bool bInAiming);
	bool IsAiming() const { return bAiming; }
	void AdjustZoom(float Delta);
	float GetZoom() const { return Zoom; }
	void ToggleFlash();
	bool IsFlashEnabled() const { return bFlash; }
	void TakePhoto();

	float GetBattery() const;
	int32 GetPhotoCount() const;
	int32 GetCapacity() const;
	float GetFocusError() const { return FocusError; }
	float GetFocusDistance() const { return FocusDistance; }
	bool IsShaky() const;
	bool IsCapturing() const { return bCapturing; }
	double GetLastShotTime() const { return LastShotTime; }
	FString GetExposureText() const;

private:
	bool bAiming = false;
	float Zoom = 1.f;
	float TargetZoom = 1.f;
	bool bFlash = true;
	float FocusDistance = 400.f;
	float TargetFocus = 400.f;
	float FocusError = 0.f;
	double CooldownUntil = 0.0;
	double LastShotTime = -100.0;

	bool bCapturing = false;
	double CaptureStartTime = 0.0;
	FDelegateHandle ScreenshotHandle;
	FSUPhotoMeta PendingMeta;
	float PendingBlur = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<USUProceduralAudioComponent> ShutterSound;

	ASUCharacter* GetCharacter() const;
	void HandleScreenshot(int32 Width, int32 Height, const TArray<FColor>& Colors);
	void FinishCapture(bool bWithImage);
	void UpdateFocus(float DeltaTime);
	void ApplyView(float DeltaTime);
	void NotifyLocal(const TCHAR* Text) const;
	static void BoxBlur(TArray<FColor>& Pixels, int32 Width, int32 Height, int32 Radius);
};
