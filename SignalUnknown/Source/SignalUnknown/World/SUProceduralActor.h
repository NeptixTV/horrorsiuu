// SIGNAL UNKNOWN – base for actors whose geometry is assembled in code.
//
// Static geometry is batched into one instanced mesh component per (shape, material, collision)
// so a whole building costs a handful of draw calls. Moving parts (doors, drawers) are separate
// components created with AddPart.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/SUAssetLibrary.h"
#include "SUProceduralActor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USpotLightComponent;
class UTextRenderComponent;
class USUAssetLibrary;

/** Opening in a wall, measured along the wall from its start point. */
struct FSUOpening
{
	float Start = 0.f;
	float Width = 100.f;
	float Bottom = 0.f;
	float Top = 210.f;

	FSUOpening() = default;
	FSUOpening(float InStart, float InWidth, float InBottom, float InTop)
		: Start(InStart), Width(InWidth), Bottom(InBottom), Top(InTop) {}
};

UCLASS()
class SIGNALUNKNOWN_API ASUProceduralActor : public AActor
{
	GENERATED_BODY()

public:
	ASUProceduralActor();

	// ---------------------------------------------------------------- batched geometry (actor space)
	void AddBox(const FVector& Center, const FVector& Size, ESUMat Mat, const FRotator& Rot = FRotator::ZeroRotator, bool bCollision = true);
	void AddShape(ESUShape Shape, const FVector& Center, const FVector& Size, ESUMat Mat, const FRotator& Rot = FRotator::ZeroRotator, bool bCollision = true);
	/** Box spanning two points (beams, braces, rails). */
	void AddBeam(const FVector& From, const FVector& To, float Thickness, ESUMat Mat, bool bCollision = true);
	/** Wall from A to B (actor space XY) with openings for doors/windows. */
	void AddWall(const FVector2D& A, const FVector2D& B, float BaseZ, float Height, float Thickness, ESUMat Mat, const TArray<FSUOpening>& Openings = TArray<FSUOpening>());
	/** Window frame with cross bars inside an opening of a wall from A to B. */
	void AddWindowFrame(const FVector2D& A, const FVector2D& B, float BaseZ, float Thickness, const FSUOpening& Opening, ESUMat Mat, bool bCross = true);
	/** Gable roof: ridge along local X of the given yaw, eaves at EaveZ. */
	void AddGableRoof(const FVector& Center, float Length, float Width, float Rise, float Overhang, float Thickness, ESUMat Mat, float Yaw = 0.f);
	/** Straight flight of stairs from Start (front edge centre of the first step) towards Dir. */
	void AddStairs(const FVector& Start, const FVector2D& Dir, int32 Steps, float Rise, float Run, float Width, ESUMat Mat, bool bSolid = false);

	/** Surface tag (SUSurfaceTag) for batches created after this call. */
	void SetSurface(FName SurfaceTag) { CurrentSurface = SurfaceTag; }

	// ---------------------------------------------------------------- individual components
	UStaticMeshComponent* AddPart(USceneComponent* Parent, ESUShape Shape, const FVector& Location, const FVector& Size, ESUMat Mat, const FRotator& Rot = FRotator::ZeroRotator, bool bCollision = true);
	UPointLightComponent* AddPointLight(USceneComponent* Parent, const FVector& Location, float Intensity, float Radius, const FLinearColor& Color, bool bShadows = true);
	USpotLightComponent* AddSpotLight(USceneComponent* Parent, const FVector& Location, const FRotator& Rotation, float Intensity, float Radius, float OuterCone, const FLinearColor& Color);
	UTextRenderComponent* AddText(USceneComponent* Parent, const FVector& Location, const FRotator& Rotation, const FString& Text, float Size, const FColor& Color);

	/** Rotation whose X axis points along X and whose Y axis lies in the X/Y plane. */
	static FRotator RotFromXY(const FVector& X, const FVector& Y);
	static FRotator YawRot(float Yaw) { return FRotator(0.f, Yaw, 0.f); }

	USceneComponent* GetSceneRoot() const { return SceneRoot; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Signal Unknown")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Static geometry never moves (sites); interactive actors use movable geometry. */
	bool bStaticGeometry = true;

	USUAssetLibrary* Assets() const;
	UInstancedStaticMeshComponent* GetBatch(ESUShape Shape, ESUMat Mat, bool bCollision);

private:
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> Batches;

	FName CurrentSurface;
	int32 PartCounter = 0;
};
