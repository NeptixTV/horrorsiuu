// SIGNAL UNKNOWN – shared enums, structs and event names.
#pragma once

#include "CoreMinimal.h"
#include "SUTypes.generated.h"

class USoundBase;

/** Which of the two interaction keys was used (E = primary, Q = secondary). */
UENUM(BlueprintType)
enum class ESUInteractOption : uint8
{
	Primary,
	Secondary
};

/** Full-screen / overlay screens drawn by ASUHUD. */
UENUM(BlueprintType)
enum class ESUScreen : uint8
{
	None,
	MainMenu,
	Pause,
	Settings,
	Join,
	Inventory,
	Terminal,
	Map,
	Shop,
	Document,
	Gallery,
	Help,
	Trunk
};

/** Physical surfaces (footsteps, tyre grip). */
UENUM(BlueprintType)
enum class ESUSurface : uint8
{
	Grass,
	Dirt,
	Gravel,
	Asphalt,
	Wood,
	Concrete,
	Metal
};

UENUM(BlueprintType)
enum class ESUMissionPhase : uint8
{
	None,          // nothing going on, waiting for the next call
	Incoming,      // radio is crackling, nobody answered yet
	Briefing,      // the dispatcher is talking
	Offered,       // waiting for the player to confirm
	Active,        // objectives running
	Completed      // report submitted
};

/** What a character holds in the hands (besides the flashlight). */
UENUM(BlueprintType)
enum class ESUHeldItem : uint8
{
	None,
	Camera,
	Nozzle
};

/** Text shown by the interaction prompt. */
USTRUCT(BlueprintType)
struct FSUInteractionPrompt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FText Primary;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FText Secondary;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	bool bPrimary = false;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	bool bSecondary = false;

	/** Shown greyed out (e.g. "Verschlossen"). */
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FText Info;
};

/** One line of dialogue / radio speech with subtitles. */
USTRUCT(BlueprintType)
struct FSUSpeechLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speech")
	FText Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speech")
	FText Text;

	/** Seconds the line stays on screen / the voice talks. 0 = estimate from text length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speech")
	float Duration = 0.f;

	/** Optional recorded voice. Without it the line is voiced by the procedural radio voice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speech")
	TSoftObjectPtr<USoundBase> Voice;

	float GetDuration() const
	{
		return Duration > 0.f ? Duration : FMath::Clamp(1.2f + Text.ToString().Len() * 0.055f, 2.0f, 9.0f);
	}
};

/** A booking on the team account. Amounts are in cents. */
USTRUCT(BlueprintType)
struct FSUTransaction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 AmountCents = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	float DayTimeHours = 0.f;
};

/** Short-lived HUD notification. */
USTRUCT(BlueprintType)
struct FSUNotification
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	FText Text;

	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	float Duration = 4.f;
};

/** Event names used by the event bus (USUEventSubsystem) and mission objectives. */
namespace SUEvent
{
	inline const FName RadioAnswered = TEXT("Radio.Answered");
	inline const FName MissionAccepted = TEXT("Mission.Accepted");
	inline const FName MapViewed = TEXT("Map.Viewed");
	inline const FName ItemPicked = TEXT("Item.Picked");
	inline const FName ItemInspected = TEXT("Item.Inspected");
	inline const FName AreaEntered = TEXT("Area.Entered");
	inline const FName AreaExited = TEXT("Area.Exited");
	inline const FName VehicleEntered = TEXT("Vehicle.Entered");
	inline const FName VehicleExited = TEXT("Vehicle.Exited");
	inline const FName EngineStarted = TEXT("Vehicle.EngineStarted");
	inline const FName PhotoEvidence = TEXT("Photo.Evidence");
	inline const FName PhotoCategory = TEXT("Photo.Category");
	inline const FName HorrorScripted = TEXT("Horror.Scripted");
	inline const FName TerminalOpened = TEXT("Terminal.Opened");
	inline const FName ReportSubmitted = TEXT("Report.Submitted");
	inline const FName Refueled = TEXT("Fuel.Refueled");
	inline const FName FuelPaid = TEXT("Fuel.Paid");
	inline const FName Slept = TEXT("Player.Slept");
}

/** Area ids used by ASUAreaTrigger. */
namespace SUArea
{
	inline const FName TowerCabin = TEXT("TowerCabin");
	inline const FName TowerParking = TEXT("TowerParking");
	inline const FName TowerGrounds = TEXT("TowerGrounds");
	inline const FName GasStation = TEXT("GasStation");
	inline const FName GasShop = TEXT("GasShop");
	inline const FName ForsthausGrounds = TEXT("ForsthausGrounds");
	inline const FName ForsthausInterior = TEXT("ForsthausInterior");
	inline const FName M02Site = TEXT("M02Site");
	inline const FName Settlement = TEXT("Settlement");
	inline const FName Shed = TEXT("Shed");
}

/** Component tags that mark the surface type of a collision mesh. */
namespace SUSurfaceTag
{
	inline const FName Asphalt = TEXT("Surface.Asphalt");
	inline const FName Gravel = TEXT("Surface.Gravel");
	inline const FName Dirt = TEXT("Surface.Dirt");
	inline const FName Wood = TEXT("Surface.Wood");
	inline const FName Concrete = TEXT("Surface.Concrete");
	inline const FName Metal = TEXT("Surface.Metal");
	inline const FName Grass = TEXT("Surface.Grass");
}

/** Convenience: literal text (all player facing text is German for now). */
inline FText SUText(const TCHAR* InText)
{
	return FText::FromString(FString(InText));
}

/** Formats cents as "123,45 $". */
inline FString SUFormatMoney(int32 Cents)
{
	const bool bNegative = Cents < 0;
	const int32 Abs = FMath::Abs(Cents);
	return FString::Printf(TEXT("%s%d,%02d $"), bNegative ? TEXT("-") : TEXT(""), Abs / 100, Abs % 100);
}

/** Formats a day time (hours, 0..24) as "17:42". */
inline FString SUFormatTime(float Hours)
{
	const float H = FMath::Fmod(FMath::Max(0.f, Hours), 24.f);
	const int32 Hour = FMath::FloorToInt(H);
	const int32 Minute = FMath::Clamp(FMath::FloorToInt((H - Hour) * 60.f), 0, 59);
	return FString::Printf(TEXT("%02d:%02d"), Hour, Minute);
}
