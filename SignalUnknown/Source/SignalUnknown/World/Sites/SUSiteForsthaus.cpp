#include "World/Sites/SUSiteForsthaus.h"
#include "World/SUWorldBuilder.h"
#include "Core/SUTypes.h"
#include "Core/SUItemDatabase.h"
#include "Interaction/SUDoor.h"
#include "Interaction/SULighting.h"
#include "Interaction/SUDocument.h"
#include "Mission/SUAreaTrigger.h"
#include "Horror/SUScriptedWindowFigure.h"

// local layout: +X points towards the driveway. House: x [-1100,-200], y [-600,600], floor at z=40
namespace HouseLayout
{
	constexpr float X0 = -1100.f;
	constexpr float X1 = -200.f;
	constexpr float Y0 = -600.f;
	constexpr float Y1 = 600.f;
	constexpr float Floor = 40.f;
	constexpr float WallH = 280.f;
	constexpr float Ext = 15.f;
	constexpr float Int = 10.f;
}

using namespace HouseLayout;

void ASUSiteForsthaus::BuildSite(ASUWorldBuilder* InBuilder, bool bInServer)
{
	Super::BuildSite(InBuilder, bInServer);
	SetSurface(SUSurfaceTag::Wood);
	BuildHouse();
	BuildLivingRoom();
	BuildHallKitchen();
	BuildOfficeBedroom();
	SetSurface(SUSurfaceTag::Dirt);
	BuildOutside();
	SetSurface(NAME_None);

	AddArea(SUArea::ForsthausGrounds, FVector(-300.f, 0.f, 400.f), FVector(2800.f, 2800.f, 900.f));
	ASUAreaTrigger* Interior = AddArea(SUArea::ForsthausInterior, FVector((X0 + X1) * 0.5f, 0.f, Floor + 140.f), FVector((X1 - X0) * 0.5f - 10.f, Y1 - 10.f, 140.f));
	RegisterPoint(TEXT("ForsthausDoor"), FVector(X1 + 150.f, 0.f, Floor));
	SpawnInteractables(Interior);
}

