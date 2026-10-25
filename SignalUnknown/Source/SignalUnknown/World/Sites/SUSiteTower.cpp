#include "World/Sites/SUSiteTower.h"
#include "World/SUWorldBuilder.h"
#include "Core/SUTypes.h"
#include "Core/SUItemDatabase.h"
#include "Interaction/SUDoor.h"
#include "Interaction/SULighting.h"
#include "Interaction/SUFurniture.h"
#include "Interaction/SUPickupItem.h"
#include "Interaction/SUDocument.h"
#include "Base/SUBaseDevices.h"
#include "Vehicle/SUVehicle.h"
#include "Mission/SUAreaTrigger.h"

// Local layout (cm). +X points from the tower towards the access road.
namespace TowerLayout
{
	constexpr float TX = -900.f;          // tower centre
	constexpr float TY = 0.f;
	constexpr float Leg = 300.f;          // leg offset from centre
	constexpr float Deck = 1200.f;        // platform / cabin floor height
	constexpr float Catwalk = 330.f;      // platform half size
	constexpr float CabinHalf = 250.f;    // cabin interior half size
	constexpr float WallH = 260.f;
	constexpr float LaneA = -405.f;       // stair lane centres (x)
	constexpr float LaneB = -515.f;
	constexpr float LaneW = 105.f;
	constexpr float Rise = 20.f;
	constexpr float Run = 28.f;
	constexpr int32 Steps = 15;
}

using namespace TowerLayout;

void ASUSiteTower::BuildSite(ASUWorldBuilder* InBuilder, bool bInServer)
{
	Super::BuildSite(InBuilder, bInServer);
	SetSurface(SUSurfaceTag::Wood);
	BuildStructure();
	BuildStairs();
	BuildCabin();
	BuildInterior();
	SetSurface(SUSurfaceTag::Gravel);
	BuildGrounds();
	SetSurface(NAME_None);
	SpawnInteractables();

	const float CX = TX;
	const float CY = TY;
	RegisterPoint(TEXT("Spawn0"), FVector(CX - 20.f, CY - 100.f, Deck + 100.f), 0.f);
	RegisterPoint(TEXT("Spawn1"), FVector(CX + 90.f, CY - 105.f, Deck + 100.f), 0.f);
	RegisterPoint(TEXT("Spawn2"), FVector(CX - 30.f, CY + 110.f, Deck + 100.f), 0.f);
	RegisterPoint(TEXT("Spawn3"), FVector(CX + 90.f, CY + 110.f, Deck + 100.f), 0.f);
	RegisterPoint(TEXT("TowerRadio"), FVector(CX - 225.f, CY - 80.f, Deck + 80.f));
	RegisterPoint(SUArea::TowerParking, FVector(1350.f, 0.f, 100.f));
	RegisterPoint(TEXT("TowerTop"), FVector(TX, TY, Deck + 150.f));

	// menu camera: on the catwalk, looking over the forest into the evening sun (world yaw ~250°)
	if (ASUWorldBuilder* B = Builder.Get())
	{
		const FVector CamPos = ToWorld(FVector(TX + 120.f, TY - 420.f, Deck + 170.f));
		B->RegisterPoint(TEXT("MenuCamera"), FTransform(FRotator(-7.f, 252.f, 0.f), CamPos));
	}

	AddArea(SUArea::TowerCabin, FVector(TX, TY, Deck + 130.f), FVector(CabinHalf, CabinHalf, 140.f));
	AddArea(SUArea::TowerParking, FVector(1350.f, 0.f, 200.f), FVector(650.f, 750.f, 400.f));
	AddArea(SUArea::TowerGrounds, FVector(0.f, 0.f, 600.f), FVector(3600.f, 3600.f, 2200.f));
}

