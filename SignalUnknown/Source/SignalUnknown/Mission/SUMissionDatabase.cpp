#include "Mission/SUMissionDatabase.h"
#include "SignalUnknown.h"
#include "Core/SUDeveloperSettings.h"
#include "Core/SUItemDatabase.h"
#include "Engine/DataTable.h"

namespace
{
	FSUSpeechLine Line(const TCHAR* Speaker, const TCHAR* Text, float Duration = 0.f)
	{
		FSUSpeechLine L;
		L.Speaker = SUText(Speaker);
		L.Text = SUText(Text);
		L.Duration = Duration;
		return L;
	}

	FSUObjectiveDef Obj(const TCHAR* Id, int32 Stage, const TCHAR* Text, FName Event, FName Param = NAME_None, int32 Required = 1, bool bOptional = false, FName Marker = NAME_None)
	{
		FSUObjectiveDef O;
		O.Id = FName(Id);
		O.Stage = Stage;
		O.Text = SUText(Text);
		O.EventName = Event;
		O.EventParam = Param;
		O.Required = Required;
		O.bOptional = bOptional;
		O.MarkerSite = Marker;
		return O;
	}

	FSUEvidenceRequirement Ev(const TCHAR* Id, const TCHAR* Name, const TCHAR* Category, int32 BonusCents, bool bRequired, bool bRare = false, int32 RareBonus = 0)
	{
		FSUEvidenceRequirement E;
		E.EvidenceId = FName(Id);
		E.DisplayName = SUText(Name);
		E.Category = FName(Category);
		E.BonusCents = BonusCents;
		E.bRequired = bRequired;
		E.bRare = bRare;
		E.RareBonusCents = RareBonus;
		return E;
	}

