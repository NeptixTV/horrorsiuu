#include "World/Sites/SUSiteBase.h"
#include "World/SUWorldBuilder.h"
#include "Mission/SUAreaTrigger.h"
#include "Interaction/SUPickupItem.h"
#include "Interaction/SUDocument.h"
#include "Evidence/SUEvidenceComponent.h"

ASUSiteBase::ASUSiteBase()
{
	bReplicates = false;
}

void ASUSiteBase::BuildSite(ASUWorldBuilder* InBuilder, bool bInServer)
{
	Builder = InBuilder;
	bServer = bInServer;
}

FTransform ASUSiteBase::ToWorld(const FVector& Local, float LocalYaw) const
{
	return FTransform(FRotator(0.f, LocalYaw, 0.f), Local) * GetActorTransform();
}

float ASUSiteBase::GroundZ(float LocalX, float LocalY) const
{
	const ASUWorldBuilder* B = Builder.Get();
	if (!B)
	{
		return 0.f;
	}
	const FVector W = ToWorld(FVector(LocalX, LocalY, 0.f));
	return B->GetGroundHeight(W.X, W.Y) - GetActorLocation().Z;
}

ASUPickupItem* ASUSiteBase::SpawnItem(FName ItemId, const FVector& LocalPos, float LocalYaw, int32 Count, AActor* AttachTo)
{
	if (!bServer)
	{
		return nullptr;
	}
	return ASUPickupItem::SpawnItem(GetWorld(), ItemId, Count, 1.f, ToWorld(LocalPos, LocalYaw), AttachTo);
}

ASUDocument* ASUSiteBase::SpawnDocument(const FVector& LocalPos, float LocalYaw, const TCHAR* Title, const TCHAR* Body, int32 Look, FName DocumentId, FName TakeItemId)
{
	return SpawnInteractable<ASUDocument>(LocalPos, LocalYaw, [&](ASUDocument& D)
	{
		D.Title = SUText(Title);
		D.Body = SUText(Body);
		D.Look = static_cast<ESUDocumentLook>(Look);
		D.DocumentId = DocumentId;
		D.TakeItemId = TakeItemId;
	});
}

ASUAreaTrigger* ASUSiteBase::AddArea(FName AreaId, const FVector& LocalCenter, const FVector& Extent, float LocalYaw)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	ASUAreaTrigger* Area = GetWorld()->SpawnActor<ASUAreaTrigger>(ASUAreaTrigger::StaticClass(), ToWorld(LocalCenter, LocalYaw), Params);
	if (Area)
	{
		Area->Setup(AreaId, Extent);
	}
	return Area;
}

USUEvidenceComponent* ASUSiteBase::AddEvidence(FName EvidenceId, const TCHAR* DisplayName, FName Category, const FVector& LocalCenter, const FVector& Extent, bool bRequiresLight, float MaxDistance, const FRotator& LocalRot)
{
	USUEvidenceComponent* E = NewObject<USUEvidenceComponent>(this, EvidenceId);
	E->SetMobility(EComponentMobility::Static);
	E->SetupAttachment(SceneRoot);
	E->SetRelativeLocationAndRotation(LocalCenter, LocalRot);
	E->EvidenceId = EvidenceId;
	E->DisplayName = SUText(DisplayName);
	E->Category = Category;
	E->BoxExtent = Extent;
	E->bRequiresLight = bRequiresLight;
	E->MaxDistance = MaxDistance;
	E->RegisterComponent();
	return E;
}

void ASUSiteBase::RegisterPoint(FName Name, const FVector& LocalPos, float LocalYaw)
{
	if (ASUWorldBuilder* B = Builder.Get())
	{
		B->RegisterPoint(Name, ToWorld(LocalPos, LocalYaw));
	}
}