void ASUSiteForsthaus::BuildHouse()
{
	// foundation and floor
	AddBox(FVector((X0 + X1) * 0.5f, 0.f, Floor * 0.5f - 20.f), FVector(X1 - X0 + 20.f, Y1 - Y0 + 20.f, Floor + 40.f), ESUMat::Concrete);
	AddBox(FVector((X0 + X1) * 0.5f, 0.f, Floor - 2.f), FVector(X1 - X0, Y1 - Y0, 4.f), ESUMat::WoodDark);

	// exterior walls
	const FSUOpening OfficeWin(150.f, 200.f, 90.f, 200.f);
	const FSUOpening FrontDoor(550.f, 100.f, 0.f, 215.f);
	const FSUOpening LivingWin(900.f, 200.f, 90.f, 200.f);
	AddWall(FVector2D(X1, Y0), FVector2D(X1, Y1), Floor, WallH, Ext, ESUMat::WoodPainted, {OfficeWin, FrontDoor, LivingWin});
	AddWindowFrame(FVector2D(X1, Y0), FVector2D(X1, Y1), Floor, Ext, OfficeWin, ESUMat::White);
	AddWindowFrame(FVector2D(X1, Y0), FVector2D(X1, Y1), Floor, Ext, LivingWin, ESUMat::White);

	const FSUOpening BedWin(150.f, 200.f, 90.f, 200.f);
	const FSUOpening BackDoor(900.f, 90.f, 0.f, 210.f);
	AddWall(FVector2D(X0, Y0), FVector2D(X0, Y1), Floor, WallH, Ext, ESUMat::WoodPainted, {BedWin, BackDoor});
	AddWindowFrame(FVector2D(X0, Y0), FVector2D(X0, Y1), Floor, Ext, BedWin, ESUMat::White);

	const FSUOpening KitchenWin(100.f, 150.f, 100.f, 200.f);
	const FSUOpening FigureWin(550.f, 200.f, 85.f, 205.f); // the window of the scripted event (broken)
	AddWall(FVector2D(X0, Y1), FVector2D(X1, Y1), Floor, WallH, Ext, ESUMat::WoodPainted, {KitchenWin, FigureWin});
	AddWindowFrame(FVector2D(X0, Y1), FVector2D(X1, Y1), Floor, Ext, KitchenWin, ESUMat::White);
	AddWindowFrame(FVector2D(X0, Y1), FVector2D(X1, Y1), Floor, Ext, FigureWin, ESUMat::White, false);

	const FSUOpening BedSideWin(150.f, 200.f, 90.f, 200.f);
	const FSUOpening OfficeSideWin(680.f, 120.f, 100.f, 200.f);
	AddWall(FVector2D(X0, Y0), FVector2D(X1, Y0), Floor, WallH, Ext, ESUMat::WoodPainted, {BedSideWin, OfficeSideWin});
	AddWindowFrame(FVector2D(X0, Y0), FVector2D(X1, Y0), Floor, Ext, BedSideWin, ESUMat::White);
	AddWindowFrame(FVector2D(X0, Y0), FVector2D(X1, Y0), Floor, Ext, OfficeSideWin, ESUMat::White);

	// interior walls (hallway along X in the middle)
	const FSUOpening KitchenDoor(150.f, 90.f, 0.f, 210.f);
	const FSUOpening LivingDoor(680.f, 90.f, 0.f, 210.f);
	AddWall(FVector2D(X0, 100.f), FVector2D(X1, 100.f), Floor, WallH, Int, ESUMat::WoodLight, {KitchenDoor, LivingDoor});
	AddWall(FVector2D(X0, -100.f), FVector2D(X1, -100.f), Floor, WallH, Int, ESUMat::WoodLight, {KitchenDoor, LivingDoor});
	AddWall(FVector2D(-700.f, 100.f), FVector2D(-700.f, Y1), Floor, WallH, Int, ESUMat::WoodLight, {FSUOpening(200.f, 100.f, 0.f, 215.f)});
	AddWall(FVector2D(-500.f, Y0), FVector2D(-500.f, -100.f), Floor, WallH, Int, ESUMat::WoodLight);

	// ceiling and roof
	AddBox(FVector((X0 + X1) * 0.5f, 0.f, Floor + WallH + 5.f), FVector(X1 - X0 + Ext, Y1 - Y0 + Ext, 10.f), ESUMat::WoodLight);
	AddGableRoof(FVector((X0 + X1) * 0.5f, 0.f, Floor + WallH + 10.f), X1 - X0, Y1 - Y0, 230.f, 50.f, 14.f, ESUMat::Shingle, 0.f);
	AddBox(FVector(-950.f, -350.f, Floor + WallH + 200.f), FVector(70.f, 70.f, 280.f), ESUMat::Brick); // chimney

	// porch
	AddBox(FVector(X1 + 125.f, 0.f, Floor * 0.5f), FVector(250.f, 700.f, Floor), ESUMat::WoodGrey);
	AddStairs(FVector(X1 + 250.f + 70.f, 0.f, 0.f), FVector2D(-1.f, 0.f), 2, 20.f, 35.f, 160.f, ESUMat::WoodGrey, true);
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddBox(FVector(X1 + 235.f, S * 330.f, Floor + 120.f), FVector(12.f, 12.f, 240.f), ESUMat::WoodGrey);
	}
	AddBox(FVector(X1 + 125.f, 0.f, Floor + 255.f), FVector(280.f, 720.f, 8.f), ESUMat::Shingle, FRotator(-8.f, 0.f, 0.f));
	AddText(SceneRoot, FVector(X1 + 9.f, 220.f, Floor + 240.f), FRotator::ZeroRotator, TEXT("FORSTHAUS NORDWALD"), 14.f, FColor(230, 220, 190));
}