	FSUMissionDef BuildM01()
	{
		FSUMissionDef M;
		M.Id = TEXT("M01_LastTransmission");
		M.Title = SUText(TEXT("Der letzte Funkspruch"));
		M.Summary = SUText(TEXT("Seit mehreren Stunden kein Kontakt zum Forsthaus im nördlichen Waldgebiet. Lage prüfen, Schäden dokumentieren, Bericht abgeben."));
		M.TargetSite = TEXT("Forsthaus");
		M.Briefing = {
			Line(TEXT("Zentrale"), TEXT("Zentrale an Beobachtungsposten 07, bitte kommen.")),
			Line(TEXT("Zentrale"), TEXT("Beobachtungsposten 07, wir haben seit mehreren Stunden keinen Kontakt mehr zu einem Forsthaus im nördlichen Waldgebiet.")),
			Line(TEXT("Zentrale"), TEXT("Förster Hallberg meldet sich weder über Funk noch über Telefon.")),
			Line(TEXT("Zentrale"), TEXT("Überprüfen Sie die Lage und dokumentieren Sie mögliche Schäden. Wir brauchen Fotos.")),
			Line(TEXT("Zentrale"), TEXT("Die Position ist auf Ihrer Wandkarte markiert. Bestätigen Sie den Auftrag, 07.")),
		};
		M.AcceptLines = {
			Line(TEXT("Zentrale"), TEXT("Verstanden, 07. Nehmen Sie Kamera und Lampe mit, es wird bald dunkel.")),
			Line(TEXT("Zentrale"), TEXT("Und 07 … seien Sie vorsichtig da draußen. Zentrale Ende.")),
		};
		M.CompletionLines = {
			Line(TEXT("Zentrale"), TEXT("Zentrale an 07. Ihr Bericht ist eingegangen, die Zahlung ist angewiesen.")),
			Line(TEXT("Zentrale"), TEXT("Diese Spuren … das passt zu zwei weiteren Meldungen aus dem Norden. Wir melden uns. Ende.")),
		};
		const FName Furniture(TEXT("Furniture"));
		const FName Traces(TEXT("Traces"));
		M.Objectives = {
			Obj(TEXT("Map"), 0, TEXT("Einsatzort auf der Karte ansehen (Wandkarte oder [M])"), SUEvent::MapViewed, NAME_None, 1, false, TEXT("Forsthaus")),
			Obj(TEXT("Camera"), 1, TEXT("Kamera einpacken"), SUEvent::ItemPicked, SUItem::Camera),
			Obj(TEXT("Flashlight"), 1, TEXT("Taschenlampe einpacken"), SUEvent::ItemPicked, SUItem::Flashlight),
			Obj(TEXT("ToCar"), 2, TEXT("Den Turm verlassen und zum Geländewagen gehen"), SUEvent::AreaEntered, SUArea::TowerParking, 1, false, SUArea::TowerParking),
			Obj(TEXT("EnterCar"), 3, TEXT("Einsteigen"), SUEvent::VehicleEntered),
			Obj(TEXT("Engine"), 4, TEXT("Motor starten [F]"), SUEvent::EngineStarted),
			Obj(TEXT("Drive"), 5, TEXT("Zum Forsthaus im nördlichen Waldgebiet fahren"), SUEvent::AreaEntered, SUArea::ForsthausGrounds, 1, false, TEXT("Forsthaus")),
			Obj(TEXT("Fuel"), 5, TEXT("Optional: an der Tankstelle Pine Ridge tanken"), SUEvent::Refueled, NAME_None, 1, true, TEXT("GasStation")),
			Obj(TEXT("EnterHouse"), 6, TEXT("Das Forsthaus betreten"), SUEvent::AreaEntered, SUArea::ForsthausInterior, 1, false, TEXT("Forsthaus")),
			Obj(TEXT("PhotoFurniture"), 7, TEXT("Beschädigte Möbel fotografieren"), SUEvent::PhotoCategory, Furniture, 2),
			Obj(TEXT("PhotoTraces"), 7, TEXT("Seltsame Spuren dokumentieren"), SUEvent::PhotoCategory, Traces, 2),
			Obj(TEXT("Clue"), 7, TEXT("Hinweise auf den Verbleib des Försters finden"), SUEvent::ItemInspected, SUItem::RangerNotebook),
			Obj(TEXT("LookAround"), 8, TEXT("Sieh dich im Haus weiter um …"), SUEvent::HorrorScripted, TEXT("M01_WindowFigure")),
			Obj(TEXT("LeaveHouse"), 9, TEXT("Das Forsthaus verlassen"), SUEvent::AreaExited, SUArea::ForsthausInterior),
			Obj(TEXT("BackToCar"), 10, TEXT("Zum Geländewagen zurückkehren"), SUEvent::VehicleEntered),
			Obj(TEXT("Return"), 11, TEXT("Zurück zum Aussichtsturm fahren"), SUEvent::AreaEntered, SUArea::TowerParking, 1, false, SUArea::TowerParking),
			Obj(TEXT("Review"), 12, TEXT("Fotos am Einsatzcomputer auswerten"), SUEvent::TerminalOpened, NAME_None, 1, false, TEXT("Tower")),
			Obj(TEXT("Report"), 13, TEXT("Einsatzbericht abschicken"), SUEvent::ReportSubmitted, TEXT("M01_LastTransmission")),
		};
		M.Evidence = {
			Ev(TEXT("EV_M01_Table"), TEXT("Umgestürzter Esstisch"), TEXT("Furniture"), 4500, true),
			Ev(TEXT("EV_M01_Cabinet"), TEXT("Zertrümmerter Schrank"), TEXT("Furniture"), 4500, true),
			Ev(TEXT("EV_M01_Chair"), TEXT("Zerbrochener Stuhl"), TEXT("Furniture"), 2000, false),
			Ev(TEXT("EV_M01_ClawMarks"), TEXT("Kratzspuren an der Wand"), TEXT("Traces"), 6000, true),
			Ev(TEXT("EV_M01_BloodTrail"), TEXT("Blutspur im Flur"), TEXT("Traces"), 5000, true),
			Ev(TEXT("EV_M01_Footprints"), TEXT("Fußabdrücke am Hintereingang"), TEXT("Traces"), 4000, false),
			Ev(TEXT("EV_M01_Notebook"), TEXT("Notizbuch des Försters"), TEXT("Clue"), 2500, false),
			Ev(TEXT("EV_M01_WindowTracks"), TEXT("Abdrücke unter dem Fenster"), TEXT("Traces"), 0, false, true, 8000),
			Ev(TEXT("EV_Observer"), TEXT("Unbekannte Gestalt"), TEXT("Creature"), 0, false, true, 15000),
		};
		M.BasePayCents = 22000;
		M.CompletionBonusCents = 6000;
		M.NextMissionId = TEXT("M02_NorthRoad");
		M.StoryStageOnAccept = 1;
		M.StoryStageOnComplete = 2;
		return M;
	}