void ASUSiteBase::AddSimpleHouse(const FVector& Center, float SizeX, float SizeY, float WallHeight, ESUMat Walls, ESUMat Roof, bool bWindows, int32 Seed)
{
	const float T = 15.f;
	const float Z = Center.Z;
	const FVector2D C(Center.X, Center.Y);
	const float HX = SizeX * 0.5f;
	const float HY = SizeY * 0.5f;
	// foundation
	AddBox(FVector(Center.X, Center.Y, Z - 30.f), FVector(SizeX + 20.f, SizeY + 20.f, 80.f), ESUMat::Concrete);
	AddBox(FVector(Center.X, Center.Y, Z + 5.f), FVector(SizeX, SizeY, 10.f), ESUMat::WoodDark);

	TArray<FSUOpening> Front;
	TArray<FSUOpening> Side;
	if (bWindows)
	{
		Front.Add(FSUOpening(SizeY * 0.5f - 50.f, 100.f, 0.f, 215.f)); // door
		Front.Add(FSUOpening(SizeY * 0.15f, 120.f, 95.f, 205.f));
		Front.Add(FSUOpening(SizeY * 0.72f, 120.f, 95.f, 205.f));
		Side.Add(FSUOpening(SizeX * 0.3f, 110.f, 100.f, 200.f));
		Side.Add(FSUOpening(SizeX * 0.6f, 110.f, 100.f, 200.f));
	}
	// front wall faces +X
	AddWall(C + FVector2D(HX, -HY), C + FVector2D(HX, HY), Z + 10.f, WallHeight, T, Walls, Front);
	AddWall(C + FVector2D(-HX, -HY), C + FVector2D(-HX, HY), Z + 10.f, WallHeight, T, Walls, Side);
	AddWall(C + FVector2D(-HX, HY), C + FVector2D(HX, HY), Z + 10.f, WallHeight, T, Walls, Side);
	AddWall(C + FVector2D(-HX, -HY), C + FVector2D(HX, -HY), Z + 10.f, WallHeight, T, Walls, Side);
	if (bWindows)
	{
		for (const FSUOpening& O : Front)
		{
			if (O.Bottom > 10.f)
			{
				AddWindowFrame(C + FVector2D(HX, -HY), C + FVector2D(HX, HY), Z + 10.f, T, O, ESUMat::White);
				// dark interior behind the window (house is not enterable)
				const FVector2D P = C + FVector2D(HX - 40.f, -HY + O.Start + O.Width * 0.5f);
				AddBox(FVector(P.X, P.Y, Z + 10.f + (O.Bottom + O.Top) * 0.5f), FVector(4.f, O.Width + 10.f, O.Top - O.Bottom + 10.f), ESUMat::Black, FRotator::ZeroRotator, false);
			}
		}
	}
	AddBox(FVector(Center.X, Center.Y, Z + 10.f + WallHeight - 4.f), FVector(SizeX, SizeY, 8.f), ESUMat::WoodGrey, FRotator::ZeroRotator, true);
	AddGableRoof(FVector(Center.X, Center.Y, Z + 10.f + WallHeight), SizeX, SizeY, FMath::Min(SizeY, SizeX) * 0.28f, 45.f, 14.f, Roof, 0.f);
	// chimney on some houses
	if ((Seed % 3) == 0)
	{
		AddBox(FVector(Center.X - HX * 0.4f, Center.Y + HY * 0.3f, Z + WallHeight + SizeY * 0.25f), FVector(60.f, 60.f, SizeY * 0.4f), ESUMat::Brick);
	}
}

void ASUSiteBase::AddRoadBlock(const FVector& Center, float Width, const TCHAR* SignText)
{
	// concrete barriers across local Y
	const int32 Count = FMath::Max(2, FMath::CeilToInt(Width / 220.f));
	for (int32 I = 0; I < Count; ++I)
	{
		const float Y = -Width * 0.5f + (I + 0.5f) * (Width / Count);
		AddBox(Center + FVector(0.f, Y, 45.f), FVector(60.f, 200.f, 90.f), ESUMat::Concrete);
		AddBox(Center + FVector(0.f, Y, 95.f), FVector(30.f, 200.f, 12.f), (I % 2 == 0) ? ESUMat::Red : ESUMat::White, FRotator::ZeroRotator, false);
	}
	AddBox(Center + FVector(-120.f, Width * 0.5f + 80.f, 110.f), FVector(8.f, 8.f, 220.f), ESUMat::MetalDark);
	AddBox(Center + FVector(-120.f, Width * 0.5f + 80.f, 210.f), FVector(4.f, 160.f, 70.f), ESUMat::White);
	AddText(SceneRoot, Center + FVector(-115.f, Width * 0.5f + 80.f, 210.f), FRotator::ZeroRotator, SignText, 14.f, FColor(150, 10, 10));
	AddPointLight(SceneRoot, Center + FVector(40.f, 0.f, 130.f), 60.f, 900.f, FLinearColor(1.f, 0.45f, 0.05f), false);
}