void ASUSiteForsthaus::BuildLivingRoom()
{
	const float Z = Floor;
	// overturned dining table, standing on its edge
	AddBox(FVector(-470.f, 330.f, Z + 45.f), FVector(160.f, 5.f, 90.f), ESUMat::WoodLight, FRotator(0.f, 12.f, 0.f));
	for (int32 I = 0; I < 4; ++I)
	{
		const float LX = -470.f + ((I & 1) ? 70.f : -70.f);
		const float LZ = Z + ((I & 2) ? 80.f : 10.f);
		AddBox(FVector(LX, 330.f + 38.f, LZ), FVector(5.f, 72.f, 5.f), ESUMat::WoodLight, FRotator(0.f, 12.f, 0.f), false);
	}
	AddEvidence(TEXT("EV_M01_Table"), TEXT("Umgestürzter Esstisch"), TEXT("Furniture"), FVector(-470.f, 345.f, Z + 50.f), FVector(85.f, 45.f, 50.f), true);

	// broken chair: seat tilted, legs and backrest on the floor
	AddBox(FVector(-300.f, 470.f, Z + 15.f), FVector(42.f, 42.f, 4.f), ESUMat::WoodDark, FRotator(25.f, 30.f, 10.f));
	AddBox(FVector(-260.f, 440.f, Z + 3.f), FVector(45.f, 4.f, 40.f), ESUMat::WoodDark, FRotator(0.f, 70.f, 90.f), false);
	AddBeam(FVector(-340.f, 500.f, Z + 3.f), FVector(-300.f, 540.f, Z + 3.f), 4.f, ESUMat::WoodDark, false);
	AddBeam(FVector(-250.f, 520.f, Z + 3.f), FVector(-230.f, 470.f, Z + 3.f), 4.f, ESUMat::WoodDark, false);
	AddEvidence(TEXT("EV_M01_Chair"), TEXT("Zerbrochener Stuhl"), TEXT("Furniture"), FVector(-290.f, 480.f, Z + 20.f), FVector(55.f, 55.f, 25.f), true);

	// smashed cabinet leaning against the wall, doors torn off
	AddBox(FVector(-640.f, 530.f, Z + 92.f), FVector(55.f, 100.f, 185.f), ESUMat::WoodDark, FRotator(-14.f, 0.f, 0.f));
	AddBox(FVector(-560.f, 470.f, Z + 3.f), FVector(48.f, 90.f, 3.f), ESUMat::WoodDark, FRotator(0.f, 25.f, 0.f), false);
	AddBox(FVector(-520.f, 560.f, Z + 3.f), FVector(48.f, 90.f, 3.f), ESUMat::WoodDark, FRotator(0.f, -40.f, 0.f), false);
	for (int32 I = 0; I < 6; ++I)
	{
		AddBox(FVector(-600.f + I * 18.f, 450.f + (I % 3) * 25.f, Z + 2.f), FVector(20.f, 4.f, 2.f), ESUMat::WoodLight, FRotator(0.f, I * 37.f, 0.f), false);
		AddBox(FVector(-560.f + I * 12.f, 500.f - (I % 2) * 30.f, Z + 1.5f), FVector(10.f, 7.f, 2.f), ESUMat::White, FRotator(0.f, I * 61.f, 0.f), false); // broken plates
	}
	AddEvidence(TEXT("EV_M01_Cabinet"), TEXT("Zertrümmerter Schrank"), TEXT("Furniture"), FVector(-630.f, 525.f, Z + 100.f), FVector(50.f, 60.f, 95.f), true);

	// sofa (torn) along the hallway wall
	AddBox(FVector(-450.f, 160.f, Z + 25.f), FVector(200.f, 80.f, 45.f), ESUMat::Fabric);
	AddBox(FVector(-450.f, 125.f, Z + 65.f), FVector(200.f, 15.f, 50.f), ESUMat::Fabric);
	AddShape(ESUShape::Sphere, FVector(-420.f, 230.f, Z + 8.f), FVector(30.f, 25.f, 15.f), ESUMat::Paper, FRotator::ZeroRotator, false);
	// rug, shelf with scattered books
	AddBox(FVector(-440.f, 360.f, Z + 0.6f), FVector(260.f, 180.f, 1.2f), ESUMat::Brick, FRotator::ZeroRotator, false);
	AddBox(FVector(-215.f, 200.f, Z + 90.f), FVector(25.f, 80.f, 180.f), ESUMat::WoodLight);
	for (int32 I = 0; I < 7; ++I)
	{
		AddBox(FVector(-260.f - I * 14.f, 260.f + (I % 3) * 20.f, Z + 2.f), FVector(16.f, 12.f, 4.f), (I % 2) ? ESUMat::Red : ESUMat::FabricBlue, FRotator(0.f, I * 47.f, 0.f), false);
	}

	// claw marks on the hallway wall (living room side)
	for (int32 I = 0; I < 4; ++I)
	{
		AddBox(FVector(-600.f + I * 12.f, 106.f, Z + 165.f + I * 4.f), FVector(130.f, 2.f, 3.f), ESUMat::Black, FRotator(38.f, 0.f, 0.f), false);
		AddBox(FVector(-600.f + I * 12.f, 106.5f, Z + 167.f + I * 4.f), FVector(110.f, 1.f, 1.f), ESUMat::WoodLight, FRotator(38.f, 0.f, 0.f), false);
	}
	AddEvidence(TEXT("EV_M01_ClawMarks"), TEXT("Kratzspuren an der Wand"), TEXT("Traces"), FVector(-585.f, 108.f, Z + 170.f), FVector(70.f, 8.f, 50.f), true);

	// glass shards below the broken window
	for (int32 I = 0; I < 10; ++I)
	{
		AddBox(FVector(-560.f + I * 22.f, 560.f - (I % 3) * 15.f, Z + 0.5f), FVector(8.f, 5.f, 0.5f), ESUMat::Glass, FRotator(0.f, I * 53.f, 0.f), false);
	}
}

