#include "World/Sites/SUSiteGasStation.h"
#include "World/SUWorldBuilder.h"
#include "Core/SUTypes.h"
#include "Core/SUItemDatabase.h"
#include "Interaction/SUDoor.h"
#include "Interaction/SULighting.h"
#include "Interaction/SUPickupItem.h"
#include "GasStation/SUFuelPump.h"
#include "GasStation/SUShopCounter.h"
#include "NPC/SUNPC.h"

// local layout: +X points towards the main road (about 34 m away)
namespace GasLayout
{
	constexpr float ShopX0 = -1000.f;
	constexpr float ShopX1 = -200.f;
	constexpr float ShopY = 600.f;
	constexpr float ShopH = 380.f;
	constexpr float CanopyX = 1500.f;
}

using namespace GasLayout;

void ASUSiteGasStation::BuildSite(ASUWorldBuilder* InBuilder, bool bInServer)
{
	Super::BuildSite(InBuilder, bInServer);
	SetSurface(SUSurfaceTag::Asphalt);
	BuildForecourt();
	SetSurface(SUSurfaceTag::Concrete);
	BuildShop();
	SetSurface(NAME_None);
	SpawnInteractables();

	RegisterPoint(TEXT("GasStation"), FVector(CanopyX, 0.f, 100.f));
	AddArea(SUArea::GasStation, FVector(900.f, 0.f, 300.f), FVector(2300.f, 1700.f, 600.f));
	AddArea(SUArea::GasShop, FVector((ShopX0 + ShopX1) * 0.5f, 0.f, 190.f), FVector((ShopX1 - ShopX0) * 0.5f - 15.f, ShopY - 15.f, 180.f));
}

void ASUSiteGasStation::BuildForecourt()
{
	// asphalt apron connecting to the road
	AddBox(FVector(1350.f, 0.f, -6.f), FVector(3300.f, 2800.f, 20.f), ESUMat::Asphalt);
	// parking lines in front of the shop
	for (int32 I = 0; I < 5; ++I)
	{
		AddBox(FVector(150.f, -580.f + I * 280.f, 4.5f), FVector(500.f, 10.f, 1.f), ESUMat::RoadLineWhite, FRotator::ZeroRotator, false);
	}
	// canopy
	AddBox(FVector(CanopyX, 0.f, 540.f), FVector(900.f, 1500.f, 40.f), ESUMat::MetalWhite);
	AddBox(FVector(CanopyX, 0.f, 520.f), FVector(910.f, 1510.f, 6.f), ESUMat::GasRed, FRotator::ZeroRotator, false);
	for (int32 I = 0; I < 4; ++I)
	{
		AddBox(FVector(CanopyX + ((I & 1) ? 330.f : -330.f), (I & 2) ? 600.f : -600.f, 260.f), FVector(32.f, 32.f, 520.f), ESUMat::MetalWhite);
	}
	// pump island
	AddBox(FVector(CanopyX, 0.f, 7.f), FVector(120.f, 920.f, 14.f), ESUMat::Concrete);
	AddBox(FVector(CanopyX, -480.f, 50.f), FVector(20.f, 20.f, 90.f), ESUMat::Yellow);
	AddBox(FVector(CanopyX, 480.f, 50.f), FVector(20.f, 20.f, 90.f), ESUMat::Yellow);

	// price sign at the road
	AddBox(FVector(2700.f, -1150.f, 320.f), FVector(20.f, 20.f, 640.f), ESUMat::MetalDark);
	AddBox(FVector(2700.f, -1150.f, 560.f), FVector(16.f, 260.f, 200.f), ESUMat::White);
	AddBox(FVector(2700.f, -1150.f, 640.f), FVector(17.f, 262.f, 40.f), ESUMat::GasRed);
	const int32 Price = 179;
	AddText(SceneRoot, FVector(2710.f, -1150.f, 640.f), FRotator::ZeroRotator, TEXT("PINE RIDGE"), 26.f, FColor(255, 240, 230));
	AddText(SceneRoot, FVector(2710.f, -1150.f, 585.f), FRotator::ZeroRotator, FString::Printf(TEXT("SUPER  %d,%02d"), Price / 100, Price % 100), 30.f, FColor(30, 30, 30));
	AddText(SceneRoot, FVector(2710.f, -1150.f, 530.f), FRotator::ZeroRotator, TEXT("DIESEL 1,69"), 30.f, FColor(30, 30, 30));
	AddText(SceneRoot, FVector(2690.f, -1150.f, 585.f), FRotator(0.f, 180.f, 0.f), FString::Printf(TEXT("SUPER  %d,%02d"), Price / 100, Price % 100), 30.f, FColor(30, 30, 30));
	AddPointLight(SceneRoot, FVector(2800.f, -1150.f, 560.f), 120.f, 900.f, FLinearColor(1.f, 0.95f, 0.85f), false);

	// props: dumpster, propane cage, air pump, pay phone, bench
	AddBox(FVector(-700.f, -900.f, 70.f), FVector(180.f, 120.f, 140.f), ESUMat::Green);
	AddBox(FVector(-200.f, 900.f, 90.f), FVector(100.f, 160.f, 180.f), ESUMat::MetalWhite);
	for (int32 I = 0; I < 4; ++I)
	{
		AddShape(ESUShape::Cylinder, FVector(-200.f, 840.f + I * 38.f, 45.f), FVector(30.f, 30.f, 80.f), ESUMat::White, FRotator::ZeroRotator, false);
	}
	AddBox(FVector(500.f, 1000.f, 75.f), FVector(30.f, 40.f, 150.f), ESUMat::Red);
	AddBox(FVector(-120.f, -800.f, 100.f), FVector(40.f, 60.f, 200.f), ESUMat::MetalDark);
	AddBox(FVector(-100.f, -800.f, 120.f), FVector(4.f, 40.f, 60.f), ESUMat::Chrome, FRotator::ZeroRotator, false);
	AddBox(FVector(-150.f, 300.f, 45.f), FVector(50.f, 180.f, 6.f), ESUMat::WoodLight);
}

