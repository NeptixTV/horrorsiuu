#include "World/Sites/SUSiteManor.h"
#include "SignalUnknown.h"
#include "Core/SUTypes.h"
#include "Core/SUItemDatabase.h"
#include "World/SUModelLibrary.h"
#include "World/SUWorldBuilder.h"
#include "World/SUSkySystem.h"
#include "Horror/SUCreature.h"
#include "Interaction/SUDoor.h"
#include "Interaction/SUDocument.h"
#include "Components/PointLightComponent.h"

FVector ASUSiteManor::Anchor(const char* Name)
{
	for (const SUManor::Anchor& A : SUManor::Anchors)
	{
		if (FCStringAnsi::Strcmp(A.Name, Name) == 0)
		{
			return L(A.P);
		}
	}
	UE_LOG(LogSignalUnknown, Warning, TEXT("Manor: unknown anchor %s"), ANSI_TO_TCHAR(Name));
	return FVector::ZeroVector;
}

FBox ASUSiteManor::ZoneBox(const char* Name, float Grow) const
{
	for (const SUManor::Zone& Z : SUManor::Zones)
	{
		if (FCStringAnsi::Strcmp(Z.Name, Name) == 0)
		{
			return FBox(L(Z.Lo), L(Z.Hi)).ExpandBy(Grow);
		}
	}
	return FBox(FVector::ZeroVector, FVector::ZeroVector);
}

void ASUSiteManor::BuildFallback()
{
	// model files missing: a plain house so the site is at least visible
	AddSimpleHouse(FVector(-900.f, 0.f, 0.f) - FVector(SUManor::PadCentre.X, SUManor::PadCentre.Y, 0.f), 1800.f, 1200.f, 600.f, ESUMat::WoodGrey, ESUMat::Roof, true, 77);
	AddArea(SUArea::ManorGrounds, FVector(0.f, 0.f, 400.f), FVector(6000.f, 6000.f, 1500.f));
}

void ASUSiteManor::BuildGarden()
{
	USUModelLibrary* Models = USUModelLibrary::Get(this);
	if (!Models)
	{
		return;
	}
	// overgrown grounds: single trees and bushes around the house, the old shed next to the drive
	static const TCHAR* Trees[] = {TEXT("SM_Veg_ConiferTall"), TEXT("SM_Veg_PineTall"), TEXT("SM_Veg_ConiferMid"), TEXT("SM_Veg_SpruceBushy"), TEXT("SM_Veg_ConiferLight")};
	FRandomStream Rng(4711);
	for (int32 I = 0; I < 46; ++I)
	{
		const float Angle = Rng.FRandRange(0.f, 360.f);
		const float Radius = Rng.FRandRange(3300.f, 6300.f);
		const FVector P(FMath::Cos(FMath::DegreesToRadians(Angle)) * Radius, FMath::Sin(FMath::DegreesToRadians(Angle)) * Radius, -15.f);
		// keep the drive (it arrives along local +X in front of the door) and the house itself free
		const FVector Door = L(SUManor::Vec{0.f, 0.f, 0.f});
		if (P.X > Door.X - 200.f && FMath::Abs(P.Y - Door.Y) < 900.f)
		{
			continue;
		}
		if (P.X < Door.X + 900.f && P.X > Door.X - 3000.f && FMath::Abs(P.Y - Door.Y) < 1900.f)
		{
			continue;
		}
		const float Scale = Rng.FRandRange(0.8f, 1.25f);
		Models->SpawnMesh(this, SceneRoot, Trees[I % 5], FTransform(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), P, FVector(Scale)), false);
		// trunk collision
		AddShape(ESUShape::Cylinder, P + FVector(0.f, 0.f, 200.f), FVector(55.f, 55.f, 400.f), ESUMat::Bark, FRotator::ZeroRotator, true);
	}
	for (int32 I = 0; I < 30; ++I)
	{
		const float Angle = Rng.FRandRange(0.f, 360.f);
		const float Radius = Rng.FRandRange(1700.f, 6000.f);
		const FVector P(FMath::Cos(FMath::DegreesToRadians(Angle)) * Radius, FMath::Sin(FMath::DegreesToRadians(Angle)) * Radius, -5.f);
		const FVector Door = L(SUManor::Vec{0.f, 0.f, 0.f});
		if (P.X < Door.X + 800.f && P.X > Door.X - 2700.f && FMath::Abs(P.Y - Door.Y) < 1500.f)
		{
			continue;
		}
		if (P.X > Door.X && FMath::Abs(P.Y - Door.Y) < 500.f)
		{
			continue;
		}
		Models->SpawnMesh(this, SceneRoot, TEXT("SM_Veg_Bush"), FTransform(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), P, FVector(Rng.FRandRange(0.7f, 1.4f))), false);
	}
	const FVector Shed = L(SUManor::Vec{900.f, -1900.f, 0.f});
	Models->SpawnMesh(this, SceneRoot, TEXT("SM_Shed"), FTransform(FRotator(0.f, 200.f, 0.f), Shed), true, SUSurfaceTag::Wood);
}