void ASUSiteForsthaus::AddBloodTrail(const FVector& From, const FVector& To, int32 Count, int32 Seed)
{
	FRandomStream Rng(Seed);
	for (int32 I = 0; I < Count; ++I)
	{
		const float T = static_cast<float>(I) / FMath::Max(1, Count - 1);
		const FVector P = FMath::Lerp(From, To, T) + FVector(Rng.FRandRange(-12.f, 12.f), Rng.FRandRange(-18.f, 18.f), 0.f);
		const float S = Rng.FRandRange(8.f, 30.f);
		AddShape(ESUShape::Cylinder, P, FVector(S, S * Rng.FRandRange(0.5f, 1.8f), 0.4f), (I % 4 == 0) ? ESUMat::BloodDry : ESUMat::Blood, FRotator(0.f, Rng.FRandRange(0.f, 180.f), 0.f), false);
	}
}

void ASUSiteForsthaus::AddClawPrint(const FVector& At, float Yaw)
{
	const FRotator R(0.f, Yaw, 0.f);
	AddShape(ESUShape::Cylinder, At, FVector(14.f, 11.f, 0.6f), ESUMat::Mud, R, false);
	for (int32 T = -1; T <= 1; ++T)
	{
		const FVector Toe = At + R.RotateVector(FVector(22.f, T * 8.f, 0.f));
		AddBox(Toe, FVector(26.f, 3.f, 0.6f), ESUMat::Mud, FRotator(0.f, Yaw + T * 14.f, 0.f), false);
	}
}

void ASUSiteForsthaus::BuildHallKitchen()
{
	const float Z = Floor;
	// blood trail down the hallway towards the back door
	AddBloodTrail(FVector(-250.f, 0.f, Z + 0.3f), FVector(-1060.f, 30.f, Z + 0.3f), 34, 77);
	AddBox(FVector(-880.f, 40.f, Z + 0.4f), FVector(90.f, 40.f, 0.4f), ESUMat::Blood, FRotator(0.f, 20.f, 0.f), false); // smear
	AddBox(FVector(-700.f, -94.f, Z + 115.f), FVector(16.f, 1.f, 20.f), ESUMat::Blood, FRotator::ZeroRotator, false);   // hand print
	AddEvidence(TEXT("EV_M01_BloodTrail"), TEXT("Blutspur im Flur"), TEXT("Traces"), FVector(-560.f, 10.f, Z + 8.f), FVector(260.f, 45.f, 10.f), true, 1200.f);

	// kitchen: counter, stove, open fridge, table
	AddBox(FVector(-900.f, 565.f, Z + 45.f), FVector(380.f, 60.f, 90.f), ESUMat::WoodLight);
	AddBox(FVector(-900.f, 565.f, Z + 92.f), FVector(384.f, 64.f, 4.f), ESUMat::Concrete);
	AddBox(FVector(-760.f, 560.f, Z + 45.f), FVector(60.f, 60.f, 90.f), ESUMat::MetalWhite); // stove
	AddBox(FVector(-1060.f, 300.f, Z + 85.f), FVector(60.f, 60.f, 170.f), ESUMat::MetalWhite); // fridge
	AddBox(FVector(-1020.f, 255.f, Z + 85.f), FVector(4.f, 56.f, 160.f), ESUMat::MetalWhite, FRotator(0.f, -70.f, 0.f), false); // fridge door open
	AddBox(FVector(-880.f, 300.f, Z + 75.f), FVector(110.f, 80.f, 4.f), ESUMat::WoodLight);
	AddShape(ESUShape::Cylinder, FVector(-840.f, 220.f, Z + 12.f), FVector(28.f, 28.f, 25.f), ESUMat::MetalDark, FRotator(0.f, 0.f, 90.f), false); // bucket on its side
	for (int32 I = 0; I < 8; ++I)
	{
		AddBox(FVector(-950.f + I * 20.f, 380.f + (I % 2) * 30.f, Z + 0.8f), FVector(12.f, 9.f, 1.5f), ESUMat::White, FRotator(0.f, I * 33.f, 0.f), false);
	}
}