void ASUSiteGasStation::BuildShop()
{
	const FVector2D A(ShopX1, -ShopY);
	const FVector2D B(ShopX1, ShopY);
	const float T = 20.f;
	AddBox(FVector((ShopX0 + ShopX1) * 0.5f, 0.f, 3.f), FVector(ShopX1 - ShopX0, ShopY * 2.f, 6.f), ESUMat::MetalWhite); // floor tiles

	// front: two shop windows and the door
	const FSUOpening Door(550.f, 100.f, 0.f, 220.f);
	const FSUOpening WinL(80.f, 400.f, 60.f, 300.f);
	const FSUOpening WinR(720.f, 400.f, 60.f, 300.f);
	AddWall(A, B, 6.f, ShopH, T, ESUMat::Brick, {Door, WinL, WinR});
	AddWindowFrame(A, B, 6.f, T, WinL, ESUMat::MetalWhite, false);
	AddWindowFrame(A, B, 6.f, T, WinR, ESUMat::MetalWhite, false);
	// sides and back
	const FSUOpening SideWin(250.f, 200.f, 120.f, 230.f);
	AddWall(FVector2D(ShopX0, -ShopY), FVector2D(ShopX1, -ShopY), 6.f, ShopH, T, ESUMat::Brick, {SideWin});
	AddWall(FVector2D(ShopX0, ShopY), FVector2D(ShopX1, ShopY), 6.f, ShopH, T, ESUMat::Brick);
	const FSUOpening BackDoor(900.f, 100.f, 0.f, 215.f);
	AddWall(FVector2D(ShopX0, -ShopY), FVector2D(ShopX0, ShopY), 6.f, ShopH, T, ESUMat::Brick, {BackDoor});
	// roof with parapet and sign
	AddBox(FVector((ShopX0 + ShopX1) * 0.5f, 0.f, ShopH + 16.f), FVector(ShopX1 - ShopX0 + 40.f, ShopY * 2.f + 40.f, 24.f), ESUMat::Roof);
	AddBox(FVector(ShopX1 + 8.f, 0.f, ShopH + 70.f), FVector(16.f, ShopY * 2.f + 40.f, 110.f), ESUMat::GasRed);
	AddText(SceneRoot, FVector(ShopX1 + 18.f, 0.f, ShopH + 70.f), FRotator::ZeroRotator, TEXT("PINE RIDGE  GAS & MART"), 62.f, FColor(255, 245, 235));
	// awning over the windows
	AddBox(FVector(ShopX1 + 70.f, 0.f, 320.f), FVector(140.f, ShopY * 2.f, 8.f), ESUMat::GasRed, FRotator(-8.f, 0.f, 0.f), false);

	// sales counter (customers stand on the +X side)
	AddBox(FVector(-480.f, 370.f, 50.f), FVector(80.f, 380.f, 100.f), ESUMat::WoodDark);
	AddBox(FVector(-480.f, 370.f, 102.f), FVector(86.f, 386.f, 4.f), ESUMat::Concrete);
	AddBox(FVector(-480.f, 520.f, 125.f), FVector(40.f, 34.f, 44.f), ESUMat::Black, FRotator::ZeroRotator, false); // coffee machine
	AddBox(FVector(-470.f, 210.f, 120.f), FVector(30.f, 60.f, 30.f), ESUMat::Yellow, FRotator::ZeroRotator, false); // candy rack
	// cigarettes / shelf behind the counter
	AddBox(FVector(-700.f, 370.f, 150.f), FVector(30.f, 380.f, 220.f), ESUMat::WoodLight);
	for (int32 I = 0; I < 12; ++I)
	{
		AddBox(FVector(-682.f, 210.f + I * 28.f, 120.f + (I % 3) * 50.f), FVector(6.f, 22.f, 30.f), (I % 2) ? ESUMat::Red : ESUMat::White, FRotator::ZeroRotator, false);
	}

	// aisles with products
	FRandomStream Rng(4711);
	const ESUMat Products[] = {ESUMat::Red, ESUMat::Yellow, ESUMat::FabricBlue, ESUMat::Green, ESUMat::White, ESUMat::GasRed, ESUMat::WoodLight};
	for (int32 Aisle = 0; Aisle < 3; ++Aisle)
	{
		const float Y = -480.f + Aisle * 180.f;
		AddBox(FVector(-600.f, Y, 80.f), FVector(500.f, 50.f, 160.f), ESUMat::MetalWhite);
		for (int32 Tier = 0; Tier < 3; ++Tier)
		{
			for (int32 K = 0; K < 14; ++K)
			{
				const float X = -830.f + K * 34.f;
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					const float H = Rng.FRandRange(18.f, 34.f);
					AddBox(FVector(X, Y + Side * 32.f, 22.f + Tier * 50.f + H * 0.5f), FVector(26.f, 14.f, H), Products[Rng.RandRange(0, 6)], FRotator::ZeroRotator, false);
				}
			}
		}
	}
	// drink coolers along the back wall (glass fronts, lit inside)
	for (int32 I = 0; I < 4; ++I)
	{
		const float Y = -520.f + I * 165.f;
		AddBox(FVector(-960.f, Y, 105.f), FVector(60.f, 160.f, 210.f), ESUMat::MetalWhite);
		AddBox(FVector(-929.f, Y, 110.f), FVector(2.f, 140.f, 180.f), ESUMat::Glass, FRotator::ZeroRotator, false);
		for (int32 R = 0; R < 4; ++R)
		{
			for (int32 C = 0; C < 5; ++C)
			{
				AddShape(ESUShape::Cylinder, FVector(-945.f, Y - 56.f + C * 28.f, 40.f + R * 45.f), FVector(10.f, 10.f, 24.f), Products[(I + R + C) % 7], FRotator::ZeroRotator, false);
			}
		}
	}
	AddPointLight(SceneRoot, FVector(-900.f, -280.f, 150.f), 40.f, 500.f, FLinearColor(0.75f, 0.9f, 1.f), false);
	// magazines / newspaper stand near the door
	AddBox(FVector(-260.f, 120.f, 60.f), FVector(40.f, 80.f, 120.f), ESUMat::WoodLight);
	AddText(SceneRoot, FVector(-238.f, 120.f, 100.f), FRotator::ZeroRotator, TEXT("KURIER"), 8.f, FColor(20, 20, 20));
	AddText(SceneRoot, FVector(-238.f, 120.f, 85.f), FRotator::ZeroRotator, TEXT("Wanderer vermisst"), 4.f, FColor(20, 20, 20));
}

