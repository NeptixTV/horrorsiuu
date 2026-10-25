// SIGNAL UNKNOWN – item definitions.
#pragma once

#include "CoreMinimal.h"
#include "SUItemDatabase.generated.h"

UENUM(BlueprintType)
enum class ESUItemShape : uint8
{
	Box,
	Cylinder,
	Sphere,
	Flat
};

USTRUCT(BlueprintType)
struct FSUItemDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 MaxStack = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 PriceCents = 0;

	/** Consumed on use (food, bandage, battery). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bConsumable = false;

	/** Health restored when used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float HealthRestore = 0.f;

	/** Device with a battery (charge stored per slot). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bHasBattery = false;

	/** Mission item – cannot be dropped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bQuestItem = false;

	/** Placeholder world representation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	ESUItemShape Shape = ESUItemShape::Box;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FVector Size = FVector(10.f, 10.f, 10.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FLinearColor Color = FLinearColor(0.2f, 0.2f, 0.2f);
};

namespace SUItem
{
	inline const FName Flashlight = TEXT("Flashlight");
	inline const FName Camera = TEXT("Camera");
	inline const FName Battery = TEXT("Battery");
	inline const FName Bandage = TEXT("Bandage");
	inline const FName Toolkit = TEXT("Toolkit");
	inline const FName Snack = TEXT("Snack");
	inline const FName Drink = TEXT("Drink");
	inline const FName Coffee = TEXT("Coffee");
	inline const FName CarKey = TEXT("CarKey");
	inline const FName RangerNotebook = TEXT("RangerNotebook");
	inline const FName M02Phone = TEXT("M02Phone");
}

/** Static item table. Content can later move into a DataTable without changing callers. */
class SIGNALUNKNOWN_API FSUItemDatabase
{
public:
	static const TArray<FSUItemDef>& All();
	static const FSUItemDef* Find(FName Id);
	static FText GetName(FName Id);
};