void ASUSiteForsthaus::BuildOfficeBedroom()
{
	const float Z = Floor;
	// office: desk with broken radio, knocked over chair, maps
	AddBox(FVector(-330.f, -540.f, Z + 75.f), FVector(150.f, 65.f, 4.f), ESUMat::WoodDark);
	AddBox(FVector(-395.f, -540.f, Z + 37.f), FVector(10.f, 60.f, 74.f), ESUMat::WoodDark);
	AddBox(FVector(-265.f, -540.f, Z + 37.f), FVector(10.f, 60.f, 74.f), ESUMat::WoodDark);
	AddBox(FVector(-290.f, -555.f, Z + 90.f), FVector(28.f, 40.f, 26.f), ESUMat::MetalPainted, FRotator(0.f, 0.f, 25.f)); // smashed radio
	AddBeam(FVector(-290.f, -530.f, Z + 100.f), FVector(-250.f, -470.f, Z + 115.f), 1.5f, ESUMat::MetalDark, false);
	AddBox(FVector(-320.f, -430.f, Z + 20.f), FVector(45.f, 45.f, 5.f), ESUMat::WoodDark, FRotator(0.f, 30.f, 80.f)); // chair on its side
	AddBox(FVector(-400.f, -594.f, Z + 170.f), FVector(1.f, 120.f, 80.f), ESUMat::Paper, FRotator::ZeroRotator, false); // map on the wall
	AddBox(FVector(-210.f, -300.f, Z + 100.f), FVector(20.f, 60.f, 200.f), ESUMat::WoodDark); // gun cabinet (empty, open)
	AddBox(FVector(-240.f, -335.f, Z + 100.f), FVector(4.f, 55.f, 190.f), ESUMat::WoodDark, FRotator(0.f, 60.f, 0.f), false);
	AddText(SceneRoot, FVector(-399.f, -594.f, Z + 220.f), FRotator(0.f, 90.f, 0.f), TEXT("REVIER NORD"), 6.f, FColor(40, 30, 20));

	// bedroom: bed shoved diagonally, mattress half off, wardrobe
	AddBox(FVector(-800.f, -360.f, Z + 22.f), FVector(200.f, 95.f, 25.f), ESUMat::WoodDark, FRotator(0.f, 28.f, 0.f));
	AddBox(FVector(-760.f, -300.f, Z + 30.f), FVector(190.f, 90.f, 15.f), ESUMat::White, FRotator(0.f, 40.f, 12.f), false);
	AddBox(FVector(-1050.f, -180.f, Z + 100.f), FVector(60.f, 120.f, 200.f), ESUMat::WoodLight);
	AddBox(FVector(-1000.f, -260.f, Z + 3.f), FVector(80.f, 50.f, 5.f), ESUMat::Cloth, FRotator(0.f, 15.f, 0.f), false); // clothes on the floor
	// scratches on the bedroom door frame
	for (int32 I = 0; I < 3; ++I)
	{
		AddBox(FVector(-855.f, -95.f, Z + 120.f + I * 8.f), FVector(40.f, 2.f, 2.f), ESUMat::Black, FRotator(20.f, 0.f, 0.f), false);
	}
}