void ASUSiteGasStation::SpawnInteractables()
{
	if (!bServer)
	{
		return;
	}
	SpawnInteractable<ASUDoor>(FVector(ShopX1, 0.f, 6.f), 0.f, [](ASUDoor& D)
	{
		D.Width = 100.f;
		D.Height = 215.f;
		D.Material = ESUMat::MetalWhite;
		D.DisplayName = SUText(TEXT("(Laden)"));
	});
	SpawnInteractable<ASUDoor>(FVector(ShopX0, -ShopY + 950.f, 6.f), 180.f, [](ASUDoor& D)
	{
		D.Width = 100.f;
		D.Height = 210.f;
		D.Material = ESUMat::MetalDark;
		D.bLocked = true;
		D.LockedText = SUText(TEXT("Lager – nur für Personal."));
	});
	// shop lights (24/7) and canopy lights
	for (int32 I = 0; I < 3; ++I)
	{
		SpawnInteractable<ASULamp>(FVector(-750.f + I * 230.f, -150.f + (I % 2) * 300.f, ShopH - 8.f), 0.f, [](ASULamp& L)
		{
			L.Style = ESULampStyle::Neon;
			L.Intensity = 260.f;
			L.Radius = 900.f;
			L.LightColor = FLinearColor(0.92f, 0.97f, 1.f);
			L.bCastShadows = false;
		});
	}
	for (int32 I = 0; I < 4; ++I)
	{
		SpawnInteractable<ASULamp>(FVector(CanopyX + ((I & 1) ? 200.f : -200.f), (I & 2) ? 350.f : -350.f, 515.f), 90.f, [](ASULamp& L)
		{
			L.Style = ESULampStyle::Neon;
			L.Intensity = 700.f;
			L.Radius = 1800.f;
			L.LightColor = FLinearColor(1.f, 0.97f, 0.9f);
			L.bCastShadows = true;
		});
	}
	SpawnInteractable<ASULamp>(FVector(400.f, -1200.f, 0.f), 90.f, [](ASULamp& L)
	{
		L.Style = ESULampStyle::Post;
		L.Intensity = 800.f;
		L.Radius = 2500.f;
		L.bAutoNight = true;
	});

	// pumps (nozzles on the +X side)
	TArray<ASUFuelPump*> Pumps;
	for (int32 I = 0; I < 2; ++I)
	{
		ASUFuelPump* Pump = SpawnInteractable<ASUFuelPump>(FVector(CanopyX, I == 0 ? -250.f : 250.f, 14.f), 0.f, [I](ASUFuelPump& P)
		{
			P.PumpNumber = I + 1;
		});
		if (Pump)
		{
			Pumps.Add(Pump);
		}
	}

	// clerk and register
	ASUNPC* Clerk = SpawnInteractable<ASUNPC>(FVector(-600.f, 380.f, 100.f), 0.f, [this](ASUNPC& N)
	{
		N.Role = ESUNPCRole::Clerk;
		N.DisplayName = SUText(TEXT("Ruth"));
		N.WanderPoints = {ToWorld(FVector(-600.f, 380.f, 100.f)), ToWorld(FVector(-600.f, 250.f, 100.f)), ToWorld(FVector(-760.f, -380.f, 100.f)), ToWorld(FVector(-560.f, -210.f, 100.f))};
	});
	SpawnInteractable<ASUShopCounter>(FVector(-480.f, 300.f, 104.f), 0.f, [Pumps, Clerk](ASUShopCounter& C)
	{
		for (ASUFuelPump* P : Pumps)
		{
			C.Pumps.Add(P);
		}
		C.Clerk = Clerk;
	});
}

// ====================================================================================== road block

void ASUSiteRoadBlock::BuildSite(ASUWorldBuilder* InBuilder, bool bInServer)
{
	Super::BuildSite(InBuilder, bInServer);
	AddRoadBlock(FVector::ZeroVector, RoadWidth + 200.f, TEXT("STRASSE GESPERRT"));
}
