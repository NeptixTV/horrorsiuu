#include "Evidence/SUEvidenceSubsystem.h"
#include "Evidence/SUEvidenceComponent.h"
#include "Interaction/SULighting.h"
#include "Core/SUGameState.h"
#include "Logic/SUEvidenceLogic.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	SULogic::Vec3 ToV(const FVector& V)
	{
		return SULogic::Vec3(V.X, V.Y, V.Z);
	}
}

void USUEvidenceSubsystem::Register(USUEvidenceComponent* E)
{
	if (E)
	{
		Evidence.AddUnique(E);
	}
}

void USUEvidenceSubsystem::Unregister(USUEvidenceComponent* E)
{
	Evidence.RemoveAll([E](const TWeakObjectPtr<USUEvidenceComponent>& W) { return !W.IsValid() || W.Get() == E; });
}

float USUEvidenceSubsystem::MeasureVisibility(const USUEvidenceComponent* E, const FSUPhotoShot& Shot) const
{
	FVector Corners[8];
	E->GetCorners(Corners);
	const FVector Center = E->GetCenter();
	TArray<FVector, TInlineAllocator<9>> Samples;
	Samples.Add(Center);
	for (const FVector& C : Corners)
	{
		Samples.Add(FMath::Lerp(Center, C, 0.6f));
	}

	FCollisionQueryParams Params(FName(TEXT("SUEvidenceVisibility")), false);
	for (const AActor* A : Shot.IgnoredActors)
	{
		Params.AddIgnoredActor(A);
	}
	const AActor* Owner = E->GetOwner();
	int32 Visible = 0;
	for (const FVector& P : Samples)
	{
		FHitResult Hit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Shot.CameraLocation, P, ECC_Visibility, Params);
		if (!bBlocked)
		{
			++Visible;
			continue;
		}
		// hitting the evidence object itself, or something right at the sample point, counts as visible
		const bool bSelf = Owner && Hit.GetActor() == Owner && (Hit.Location - P).Size() < FMath::Max(40.f, E->BoxExtent.GetMax());
		if (bSelf || (Hit.Location - P).Size() < 12.f)
		{
			++Visible;
		}
	}
	return static_cast<float>(Visible) / Samples.Num();
}

bool USUEvidenceSubsystem::IsLit(const USUEvidenceComponent* E, const FSUPhotoShot& Shot) const
{
	const FVector Center = E->GetCenter();
	const float Dist = FVector::Dist(Center, Shot.CameraLocation);
	if (Shot.bFlash && Dist < 1100.f)
	{
		return true;
	}
	if (Shot.bFlashlightOn && Dist < 2600.f)
	{
		const FVector ToTarget = (Center - Shot.CameraLocation).GetSafeNormal();
		if (FVector::DotProduct(ToTarget, Shot.FlashlightDirection) > FMath::Cos(FMath::DegreesToRadians(24.f)))
		{
			return true;
		}
	}
	for (TActorIterator<ASULamp> It(GetWorld()); It; ++It)
	{
		if (It->IsLit() && FVector::Dist(It->GetActorLocation(), Center) < It->Radius * 0.75f)
		{
			return true;
		}
	}
	if (!E->bRequiresLight)
	{
		const ASUGameState* GS = ASUGameState::Get(this);
		return !GS || GS->GetDarkness() < 0.7f;
	}
	return false;
}

TArray<FSUEvidenceHit> USUEvidenceSubsystem::EvaluatePhoto(const FSUPhotoShot& Shot) const
{
	TArray<FSUEvidenceHit> Hits;

	SULogic::PhotoCamera Cam;
	const FRotationMatrix Rot(Shot.CameraRotation);
	Cam.Position = ToV(Shot.CameraLocation);
	Cam.Forward = ToV(Rot.GetScaledAxis(EAxis::X));
	Cam.Right = ToV(Rot.GetScaledAxis(EAxis::Y));
	Cam.Up = ToV(Rot.GetScaledAxis(EAxis::Z));
	Cam.TanHalfFovX = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Shot.HorizontalFovDeg, 5.f, 170.f) * 0.5f));
	Cam.TanHalfFovY = Cam.TanHalfFovX / FMath::Max(0.1f, Shot.AspectRatio);

	for (const TWeakObjectPtr<USUEvidenceComponent>& Weak : Evidence)
	{
		const USUEvidenceComponent* E = Weak.Get();
		if (!E || !E->bEnabled || E->EvidenceId.IsNone())
		{
			continue;
		}
		const float Distance = FVector::Dist(E->GetCenter(), Shot.CameraLocation);
		if (Distance > E->MaxDistance * 1.1f)
		{
			continue;
		}
		FVector Corners[8];
		E->GetCorners(Corners);
		SULogic::Vec3 C8[8];
		for (int32 I = 0; I < 8; ++I)
		{
			C8[I] = ToV(Corners[I]);
		}
		SULogic::EvidenceShot S;
		S.Frame = SULogic::MeasureFrame(SULogic::ProjectCorners(C8, Cam));
		if (S.Frame.Coverage < E->MinCoverage * 0.5)
		{
			continue;
		}
		S.Distance = Distance;
		S.MaxDistance = E->MaxDistance;
		S.MinCoverage = E->MinCoverage;
		S.IdealCoverage = E->IdealCoverage;
		S.Visibility = MeasureVisibility(E, Shot);
		S.bLit = IsLit(E, Shot);
		S.bBlurred = Shot.bBlurred;
		S.FocusError = Shot.FocusError;

		const double Quality = SULogic::EvidenceQuality(S);
		if (Quality <= 0.0)
		{
			continue;
		}
		FSUEvidenceHit Hit;
		Hit.EvidenceId = E->EvidenceId;
		Hit.Category = E->Category;
		Hit.DisplayName = E->DisplayName;
		Hit.Quality = static_cast<float>(Quality);
		Hit.Coverage = static_cast<float>(S.Frame.Coverage);
		Hit.Visibility = static_cast<float>(S.Visibility);
		Hit.Distance = Distance;
		Hits.Add(Hit);
	}
	Hits.Sort([](const FSUEvidenceHit& A, const FSUEvidenceHit& B) { return A.Quality > B.Quality; });
	return Hits;
}
