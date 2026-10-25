#include "World/SUProceduralActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"

ASUProceduralActor::ASUProceduralActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	RootComponent = SceneRoot;
}

USUAssetLibrary* ASUProceduralActor::Assets() const
{
	return USUAssetLibrary::Get(this);
}

FRotator ASUProceduralActor::RotFromXY(const FVector& X, const FVector& Y)
{
	return FRotationMatrix::MakeFromXY(X.GetSafeNormal(), Y.GetSafeNormal()).Rotator();
}

UInstancedStaticMeshComponent* ASUProceduralActor::GetBatch(ESUShape Shape, ESUMat Mat, bool bCollision)
{
	const FName Key(*FString::Printf(TEXT("B_%d_%d_%d_%s"), static_cast<int32>(Shape), static_cast<int32>(Mat), bCollision ? 1 : 0, *CurrentSurface.ToString()));
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Batches.Find(Key))
	{
		return *Found;
	}
	USUAssetLibrary* Lib = Assets();
	if (!Lib)
	{
		return nullptr;
	}
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, Key);
	ISM->SetMobility(bStaticGeometry ? EComponentMobility::Static : EComponentMobility::Movable);
	ISM->SetupAttachment(SceneRoot);
	ISM->SetStaticMesh(Lib->GetMesh(Shape));
	ISM->SetMaterial(0, Lib->GetMaterial(Mat));
	if (bCollision)
	{
		ISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (!CurrentSurface.IsNone())
	{
		ISM->ComponentTags.Add(CurrentSurface);
	}
	ISM->RegisterComponent();
	Batches.Add(Key, ISM);
	return ISM;
}

void ASUProceduralActor::AddShape(ESUShape Shape, const FVector& Center, const FVector& Size, ESUMat Mat, const FRotator& Rot, bool bCollision)
{
	UInstancedStaticMeshComponent* ISM = GetBatch(Shape, Mat, bCollision);
	if (!ISM)
	{
		return;
	}
	FVector Scale = Size / 100.f;
	if (Shape == ESUShape::Plane)
	{
		Scale.Z = 1.f;
	}
	ISM->AddInstance(FTransform(Rot, Center, Scale));
}

void ASUProceduralActor::AddBox(const FVector& Center, const FVector& Size, ESUMat Mat, const FRotator& Rot, bool bCollision)
{
	AddShape(ESUShape::Cube, Center, Size, Mat, Rot, bCollision);
}

void ASUProceduralActor::AddBeam(const FVector& From, const FVector& To, float Thickness, ESUMat Mat, bool bCollision)
{
	const FVector Delta = To - From;
	const float Length = Delta.Size();
	if (Length < 1.f)
	{
		return;
	}
	const FVector X = Delta / Length;
	FVector Y = FVector::CrossProduct(FVector::UpVector, X);
	if (Y.SizeSquared() < 1e-4f)
	{
		Y = FVector::RightVector;
	}
	AddBox((From + To) * 0.5f, FVector(Length, Thickness, Thickness), Mat, RotFromXY(X, Y), bCollision);
}

void ASUProceduralActor::AddWall(const FVector2D& A, const FVector2D& B, float BaseZ, float Height, float Thickness, ESUMat Mat, const TArray<FSUOpening>& Openings)
{
	const FVector2D Delta = B - A;
	const float Length = Delta.Size();
	if (Length < 1.f)
	{
		return;
	}
	const FVector2D Dir = Delta / Length;
	const FRotator Rot = YawRot(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)));

	auto Piece = [&](float From, float To, float Bottom, float Top)
	{
		if (To - From < 0.5f || Top - Bottom < 0.5f)
		{
			return;
		}
		const float Mid = (From + To) * 0.5f;
		const FVector2D P = A + Dir * Mid;
		AddBox(FVector(P.X, P.Y, BaseZ + (Bottom + Top) * 0.5f), FVector(To - From, Thickness, Top - Bottom), Mat, Rot, true);
	};

	TArray<FSUOpening> Sorted = Openings;
	Sorted.Sort([](const FSUOpening& L, const FSUOpening& R) { return L.Start < R.Start; });

	float Cursor = 0.f;
	for (const FSUOpening& O : Sorted)
	{
		const float OStart = FMath::Clamp(O.Start, 0.f, Length);
		const float OEnd = FMath::Clamp(O.Start + O.Width, 0.f, Length);
		Piece(Cursor, OStart, 0.f, Height);
		Piece(OStart, OEnd, 0.f, FMath::Clamp(O.Bottom, 0.f, Height));
		Piece(OStart, OEnd, FMath::Clamp(O.Top, 0.f, Height), Height);
		Cursor = FMath::Max(Cursor, OEnd);
	}
	Piece(Cursor, Length, 0.f, Height);
}

