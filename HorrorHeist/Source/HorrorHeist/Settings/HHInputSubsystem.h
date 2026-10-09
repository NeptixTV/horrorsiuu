#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "InputCoreTypes.h"
#include "HHInputSubsystem.generated.h"

class UInputAction;
class UInputMappingContext;

/** Gameplay actions. Created at runtime so no input assets are required. */
UENUM()
enum class EHHInputAction : uint8
{
	Move,
	Look,
	LookGamepad,
	Jump,
	Sprint,
	Crouch,
	Interact,
	Flashlight,
	PushToTalk,
	Inventory,
	Map,
	Menu,
	Ready,
	ToggleView,
	MAX UMETA(Hidden)
};

/** Movement directions share the Move action and differ by modifiers. */
enum class EHHAxisRole : uint8
{
	None,
	Forward,
	Backward,
	Left,
	Right
};

/** A rebindable keyboard/mouse binding shown in Settings > Controls. */
struct FHHBindingInfo
{
	FName Id;
	FText DisplayName;
	EHHInputAction Action = EHHInputAction::Interact;
	FKey DefaultKey;
	EHHAxisRole AxisRole = EHHAxisRole::None;
};

/**
 * Builds the Enhanced Input actions + mapping contexts for a local player, applies the
 * player's key bindings from UHHGameUserSettings and rebuilds mappings when they change.
 */
UCLASS()
class HORRORHEIST_API UHHInputSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UHHInputSubsystem* Get(const class APlayerController* PlayerController);

	UInputAction* GetAction(EHHInputAction Action) const;

	/** Rebuilds the keyboard context from current settings and re-registers both contexts. */
	void ApplyMappings();

	static const TArray<FHHBindingInfo>& GetRebindableBindings();
	FKey GetCurrentKey(FName BindingId) const;

	/** Assigns a key; if another binding used it, the two swap. Returns the swapped binding id (or None). */
	FName RebindKey(FName BindingId, const FKey& NewKey);
	void ResetBindingsToDefault();

	/** Keys that may never be bound (menu, console). */
	static bool IsReservedKey(const FKey& Key);

	/** Display text for the key bound to a binding, e.g. "E". */
	FText GetKeyDisplayText(FName BindingId) const;

private:
	void CreateActions();
	void BuildKeyboardContext();
	void BuildGamepadContext();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> Actions;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> KeyboardContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> GamepadContext;

	FDelegateHandle SettingsHandle;
};
