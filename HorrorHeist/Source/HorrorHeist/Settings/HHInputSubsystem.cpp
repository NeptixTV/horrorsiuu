#include "Settings/HHInputSubsystem.h"
#include "Settings/HHGameUserSettings.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "HHInput"

namespace
{
	FHHBindingInfo MakeBinding(const TCHAR* Id, const FText& Name, EHHInputAction Action, const FKey& Key, EHHAxisRole Role = EHHAxisRole::None)
	{
		FHHBindingInfo Info;
		Info.Id = FName(Id);
		Info.DisplayName = Name;
		Info.Action = Action;
		Info.DefaultKey = Key;
		Info.AxisRole = Role;
		return Info;
	}
}

const TArray<FHHBindingInfo>& UHHInputSubsystem::GetRebindableBindings()
{
	static const TArray<FHHBindingInfo> Bindings =
	{
		MakeBinding(TEXT("MoveForward"),	LOCTEXT("MoveForward", "Move Forward"),		EHHInputAction::Move,		EKeys::W, EHHAxisRole::Forward),
		MakeBinding(TEXT("MoveBackward"),	LOCTEXT("MoveBackward", "Move Backward"),	EHHInputAction::Move,		EKeys::S, EHHAxisRole::Backward),
		MakeBinding(TEXT("MoveLeft"),		LOCTEXT("MoveLeft", "Move Left"),			EHHInputAction::Move,		EKeys::A, EHHAxisRole::Left),
		MakeBinding(TEXT("MoveRight"),		LOCTEXT("MoveRight", "Move Right"),			EHHInputAction::Move,		EKeys::D, EHHAxisRole::Right),
		MakeBinding(TEXT("Sprint"),			LOCTEXT("Sprint", "Sprint"),				EHHInputAction::Sprint,		EKeys::LeftShift),
		MakeBinding(TEXT("Crouch"),			LOCTEXT("Crouch", "Crouch"),				EHHInputAction::Crouch,		EKeys::LeftControl),
		MakeBinding(TEXT("Jump"),			LOCTEXT("Jump", "Jump"),					EHHInputAction::Jump,		EKeys::SpaceBar),
		MakeBinding(TEXT("Interact"),		LOCTEXT("Interact", "Interact"),			EHHInputAction::Interact,	EKeys::E),
		MakeBinding(TEXT("Flashlight"),		LOCTEXT("Flashlight", "Flashlight"),		EHHInputAction::Flashlight,	EKeys::F),
		MakeBinding(TEXT("PushToTalk"),		LOCTEXT("PushToTalk", "Push To Talk"),		EHHInputAction::PushToTalk,	EKeys::V),
		MakeBinding(TEXT("Inventory"),		LOCTEXT("Inventory", "Inventory / Loadout"), EHHInputAction::Inventory,	EKeys::Tab),
		MakeBinding(TEXT("Map"),			LOCTEXT("Map", "Map / Job Board"),			EHHInputAction::Map,		EKeys::M),
		MakeBinding(TEXT("Ready"),			LOCTEXT("Ready", "Toggle Ready"),			EHHInputAction::Ready,		EKeys::R),
		MakeBinding(TEXT("ToggleView"),		LOCTEXT("ToggleView", "First / Third Person"), EHHInputAction::ToggleView, EKeys::T),
	};
	return Bindings;
}

UHHInputSubsystem* UHHInputSubsystem::Get(const APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UHHInputSubsystem>() : nullptr;
}

void UHHInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CreateActions();
	BuildGamepadContext();
	BuildKeyboardContext();

	SettingsHandle = UHHGameUserSettings::OnSettingsApplied().AddUObject(this, &UHHInputSubsystem::ApplyMappings);
}

void UHHInputSubsystem::Deinitialize()
{
	UHHGameUserSettings::OnSettingsApplied().Remove(SettingsHandle);
	Super::Deinitialize();
}

void UHHInputSubsystem::CreateActions()
{
	Actions.SetNum(static_cast<int32>(EHHInputAction::MAX));

	const UEnum* Enum = StaticEnum<EHHInputAction>();
	for (int32 Index = 0; Index < static_cast<int32>(EHHInputAction::MAX); ++Index)
	{
		const FString Name = FString::Printf(TEXT("IA_%s"), *Enum->GetNameStringByIndex(Index));
		UInputAction* Action = NewObject<UInputAction>(this, FName(*Name), RF_Transient);
		const EHHInputAction Type = static_cast<EHHInputAction>(Index);
		if (Type == EHHInputAction::Move || Type == EHHInputAction::Look || Type == EHHInputAction::LookGamepad)
		{
			Action->ValueType = EInputActionValueType::Axis2D;
		}
		else
		{
			Action->ValueType = EInputActionValueType::Boolean;
		}
		Actions[Index] = Action;
	}
}

UInputAction* UHHInputSubsystem::GetAction(EHHInputAction Action) const
{
	const int32 Index = static_cast<int32>(Action);
	return Actions.IsValidIndex(Index) ? Actions[Index].Get() : nullptr;
}

