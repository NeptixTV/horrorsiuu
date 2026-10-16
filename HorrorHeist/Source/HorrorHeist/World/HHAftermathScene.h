#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HHTypes.h"
#include "HHAftermathScene.generated.h"

class UPointLightComponent;
class UAudioComponent;

/**
 * The morning after a job at one house: tape across the yard, the patrol car with its lights
 * going, forensics taking photos, a covered body on the lawn, the street closed off, the owner
 * telling a reporter what happened. Hidden until the lobby game state's FHHAftermath names this
 * scene's job; never on job maps.
 *
 * The scene's pieces are ordinary level actors tagged with SceneTag or one of its variants -
 * StaticMeshActors are shown/hidden, AHHTownspeople with that SceneTag are switched on:
 *   <SceneTag>          whenever the town noticed something (owner + reporter)
 *   <SceneTag>_Police   police were there (car, officer, tape, forensics)
 *   <SceneTag>_Body     a crew member died in the house (covered body, coroner)
 *   <SceneTag>_Road     the street is closed (cones, barriers; ClosedNodes stop the traffic)
 *   <SceneTag>_Haunt    the house showed itself (the owners tell the paper about it)
 * The actor itself sits on the patrol car's roof: it flashes the light bar and a camera flash.
 */
UCLASS()
class HORRORHEIST_API AHHAftermathScene : public AActor
{
	GENERATED_BODY()

public:
	AHHAftermathScene();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** True while police work at an active scene (or its road block stands) at this node: traffic stays home. */
	static bool IsNodeClosed(const UObject* WorldContextObject, FName NodeId);

	/** "BURGLARS FLEE POLICE ON ASHGROVE LANE" and two sentences for the morning paper. */
	static FText Headline(const UObject* WorldContextObject, const FHHMissionSummary& Summary);
	static FText Story(const UObject* WorldContextObject, const FHHMissionSummary& Summary);

protected:
	void Refresh();
	void Apply(const TSet<FName>& Active);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftermath")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftermath")
	TObjectPtr<UPointLightComponent> LightRed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftermath")
	TObjectPtr<UPointLightComponent> LightBlue;

	/** The forensic photographer's flash (placed at FlashOffset). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftermath")
	TObjectPtr<UPointLightComponent> CameraFlash;

	/** Police radio murmur by the car. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aftermath")
	TObjectPtr<UAudioComponent> Radio;

	/** Job whose aftermath this is (mission id). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aftermath")
	FName MissionId;

	/** Tag of the scene's actors (variants add _Police, _Body, _Road, _Haunt). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aftermath")
	FName SceneTag;

	/** Road nodes the town's traffic avoids while the police are here or the road block stands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aftermath")
	TArray<FName> ClosedNodes;

	/** Where the photographer's flash goes off, relative to this actor (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aftermath")
	FVector FlashOffset = FVector(0.f, 0.f, 150.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aftermath")
	TObjectPtr<USoundBase> FlashSound;

private:
	TSet<FName> Shown;
	bool bApplied = false;
	bool bPoliceLights = false;
	bool bPhotographer = false;
	bool bRoadClosed = false;
	float RefreshTimer = 0.f;
	float FlashTimer = 0.f;
	float NextPhoto = 3.f;
	float FlashOff = 0.f;
	bool bFlashPhase = false;
};