	FSUMissionDef BuildM02()
	{
		FSUMissionDef M;
		M.Id = TEXT("M02_NorthRoad");
		M.Title = SUText(TEXT("Die Nordstraße"));
		M.Summary = SUText(TEXT("Ein verlassener Wagen mit offenen Türen auf der Nordstraße. Vorfall prüfen und Hinweise dokumentieren."));
		M.TargetSite = TEXT("M02Car");
		M.Briefing = {
			Line(TEXT("Zentrale"), TEXT("Zentrale an Beobachtungsposten 07.")),
			Line(TEXT("Zentrale"), TEXT("Wir haben eine Meldung über einen verlassenen Wagen auf der Nordstraße erhalten.")),
			Line(TEXT("Zentrale"), TEXT("Das Fahrzeug steht seit mehreren Stunden mit geöffneten Türen dort.")),
			Line(TEXT("Zentrale"), TEXT("Überprüfen Sie den Vorfall und dokumentieren Sie mögliche Hinweise. Bestätigen Sie, 07.")),
		};
		M.AcceptLines = {
			Line(TEXT("Zentrale"), TEXT("Danke, 07. Die Nordstraße ist nachts nicht beleuchtet. Fahren Sie langsam. Ende.")),
		};
		M.CompletionLines = {
			Line(TEXT("Zentrale"), TEXT("Zentrale an 07. Bericht erhalten.")),
			Line(TEXT("Zentrale"), TEXT("Der Halter des Wagens wird seit gestern Abend vermisst. Sie sind nicht der erste Posten, der so etwas meldet.")),
			Line(TEXT("Zentrale"), TEXT("Bleiben Sie in der Nähe des Turms. Weitere Einsätze folgen. Ende.")),
		};
		M.Objectives = {
			Obj(TEXT("Map"), 0, TEXT("Einsatzort auf der Karte ansehen"), SUEvent::MapViewed, NAME_None, 1, false, TEXT("M02Car")),
			Obj(TEXT("Find"), 1, TEXT("Den verlassenen Wagen auf der Nordstraße finden"), SUEvent::AreaEntered, SUArea::M02Site, 1, false, TEXT("M02Car")),
			Obj(TEXT("PhotoCar"), 2, TEXT("Den Wagen fotografieren"), SUEvent::PhotoEvidence, TEXT("EV_M02_Car")),
			Obj(TEXT("PhotoTraces"), 2, TEXT("Spuren dokumentieren"), SUEvent::PhotoCategory, TEXT("Traces"), 2),
			Obj(TEXT("Phone"), 2, TEXT("Das zurückgelassene Telefon untersuchen"), SUEvent::ItemInspected, SUItem::M02Phone),
			Obj(TEXT("Return"), 3, TEXT("Zurück zum Aussichtsturm fahren"), SUEvent::AreaEntered, SUArea::TowerParking, 1, false, SUArea::TowerParking),
			Obj(TEXT("Review"), 4, TEXT("Fotos am Einsatzcomputer auswerten"), SUEvent::TerminalOpened, NAME_None, 1, false, TEXT("Tower")),
			Obj(TEXT("Report"), 5, TEXT("Einsatzbericht abschicken"), SUEvent::ReportSubmitted, TEXT("M02_NorthRoad")),
		};
		M.Evidence = {
			Ev(TEXT("EV_M02_Car"), TEXT("Verlassener Wagen"), TEXT("Vehicle"), 5000, true),
			Ev(TEXT("EV_M02_BloodSeat"), TEXT("Blut am Fahrersitz"), TEXT("Traces"), 5000, true),
			Ev(TEXT("EV_M02_DragMarks"), TEXT("Schleifspuren in den Wald"), TEXT("Traces"), 5000, true),
			Ev(TEXT("EV_M02_Handprint"), TEXT("Handabdruck an der Scheibe"), TEXT("Traces"), 3500, false),
			Ev(TEXT("EV_M02_Phone"), TEXT("Zurückgelassenes Telefon"), TEXT("Clue"), 2500, false),
			Ev(TEXT("EV_Observer"), TEXT("Unbekannte Gestalt"), TEXT("Creature"), 0, false, true, 15000),
			Ev(TEXT("EV_Hunter"), TEXT("Der Jäger"), TEXT("Creature"), 0, false, true, 20000),
		};
		M.BasePayCents = 26000;
		M.CompletionBonusCents = 7000;
		M.StoryStageOnAccept = 3;
		M.StoryStageOnComplete = 4;
		return M;
	}

