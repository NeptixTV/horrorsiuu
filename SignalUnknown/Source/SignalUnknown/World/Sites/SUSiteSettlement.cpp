#include "World/Sites/SUSiteSettlement.h"
#include "Core/SUTypes.h"
#include "Interaction/SUDoor.h"
#include "NPC/SUNPC.h"

void ASUSiteSettlement::BuildSite(ASUWorldBuilder* InBuilder, bool bInServer)
{
	Super::BuildSite(InBuilder, bInServer);

	const int32 Seed = static_cast<int32>(GetTypeHash(SiteId));
	FRandomStream Rng(Seed);
	const ESUMat WallChoices[] = {ESUMat::WoodPainted, ESUMat::WoodGrey, ESUMat::WoodLight, ESUMat::White, ESUMat::MetalPainted};
	const ESUMat Walls = WallChoices[Rng.RandRange(0, 4)];
	const float SizeX = Rng.FRandRange(750.f, 900.f);
	const float SizeY = Rng.FRandRange(950.f, 1150.f);
	// house front faces the road (+X), centred a bit behind the pad centre
	const FVector Center(-250.f, 0.f, 0.f);
	SetSurface(SUSurfaceTag::Wood);
	AddSimpleHouse(Center, SizeX, SizeY, 290.f, Walls, ESUMat::Shingle, true, Seed);
	SetSurface(NAME_None);

	// porch, path, mailbox, fence
	AddBox(FVector(Center.X + SizeX * 0.5f + 100.f, 0.f, 15.f), FVector(200.f, 400.f, 30.f), ESUMat::WoodGrey);
	AddBox(FVector(Center.X + SizeX * 0.5f + 700.f, 0.f, 1.f), FVector(1000.f, 120.f, 4.f), ESUMat::Gravel, FRotator::ZeroRotator, false);
	AddBox(FVector(1150.f, 280.f, 55.f), FVector(8.f, 8.f, 110.f), ESUMat::WoodDark);
	AddBox(FVector(1150.f, 280.f, 115.f), FVector(45.f, 22.f, 25.f), ESUMat::MetalPainted);
	for (int32 I = 0; I < 9; ++I)
	{
		const float Y = -1300.f + I * 120.f;
		AddBox(FVector(900.f, Y, 50.f), FVector(8.f, 8.f, 100.f), ESUMat::White, FRotator::ZeroRotator, true);
	}
	AddBox(FVector(900.f, -820.f, 80.f), FVector(4.f, 960.f, 8.f), ESUMat::White, FRotator::ZeroRotator, true);

	// lights in the windows (people live here)
	AddPointLight(SceneRoot, FVector(Center.X + SizeX * 0.3f, -SizeY * 0.3f, 160.f), 25.f, 350.f, FLinearColor(1.f, 0.75f, 0.45f), false);
	if (Rng.FRand() < 0.6f)
	{
		AddPointLight(SceneRoot, FVector(Center.X + SizeX * 0.3f, SizeY * 0.3f, 160.f), 20.f, 300.f, FLinearColor(0.7f, 0.8f, 1.f), false);
	}

	// some yards have a parked car
	if (Rng.FRand() < 0.5f)
	{
		const FVector Car(500.f, SizeY * 0.5f + 300.f, 0.f);
		AddBox(Car + FVector(0.f, 0.f, 55.f), FVector(430.f, 180.f, 55.f), Rng.FRand() < 0.5f ? ESUMat::Red : ESUMat::FabricBlue);
		AddBox(Car + FVector(-30.f, 0.f, 110.f), FVector(220.f, 170.f, 55.f), ESUMat::Glass);
		for (int32 W = 0; W < 4; ++W)
		{
			AddShape(ESUShape::Cylinder, Car + FVector((W & 1) ? 140.f : -140.f, (W & 2) ? 85.f : -85.f, 33.f), FVector(65.f, 65.f, 22.f), ESUMat::Tire, FRotator(0.f, 0.f, 90.f));
		}
	}

	if (bServer)
	{
		SpawnInteractable<ASUDoor>(FVector(Center.X + SizeX * 0.5f, 0.f, 10.f), 0.f, [](ASUDoor& D)
		{
			D.Width = 98.f;
			D.Height = 213.f;
			D.Material = ESUMat::WoodDark;
			D.bLocked = true;
			D.LockedText = SUText(TEXT("Abgeschlossen. Drinnen bewegt sich nichts."));
		});
		// a couple of residents spend the day outside
		if (SiteId == FName(TEXT("House2")) || SiteId == FName(TEXT("House5")))
		{
			SpawnInteractable<ASUNPC>(FVector(600.f, -300.f, 100.f), 0.f, [this](ASUNPC& N)
			{
				N.NPCRole = ESUNPCRole::Resident;
				N.DisplayName = SiteId == FName(TEXT("House2")) ? SUText(TEXT("Herr Lindqvist")) : SUText(TEXT("Frau Okafor"));
				N.WanderPoints = {ToWorld(FVector(600.f, -300.f, 100.f)), ToWorld(FVector(400.f, 400.f, 100.f)), ToWorld(FVector(800.f, 100.f, 100.f))};
				N.bDaytimeOnly = true;
			});
		}
	}
}
