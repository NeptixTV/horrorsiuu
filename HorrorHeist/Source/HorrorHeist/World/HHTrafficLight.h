#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HHTrafficLight.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EHHSignal : uint8
{
	Red,
	Amber,
	Green
};

/**
 * One signal head on a pole at a junction. The phases are a pure function of the game-state
 * clock, so every light, every car and every machine agrees without replication:
 * axis 0 (east-west traffic) green, amber, all red, then axis 1 (north-south), amber, all red.
 * The three lamp components (lit lenses) are shown or hidden; the housing has dark lenses.
 */
UCLASS()
class HORRORHEIST_API AHHTrafficLight : public AActor
{
	GENERATED_BODY()

public:
	AHHTrafficLight();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Signal for traffic along Axis (0 = along world X, 1 = along world Y) at a game-state time. */
	static EHHSignal GetSignal(int32 SignalAxis, float ServerTime);
	/** Seconds of one full cycle. */
	static constexpr float CycleSeconds = 34.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
	TObjectPtr<UStaticMeshComponent> Housing;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
	TObjectPtr<UStaticMeshComponent> RedLamp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
	TObjectPtr<UStaticMeshComponent> AmberLamp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
	TObjectPtr<UStaticMeshComponent> GreenLamp;

	/** Which traffic this head shows its colours to (0 = along X, 1 = along Y). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	int32 Axis = 0;

private:
	EHHSignal Shown = EHHSignal::Green;
	bool bFirst = true;
};