void ASUSiteTower::BuildStructure()
{
	// legs
	for (int32 I = 0; I < 4; ++I)
	{
		const float LX = TX + ((I & 1) ? Leg : -Leg);
		const float LY = TY + ((I & 2) ? Leg : -Leg);
		AddBox(FVector(LX, LY, Deck * 0.5f - 20.f), FVector(36.f, 36.f, Deck + 40.f), ESUMat::WoodDark);
		AddBox(FVector(LX, LY, 15.f), FVector(70.f, 70.f, 30.f), ESUMat::Concrete);
	}
	// cross bracing on every side, two storeys
	const FVector2D Corners[4] = {FVector2D(TX - Leg, TY - Leg), FVector2D(TX + Leg, TY - Leg), FVector2D(TX + Leg, TY + Leg), FVector2D(TX - Leg, TY + Leg)};
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const FVector2D A = Corners[Side];
		const FVector2D B = Corners[(Side + 1) % 4];
		for (int32 Level = 0; Level < 2; ++Level)
		{
			const float Z0 = 60.f + Level * 560.f;
			const float Z1 = Z0 + 540.f;
			AddBeam(FVector(A.X, A.Y, Z0), FVector(B.X, B.Y, Z1), 12.f, ESUMat::WoodDark, true);
			AddBeam(FVector(B.X, B.Y, Z0), FVector(A.X, A.Y, Z1), 12.f, ESUMat::WoodDark, true);
			AddBeam(FVector(A.X, A.Y, Z1), FVector(B.X, B.Y, Z1), 16.f, ESUMat::WoodDark, true);
		}
	}
	// platform (catwalk + cabin floor)
	AddBox(FVector(TX, TY, Deck - 10.f), FVector(Catwalk * 2.f, Catwalk * 2.f, 20.f), ESUMat::WoodLight);
	// catwalk railing, open towards the stairs (+X side, y in [-330,-210])
	const float RailZ = Deck + 100.f;
	const float MidZ = Deck + 50.f;
	auto Rail = [this, RailZ, MidZ](const FVector2D& A, const FVector2D& B)
	{
		AddBeam(FVector(A.X, A.Y, RailZ), FVector(B.X, B.Y, RailZ), 8.f, ESUMat::WoodDark, true);
		AddBeam(FVector(A.X, A.Y, MidZ), FVector(B.X, B.Y, MidZ), 6.f, ESUMat::WoodDark, true);
		const int32 Posts = FMath::Max(1, FMath::RoundToInt((B - A).Size() / 110.f));
		for (int32 P = 0; P <= Posts; ++P)
		{
			const FVector2D Q = FMath::Lerp(A, B, static_cast<float>(P) / Posts);
			AddBox(FVector(Q.X, Q.Y, Deck + 52.f), FVector(8.f, 8.f, 104.f), ESUMat::WoodDark, FRotator::ZeroRotator, true);
		}
	};
	Rail(FVector2D(TX - Catwalk, TY - Catwalk), FVector2D(TX - Catwalk, TY + Catwalk));
	Rail(FVector2D(TX - Catwalk, TY + Catwalk), FVector2D(TX + Catwalk, TY + Catwalk));
	Rail(FVector2D(TX - Catwalk, TY - Catwalk), FVector2D(TX + Catwalk, TY - Catwalk));
	Rail(FVector2D(TX + Catwalk, TY - 210.f), FVector2D(TX + Catwalk, TY + Catwalk));
}