void ASUSiteForsthaus::BuildOutside()
{
	// footprints leading from the back door into the forest
	for (int32 I = 0; I < 14; ++I)
	{
		const float X = X0 - 70.f - I * 95.f;
		const float Y = 345.f + ((I % 2) ? 22.f : -22.f) - I * 6.f;
		AddClawPrint(FVector(X, Y, GroundZ(X, Y) + 1.f), 180.f);
	}
	AddEvidence(TEXT("EV_M01_Footprints"), TEXT("Fußabdrücke am Hintereingang"), TEXT("Traces"), FVector(X0 - 280.f, 330.f, GroundZ(X0 - 280.f, 330.f) + 5.f), FVector(230.f, 60.f, 10.f), false, 1300.f);

	// ranger's pickup truck (static wreck: doors open, no keys)
	const FVector Truck(500.f, 700.f, 0.f);
	const FRotator TR(0.f, 160.f, 0.f);
	auto TP = [&](const FVector& L) { return Truck + TR.RotateVector(L); };
	AddBox(TP(FVector(0.f, 0.f, 75.f)), FVector(480.f, 190.f, 60.f), ESUMat::Green, TR);
	AddBox(TP(FVector(60.f, 0.f, 140.f)), FVector(170.f, 180.f, 75.f), ESUMat::Green, TR);
	AddBox(TP(FVector(150.f, 0.f, 140.f)), FVector(6.f, 165.f, 60.f), ESUMat::Glass, TR, false);
	AddBox(TP(FVector(-160.f, 0.f, 112.f)), FVector(160.f, 180.f, 20.f), ESUMat::Green, TR);
	for (int32 I = 0; I < 4; ++I)
	{
		AddShape(ESUShape::Cylinder, TP(FVector((I & 1) ? 160.f : -150.f, (I & 2) ? 95.f : -95.f, 40.f)), FVector(80.f, 80.f, 28.f), ESUMat::Tire, TR + FRotator(0.f, 0.f, 90.f));
	}
	AddBox(TP(FVector(80.f, 130.f, 120.f)), FVector(110.f, 5.f, 80.f), ESUMat::Green, TR + FRotator(0.f, 55.f, 0.f), false); // open door

	// wood pile, chopping block, outhouse, clothesline, dead generator, mailbox, sign
	for (int32 I = 0; I < 15; ++I)
	{
		AddShape(ESUShape::Cylinder, FVector(-700.f + (I % 5) * 3.f, -900.f + (I % 5) * 28.f, 14.f + (I / 5) * 26.f), FVector(25.f, 25.f, 110.f), ESUMat::Bark, FRotator(0.f, 0.f, 90.f));
	}
	AddShape(ESUShape::Cylinder, FVector(-450.f, -900.f, 30.f), FVector(55.f, 55.f, 60.f), ESUMat::Bark);
	AddBox(FVector(-440.f, -890.f, 75.f), FVector(4.f, 4.f, 60.f), ESUMat::WoodDark, FRotator(30.f, 0.f, 0.f), false);
	AddBox(FVector(-1500.f, -900.f, 120.f), FVector(130.f, 130.f, 240.f), ESUMat::WoodGrey);
	AddBox(FVector(-800.f, 1150.f, 100.f), FVector(8.f, 8.f, 200.f), ESUMat::WoodDark);
	AddBox(FVector(-300.f, 1150.f, 100.f), FVector(8.f, 8.f, 200.f), ESUMat::WoodDark);
	AddBeam(FVector(-800.f, 1150.f, 190.f), FVector(-300.f, 1150.f, 185.f), 1.f, ESUMat::White, false);
	AddBox(FVector(-450.f, 1150.f, 160.f), FVector(4.f, 50.f, 45.f), ESUMat::Cloth, FRotator::ZeroRotator, false);
	AddBox(FVector(-1250.f, -400.f, 45.f), FVector(110.f, 70.f, 70.f), ESUMat::Rust);
	AddBox(FVector(1100.f, 180.f, 60.f), FVector(8.f, 8.f, 120.f), ESUMat::WoodDark);
	AddBox(FVector(1100.f, 180.f, 125.f), FVector(45.f, 25.f, 25.f), ESUMat::MetalPainted);
	AddText(SceneRoot, FVector(1124.f, 180.f, 128.f), FRotator::ZeroRotator, TEXT("HALLBERG"), 6.f, FColor(240, 240, 230));
}