void UHHInputSubsystem::BuildKeyboardContext()
{
	if (!KeyboardContext)
	{
		KeyboardContext = NewObject<UInputMappingContext>(this, TEXT("IMC_KeyboardMouse"), RF_Transient);
	}
	KeyboardContext->UnmapAll();

	// Fixed, non-rebindable mappings.
	KeyboardContext->MapKey(GetAction(EHHInputAction::Look), EKeys::Mouse2D);
	KeyboardContext->MapKey(GetAction(EHHInputAction::Menu), EKeys::Escape);

	for (const FHHBindingInfo& Binding : GetRebindableBindings())
	{
		const FKey Key = GetCurrentKey(Binding.Id);
		if (!Key.IsValid())
		{
			continue;
		}

		FEnhancedActionKeyMapping& Mapping = KeyboardContext->MapKey(GetAction(Binding.Action), Key);
		switch (Binding.AxisRole)
		{
		case EHHAxisRole::Forward:
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(KeyboardContext));
			break;
		case EHHAxisRole::Backward:
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(KeyboardContext));
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(KeyboardContext));
			break;
		case EHHAxisRole::Left:
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(KeyboardContext));
			break;
		default:
			break;
		}
	}
}

void UHHInputSubsystem::BuildGamepadContext()
{
	GamepadContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Gamepad"), RF_Transient);

	auto MapStick = [this](EHHInputAction Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = GamepadContext->MapKey(GetAction(Action), Key);
		UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(GamepadContext);
		DeadZone->LowerThreshold = 0.18f;
		Mapping.Modifiers.Add(DeadZone);
	};

	MapStick(EHHInputAction::Move, EKeys::Gamepad_Left2D);
	MapStick(EHHInputAction::LookGamepad, EKeys::Gamepad_Right2D);

	GamepadContext->MapKey(GetAction(EHHInputAction::Jump), EKeys::Gamepad_FaceButton_Bottom);
	GamepadContext->MapKey(GetAction(EHHInputAction::Crouch), EKeys::Gamepad_FaceButton_Right);
	GamepadContext->MapKey(GetAction(EHHInputAction::Interact), EKeys::Gamepad_FaceButton_Left);
	GamepadContext->MapKey(GetAction(EHHInputAction::Inventory), EKeys::Gamepad_FaceButton_Top);
	GamepadContext->MapKey(GetAction(EHHInputAction::Sprint), EKeys::Gamepad_LeftThumbstick);
	GamepadContext->MapKey(GetAction(EHHInputAction::ToggleView), EKeys::Gamepad_RightThumbstick);
	GamepadContext->MapKey(GetAction(EHHInputAction::Flashlight), EKeys::Gamepad_DPad_Up);
	GamepadContext->MapKey(GetAction(EHHInputAction::Ready), EKeys::Gamepad_DPad_Down);
	GamepadContext->MapKey(GetAction(EHHInputAction::PushToTalk), EKeys::Gamepad_RightShoulder);
	GamepadContext->MapKey(GetAction(EHHInputAction::Map), EKeys::Gamepad_Special_Left);
	GamepadContext->MapKey(GetAction(EHHInputAction::Menu), EKeys::Gamepad_Special_Right);
}

void UHHInputSubsystem::ApplyMappings()
{
	BuildKeyboardContext();

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!EnhancedInput)
	{
		return;
	}

	EnhancedInput->RemoveMappingContext(KeyboardContext);
	EnhancedInput->RemoveMappingContext(GamepadContext);
	EnhancedInput->AddMappingContext(KeyboardContext, 0);
	EnhancedInput->AddMappingContext(GamepadContext, 0);
	EnhancedInput->RequestRebuildControlMappings();
}

FKey UHHInputSubsystem::GetCurrentKey(FName BindingId) const
{
	for (const FHHBindingInfo& Binding : GetRebindableBindings())
	{
		if (Binding.Id == BindingId)
		{
			const UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
			return Settings ? Settings->GetKeyBinding(BindingId, Binding.DefaultKey) : Binding.DefaultKey;
		}
	}
	return EKeys::Invalid;
}

bool UHHInputSubsystem::IsReservedKey(const FKey& Key)
{
	return !Key.IsValid()
		|| Key == EKeys::Escape
		|| Key == EKeys::Tilde
		|| Key == EKeys::Enter
		|| Key.IsGamepadKey()
		|| Key.IsAxis1D()
		|| Key.IsAxis2D()
		|| Key.IsTouch();
}

FName UHHInputSubsystem::RebindKey(FName BindingId, const FKey& NewKey)
{
	UHHGameUserSettings* Settings = UHHGameUserSettings::Get();
	if (!Settings || IsReservedKey(NewKey))
	{
		return NAME_None;
	}

	const FKey OldKey = GetCurrentKey(BindingId);
	FName Swapped = NAME_None;
	for (const FHHBindingInfo& Other : GetRebindableBindings())
	{
		if (Other.Id != BindingId && GetCurrentKey(Other.Id) == NewKey)
		{
			Settings->SetKeyBinding(Other.Id, OldKey);
			Swapped = Other.Id;
			break;
		}
	}

	Settings->SetKeyBinding(BindingId, NewKey);
	Settings->SaveSettings();
	ApplyMappings();
	return Swapped;
}

void UHHInputSubsystem::ResetBindingsToDefault()
{
	if (UHHGameUserSettings* Settings = UHHGameUserSettings::Get())
	{
		Settings->ResetKeyBindings();
		Settings->SaveSettings();
	}
	ApplyMappings();
}

FText UHHInputSubsystem::GetKeyDisplayText(FName BindingId) const
{
	const FKey Key = GetCurrentKey(BindingId);
	return Key.IsValid() ? Key.GetDisplayName(false) : LOCTEXT("Unbound", "-");
}

#undef LOCTEXT_NAMESPACE