void ASUSiteTower::BuildStairs()
{
	// switchback stair column on the +X side of the tower: lanes run along local Y
	const float XMin = TX + Catwalk;          // -570
	const float XMax = XMin + 2.f * LaneW + 10.f;
	const float XMid = (XMin + XMax) * 0.5f;
	const float Flight = Steps * Run;         // 420
	const float YLow = -Flight * 0.5f;        // -210
	const float YHigh = Flight * 0.5f;        // +210

	for (int32 F = 0; F < 4; ++F)
	{
		const float Z0 = F * Steps * Rise;
		const bool bUp = (F % 2) == 0; // A flights climb towards +Y
		const float LaneX = bUp ? LaneA : LaneB;
		if (bUp)
		{
			AddStairs(FVector(LaneX, YLow, Z0), FVector2D(0.f, 1.f), Steps, Rise, Run, LaneW, ESUMat::WoodLight);
		}
		else
		{
			AddStairs(FVector(LaneX, YHigh, Z0), FVector2D(0.f, -1.f), Steps, Rise, Run, LaneW, ESUMat::WoodLight);
		}
		// hand rails along both sides of the flight
		const float Z1 = Z0 + Steps * Rise;
		const float YStart = bUp ? YLow : YHigh;
		const float YEnd = bUp ? YHigh : YLow;
		for (int32 S = -1; S <= 1; S += 2)
		{
			const float RX = LaneX + S * (LaneW * 0.5f + 2.f);
			AddBeam(FVector(RX, YStart, Z0 + 95.f), FVector(RX, YEnd, Z1 + 95.f), 6.f, ESUMat::WoodDark, true);
			AddBeam(FVector(RX, YStart, Z0 + 45.f), FVector(RX, YEnd, Z1 + 45.f), 5.f, ESUMat::WoodDark, true);
		}
		// landing at the top of this flight
		const float LandY = bUp ? (YHigh + 60.f) : (YLow - 60.f);
		AddBox(FVector(XMid, LandY, Z1 - 10.f), FVector(XMax - XMin, 120.f, 20.f), ESUMat::WoodLight);
		// landing rails (outer end and outer side)
		const float EndY = bUp ? YHigh + 120.f : YLow - 120.f;
		AddBeam(FVector(XMin, EndY, Z1 + 100.f), FVector(XMax, EndY, Z1 + 100.f), 7.f, ESUMat::WoodDark, true);
		AddBeam(FVector(XMax, LandY - 60.f, Z1 + 100.f), FVector(XMax, LandY + 60.f, Z1 + 100.f), 7.f, ESUMat::WoodDark, true);
	}
	// posts of the stair column
	for (int32 I = 0; I < 6; ++I)
	{
		const float PX = (I % 2 == 0) ? XMin : XMax;
		const float PY = (I < 2) ? -Catwalk : ((I < 4) ? 0.f : Catwalk);
		AddBox(FVector(PX, PY, (Deck + 100.f) * 0.5f), FVector(20.f, 20.f, Deck + 100.f), ESUMat::WoodDark);
	}
}

void ASUSiteTower::BuildCabin()
{
	const float Z = Deck;
	const float H = CabinHalf;
	const float T = 12.f;
	const FVector2D C(TX, TY);
	const FSUOpening Band(30.f, 440.f, 100.f, 215.f);

	// west (back) wall: full window band
	AddWall(C + FVector2D(-H, -H), C + FVector2D(-H, H), Z, WallH, T, ESUMat::WoodDark, {Band});
	AddWindowFrame(C + FVector2D(-H, -H), C + FVector2D(-H, H), Z, T, Band, ESUMat::WoodLight);
	// south wall
	AddWall(C + FVector2D(-H, -H), C + FVector2D(H, -H), Z, WallH, T, ESUMat::WoodDark, {Band});
	AddWindowFrame(C + FVector2D(-H, -H), C + FVector2D(H, -H), Z, T, Band, ESUMat::WoodLight);
	// north wall: solid part for the map board, one window towards the east
	const FSUOpening NorthWindow(360.f, 120.f, 100.f, 215.f);
	AddWall(C + FVector2D(-H, H), C + FVector2D(H, H), Z, WallH, T, ESUMat::WoodDark, {NorthWindow});
	AddWindowFrame(C + FVector2D(-H, H), C + FVector2D(H, H), Z, T, NorthWindow, ESUMat::WoodLight);
	// east wall (towards the stairs): door + window
	const FSUOpening Door(20.f, 92.f, 0.f, 212.f);
	const FSUOpening EastWindow(140.f, 330.f, 100.f, 215.f);
	AddWall(C + FVector2D(H, -H), C + FVector2D(H, H), Z, WallH, T, ESUMat::WoodDark, {Door, EastWindow});
	AddWindowFrame(C + FVector2D(H, -H), C + FVector2D(H, H), Z, T, EastWindow, ESUMat::WoodLight);
	// door frame
	AddBox(FVector(TX + H, TY - H + 20.f - 4.f, Z + 106.f), FVector(16.f, 8.f, 212.f), ESUMat::WoodLight, FRotator::ZeroRotator, false);
	AddBox(FVector(TX + H, TY - H + 112.f + 4.f, Z + 106.f), FVector(16.f, 8.f, 212.f), ESUMat::WoodLight, FRotator::ZeroRotator, false);

	// ceiling and roof
	AddBox(FVector(TX, TY, Z + WallH + 4.f), FVector(2.f * H + T, 2.f * H + T, 8.f), ESUMat::WoodLight);
	AddGableRoof(FVector(TX, TY, Z + WallH + 8.f), Catwalk * 2.f, Catwalk * 2.f, 130.f, 45.f, 12.f, ESUMat::Roof, 0.f);
	// lightning rod and antenna
	AddBox(FVector(TX - 100.f, TY, Z + WallH + 260.f), FVector(4.f, 4.f, 250.f), ESUMat::MetalDark, FRotator::ZeroRotator, false);
	AddBox(FVector(TX + 150.f, TY + 150.f, Z + WallH + 220.f), FVector(3.f, 3.f, 300.f), ESUMat::MetalDark, FRotator::ZeroRotator, false);
	// sign on the cabin
	AddText(SceneRoot, FVector(TX + H + 8.f, TY + 120.f, Z + 235.f), FRotator::ZeroRotator, TEXT("POSTEN 07"), 22.f, FColor(230, 225, 210));
}