	TArray<FSUMissionDef> BuildAll()
	{
		TArray<FSUMissionDef> Missions;
		const USUGameplaySettings* Settings = GetDefault<USUGameplaySettings>();
		if (!Settings->MissionTable.IsNull())
		{
			if (UDataTable* Table = Settings->MissionTable.LoadSynchronous())
			{
				TArray<FSUMissionDef*> Rows;
				Table->GetAllRows<FSUMissionDef>(TEXT("SUMissionDatabase"), Rows);
				for (const FSUMissionDef* Row : Rows)
				{
					if (Row)
					{
						Missions.Add(*Row);
					}
				}
				UE_LOG(LogSignalUnknown, Log, TEXT("Loaded %d missions from %s"), Missions.Num(), *Table->GetName());
			}
		}
		if (Missions.Num() == 0)
		{
			Missions.Add(BuildM01());
			Missions.Add(BuildM02());
		}
		return Missions;
	}
}

const TArray<FSUMissionDef>& FSUMissionDatabase::All()
{
	static const TArray<FSUMissionDef> Missions = BuildAll();
	return Missions;
}

const FSUMissionDef* FSUMissionDatabase::Find(FName Id)
{
	for (const FSUMissionDef& M : All())
	{
		if (M.Id == Id)
		{
			return &M;
		}
	}
	return nullptr;
}

const TArray<FSUSpeechLine>& FSUMissionDatabase::Chatter()
{
	static const TArray<FSUSpeechLine> Lines = {
		Line(TEXT("Posten 12"), TEXT("Posten 12 an Zentrale, Rauch im Sektor Ost, vermutlich ein Lagerfeuer. Wir behalten es im Auge.")),
		Line(TEXT("Zentrale"), TEXT("Zentrale an alle Posten: Für die Nacht sind Gewitter möglich. Bleiben Sie erreichbar.")),
		Line(TEXT("Zentrale"), TEXT("Zentrale an Posten 03, Funkcheck. … Posten 03, hören Sie mich?")),
		Line(TEXT("Streife 4"), TEXT("Hier Streife 4. Die Sperre an der Ostbrücke steht. Niemand kommt durch.")),
		Line(TEXT("Zentrale"), TEXT("Zentrale an Posten 09, bitte melden. … Posten 09, bitte melden.")),
		Line(TEXT("Posten 05"), TEXT("Posten 05. Wir hören seit einer Stunde Klopfen am Turm. Ist jemand aus dem Tal unterwegs?")),
	};
	return Lines;
}