void ASUSiteForsthaus::SpawnInteractables(ASUAreaTrigger* Interior)
{
	if (!bServer)
	{
		return;
	}
	const float Z = Floor;
	SpawnInteractable<ASUDoor>(FVector(X1, 0.f, Z), 0.f, [](ASUDoor& D)
	{
		D.Width = 98.f;
		D.Height = 213.f;
		D.Material = ESUMat::WoodDark;
	});
	SpawnInteractable<ASUDoor>(FVector(X0, Y0 + 945.f, Z), 180.f, [this](ASUDoor& D)
	{
		D.Width = 88.f;
		D.Height = 208.f;
		D.Material = ESUMat::WoodDark;
		D.SetOpen(true, ToWorld(FVector(X0 + 300.f, 345.f, 100.f))); // the back door stands open
	});
	// room doors along the hallway
	ASUDoor* BedroomDoor = SpawnInteractable<ASUDoor>(FVector(-905.f, -100.f, Z), 90.f, [](ASUDoor& D)
	{
		D.Width = 88.f;
		D.Height = 208.f;
		D.Thickness = 4.f;
		D.Material = ESUMat::WoodLight;
	});
	SpawnInteractable<ASUDoor>(FVector(-375.f, -100.f, Z), 90.f, [](ASUDoor& D)
	{
		D.Width = 88.f;
		D.Height = 208.f;
		D.Thickness = 4.f;
		D.Material = ESUMat::WoodLight;
	});
	SpawnInteractable<ASUDoor>(FVector(-375.f, 100.f, Z), -90.f, [this](ASUDoor& D)
	{
		D.Width = 88.f;
		D.Height = 208.f;
		D.Thickness = 4.f;
		D.Material = ESUMat::WoodLight;
		D.SetOpen(true, ToWorld(FVector(-375.f, 0.f, 100.f)));
	});

	// dead lamps (no power) – they flicker during the horror sequence
	for (const FVector& P : {FVector(-450.f, 350.f, Z + WallH - 2.f), FVector(-650.f, 0.f, Z + WallH - 2.f), FVector(-900.f, 380.f, Z + WallH - 2.f)})
	{
		SpawnInteractable<ASULamp>(P, 0.f, [](ASULamp& L)
		{
			L.Style = ESULampStyle::Ceiling;
			L.Intensity = 90.f;
			L.Radius = 700.f;
			L.LightColor = FLinearColor(1.f, 0.7f, 0.45f);
		})->SetOn(false);
	}

	// the ranger's notebook – the clue for mission 1
	SpawnDocument(FVector(-345.f, -530.f, Z + 77.5f), 10.f, TEXT("Notizbuch des Försters"),
		TEXT("12. Okt. – Wieder Spuren am Bach. Drei Zehen, viel zu lang. Kein Bär.\n\n")
		TEXT("14. Okt. – Die Rehe kommen nicht mehr an die Futterstelle. Der Wald ist zu still.\n\n")
		TEXT("16. Okt. – Nachts stand etwas vor dem Fenster. Groß, dünn. Es hat sich nicht bewegt, bis ich das Licht anmachte. Dann war es weg.\n\n")
		TEXT("17. Okt. – Die Zentrale glaubt mir nicht. Ich habe Fotos gemacht, auf allen nur Dunkelheit.\n\n")
		TEXT("18. Okt. – Es klopft. Immer dreimal. Ich mache die Tür nicht auf.\n\n")
		TEXT("19. Okt. – Es hat meine Stimme benutzt. Draußen hat jemand mit MEINER Stimme nach mir gerufen.\n\n")
		TEXT("(Die letzte Seite ist herausgerissen.)"),
		static_cast<int32>(ESUDocumentLook::Notebook), SUItem::RangerNotebook, SUItem::RangerNotebook);
	SpawnDocument(FVector(-890.f, 300.f, Z + 77.5f), -20.f, TEXT("Einkaufszettel"),
		TEXT("Batterien (viele!)\nLampenöl\nSchloss für die Hintertür\nPatronen\nKaffee"),
		static_cast<int32>(ESUDocumentLook::Note));

	// the scripted horror sequence of mission 1
	SpawnInteractable<ASUScriptedWindowFigure>(FVector(-450.f, 0.f, Z), 0.f, [this, BedroomDoor, Interior](ASUScriptedWindowFigure& S)
	{
		S.SequenceId = TEXT("M01_WindowFigure");
		S.MissionId = TEXT("M01_LastTransmission");
		S.ObjectiveId = TEXT("LookAround");
		S.NoiseLocation = ToWorld(FVector(-820.f, -350.f, Floor + 120.f));
		S.WindowLocation = ToWorld(FVector(-450.f, Y1, Floor + 145.f));
		S.FigureLocation = ToWorld(FVector(-450.f, Y1 + 380.f, GroundZ(-450.f, Y1 + 380.f) + 130.f));
		S.RetreatLocation = ToWorld(FVector(-500.f, Y1 + 2500.f, GroundZ(-500.f, Y1 + 2500.f) + 130.f));
		S.TracksLocation = ToWorld(FVector(-450.f, Y1 + 120.f, GroundZ(-450.f, Y1 + 120.f) + 1.f));
		S.SlamDoor = BedroomDoor;
		S.InteriorArea = Interior;
	});
}