void ASUSiteTower::BuildInterior()
{
	const float CX = TX;
	const float CY = TY;
	const float Z = Deck;
	SetSurface(SUSurfaceTag::Wood);

	// rug
	AddBox(FVector(CX + 20.f, CY - 20.f, Z + 0.6f), FVector(220.f, 160.f, 1.2f), ESUMat::FabricBlue, FRotator::ZeroRotator, false);

	// desk along the west wall
	AddBox(FVector(CX - 215.f, CY, Z + 73.f), FVector(66.f, 262.f, 4.f), ESUMat::WoodLight);
	for (int32 I = 0; I < 4; ++I)
	{
		AddBox(FVector(CX - 215.f + ((I & 1) ? 28.f : -28.f), CY + ((I & 2) ? 125.f : -125.f), Z + 36.f), FVector(5.f, 5.f, 72.f), ESUMat::WoodDark, FRotator::ZeroRotator, false);
	}
	AddBox(FVector(CX - 215.f, CY + 95.f, Z + 40.f), FVector(60.f, 60.f, 62.f), ESUMat::WoodLight); // drawer block
	// chair
	AddBox(FVector(CX - 150.f, CY - 10.f, Z + 45.f), FVector(45.f, 45.f, 5.f), ESUMat::WoodDark);
	AddBox(FVector(CX - 128.f, CY - 10.f, Z + 80.f), FVector(4.f, 45.f, 65.f), ESUMat::WoodDark);
	for (int32 I = 0; I < 4; ++I)
	{
		AddBox(FVector(CX - 150.f + ((I & 1) ? 18.f : -18.f), CY - 10.f + ((I & 2) ? 18.f : -18.f), Z + 22.f), FVector(4.f, 4.f, 44.f), ESUMat::WoodDark, FRotator::ZeroRotator, false);
	}
	// mug, books, binoculars on the desk
	AddShape(ESUShape::Cylinder, FVector(CX - 200.f, CY - 25.f, Z + 80.f), FVector(8.f, 8.f, 10.f), ESUMat::White, FRotator::ZeroRotator, false);
	AddBox(FVector(CX - 230.f, CY + 125.f, Z + 79.f), FVector(22.f, 16.f, 8.f), ESUMat::Red, FRotator(0.f, 12.f, 0.f), false);
	AddBox(FVector(CX - 230.f, CY + 125.f, Z + 86.f), FVector(20.f, 15.f, 6.f), ESUMat::FabricBlue, FRotator(0.f, -5.f, 0.f), false);
	AddShape(ESUShape::Cylinder, FVector(CX - 240.f, CY - 140.f, Z + 108.f), FVector(6.f, 6.f, 14.f), ESUMat::Black, FRotator(90.f, 0.f, 0.f), false);
	AddShape(ESUShape::Cylinder, FVector(CX - 240.f, CY - 128.f, Z + 108.f), FVector(6.f, 6.f, 14.f), ESUMat::Black, FRotator(90.f, 0.f, 0.f), false);

	// fire finder table in the middle of the cabin (classic lookout instrument)
	AddShape(ESUShape::Cylinder, FVector(CX + 40.f, CY, Z + 45.f), FVector(18.f, 18.f, 90.f), ESUMat::WoodDark);
	AddShape(ESUShape::Cylinder, FVector(CX + 40.f, CY, Z + 92.f), FVector(80.f, 80.f, 5.f), ESUMat::WoodLight);
	AddShape(ESUShape::Cylinder, FVector(CX + 40.f, CY, Z + 96.f), FVector(70.f, 70.f, 2.f), ESUMat::Paper, FRotator::ZeroRotator, false);
	AddBox(FVector(CX + 40.f, CY, Z + 104.f), FVector(70.f, 3.f, 12.f), ESUMat::Chrome, FRotator(0.f, 35.f, 0.f), false);

	// kitchen counter along the north wall
	AddBox(FVector(CX + 160.f, CY + 218.f, Z + 44.f), FVector(160.f, 58.f, 88.f), ESUMat::WoodLight);
	AddBox(FVector(CX + 160.f, CY + 218.f, Z + 90.f), FVector(164.f, 62.f, 4.f), ESUMat::Concrete);
	AddBox(FVector(CX + 200.f, CY + 222.f, Z + 88.f), FVector(40.f, 34.f, 6.f), ESUMat::MetalWhite, FRotator::ZeroRotator, false); // sink
	AddBox(FVector(CX + 120.f, CY + 222.f, Z + 94.f), FVector(36.f, 30.f, 6.f), ESUMat::MetalDark, FRotator::ZeroRotator, false); // hot plate
	AddShape(ESUShape::Cylinder, FVector(CX + 150.f, CY + 220.f, Z + 102.f), FVector(16.f, 16.f, 12.f), ESUMat::MetalDark, FRotator::ZeroRotator, false); // kettle

	// fridge body (door is an interactive ASUDoor)
	const float FX = CX + 220.f;
	const float FY = CY + 125.f;
	AddBox(FVector(FX + 27.f, FY, Z + 85.f), FVector(6.f, 60.f, 170.f), ESUMat::MetalWhite);
	AddBox(FVector(FX, FY - 28.f, Z + 85.f), FVector(60.f, 4.f, 170.f), ESUMat::MetalWhite);
	AddBox(FVector(FX, FY + 28.f, Z + 85.f), FVector(60.f, 4.f, 170.f), ESUMat::MetalWhite);
	AddBox(FVector(FX, FY, Z + 168.f), FVector(60.f, 60.f, 4.f), ESUMat::MetalWhite);
	AddBox(FVector(FX, FY, Z + 6.f), FVector(60.f, 60.f, 12.f), ESUMat::MetalWhite);
	AddBox(FVector(FX + 3.f, FY, Z + 88.f), FVector(52.f, 52.f, 2.f), ESUMat::MetalWhite, FRotator::ZeroRotator, false); // shelf

	// equipment shelf on the east wall (front faces -X)
	const float SX = CX + 228.f;
	const float SY = CY - 40.f;
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddBox(FVector(SX, SY + S * 60.f, Z + 85.f), FVector(40.f, 4.f, 170.f), ESUMat::WoodDark);
	}
	for (int32 L = 0; L < 3; ++L)
	{
		AddBox(FVector(SX, SY, Z + 40.f + L * 60.f), FVector(40.f, 120.f, 3.f), ESUMat::WoodLight);
	}
	// coats on hooks next to the door
	AddBox(FVector(CX + 180.f, CY - 242.f, Z + 150.f), FVector(30.f, 10.f, 70.f), ESUMat::GasRed, FRotator::ZeroRotator, false);

	// personal items: photo frame, radio log, boots
	AddBox(FVector(CX - 240.f, CY + 60.f, Z + 115.f), FVector(2.f, 18.f, 14.f), ESUMat::WoodDark, FRotator(0.f, 0.f, 0.f), false);
	AddBox(FVector(CX + 200.f, CY - 230.f, Z + 8.f), FVector(28.f, 12.f, 16.f), ESUMat::WoodDark, FRotator::ZeroRotator, false);
	SetSurface(NAME_None);
}