void ASUSiteManor::BuildSite(ASUWorldBuilder* InBuilder, bool bInServer)
{
	Super::BuildSite(InBuilder, bInServer);
	USUModelLibrary* Models = USUModelLibrary::Get(this);
	if (!Models || !Models->HasMesh(TEXT("SM_Manor_Shell")))
	{
		UE_LOG(LogSignalUnknown, Error, TEXT("Manor: Content/SUModels/Manor.sumodel is missing - building a placeholder."));
		BuildFallback();
		return;
	}
	const FTransform Offset(FRotator::ZeroRotator, FVector(-SUManor::PadCentre.X, -SUManor::PadCentre.Y, 0.f));

	// ---- geometry
	for (const char* Name : SUManor::ManorMeshes)
	{
		Models->SpawnMesh(this, SceneRoot, FName(ANSI_TO_TCHAR(Name)), Offset, true, SUSurfaceTag::Wood);
	}
	for (const char* Name : SUManor::UndergroundMeshes)
	{
		Models->SpawnMesh(this, SceneRoot, FName(ANSI_TO_TCHAR(Name)), Offset, true, SUSurfaceTag::Dirt);
	}
	bool bTomb = true;
	for (const char* Name : SUManor::TombMeshes)
	{
		bTomb = bTomb && Models->HasMesh(FName(ANSI_TO_TCHAR(Name)));
	}
	if (bTomb)
	{
		for (const char* Name : SUManor::TombMeshes)
		{
			Models->SpawnMesh(this, SceneRoot, FName(ANSI_TO_TCHAR(Name)), Offset, true, SUSurfaceTag::Concrete);
		}
	}
	else
	{
		// The tomb model is optional (its licence forbids putting it into the public repository).
		// Without it the passage ends in a cave-in right behind the dig chamber.
		const FVector Dig = Anchor("DigChamber");
		const FVector Pit = Anchor("TombStairsTop");
		const FVector Mid = (Dig + Pit) * 0.5f + FVector(0.f, 0.f, 150.f);
		FRandomStream Rng(99);
		for (int32 I = 0; I < 46; ++I)
		{
			const FVector P = Mid + FVector(Rng.FRandRange(-330.f, 330.f), Rng.FRandRange(-520.f, 520.f), Rng.FRandRange(-260.f, 330.f));
			const float S = Rng.FRandRange(120.f, 320.f);
			AddShape(ESUShape::Sphere, P, FVector(S, S * Rng.FRandRange(0.7f, 1.2f), S * 0.8f), ESUMat::Rock, FRotator(Rng.FRandRange(0.f, 80.f), Rng.FRandRange(0.f, 360.f), 0.f), true);
		}
		UE_LOG(LogSignalUnknown, Log, TEXT("Manor: optional tomb model not installed (Content/SUModels/Restricted/Tomb.sumodel)."));
	}
	BuildGarden();

	// ---- lights. Only a few cast shadows; the rest are cheap fill lights.
	int32 LightIndex = 0;
	for (const SUManor::Light& Light : SUManor::Lights)
	{
		const bool bShadows = (LightIndex++ % 3) == 0;
		AddPointLight(SceneRoot, L(Light.P), Light.Intensity * 22.f, Light.Radius, FLinearColor(Light.R, Light.G, Light.B), bShadows);
		if (Light.Kind == 1)
		{
			AddShape(ESUShape::Sphere, L(Light.P) + FVector(0.f, 0.f, 12.f), FVector(9.f, 9.f, 12.f), ESUMat::Lamp, FRotator::ZeroRotator, false);
		}
		else if (Light.Kind == 2)
		{
			AddBox(L(Light.P) + FVector(0.f, 0.f, 10.f), FVector(14.f, 14.f, 22.f), ESUMat::Lamp, FRotator::ZeroRotator, false);
			AddBox(L(Light.P) + FVector(0.f, 0.f, 24.f), FVector(18.f, 18.f, 5.f), ESUMat::MetalDark, FRotator::ZeroRotator, false);
		}
	}
	if (bTomb)
	{
		// torch light in the tomb: every second lamp of the model
		int32 Index = 0;
		for (const SUManor::Light& Light : SUManor::TombLights)
		{
			if ((Index++ % 2) == 0)
			{
				AddPointLight(SceneRoot, L(Light.P), 7.f, Light.Radius, FLinearColor(Light.R, Light.G, Light.B), Index == 1);
			}
		}
	}

	// ---- no daylight below ground
	if (ASUWorldBuilder* B = Builder.Get())
	{
		if (ASUSkySystem* Sky = B->GetSkySystem())
		{
			for (const char* Name : {"Cellar", "Underground", "Tomb"})
			{
				const FBox Local = ZoneBox(Name, 60.f);
				const FVector A = ToWorld(Local.Min);
				const FVector C = ToWorld(Local.Max);
				Sky->AddDarkZone(FBox(A.ComponentMin(C), A.ComponentMax(C)));
			}
		}
	}

	// ---- areas for the mission
	const FBox House = ZoneBox("House");
	AddArea(SUArea::ManorGrounds, FVector(0.f, 0.f, 500.f), FVector(6800.f, 6800.f, 2500.f));
	AddArea(SUArea::ManorInterior, House.GetCenter(), House.GetExtent());
	const FBox Cellar = ZoneBox("Cellar");
	AddArea(SUArea::ManorCellar, Cellar.GetCenter(), Cellar.GetExtent() - FVector(40.f, 40.f, 0.f));
	AddArea(SUArea::ManorMine, Anchor("MineJunction") + FVector(0.f, 0.f, 150.f), FVector(500.f, 500.f, 300.f));
	AddArea(SUArea::ManorCavern, Anchor("CavernCentre") + FVector(0.f, 0.f, 300.f), FVector(1500.f, 1500.f, 500.f));
	AddArea(SUArea::ManorDeep, (bTomb ? Anchor("TombChamber") : Anchor("Lair")) + FVector(0.f, 0.f, 150.f), bTomb ? FVector(380.f, 260.f, 260.f) : FVector(450.f, 450.f, 300.f));

	// ---- evidence
	AddEvidence(TEXT("EV_M03_Scratches"), TEXT("Kratzspuren an der Kellertreppe"), TEXT("Traces"), Anchor("CellarStairsTop") + FVector(-60.f, 0.f, 60.f), FVector(160.f, 90.f, 110.f), true, 900.f);
	AddEvidence(TEXT("EV_M03_Breach"), TEXT("Durchbruch in der Kellerwand"), TEXT("Traces"), Anchor("Breach") + FVector(0.f, 60.f, 100.f), FVector(110.f, 60.f, 110.f), true, 1200.f);
	AddEvidence(TEXT("EV_M03_Nest"), TEXT("Knochennest in der Höhle"), TEXT("Traces"), Anchor("Lair") + FVector(0.f, 0.f, 30.f), FVector(230.f, 230.f, 60.f), true, 1800.f);
	if (bTomb)
	{
		AddEvidence(TEXT("EV_M03_Source"), TEXT("Sarkophag in der Grabkammer"), TEXT("Clue"), Anchor("Sarcophagus") + FVector(0.f, 0.f, -40.f), FVector(190.f, 90.f, 70.f), true, 1500.f);
	}
	else
	{
		AddEvidence(TEXT("EV_M03_Source"), TEXT("Alter Sender im Knochennest"), TEXT("Clue"), Anchor("Lair") + FVector(120.f, 60.f, 25.f), FVector(40.f, 30.f, 25.f), true, 1200.f);
		AddBox(Anchor("Lair") + FVector(120.f, 60.f, 16.f), FVector(46.f, 30.f, 26.f), ESUMat::MetalPainted, FRotator(0.f, 25.f, 8.f), false);
		AddBox(Anchor("Lair") + FVector(120.f, 60.f, 60.f), FVector(2.f, 2.f, 70.f), ESUMat::MetalDark, FRotator(0.f, 0.f, 12.f), false);
		AddPointLight(SceneRoot, Anchor("Lair") + FVector(130.f, 60.f, 45.f), 1.2f, 260.f, FLinearColor(1.f, 0.1f, 0.05f), false);
	}
	// dried scratches / blood: visible markers for the evidence
	AddBox(Anchor("CellarStairsTop") + FVector(-110.f, -52.f, 70.f), FVector(4.f, 90.f, 60.f), ESUMat::BloodDry, FRotator(0.f, 90.f, 12.f), false);
	AddBox(Anchor("Hall") + FVector(-250.f, 60.f, 1.f), FVector(240.f, 22.f, 1.f), ESUMat::BloodDry, FRotator(0.f, 14.f, 0.f), false);
	AddEvidence(TEXT("EV_M03_HallTrail"), TEXT("Schleifspur im Flur"), TEXT("Traces"), Anchor("Hall") + FVector(-250.f, 60.f, 5.f), FVector(240.f, 40.f, 8.f), false, 1000.f, FRotator(0.f, 14.f, 0.f));

	if (!bServer)
	{
		return;
	}

	// ---- doors (replicated actors)
	for (const SUManor::Door& Door : SUManor::Doors)
	{
		if (Door.Kind == 0)
		{
			continue;
		}
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Door.DirY, Door.DirX));
		const FVector Right(-Door.DirY, Door.DirX, 0.f);
		const int32 Leaves = Door.Kind == 2 ? 2 : 1;
		for (int32 Leaf = 0; Leaf < Leaves; ++Leaf)
		{
			const float Half = Door.Width / (2.f * Leaves);
			// double door: two leaves hinged at the outer edges (the second one is turned around)
			const FVector Pos = L(Door.P) + (Leaves == 2 ? Right * (Leaf == 0 ? -Half : Half) : FVector::ZeroVector);
			SpawnInteractable<ASUDoor>(Pos, Yaw + (Leaf == 1 ? 180.f : 0.f), [&](ASUDoor& D)
			{
				D.Width = Door.Width / Leaves - 2.f;
				D.Height = Door.Height - 3.f;
				D.Thickness = 6.f;
				D.Material = ESUMat::WoodDark;
				D.DisplayName = SUText(Leaves == 2 ? TEXT("Haustür") : TEXT("Tür"));
			});
		}
	}

	// ---- things to find
	SpawnDocument(Anchor("StudyDesk") + FVector(0.f, 20.f, 2.f), 20.f, TEXT("Tagebuch von E. Brandt"),
		TEXT("3. Oktober\nDie Arbeiter weigern sich, den alten Stollen weiter zu öffnen. Sie sagen, es klopft zurück.\n\n")
		TEXT("11. Oktober\nHinter der Kellerwand ist ein Hohlraum. Ich habe die Steine selbst herausgenommen.\n\n")
		TEXT("19. Oktober\nEs gibt dort unten eine Halle. Etwas schläft darin. Es hört alles. Wer rennt, ist verloren. ")
		TEXT("Wer kriecht, kommt vorbei. In die engen Spalten passt es nicht hinein.\n\n")
		TEXT("2. November\nDarunter liegt noch etwas. Älter als das Haus. Älter als das Dorf. Von dort kommt der Ton im Funkgerät.\n\n")
		TEXT("(Die letzte Seite ist herausgerissen.)"),
		static_cast<int32>(ESUDocumentLook::Notebook), TEXT("Doc_ManorDiary"));
	AddEvidence(TEXT("EV_M03_Diary"), TEXT("Tagebuch von E. Brandt"), TEXT("Clue"), Anchor("StudyDesk") + FVector(0.f, 20.f, 4.f), FVector(16.f, 12.f, 4.f), false, 500.f);
	SpawnDocument(Anchor("StoreTable") + FVector(0.f, 0.f, 2.f), -30.f, TEXT("Zettel eines Bergmanns"),
		TEXT("Schicht 4 fährt nicht mehr ein.\n\nKarl hat es gesehen. Groß wie zwei Männer, wenn es sich aufrichtet. Ein Auge.\n")
		TEXT("Es kommt, wenn die Lampen brennen und wenn jemand rennt.\n\nWir haben die Spalten offen gelassen. Wer es hört: hinein und still sein, bis es geht."),
		static_cast<int32>(ESUDocumentLook::Note), TEXT("Doc_MinerNote"));
	SpawnItem(SUItem::Battery, Anchor("KitchenTable") + FVector(0.f, 0.f, 4.f), 15.f, 2);
	SpawnItem(SUItem::Battery, Anchor("StoreTable") + FVector(30.f, 25.f, 4.f), 70.f, 2);
	SpawnItem(SUItem::Bandage, Anchor("PantryShelf") + FVector(10.f, 0.f, 4.f), 0.f, 2);
	SpawnItem(SUItem::Bandage, Anchor("CellarStoreShelf") + FVector(15.f, 0.f, 4.f), 0.f, 1);
	SpawnItem(SUItem::Battery, Anchor("VaultShelf") + FVector(0.f, 10.f, 4.f), 40.f, 1);

	// ---- the dweller and its tunnels
	TSharedPtr<SUModel::NavGraph> Graph = MakeShared<SUModel::NavGraph>();
	const int32 NodeCount = bTomb ? static_cast<int32>(UE_ARRAY_COUNT(SUManor::NavNodes)) : SUManor::NavFirstTombNode;
	Graph->SetNodeCount(NodeCount);
	for (int32 I = 0; I < NodeCount; ++I)
	{
		const FVector W = ToWorld(L(SUManor::NavNodes[I]));
		Graph->Nodes[static_cast<size_t>(I)] = {static_cast<float>(W.X), static_cast<float>(W.Y), static_cast<float>(W.Z)};
	}
	for (const SUManor::Edge& E : SUManor::NavEdges)
	{
		if (E.A < NodeCount && E.B < NodeCount)
		{
			Graph->AddEdge(E.A, E.B);
		}
	}
	TArray<FVector> HideEntries;
	TArray<FVector> HideInsides;
	for (const SUManor::Hide& H : SUManor::Hides)
	{
		HideEntries.Add(ToWorld(L(H.Entry)));
		HideInsides.Add(ToWorld(L(H.Inside)));
	}
	const FVector SpawnLocal = L(SUManor::CyclopsSpawn) + FVector(0.f, 0.f, 130.f);
	const FVector SpawnWorld = ToWorld(SpawnLocal);
	if (ASUCreature* Dweller = ASUCreature::SpawnCreature(GetWorld(), ESUCreatureType::Dweller, SpawnWorld, static_cast<float>(GetActorRotation().Yaw) + 200.f, ESUCreatureState::Idle, nullptr))
	{
		const int32 Home = Graph->Nearest(static_cast<float>(SpawnWorld.X), static_cast<float>(SpawnWorld.Y), static_cast<float>(SpawnWorld.Z));
		Dweller->SetNavGraph(Graph, Home, HideEntries, HideInsides);
	}
	RegisterPoint(TEXT("ManorDoor"), L(SUManor::Vec{250.f, 0.f, 128.f}), 180.f);
}
