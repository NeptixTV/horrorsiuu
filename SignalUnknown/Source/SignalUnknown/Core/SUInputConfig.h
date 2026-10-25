// SIGNAL UNKNOWN – Enhanced Input actions and mapping contexts, created in code.
//
// The project intentionally has no binary .uasset files yet, so the input actions are built at
// runtime. Once input assets exist they can replace this object one by one.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "InputActionValue.h"
#include "SUInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class SIGNALUNKNOWN_API USUInputConfig : public UObject
{
	GENERATED_BODY()

public:
	void Build();

	// ---------------------------------------------------------------- contexts
	/** Always active outside of menus: look, interact, menus. */
	UPROPERTY() TObjectPtr<UInputMappingContext> GlobalContext;
	/** Walking around. */
	UPROPERTY() TObjectPtr<UInputMappingContext> OnFootContext;
	/** Driver seat. */
	UPROPERTY() TObjectPtr<UInputMappingContext> DriverContext;
	/** Any seat: radio controls. */
	UPROPERTY() TObjectPtr<UInputMappingContext> PassengerContext;
	/** Menus and in-world device screens. */
	UPROPERTY() TObjectPtr<UInputMappingContext> UIContext;

	// ---------------------------------------------------------------- global
	UPROPERTY() TObjectPtr<UInputAction> Look;
	UPROPERTY() TObjectPtr<UInputAction> LookGamepad;
	UPROPERTY() TObjectPtr<UInputAction> Interact;
	UPROPERTY() TObjectPtr<UInputAction> Pause;
	UPROPERTY() TObjectPtr<UInputAction> Map;
	UPROPERTY() TObjectPtr<UInputAction> Inventory;
	UPROPERTY() TObjectPtr<UInputAction> Journal;
	UPROPERTY() TObjectPtr<UInputAction> Gallery;
	UPROPERTY() TObjectPtr<UInputAction> Help;
	UPROPERTY() TObjectPtr<UInputAction> QuickSave;
	UPROPERTY() TObjectPtr<UInputAction> QuickLoad;
	UPROPERTY() TObjectPtr<UInputAction> PushToTalk;

	// ---------------------------------------------------------------- on foot
	UPROPERTY() TObjectPtr<UInputAction> Move;
	UPROPERTY() TObjectPtr<UInputAction> Jump;
	UPROPERTY() TObjectPtr<UInputAction> Sprint;
	UPROPERTY() TObjectPtr<UInputAction> Crouch;
	UPROPERTY() TObjectPtr<UInputAction> InteractAlt;
	UPROPERTY() TObjectPtr<UInputAction> Flashlight;
	UPROPERTY() TObjectPtr<UInputAction> CameraToggle;
	UPROPERTY() TObjectPtr<UInputAction> Aim;
	UPROPERTY() TObjectPtr<UInputAction> Shutter;
	UPROPERTY() TObjectPtr<UInputAction> Zoom;
	UPROPERTY() TObjectPtr<UInputAction> CameraFlash;

	// ---------------------------------------------------------------- vehicle
	UPROPERTY() TObjectPtr<UInputAction> Throttle;
	UPROPERTY() TObjectPtr<UInputAction> Brake;
	UPROPERTY() TObjectPtr<UInputAction> Steer;
	UPROPERTY() TObjectPtr<UInputAction> Handbrake;
	UPROPERTY() TObjectPtr<UInputAction> Engine;
	UPROPERTY() TObjectPtr<UInputAction> Lights;
	UPROPERTY() TObjectPtr<UInputAction> HighBeam;
	UPROPERTY() TObjectPtr<UInputAction> Horn;
	UPROPERTY() TObjectPtr<UInputAction> BlinkLeft;
	UPROPERTY() TObjectPtr<UInputAction> BlinkRight;
	UPROPERTY() TObjectPtr<UInputAction> Hazard;
	UPROPERTY() TObjectPtr<UInputAction> Wipers;
	UPROPERTY() TObjectPtr<UInputAction> RadioPower;
	UPROPERTY() TObjectPtr<UInputAction> RadioNext;
	UPROPERTY() TObjectPtr<UInputAction> RadioPrev;
	UPROPERTY() TObjectPtr<UInputAction> RadioVolume;

	// ---------------------------------------------------------------- UI
	UPROPERTY() TObjectPtr<UInputAction> UINavigate;
	UPROPERTY() TObjectPtr<UInputAction> UIAccept;
	UPROPERTY() TObjectPtr<UInputAction> UIBack;
	UPROPERTY() TObjectPtr<UInputAction> UIClick;
	UPROPERTY() TObjectPtr<UInputAction> UIScroll;
	UPROPERTY() TObjectPtr<UInputAction> UITab;

	/** Human readable key list for the help screen and prompts. */
	static FString KeyName(const FKey& Key);

private:
	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType Type);
	UInputMappingContext* MakeContext(const TCHAR* Name);
	void MapKey(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bNegate = false, bool bSwizzle = false, float Scale = 1.f);
};