void ASUSiteTower::BuildGrounds()
{
	// gravel parking area and path to the stairs
	AddBox(FVector(1350.f, 0.f, -8.f), FVector(1100.f, 1500.f, 20.f), ESUMat::Gravel);
	AddBox(FVector(200.f, -150.f, -9.f), FVector(1500.f, 260.f, 20.f), ESUMat::Gravel);
	// parking bumper logs
	AddShape(ESUShape::Cylinder, FVector(820.f, -380.f, 12.f), FVector(24.f, 24.f, 300.f), ESUMat::Bark, FRotator(0.f, 0.f, 90.f), true);
	AddShape(ESUShape::Cylinder, FVector(820.f, 380.f, 12.f), FVector(24.f, 24.f, 300.f), ESUMat::Bark, FRotator(0.f, 0.f, 90.f), true);

	// shed (door is interactive)
	const FVector2D Shed(300.f, 1300.f);
	const float SH = 240.f;
	AddBox(FVector(Shed.X, Shed.Y, 5.f), FVector(320.f, 260.f, 10.f), ESUMat::Concrete);
	const FSUOpening ShedDoor(85.f, 90.f, 0.f, 205.f);
	AddWall(Shed + FVector2D(160.f, -130.f), Shed + FVector2D(160.f, 130.f), 10.f, SH, 10.f, ESUMat::WoodGrey, {ShedDoor});
	AddWall(Shed + FVector2D(-160.f, -130.f), Shed + FVector2D(-160.f, 130.f), 10.f, SH, 10.f, ESUMat::WoodGrey);
	AddWall(Shed + FVector2D(-160.f, -130.f), Shed + FVector2D(160.f, -130.f), 10.f, SH, 10.f, ESUMat::WoodGrey);
	AddWall(Shed + FVector2D(-160.f, 130.f), Shed + FVector2D(160.f, 130.f), 10.f, SH, 10.f, ESUMat::WoodGrey);
	AddGableRoof(FVector(Shed.X, Shed.Y, SH + 10.f), 320.f, 260.f, 70.f, 25.f, 8.f, ESUMat::Roof, 0.f);
	AddBox(FVector(Shed.X - 120.f, Shed.Y, 80.f), FVector(50.f, 220.f, 4.f), ESUMat::WoodLight); // work bench
	AddBox(FVector(Shed.X - 120.f, Shed.Y, 40.f), FVector(50.f, 220.f, 4.f), ESUMat::WoodLight);
	AddShape(ESUShape::Cylinder, FVector(Shed.X + 60.f, Shed.Y + 90.f, 45.f), FVector(55.f, 55.f, 90.f), ESUMat::GasRed); // fuel drum
	AddArea(SUArea::Shed, FVector(Shed.X, Shed.Y, 130.f), FVector(150.f, 120.f, 120.f));

	// firewood stack, picnic table, fire ring
	for (int32 I = 0; I < 12; ++I)
	{
		AddShape(ESUShape::Cylinder, FVector(-200.f + (I % 4) * 2.f, 1500.f + (I % 4) * 30.f - 45.f, 15.f + (I / 4) * 28.f), FVector(26.f, 26.f, 120.f), ESUMat::Bark, FRotator(0.f, 90.f, 90.f), true);
	}
	AddBox(FVector(-250.f, -1300.f, 75.f), FVector(90.f, 200.f, 6.f), ESUMat::WoodGrey);
	AddBox(FVector(-320.f, -1300.f, 45.f), FVector(30.f, 200.f, 5.f), ESUMat::WoodGrey);
	AddBox(FVector(-180.f, -1300.f, 45.f), FVector(30.f, 200.f, 5.f), ESUMat::WoodGrey);
	for (int32 I = 0; I < 8; ++I)
	{
		const float A = I * 45.f;
		AddShape(ESUShape::Sphere, FVector(400.f + 70.f * FMath::Cos(FMath::DegreesToRadians(A)), -1100.f + 70.f * FMath::Sin(FMath::DegreesToRadians(A)), 10.f), FVector(30.f, 26.f, 22.f), ESUMat::Rock);
	}

	// sign at the road
	AddBox(FVector(2050.f, 520.f, 100.f), FVector(10.f, 10.f, 200.f), ESUMat::WoodDark);
	AddBox(FVector(2050.f, 620.f, 100.f), FVector(10.f, 10.f, 200.f), ESUMat::WoodDark);
	AddBox(FVector(2050.f, 570.f, 170.f), FVector(6.f, 150.f, 50.f), ESUMat::WoodLight);
	AddText(SceneRoot, FVector(2054.f, 570.f, 172.f), FRotator::ZeroRotator, TEXT("TOWER 07"), 26.f, FColor(250, 240, 220));
	AddText(SceneRoot, FVector(2046.f, 570.f, 172.f), FRotator(0.f, 180.f, 0.f), TEXT("TOWER 07"), 26.f, FColor(250, 240, 220));
}