const TArray<FSUSpeechLine>& FSUMissionDatabase::StrangeTransmissions()
{
	static const TArray<FSUSpeechLine> Lines = {
		Line(TEXT("???"), TEXT("Fahren Sie nicht auf die Nordstraße."), 3.5f),
		Line(TEXT("???"), TEXT("… ich … bin … hier … irgend… Keller … hört … mich …?"), 5.f),
		Line(TEXT("???"), TEXT("Es steht wieder am Straßenrand."), 3.f),
		Line(TEXT("???"), TEXT("Zählen Sie Ihre Leute, Posten 07."), 3.f),
		Line(TEXT("???"), TEXT("… Hallberg an Zentrale … nicht … rausgehen …"), 4.f),
	};
	return Lines;
}

const TArray<FSUSpeechLine>& FSUMissionDatabase::EndOfContentLines()
{
	static const TArray<FSUSpeechLine> Lines = {
		Line(TEXT("Zentrale"), TEXT("Zentrale an alle Posten. Wir verlieren den Kontakt zu immer mehr Stationen im Norden. Halten Sie Ihre Position.")),
	};
	return Lines;
}

TArray<FString> FSUMissionDatabase::Validate()
{
	TArray<FString> Errors;
	TSet<FName> Ids;
	for (const FSUMissionDef& M : All())
	{
		if (M.Id.IsNone())
		{
			Errors.Add(TEXT("Mission without id"));
			continue;
		}
		if (Ids.Contains(M.Id))
		{
			Errors.Add(FString::Printf(TEXT("Duplicate mission %s"), *M.Id.ToString()));
		}
		Ids.Add(M.Id);
		if (M.Briefing.Num() == 0)
		{
			Errors.Add(FString::Printf(TEXT("%s: no briefing"), *M.Id.ToString()));
		}
		bool bHasReport = false;
		TSet<FName> ObjectiveIds;
		int32 LastStage = -1;
		for (const FSUObjectiveDef& O : M.Objectives)
		{
			if (ObjectiveIds.Contains(O.Id))
			{
				Errors.Add(FString::Printf(TEXT("%s: duplicate objective %s"), *M.Id.ToString(), *O.Id.ToString()));
			}
			ObjectiveIds.Add(O.Id);
			if (O.EventName.IsNone())
			{
				Errors.Add(FString::Printf(TEXT("%s: objective %s without event"), *M.Id.ToString(), *O.Id.ToString()));
			}
			if (O.Stage < LastStage)
			{
				Errors.Add(FString::Printf(TEXT("%s: objective %s out of stage order"), *M.Id.ToString(), *O.Id.ToString()));
			}
			LastStage = FMath::Max(LastStage, O.Stage);
			if (O.EventName == SUEvent::ReportSubmitted)
			{
				bHasReport = true;
				if (O.EventParam != M.Id)
				{
					Errors.Add(FString::Printf(TEXT("%s: report objective must use the mission id"), *M.Id.ToString()));
				}
			}
			if (O.EventName == SUEvent::ItemPicked && !FSUItemDatabase::Find(O.EventParam))
			{
				Errors.Add(FString::Printf(TEXT("%s: unknown item %s"), *M.Id.ToString(), *O.EventParam.ToString()));
			}
			if (O.EventName == SUEvent::PhotoCategory)
			{
				int32 Available = 0;
				for (const FSUEvidenceRequirement& E : M.Evidence)
				{
					Available += E.Category == O.EventParam ? 1 : 0;
				}
				if (Available < O.Required)
				{
					Errors.Add(FString::Printf(TEXT("%s: not enough evidence of category %s"), *M.Id.ToString(), *O.EventParam.ToString()));
				}
			}
		}
		if (!bHasReport)
		{
			Errors.Add(FString::Printf(TEXT("%s: no report objective"), *M.Id.ToString()));
		}
		if (!M.NextMissionId.IsNone() && !Find(M.NextMissionId))
		{
			Errors.Add(FString::Printf(TEXT("%s: unknown next mission %s"), *M.Id.ToString(), *M.NextMissionId.ToString()));
		}
	}
	return Errors;
}