void ASUProceduralActor::AddWindowFrame(const FVector2D& A, const FVector2D& B, float BaseZ, float Thickness, const FSUOpening& Opening, ESUMat Mat, bool bCross)
{
	const FVector2D Delta = B - A;
	const float Length = Delta.Size();
	if (Length < 1.f)
	{
		return;
	}
	const FVector2D Dir = Delta / Length;
	const FRotator Rot = YawRot(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)));
	const float Frame = 6.f;
	const float Depth = Thickness + 2.f;
	const float W = Opening.Width;
	const float H = Opening.Top - Opening.Bottom;
	const FVector2D Mid2 = A + Dir * (Opening.Start + W * 0.5f);
	const FVector Mid(Mid2.X, Mid2.Y, BaseZ + (Opening.Bottom + Opening.Top) * 0.5f);
	const FVector Along(Dir.X, Dir.Y, 0.f);

	AddBox(Mid + FVector(0, 0, H * 0.5f - Frame * 0.5f), FVector(W, Depth, Frame), Mat, Rot, false);
	AddBox(Mid - FVector(0, 0, H * 0.5f - Frame * 0.5f), FVector(W, Depth + 6.f, Frame), Mat, Rot, true); // sill
	AddBox(Mid + Along * (W * 0.5f - Frame * 0.5f), FVector(Frame, Depth, H), Mat, Rot, false);
	AddBox(Mid - Along * (W * 0.5f - Frame * 0.5f), FVector(Frame, Depth, H), Mat, Rot, false);
	if (bCross)
	{
		AddBox(Mid, FVector(W, 3.f, 3.f), Mat, Rot, false);
		AddBox(Mid, FVector(3.f, 3.f, H), Mat, Rot, false);
	}
}

void ASUProceduralActor::AddGableRoof(const FVector& Center, float Length, float Width, float Rise, float Overhang, float Thickness, ESUMat Mat, float Yaw)
{
	const FRotator YawR = YawRot(Yaw);
	const FVector RidgeAxis = YawR.RotateVector(FVector::ForwardVector);
	const FVector Side = YawR.RotateVector(FVector::RightVector);
	const float Half = Width * 0.5f;
	const float SlopeLen = FMath::Sqrt(Half * Half + Rise * Rise);
	const float CosA = Half / SlopeLen;
	const float OverSlope = Overhang / FMath::Max(0.2f, CosA);

	for (int32 S = -1; S <= 1; S += 2)
	{
		// from the eave on side S up to the ridge
		const FVector Eave = Center + Side * (Half * S);
		const FVector Ridge = Center + FVector(0.f, 0.f, Rise);
		const FVector Up = (Ridge - Eave).GetSafeNormal();
		const FVector Start = Eave - Up * OverSlope;
		const FVector Normal = FVector::CrossProduct(RidgeAxis, Up).GetSafeNormal() * (S < 0 ? -1.f : 1.f);
		const FVector SlabCenter = (Start + Ridge) * 0.5f + (Normal.Z < 0.f ? -Normal : Normal) * (Thickness * 0.5f);
		AddBox(SlabCenter, FVector(Length + Overhang * 2.f, (Ridge - Start).Size() + Thickness, Thickness), Mat, RotFromXY(RidgeAxis, Up), true);
	}

	// gable ends: stepped triangles
	const int32 Layers = 8;
	for (int32 End = -1; End <= 1; End += 2)
	{
		for (int32 L = 0; L < Layers; ++L)
		{
			const float T0 = static_cast<float>(L) / Layers;
			const float LayerH = Rise / Layers;
			const float W = Width * (1.f - (T0 + 0.5f / Layers));
			const FVector P = Center + RidgeAxis * (Length * 0.5f * End - End * 6.f) + FVector(0.f, 0.f, LayerH * (L + 0.5f));
			AddBox(P, FVector(10.f, W, LayerH), Mat == ESUMat::Roof ? ESUMat::WoodGrey : ESUMat::WoodDark, YawR, true);
		}
	}
}

