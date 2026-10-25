#include "Core/SUInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

UInputAction* USUInputConfig::MakeAction(const TCHAR* Name, EInputActionValueType Type)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
	Action->ValueType = Type;
	return Action;
}

UInputMappingContext* USUInputConfig::MakeContext(const TCHAR* Name)
{
	return NewObject<UInputMappingContext>(this, FName(Name));
}

void USUInputConfig::MapKey(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bNegate, bool bSwizzle, float Scale)
{
	if (!Context || !Action)
	{
		return;
	}
	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
	if (bSwizzle)
	{
		// default order YXZ: a 1D key drives the Y axis of a 2D action
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
	}
	if (bNegate)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}
	if (!FMath::IsNearlyEqual(Scale, 1.f))
	{
		UInputModifierScalar* Scalar = NewObject<UInputModifierScalar>(Context);
		Scalar->Scalar = FVector(Scale, Scale, Scale);
		Mapping.Modifiers.Add(Scalar);
	}
}

void USUInputConfig::Build()
{
	using EType = EInputActionValueType;

	GlobalContext = MakeContext(TEXT("IMC_Global"));
	OnFootContext = MakeContext(TEXT("IMC_OnFoot"));
	DriverContext = MakeContext(TEXT("IMC_Driver"));
	PassengerContext = MakeContext(TEXT("IMC_Passenger"));
	UIContext = MakeContext(TEXT("IMC_UI"));

	// ---- global
	Look = MakeAction(TEXT("IA_Look"), EType::Axis2D);
	LookGamepad = MakeAction(TEXT("IA_LookGamepad"), EType::Axis2D);
	Interact = MakeAction(TEXT("IA_Interact"), EType::Boolean);
	Pause = MakeAction(TEXT("IA_Pause"), EType::Boolean);
	Map = MakeAction(TEXT("IA_Map"), EType::Boolean);
	Inventory = MakeAction(TEXT("IA_Inventory"), EType::Boolean);
	Journal = MakeAction(TEXT("IA_Journal"), EType::Boolean);
	Gallery = MakeAction(TEXT("IA_Gallery"), EType::Boolean);
	Help = MakeAction(TEXT("IA_Help"), EType::Boolean);
	QuickSave = MakeAction(TEXT("IA_QuickSave"), EType::Boolean);
	QuickLoad = MakeAction(TEXT("IA_QuickLoad"), EType::Boolean);
	PushToTalk = MakeAction(TEXT("IA_PushToTalk"), EType::Boolean);

	MapKey(GlobalContext, Look, EKeys::Mouse2D);
	MapKey(GlobalContext, LookGamepad, EKeys::Gamepad_Right2D);
	MapKey(GlobalContext, Interact, EKeys::E);
	MapKey(GlobalContext, Interact, EKeys::Gamepad_FaceButton_Left);
	MapKey(GlobalContext, Pause, EKeys::Escape);
	MapKey(GlobalContext, Pause, EKeys::Gamepad_Special_Right);
	MapKey(GlobalContext, Map, EKeys::M);
	MapKey(GlobalContext, Map, EKeys::Gamepad_Special_Left);
	MapKey(GlobalContext, Inventory, EKeys::Tab);
	MapKey(GlobalContext, Inventory, EKeys::I);
	MapKey(GlobalContext, Journal, EKeys::J);
	MapKey(GlobalContext, Gallery, EKeys::P);
	MapKey(GlobalContext, Help, EKeys::F1);
	MapKey(GlobalContext, QuickSave, EKeys::F5);
	MapKey(GlobalContext, QuickLoad, EKeys::F9);
	MapKey(GlobalContext, PushToTalk, EKeys::T);

	// ---- on foot
	Move = MakeAction(TEXT("IA_Move"), EType::Axis2D);
	Jump = MakeAction(TEXT("IA_Jump"), EType::Boolean);
	Sprint = MakeAction(TEXT("IA_Sprint"), EType::Boolean);
	Crouch = MakeAction(TEXT("IA_Crouch"), EType::Boolean);
	InteractAlt = MakeAction(TEXT("IA_InteractAlt"), EType::Boolean);
	Flashlight = MakeAction(TEXT("IA_Flashlight"), EType::Boolean);
	CameraToggle = MakeAction(TEXT("IA_CameraToggle"), EType::Boolean);
	Aim = MakeAction(TEXT("IA_Aim"), EType::Boolean);
	Shutter = MakeAction(TEXT("IA_Shutter"), EType::Boolean);
	Zoom = MakeAction(TEXT("IA_Zoom"), EType::Axis1D);
	CameraFlash = MakeAction(TEXT("IA_CameraFlash"), EType::Boolean);

	// Move: X = right, Y = forward
	MapKey(OnFootContext, Move, EKeys::W, false, true);
	MapKey(OnFootContext, Move, EKeys::S, true, true);
	MapKey(OnFootContext, Move, EKeys::D);
	MapKey(OnFootContext, Move, EKeys::A, true);
	MapKey(OnFootContext, Move, EKeys::Gamepad_Left2D);
	MapKey(OnFootContext, Jump, EKeys::SpaceBar);
	MapKey(OnFootContext, Jump, EKeys::Gamepad_FaceButton_Bottom);
	MapKey(OnFootContext, Sprint, EKeys::LeftShift);
	MapKey(OnFootContext, Sprint, EKeys::Gamepad_LeftThumbstick);
	MapKey(OnFootContext, Crouch, EKeys::LeftControl);
	MapKey(OnFootContext, Crouch, EKeys::Gamepad_FaceButton_Right);
	MapKey(OnFootContext, InteractAlt, EKeys::Q);
	MapKey(OnFootContext, InteractAlt, EKeys::Gamepad_FaceButton_Top);
	MapKey(OnFootContext, Flashlight, EKeys::F);
	MapKey(OnFootContext, Flashlight, EKeys::Gamepad_DPad_Up);
	MapKey(OnFootContext, CameraToggle, EKeys::C);
	MapKey(OnFootContext, CameraToggle, EKeys::Gamepad_DPad_Down);
	MapKey(OnFootContext, Aim, EKeys::RightMouseButton);
	MapKey(OnFootContext, Aim, EKeys::Gamepad_LeftTrigger);
	MapKey(OnFootContext, Shutter, EKeys::LeftMouseButton);
	MapKey(OnFootContext, Shutter, EKeys::Gamepad_RightTrigger);
	MapKey(OnFootContext, Zoom, EKeys::MouseWheelAxis);
	MapKey(OnFootContext, Zoom, EKeys::Gamepad_DPad_Right);
	MapKey(OnFootContext, Zoom, EKeys::Gamepad_DPad_Left, true);
	MapKey(OnFootContext, CameraFlash, EKeys::G);

	// ---- driver
	Throttle = MakeAction(TEXT("IA_Throttle"), EType::Axis1D);
	Brake = MakeAction(TEXT("IA_Brake"), EType::Axis1D);
	Steer = MakeAction(TEXT("IA_Steer"), EType::Axis1D);
	Handbrake = MakeAction(TEXT("IA_Handbrake"), EType::Boolean);
	Engine = MakeAction(TEXT("IA_Engine"), EType::Boolean);
	Lights = MakeAction(TEXT("IA_Lights"), EType::Boolean);
	HighBeam = MakeAction(TEXT("IA_HighBeam"), EType::Boolean);
	Horn = MakeAction(TEXT("IA_Horn"), EType::Boolean);
	BlinkLeft = MakeAction(TEXT("IA_BlinkLeft"), EType::Boolean);
	BlinkRight = MakeAction(TEXT("IA_BlinkRight"), EType::Boolean);
	Hazard = MakeAction(TEXT("IA_Hazard"), EType::Boolean);
	Wipers = MakeAction(TEXT("IA_Wipers"), EType::Boolean);

	MapKey(DriverContext, Throttle, EKeys::W);
	MapKey(DriverContext, Throttle, EKeys::Up);
	MapKey(DriverContext, Throttle, EKeys::Gamepad_RightTriggerAxis);
	MapKey(DriverContext, Brake, EKeys::S);
	MapKey(DriverContext, Brake, EKeys::Down);
	MapKey(DriverContext, Brake, EKeys::Gamepad_LeftTriggerAxis);
	MapKey(DriverContext, Steer, EKeys::D);
	MapKey(DriverContext, Steer, EKeys::A, true);
	MapKey(DriverContext, Steer, EKeys::Gamepad_LeftX);
	MapKey(DriverContext, Handbrake, EKeys::SpaceBar);
	MapKey(DriverContext, Handbrake, EKeys::Gamepad_FaceButton_Right);
	MapKey(DriverContext, Engine, EKeys::F);
	MapKey(DriverContext, Engine, EKeys::Gamepad_FaceButton_Top);
	MapKey(DriverContext, Lights, EKeys::L);
	MapKey(DriverContext, Lights, EKeys::Gamepad_DPad_Up);
	MapKey(DriverContext, HighBeam, EKeys::K);
	MapKey(DriverContext, Horn, EKeys::H);
	MapKey(DriverContext, Horn, EKeys::Gamepad_LeftThumbstick);
	MapKey(DriverContext, BlinkLeft, EKeys::Q);
	MapKey(DriverContext, BlinkRight, EKeys::R);
	MapKey(DriverContext, Hazard, EKeys::X);
	MapKey(DriverContext, Wipers, EKeys::V);

	// ---- any seat: radio
	RadioPower = MakeAction(TEXT("IA_RadioPower"), EType::Boolean);
	RadioNext = MakeAction(TEXT("IA_RadioNext"), EType::Boolean);
	RadioPrev = MakeAction(TEXT("IA_RadioPrev"), EType::Boolean);
	RadioVolume = MakeAction(TEXT("IA_RadioVolume"), EType::Axis1D);

	MapKey(PassengerContext, RadioPower, EKeys::N);
	MapKey(PassengerContext, RadioPower, EKeys::Gamepad_DPad_Down);
	MapKey(PassengerContext, RadioNext, EKeys::Right);
	MapKey(PassengerContext, RadioNext, EKeys::Gamepad_DPad_Right);
	MapKey(PassengerContext, RadioPrev, EKeys::Left);
	MapKey(PassengerContext, RadioPrev, EKeys::Gamepad_DPad_Left);
	MapKey(PassengerContext, RadioVolume, EKeys::MouseWheelAxis);

	// ---- UI
	UINavigate = MakeAction(TEXT("IA_UINavigate"), EType::Axis2D);
	UIAccept = MakeAction(TEXT("IA_UIAccept"), EType::Boolean);
	UIBack = MakeAction(TEXT("IA_UIBack"), EType::Boolean);
	UIClick = MakeAction(TEXT("IA_UIClick"), EType::Boolean);
	UIScroll = MakeAction(TEXT("IA_UIScroll"), EType::Axis1D);
	UITab = MakeAction(TEXT("IA_UITab"), EType::Axis1D);

	MapKey(UIContext, UINavigate, EKeys::Up, false, true);
	MapKey(UIContext, UINavigate, EKeys::W, false, true);
	MapKey(UIContext, UINavigate, EKeys::Down, true, true);
	MapKey(UIContext, UINavigate, EKeys::S, true, true);
	MapKey(UIContext, UINavigate, EKeys::Right);
	MapKey(UIContext, UINavigate, EKeys::D);
	MapKey(UIContext, UINavigate, EKeys::Left, true);
	MapKey(UIContext, UINavigate, EKeys::A, true);
	MapKey(UIContext, UINavigate, EKeys::Gamepad_DPad_Up, false, true);
	MapKey(UIContext, UINavigate, EKeys::Gamepad_DPad_Down, true, true);
	MapKey(UIContext, UINavigate, EKeys::Gamepad_DPad_Right);
	MapKey(UIContext, UINavigate, EKeys::Gamepad_DPad_Left, true);
	MapKey(UIContext, UIAccept, EKeys::Enter);
	MapKey(UIContext, UIAccept, EKeys::SpaceBar);
	MapKey(UIContext, UIAccept, EKeys::Gamepad_FaceButton_Bottom);
	MapKey(UIContext, UIBack, EKeys::Escape);
	MapKey(UIContext, UIBack, EKeys::Tab);
	MapKey(UIContext, UIBack, EKeys::BackSpace);
	MapKey(UIContext, UIBack, EKeys::M);
	MapKey(UIContext, UIBack, EKeys::I);
	MapKey(UIContext, UIBack, EKeys::J);
	MapKey(UIContext, UIBack, EKeys::P);
	MapKey(UIContext, UIBack, EKeys::F1);
	MapKey(UIContext, UIBack, EKeys::Gamepad_FaceButton_Right);
	MapKey(UIContext, UIClick, EKeys::LeftMouseButton);
	MapKey(UIContext, UIScroll, EKeys::MouseWheelAxis);
	MapKey(UIContext, UITab, EKeys::E);
	MapKey(UIContext, UITab, EKeys::Q, true);
	MapKey(UIContext, UITab, EKeys::Gamepad_RightShoulder);
	MapKey(UIContext, UITab, EKeys::Gamepad_LeftShoulder, true);
}

FString USUInputConfig::KeyName(const FKey& Key)
{
	return Key.GetDisplayName().ToString();
}