void ASUSiteTower::SpawnInteractables()
{
	if (!bServer)
	{
		return;
	}
	const float CX = TX;
	const float CY = TY;
	const float Z = Deck;

	ASUGenerator* Generator = SpawnInteractable<ASUGenerator>(FVector(250.f, 900.f, 0.f), 90.f);

	// cabin door (opens into the cabin), shed door
	SpawnInteractable<ASUDoor>(FVector(CX + CabinHalf, CY - CabinHalf + 20.f + 46.f, Z), 0.f, [](ASUDoor& D)
	{
		D.Width = 90.f;
		D.Height = 210.f;
		D.Material = ESUMat::WoodLight;
	});
	SpawnInteractable<ASUDoor>(FVector(460.f, 1300.f - 130.f + 85.f + 45.f, 10.f), 0.f, [](ASUDoor& D)
	{
		D.Width = 88.f;
		D.Height = 203.f;
		D.Material = ESUMat::WoodGrey;
		D.DisplayName = SUText(TEXT("(Schuppen)"));
	});
	// fridge door (faces -X)
	SpawnInteractable<ASUDoor>(FVector(CX + 190.f, CY + 125.f, Z + 12.f), 180.f, [](ASUDoor& D)
	{
		D.Width = 56.f;
		D.Height = 154.f;
		D.Thickness = 4.f;
		D.Material = ESUMat::MetalWhite;
		D.bSmall = true;
		D.MaxAngle = 110.f;
		D.DisplayName = SUText(TEXT("(Kühlschrank)"));
	});
	SpawnItem(SUItem::Drink, FVector(CX + 225.f, CY + 110.f, Z + 90.f), 0.f);
	SpawnItem(SUItem::Drink, FVector(CX + 225.f, CY + 140.f, Z + 90.f), 0.f);

	// lamps and switch
	ASULamp* Ceiling = SpawnInteractable<ASULamp>(FVector(CX, CY, Z + WallH - 2.f), 0.f, [Generator](ASULamp& L)
	{
		L.Style = ESULampStyle::Hanging;
		L.Intensity = 160.f;
		L.Radius = 900.f;
		L.PowerSource = Generator;
	});
	ASULamp* DeskLamp = SpawnInteractable<ASULamp>(FVector(CX - 235.f, CY + 125.f, Z + 75.f), 0.f, [Generator](ASULamp& L)
	{
		L.Style = ESULampStyle::Desk;
		L.Intensity = 60.f;
		L.Radius = 500.f;
		L.PowerSource = Generator;
	});
	SpawnInteractable<ASULightSwitch>(FVector(CX + CabinHalf - 7.f, CY - 125.f, Z + 130.f), 180.f, [Ceiling, DeskLamp](ASULightSwitch& S)
	{
		S.Lamps = {Ceiling, DeskLamp};
	});
	SpawnInteractable<ASULamp>(FVector(CX + CabinHalf + 8.f, CY - 40.f, Z + 220.f), 0.f, [Generator](ASULamp& L)
	{
		L.Style = ESULampStyle::Wall;
		L.Intensity = 90.f;
		L.Radius = 1200.f;
		L.PowerSource = Generator;
		L.bAutoNight = true;
	});
	SpawnInteractable<ASULamp>(FVector(850.f, 750.f, 0.f), 0.f, [](ASULamp& L)
	{
		L.Style = ESULampStyle::Post;
		L.Intensity = 900.f;
		L.Radius = 2600.f;
		L.bAutoNight = true;
		L.LightColor = FLinearColor(1.f, 0.75f, 0.45f);
	});
	SpawnInteractable<ASULamp>(FVector(470.f, 1300.f, 200.f), 0.f, [Generator](ASULamp& L)
	{
		L.Style = ESULampStyle::Wall;
		L.Intensity = 50.f;
		L.Radius = 700.f;
		L.PowerSource = Generator;
		L.bAutoNight = true;
	});

	// devices
	SpawnInteractable<ASURadioStation>(FVector(CX - 225.f, CY - 80.f, Z + 75.f), 0.f);
	SpawnInteractable<ASUComputerTerminal>(FVector(CX - 228.f, CY + 40.f, Z + 75.f), 0.f);
	SpawnInteractable<ASUWallMap>(FVector(CX - 20.f, CY + CabinHalf - 8.f, Z + 150.f), -90.f);
	SpawnInteractable<ASUWeatherRadio>(FVector(CX + 110.f, CY + 222.f, Z + 92.f), -90.f);
	SpawnInteractable<ASUBed>(FVector(CX - 130.f, CY - 200.f, Z), 0.f);

	// documents
	SpawnDocument(FVector(CX - 180.f, CY + CabinHalf - 5.f, Z + 150.f), -90.f, TEXT("Pinnwand"),
		TEXT("DIENSTPLAN POSTEN 07\nBesetzung: 1–4 Beobachter. Funkkontakt zur Zentrale täglich.\n\n")
		TEXT("NOTFALL\nZentrale: Kanal 1 (Funkgerät am Schreibtisch).\nTankstelle Pine Ridge: an der Waldstraße Richtung Osten.\n\n")
		TEXT("ERINNERUNGEN\n– Generator unten am Schuppen läuft auf Diesel. Bei Stromausfall dort nachsehen.\n")
		TEXT("– Der Wagen säuft. Rechtzeitig tanken!\n– Kamera nach jedem Einsatz am Einsatzcomputer auswerten.\n\n")
		TEXT("(Handschriftlich, neu:) Nachts NICHT die Nordstraße allein fahren. – M."),
		static_cast<int32>(ESUDocumentLook::Board));
	SpawnDocument(FVector(CX - 205.f, CY - 10.f, Z + 75.5f), 15.f, TEXT("Übergabenotiz"),
		TEXT("Für meine Ablösung,\n\nder Turm ist in Ordnung, die Lampe über dem Tisch flackert manchmal – Wackelkontakt, denke ich.\n")
		TEXT("Die letzten Nächte habe ich Licht zwischen den Bäumen gesehen. Kein Feuer. Es bewegte sich.\n")
		TEXT("Hallberg vom Forsthaus meinte, ich soll mir nichts dabei denken.\n\n")
		TEXT("Wenn das Funkgerät nachts von allein rauscht: nicht antworten, bevor die Zentrale sich nennt.\n\n– Marek"),
		static_cast<int32>(ESUDocumentLook::Note));

	// equipment shelf: camera, flashlight, bandage; batteries in the nightstand
	SpawnItem(SUItem::Camera, FVector(CX + 225.f, CY - 75.f, Z + 101.5f), 180.f);
	SpawnItem(SUItem::Flashlight, FVector(CX + 225.f, CY - 15.f, Z + 101.5f), 0.f);
	SpawnItem(SUItem::Bandage, FVector(CX + 225.f, CY - 40.f, Z + 161.5f), 0.f);
	SpawnItem(SUItem::Snack, FVector(CX + 180.f, CY + 215.f, Z + 92.f), 30.f);
	if (ASUDrawer* Night = SpawnInteractable<ASUDrawer>(FVector(CX + 10.f, CY - CabinHalf + 45.f, Z), 90.f, [](ASUDrawer& D)
	{
		D.Width = 50.f;
		D.Depth = 40.f;
		D.CabinetHeight = 60.f;
	}))
	{
		for (int32 I = 0; I < 2; ++I)
		{
			if (ASUPickupItem* Battery = ASUPickupItem::SpawnItem(GetWorld(), SUItem::Battery, 1, 1.f, FTransform(Night->GetActorRotation(), Night->GetDrawerComponent()->GetComponentTransform().TransformPosition(Night->GetItemSlot(I)))))
			{
				Battery->AttachToComponent(Night->GetDrawerComponent(), FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}
	// toolkit in the shed
	SpawnItem(SUItem::Toolkit, FVector(300.f - 120.f, 1300.f, 82.f), 90.f);

	// the team vehicle on the parking area, facing the road
	if (ASUWorldBuilder* B = Builder.Get())
	{
		ASUVehicle* Vehicle = SpawnInteractable<ASUVehicle>(FVector(1350.f, -380.f, 90.f), 0.f);
		B->SetMainVehicle(Vehicle);
	}
}