void ASUProceduralActor::AddStairs(const FVector& Start, const FVector2D& Dir, int32 Steps, float Rise, float Run, float Width, ESUMat Mat, bool bSolid)
{
	const FVector2D D = Dir.GetSafeNormal();
	const FVector Forward(D.X, D.Y, 0.f);
	const FRotator Rot = YawRot(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)));
	const float Tread = 5.f;
	for (int32 I = 0; I < Steps; ++I)
	{
		const float TopZ = Rise * (I + 1);
		const FVector Base = Start + Forward * (Run * (I + 0.5f));
		if (bSolid)
		{
			AddBox(Base + FVector(0.f, 0.f, TopZ * 0.5f), FVector(Run + 1.f, Width, TopZ), Mat, Rot, true);
		}
		else
		{
			AddBox(Base + FVector(0.f, 0.f, TopZ - Tread * 0.5f), FVector(Run + 2.f, Width, Tread), Mat, Rot, true);
		}
	}
	if (!bSolid)
	{
		// stringers
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
		const FVector EndP = Start + Forward * (Run * Steps) + FVector(0.f, 0.f, Rise * Steps);
		for (int32 S = -1; S <= 1; S += 2)
		{
			const FVector Off = Side * (Width * 0.5f + 3.f) * S + FVector(0.f, 0.f, -8.f);
			AddBeam(Start + Off, EndP + Off, 8.f, ESUMat::WoodDark, false);
		}
	}
}

UStaticMeshComponent* ASUProceduralActor::AddPart(USceneComponent* Parent, ESUShape Shape, const FVector& Location, const FVector& Size, ESUMat Mat, const FRotator& Rot, bool bCollision)
{
	USUAssetLibrary* Lib = Assets();
	if (!Lib)
	{
		return nullptr;
	}
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, FName(*FString::Printf(TEXT("Part_%d"), PartCounter++)));
	Comp->SetMobility(bStaticGeometry ? EComponentMobility::Static : EComponentMobility::Movable);
	Comp->SetupAttachment(Parent ? Parent : SceneRoot.Get());
	Comp->SetStaticMesh(Lib->GetMesh(Shape));
	Comp->SetMaterial(0, Lib->GetMaterial(Mat));
	FVector Scale = Size / 100.f;
	if (Shape == ESUShape::Plane)
	{
		Scale.Z = 1.f;
	}
	Comp->SetRelativeTransform(FTransform(Rot, Location, Scale));
	if (bCollision)
	{
		Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Comp->RegisterComponent();
	return Comp;
}

UPointLightComponent* ASUProceduralActor::AddPointLight(USceneComponent* Parent, const FVector& Location, float Intensity, float Radius, const FLinearColor& Color, bool bShadows)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this, FName(*FString::Printf(TEXT("Light_%d"), PartCounter++)));
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetupAttachment(Parent ? Parent : SceneRoot.Get());
	Light->SetRelativeLocation(Location);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(Radius);
	Light->SetLightColor(Color);
	Light->SetCastShadows(bShadows);
	Light->SetSourceRadius(3.f);
	Light->RegisterComponent();
	return Light;
}

USpotLightComponent* ASUProceduralActor::AddSpotLight(USceneComponent* Parent, const FVector& Location, const FRotator& Rotation, float Intensity, float Radius, float OuterCone, const FLinearColor& Color)
{
	USpotLightComponent* Light = NewObject<USpotLightComponent>(this, FName(*FString::Printf(TEXT("Spot_%d"), PartCounter++)));
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetupAttachment(Parent ? Parent : SceneRoot.Get());
	Light->SetRelativeLocationAndRotation(Location, Rotation);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(Radius);
	Light->SetOuterConeAngle(OuterCone);
	Light->SetInnerConeAngle(OuterCone * 0.6f);
	Light->SetLightColor(Color);
	Light->SetCastShadows(true);
	Light->RegisterComponent();
	return Light;
}

UTextRenderComponent* ASUProceduralActor::AddText(USceneComponent* Parent, const FVector& Location, const FRotator& Rotation, const FString& Text, float Size, const FColor& Color)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this, FName(*FString::Printf(TEXT("Text_%d"), PartCounter++)));
	T->SetMobility(EComponentMobility::Movable);
	T->SetupAttachment(Parent ? Parent : SceneRoot.Get());
	T->SetRelativeLocationAndRotation(Location, Rotation);
	T->SetText(FText::FromString(Text));
	T->SetWorldSize(Size);
	T->SetTextRenderColor(Color);
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	T->RegisterComponent();
	return T;
}
