#include "World/Sites/SUSiteTown.h"
#include "Core/SUTypes.h"

void ASUSiteTown::BuildSite(ASUWorldBuilder* InBuilder, bool bInServer)
{
	Super::BuildSite(InBuilder, bInServer);

	FRandomStream Rng(9001);
	const ESUMat Facades[] = {ESUMat::Brick, ESUMat::Concrete, ESUMat::WoodPainted, ESUMat::White, ESUMat::WoodGrey};

	// blocks of houses and a few taller buildings along a grid of streets
	for (int32 BX = -4; BX <= 4; ++BX)
	{
		for (int32 BY = -3; BY <= 3; ++BY)
		{
			if (BX == 0 || BY == 0)
			{
				continue; // main streets
			}
			const FVector BlockCenter(BX * 4200.f, BY * 4200.f, 0.f);
			const int32 Buildings = Rng.RandRange(2, 4);
			for (int32 I = 0; I < Buildings; ++I)
			{
				const FVector Off(Rng.FRandRange(-1200.f, 1200.f), Rng.FRandRange(-1200.f, 1200.f), 0.f);
				const bool bTall = Rng.FRand() < 0.2f && FMath::Abs(BX) < 3;
				const float SX = Rng.FRandRange(900.f, 1600.f);
				const float SY = Rng.FRandRange(900.f, 1800.f);
				const float H = bTall ? Rng.FRandRange(1500.f, 3200.f) : Rng.FRandRange(350.f, 700.f);
				const FVector C = BlockCenter + Off;
				if (bTall)
				{
					AddBox(C + FVector(0.f, 0.f, H * 0.5f), FVector(SX, SY, H), Facades[Rng.RandRange(0, 4)]);
					// rows of dark windows
					for (float Z = 300.f; Z < H - 150.f; Z += 320.f)
					{
						AddBox(C + FVector(SX * 0.5f + 2.f, 0.f, Z), FVector(4.f, SY * 0.85f, 120.f), ESUMat::Glass, FRotator::ZeroRotator, false);
					}
				}
				else
				{
					AddSimpleHouse(C, SX, SY, H, Facades[Rng.RandRange(0, 4)], ESUMat::Roof, false, I + BX * 7 + BY * 13);
				}
			}
		}
	}
	// water tower and church as landmarks
	AddShape(ESUShape::Cylinder, FVector(-6000.f, 5000.f, 1400.f), FVector(60.f, 60.f, 2800.f), ESUMat::MetalDark);
	AddShape(ESUShape::Cylinder, FVector(-6000.f, 5000.f, 3000.f), FVector(900.f, 900.f, 600.f), ESUMat::MetalWhite);
	AddShape(ESUShape::Cone, FVector(-6000.f, 5000.f, 3450.f), FVector(950.f, 950.f, 300.f), ESUMat::Roof);
	AddText(SceneRoot, FVector(-5540.f, 5000.f, 3000.f), FRotator::ZeroRotator, TEXT("HARROW FALLS"), 160.f, FColor(40, 40, 40));
	AddBox(FVector(5000.f, -6000.f, 600.f), FVector(1400.f, 800.f, 1200.f), ESUMat::White);
	AddBox(FVector(5000.f, -5300.f, 2000.f), FVector(400.f, 400.f, 4000.f), ESUMat::White);
	AddShape(ESUShape::Cone, FVector(5000.f, -5300.f, 4600.f), FVector(500.f, 500.f, 1200.f), ESUMat::Roof);

	// a few lights so the town glows in the night
	for (int32 I = 0; I < 8; ++I)
	{
		AddPointLight(SceneRoot, FVector(Rng.FRandRange(-15000.f, 15000.f), Rng.FRandRange(-12000.f, 12000.f), 600.f), 3000.f, 5000.f, FLinearColor(1.f, 0.7f, 0.4f), false);
	}
}
