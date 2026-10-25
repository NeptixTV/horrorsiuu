#include "Core/SUItemDatabase.h"
#include "Core/SUTypes.h"

namespace
{
	FSUItemDef MakeItem(FName Id, const TCHAR* Name, const TCHAR* Desc, int32 MaxStack, int32 Price, ESUItemShape Shape, FVector Size, FLinearColor Color)
	{
		FSUItemDef D;
		D.Id = Id;
		D.Name = SUText(Name);
		D.Description = SUText(Desc);
		D.MaxStack = MaxStack;
		D.PriceCents = Price;
		D.Shape = Shape;
		D.Size = Size;
		D.Color = Color;
		return D;
	}

	TArray<FSUItemDef> BuildItems()
	{
		TArray<FSUItemDef> Items;

		FSUItemDef Flashlight = MakeItem(SUItem::Flashlight, TEXT("Taschenlampe"),
			TEXT("Robuste Stablampe. [F] ein/aus. Läuft mit Batterien."), 1, 2500, ESUItemShape::Cylinder, FVector(5.f, 5.f, 26.f), FLinearColor(0.05f, 0.05f, 0.05f));
		Flashlight.bHasBattery = true;
		Items.Add(Flashlight);

		FSUItemDef Camera = MakeItem(SUItem::Camera, TEXT("Kamera"),
			TEXT("Digitale Spiegelreflexkamera für Beweisfotos. [C] ausrüsten, [RMT] Sucher, [LMT] Foto, Mausrad Zoom, [G] Blitz."), 1, 18000, ESUItemShape::Box, FVector(14.f, 8.f, 10.f), FLinearColor(0.03f, 0.03f, 0.03f));
		Camera.bHasBattery = true;
		Items.Add(Camera);

		FSUItemDef Battery = MakeItem(SUItem::Battery, TEXT("Batterien"),
			TEXT("Lädt Taschenlampe und Kamera vollständig auf (im Inventar benutzen)."), 8, 600, ESUItemShape::Cylinder, FVector(3.f, 3.f, 6.f), FLinearColor(0.8f, 0.55f, 0.1f));
		Battery.bConsumable = true;
		Items.Add(Battery);

		FSUItemDef Bandage = MakeItem(SUItem::Bandage, TEXT("Verbandszeug"),
			TEXT("Stellt 40 Gesundheit wieder her."), 5, 800, ESUItemShape::Box, FVector(10.f, 7.f, 4.f), FLinearColor(0.9f, 0.9f, 0.88f));
		Bandage.bConsumable = true;
		Bandage.HealthRestore = 40.f;
		Items.Add(Bandage);

		FSUItemDef Toolkit = MakeItem(SUItem::Toolkit, TEXT("Werkzeugkasten"),
			TEXT("Damit lassen sich kleinere Schäden am Fahrzeug selbst reparieren (am Motor benutzen)."), 1, 4500, ESUItemShape::Box, FVector(40.f, 18.f, 16.f), FLinearColor(0.6f, 0.08f, 0.05f));
		Items.Add(Toolkit);

		FSUItemDef Snack = MakeItem(SUItem::Snack, TEXT("Müsliriegel"),
			TEXT("Ein kleiner Snack. +10 Gesundheit."), 6, 300, ESUItemShape::Box, FVector(12.f, 4.f, 2.f), FLinearColor(0.75f, 0.45f, 0.15f));
		Snack.bConsumable = true;
		Snack.HealthRestore = 10.f;
		Items.Add(Snack);

		FSUItemDef Drink = MakeItem(SUItem::Drink, TEXT("Limonade"),
			TEXT("Kalt aus dem Kühlschrank. +5 Gesundheit."), 6, 200, ESUItemShape::Cylinder, FVector(6.f, 6.f, 12.f), FLinearColor(0.1f, 0.45f, 0.2f));
		Drink.bConsumable = true;
		Drink.HealthRestore = 5.f;
		Items.Add(Drink);

		FSUItemDef Coffee = MakeItem(SUItem::Coffee, TEXT("Kaffee"),
			TEXT("Pappbecher Tankstellenkaffee. +8 Gesundheit."), 3, 250, ESUItemShape::Cylinder, FVector(8.f, 8.f, 11.f), FLinearColor(0.9f, 0.9f, 0.85f));
		Coffee.bConsumable = true;
		Coffee.HealthRestore = 8.f;
		Items.Add(Coffee);

		FSUItemDef Notebook = MakeItem(SUItem::RangerNotebook, TEXT("Notizbuch des Försters"),
			TEXT("Die letzten Einträge von Förster Hallberg. Beweisstück für die Zentrale."), 1, 0, ESUItemShape::Flat, FVector(20.f, 14.f, 2.f), FLinearColor(0.25f, 0.12f, 0.06f));
		Notebook.bQuestItem = true;
		Items.Add(Notebook);

		FSUItemDef Phone = MakeItem(SUItem::M02Phone, TEXT("Fremdes Telefon"),
			TEXT("Ein Mobiltelefon, gefunden neben dem verlassenen Wagen. Der Akku ist fast leer."), 1, 0, ESUItemShape::Flat, FVector(14.f, 7.f, 1.f), FLinearColor(0.02f, 0.02f, 0.02f));
		Phone.bQuestItem = true;
		Items.Add(Phone);

		return Items;
	}
}

const TArray<FSUItemDef>& FSUItemDatabase::All()
{
	static const TArray<FSUItemDef> Items = BuildItems();
	return Items;
}

const FSUItemDef* FSUItemDatabase::Find(FName Id)
{
	for (const FSUItemDef& Def : All())
	{
		if (Def.Id == Id)
		{
			return &Def;
		}
	}
	return nullptr;
}

FText FSUItemDatabase::GetName(FName Id)
{
	const FSUItemDef* Def = Find(Id);
	return Def ? Def->Name : FText::FromName(Id);
}
