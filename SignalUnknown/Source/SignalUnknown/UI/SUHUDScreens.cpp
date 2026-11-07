// SIGNAL UNKNOWN – HUD screens: inventory, terminal, shop, document, gallery, trunk.
#include "UI/SUHUD.h"
#include "UI/SUHUDStyle.h"
#include "SignalUnknown.h"
#include "Core/SUPlayerController.h"
#include "Core/SUPlayerState.h"
#include "Core/SUGameState.h"
#include "Core/SUItemDatabase.h"
#include "Player/SUCharacter.h"
#include "Player/SUInventoryComponent.h"
#include "Vehicle/SUVehicle.h"
#include "GasStation/SUShopCounter.h"
#include "Interaction/SUDocument.h"
#include "Mission/SUMissionManager.h"
#include "Mission/SUMissionDatabase.h"
#include "Evidence/SUPhotoStorage.h"
#include "Evidence/SUPhotoTypes.h"
#include "Logic/SUEvidenceLogic.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"

namespace
{

	FString QualityText(float Q)
	{
		return UTF8_TO_TCHAR(SULogic::QualityLabel(Q));
	}

	FString PhotoTitle(const FSUPhotoMeta& Meta)
	{
		return FString::Printf(TEXT("%s  %s"), *Meta.TakenAt.ToString(TEXT("%d.%m. %H:%M")), *SUFormatTime(Meta.DayTimeHours));
	}
}

// ====================================================================================== inventory

void ASUHUD::DrawInventory()
{
	ASUCharacter* Char = GetChar();
	USUInventoryComponent* Inv = Char ? Char->GetInventory() : nullptr;
	if (!Inv)
	{
		return;
	}
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	DrawScreenTitle(TEXT("Inventar"), TEXT("Klick: auswählen · [Esc]/[Tab] schließen"));
	const TArray<FSUInventorySlot>& Slots = Inv->GetSlots();
	const float X = S(60.f);
	const float Y0 = S(100.f);
	const float Cell = S(118.f);
	const int32 Cols = 4;
	for (int32 I = 0; I < Inv->MaxSlots; ++I)
	{
		const float CX = X + (I % Cols) * (Cell + S(10.f));
		const float CY = Y0 + (I / Cols) * (Cell + S(10.f));
		if (Slots.IsValidIndex(I) && Slots[I].Count > 0)
		{
			const FSUInventorySlot& Slot = Slots[I];
			const FSUItemDef* Def = FSUItemDatabase::Find(Slot.ItemId);
			const bool bSel = SelectedIndex == I;
			const int32 Index = I;
			Button(TEXT(""), CX, CY, Cell, Cell, [this, Index]() { SelectedIndex = Index; });
			if (bSel)
			{
				Frame(CX - 2.f, CY - 2.f, Cell + 4.f, Cell + 4.f, SUHUDStyle::Accent, 2.f);
			}
			if (Def)
			{
				Rect(CX + Cell * 0.5f - S(18.f), CY + S(18.f), S(36.f), S(36.f), Def->Color);
				TextBlock(Def->Name.ToString(), CX + S(8.f), CY + S(62.f), Cell - S(16.f), 13.f, SUHUDStyle::Text, false, 2);
			}
			if (Slot.Count > 1)
			{
				Text(FString::Printf(TEXT("×%d"), Slot.Count), CX + Cell - S(8.f), CY + S(6.f), 14.f, SUHUDStyle::Text, 2, true);
			}
			if (Def && Def->bHasBattery)
			{
				Text(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Slot.Charge * 100.f)), CX + S(8.f), CY + S(6.f), 12.f, Slot.Charge < 0.15f ? SUHUDStyle::Bad : SUHUDStyle::Dim);
			}
		}
		else
		{
			Rect(CX, CY, Cell, Cell, FLinearColor(0.08f, 0.08f, 0.08f, 0.6f));
			Frame(CX, CY, Cell, Cell, FLinearColor(0.25f, 0.25f, 0.25f, 0.6f));
		}
	}
	// details of the selected item
	const float DX = X + Cols * (Cell + S(10.f)) + S(40.f);
	const float DW = FMath::Max(S(300.f), W() - DX - S(60.f));
	if (Slots.IsValidIndex(SelectedIndex) && Slots[SelectedIndex].Count > 0)
	{
		const FSUInventorySlot Slot = Slots[SelectedIndex];
		const FSUItemDef* Def = FSUItemDatabase::Find(Slot.ItemId);
		if (Def)
		{
			Panel(DX, Y0, DW, S(360.f));
			Text(Def->Name.ToString(), DX + S(20.f), Y0 + S(18.f), 22.f, SUHUDStyle::Accent, 0, true);
			float DY = Y0 + S(58.f);
			DY += TextBlock(Def->Description.ToString(), DX + S(20.f), DY, DW - S(40.f), 15.f, SUHUDStyle::Text) + S(16.f);
			if (Def->bQuestItem)
			{
				Text(TEXT("Beweisstück – kann nicht abgelegt werden"), DX + S(20.f), DY, 14.f, SUHUDStyle::Dim);
				DY += S(30.f);
			}
			const FName Id = Slot.ItemId;
			const bool bUsable = Def->bConsumable;
			if (bUsable)
			{
				Button(Id == SUItem::Battery ? TEXT("Batterien einlegen") : TEXT("Benutzen"), DX + S(20.f), DY, S(220.f), S(42.f), [Char, Id]() { Char->ServerUseItem(Id); });
			}
			if (Id == SUItem::Camera)
			{
				Text(TEXT("[C] ausrüsten, [RMT] Sucher"), DX + S(20.f), DY + S(10.f), 14.f, SUHUDStyle::Dim);
			}
			if (!Def->bQuestItem)
			{
				Button(TEXT("Ablegen"), DX + S(260.f), DY, S(160.f), S(42.f), [this, Char, Id]() { Char->ServerDropItem(Id); SelectedIndex = INDEX_NONE; }, !Char->IsSeated());
			}
		}
	}
	else
	{
		Text(TEXT("Gegenstand auswählen"), DX, Y0 + S(10.f), 16.f, SUHUDStyle::Dim);
	}
}

// ====================================================================================== terminal

void ASUHUD::DrawTerminal()
{
	ASUPlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	// monitor look
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.01f, 0.025f, 0.015f, 0.97f));
	for (float SY = 0.f; SY < H(); SY += S(4.f))
	{
		Rect(0.f, SY, W(), 1.f, FLinearColor(0.f, 0.f, 0.f, 0.18f));
	}
	const ASUGameState* GS = ASUGameState::Get(this);
	Text(TEXT("FORSTDIENST NORDWALD · EINSATZRECHNER POSTEN 07"), S(60.f), S(36.f), 20.f, SUHUDStyle::Terminal, 0, true, false, true);
	if (GS)
	{
		Text(FString::Printf(TEXT("TAG %d  %s   KONTO %s"), GS->GetDay(), *SUFormatTime(GS->GetTimeOfDay()), *SUFormatMoney(GS->GetMoneyCents())), W() - S(60.f), S(36.f), 16.f, SUHUDStyle::Terminal, 2, false, false, true);
	}
	if (PC->HasReportToShow())
	{
		bShowReportResult = true;
	}
	const float X = S(60.f);
	const float Y = S(130.f);
	const float Width = W() - S(120.f);
	const float Height = H() - S(200.f);
	if (bShowReportResult)
	{
		DrawReportResult(X, Y, Width, Height);
		return;
	}
	static const TCHAR* Tabs[4] = {TEXT("[1] EINSATZ"), TEXT("[2] FOTOS & BERICHT"), TEXT("[3] KONTO"), TEXT("[4] ARCHIV")};
	float TX = X;
	for (int32 I = 0; I < 4; ++I)
	{
		const int32 TabIndex = I;
		const FVector2D Sz = Measure(Tabs[I], Font(16.f, false, true));
		Button(Tabs[I], TX, S(76.f), Sz.X + S(30.f), S(34.f), [this, TabIndex]() { Tab = TabIndex; Scroll = 0.f; FocusIndex = 0; }, true, 15.f);
		if (I == Tab)
		{
			Rect(TX, S(110.f), Sz.X + S(30.f), 2.f, SUHUDStyle::Terminal);
		}
		TX += Sz.X + S(44.f);
	}
	Text(TEXT("[Q]/[E] Reiter · [Esc] abmelden"), W() - S(60.f), H() - S(44.f), 13.f, SUHUDStyle::TerminalDim, 2, false, false, true);
	switch (Tab)
	{
	case 0: DrawTerminalMission(X, Y, Width, Height); break;
	case 1: DrawTerminalReport(X, Y, Width, Height); break;
	case 2: DrawTerminalAccount(X, Y, Width, Height); break;
	default: DrawTerminalArchive(X, Y, Width, Height); break;
	}
}

void ASUHUD::DrawTerminalMission(float X, float Y, float Width, float Height)
{
	const ASUMissionManager* MM = ASUMissionManager::Get(this);
	const FSUMissionDef* Def = MM ? MM->GetActiveDef() : nullptr;
	if (!MM || !Def || MM->GetPhase() == ESUMissionPhase::None || MM->GetPhase() == ESUMissionPhase::Completed)
	{
		Text(TEXT("> KEIN AKTIVER EINSATZ."), X, Y, 18.f, SUHUDStyle::Terminal, 0, false, false, true);
		Text(TEXT("> Neue Aufträge kommen über das Funkgerät der Zentrale."), X, Y + S(30.f), 16.f, SUHUDStyle::TerminalDim, 0, false, false, true);
		return;
	}
	float CY = Y;
	Text(FString::Printf(TEXT("> EINSATZ: %s"), *Def->Title.ToString().ToUpper()), X, CY, 20.f, SUHUDStyle::Terminal, 0, true, false, true);
	CY += S(36.f);
	CY += TextBlock(Def->Summary.ToString(), X, CY, Width * 0.6f, 16.f, SUHUDStyle::Terminal, true) + S(20.f);
	if (MM->GetPhase() == ESUMissionPhase::Incoming || MM->GetPhase() == ESUMissionPhase::Briefing || MM->GetPhase() == ESUMissionPhase::Offered)
	{
		Text(TEXT("> Auftrag noch nicht bestätigt – am Funkgerät annehmen."), X, CY, 16.f, SUHUDStyle::Accent, 0, false, false, true);
		CY += S(32.f);
	}
	Text(TEXT("> AUFGABEN"), X, CY, 17.f, SUHUDStyle::Terminal, 0, true, false, true);
	CY += S(28.f);
	const TArray<FSUObjectiveState>& States = MM->GetObjectiveStates();
	const int32 Stage = MM->GetCurrentStage();
	for (int32 I = 0; I < Def->Objectives.Num(); ++I)
	{
		const FSUObjectiveDef& O = Def->Objectives[I];
		if (O.bHidden || O.Stage > Stage)
		{
			continue;
		}
		const bool bDone = States.IsValidIndex(I) && States[I].bDone;
		Text(FString::Printf(TEXT("%s %s%s"), bDone ? TEXT("[x]") : TEXT("[ ]"), *O.Text.ToString(), O.bOptional ? TEXT(" (optional)") : TEXT("")), X + S(10.f), CY, 15.f, bDone ? SUHUDStyle::TerminalDim : SUHUDStyle::Terminal, 0, false, false, true);
		CY += S(24.f);
	}
	// evidence wanted by the Zentrale
	const float EX = X + Width * 0.64f;
	float EY = Y;
	Text(TEXT("> GEFORDERTE NACHWEISE"), EX, EY, 17.f, SUHUDStyle::Terminal, 0, true, false, true);
	EY += S(30.f);
	for (const FSUEvidenceRequirement& R : Def->Evidence)
	{
		if (R.bRare && !MM->IsEvidenceDocumented(R.EvidenceId))
		{
			continue; // rare evidence is a surprise
		}
		const bool bDoc = MM->IsEvidenceDocumented(R.EvidenceId);
		Text(FString::Printf(TEXT("%s %s%s"), bDoc ? TEXT("[x]") : TEXT("[ ]"), *R.DisplayName.ToString(), R.bRequired ? TEXT(" *") : TEXT("")), EX, EY, 14.f, bDoc ? SUHUDStyle::TerminalDim : SUHUDStyle::Terminal, 0, false, false, true);
		EY += S(22.f);
	}
	Text(TEXT("* Pflicht. Grundvergütung:"), EX, EY + S(10.f), 13.f, SUHUDStyle::TerminalDim, 0, false, false, true);
	Text(SUFormatMoney(Def->BasePayCents), EX + S(270.f), EY + S(10.f), 13.f, SUHUDStyle::Terminal, 0, false, false, true);
}

void ASUHUD::DrawTerminalReport(float X, float Y, float Width, float Height)
{
	ASUPlayerController* PC = GetPC();
	const ASUPlayerState* PS = PC ? PC->GetPlayerState<ASUPlayerState>() : nullptr;
	const ASUMissionManager* MM = ASUMissionManager::Get(this);
	USUPhotoStorage* Storage = USUPhotoStorage::Get(this);
	if (!PS || !MM)
	{
		return;
	}
	TArray<const FSUPhotoMeta*> Photos;
	for (const FSUPhotoMeta& M : PS->GetPhotos())
	{
		if (!M.bSubmitted)
		{
			Photos.Add(&M);
		}
	}
	// photo list with thumbnails (left)
	const float ListW = Width * 0.55f;
	const float RowH = S(92.f);
	const int32 Visible = FMath::Max(1, FMath::FloorToInt((Height - S(60.f)) / RowH));
	const int32 First = FMath::Clamp(FMath::FloorToInt(Scroll / RowH), 0, FMath::Max(0, Photos.Num() - Visible));
	Text(FString::Printf(TEXT("> %d NEUE FOTOS AUF DER KAMERA – für den Bericht auswählen"), Photos.Num()), X, Y, 16.f, SUHUDStyle::Terminal, 0, true, false, true);
	if (Photos.Num() == 0)
	{
		Text(TEXT("> Keine neuen Fotos. Mit [C] die Kamera ausrüsten, [RMT] Sucher, [LMT] auslösen."), X, Y + S(36.f), 15.f, SUHUDStyle::TerminalDim, 0, false, false, true);
	}
	float CY = Y + S(36.f);
	for (int32 I = First; I < Photos.Num() && I < First + Visible; ++I)
	{
		const FSUPhotoMeta& M = *Photos[I];
		const FGuid Id = M.PhotoId;
		const bool bSel = ReportSelection.Contains(Id);
		Button(TEXT(""), X, CY, ListW, RowH - S(8.f), [this, Id]()
		{
			if (ReportSelection.Contains(Id))
			{
				ReportSelection.Remove(Id);
			}
			else
			{
				ReportSelection.Add(Id);
			}
		});
		UTexture2D* Tex = Storage ? Storage->GetTexture(Id) : nullptr;
		Image(Tex, X + S(6.f), CY + S(6.f), S(128.f), S(72.f));
		if (!Tex)
		{
			Text(TEXT("Bild bei Teammitglied"), X + S(70.f), CY + S(34.f), 10.f, SUHUDStyle::Dim, 1);
		}
		Text(bSel ? TEXT("[x]") : TEXT("[ ]"), X + S(146.f), CY + S(8.f), 16.f, bSel ? SUHUDStyle::Accent : SUHUDStyle::Terminal, 0, true, false, true);
		Text(PhotoTitle(M), X + S(190.f), CY + S(8.f), 14.f, SUHUDStyle::Terminal, 0, false, false, true);
		FString Hits;
		for (const FSUEvidenceHit& Hit : M.Hits)
		{
			if (Hit.Quality >= SULogic::MinEvidenceQuality)
			{
				Hits += FString::Printf(TEXT("%s (%s)  "), *Hit.DisplayName.ToString(), *QualityText(Hit.Quality));
			}
		}
		if (Hits.IsEmpty())
		{
			Hits = M.bBlurred ? TEXT("verwackelt – nichts erkennbar") : TEXT("kein Beweis erkennbar");
		}
		TextBlock(Hits, X + S(190.f), CY + S(32.f), ListW - S(200.f), 12.f, M.Hits.Num() > 0 ? SUHUDStyle::Terminal : SUHUDStyle::TerminalDim, true, 2);
		CY += RowH;
	}
	if (Photos.Num() > Visible)
	{
		Text(FString::Printf(TEXT("Mausrad: blättern (%d–%d von %d)"), First + 1, FMath::Min(Photos.Num(), First + Visible), Photos.Num()), X, Y + Height - S(20.f), 12.f, SUHUDStyle::TerminalDim, 0, false, false, true);
	}

	// report preview (right)
	const float RX = X + ListW + S(40.f);
	const float RW = Width - ListW - S(40.f);
	Text(TEXT("> BERICHT – VORSCHAU"), RX, Y, 16.f, SUHUDStyle::Terminal, 0, true, false, true);
	TArray<FSUPhotoMeta> Selected;
	for (const FSUPhotoMeta* M : Photos)
	{
		if (ReportSelection.Contains(M->PhotoId))
		{
			Selected.Add(*M);
		}
	}
	const FSUReportResult Preview = MM->PreviewReport(Selected);
	float PY = Y + S(36.f);
	if (!MM->IsWaitingForReport())
	{
		PY += TextBlock(TEXT("Der aktuelle Einsatz ist noch nicht bereit für den Bericht. Fotos können trotzdem schon gesichtet werden."), RX, PY, RW, 14.f, SUHUDStyle::Accent, true) + S(10.f);
	}
	for (const FSUReportLine& L : Preview.Lines)
	{
		Text(L.Label.ToString(), RX, PY, 14.f, SUHUDStyle::Terminal, 0, false, false, true);
		Text(SUFormatMoney(L.AmountCents), RX + RW, PY, 14.f, SUHUDStyle::Terminal, 2, false, false, true);
		PY += S(22.f);
	}
	Rect(RX, PY + S(4.f), RW, 1.f, SUHUDStyle::TerminalDim);
	PY += S(12.f);
	Text(TEXT("VORAUSSICHTLICH"), RX, PY, 16.f, SUHUDStyle::Terminal, 0, true, false, true);
	Text(SUFormatMoney(Preview.TotalCents), RX + RW, PY, 16.f, SUHUDStyle::Accent, 2, true, false, true);
	PY += S(30.f);
	if (!Preview.Rating.IsEmpty())
	{
		Text(FString::Printf(TEXT("Bewertung: %s"), *Preview.Rating.ToString()), RX, PY, 14.f, SUHUDStyle::Terminal, 0, false, false, true);
		PY += S(26.f);
	}
	if (Preview.Missing.Num() > 0)
	{
		Text(TEXT("Es fehlen noch:"), RX, PY, 14.f, SUHUDStyle::Accent, 0, false, false, true);
		PY += S(22.f);
		for (const FText& Missing : Preview.Missing)
		{
			Text(FString::Printf(TEXT("- %s"), *Missing.ToString()), RX + S(10.f), PY, 13.f, SUHUDStyle::Accent, 0, false, false, true);
			PY += S(20.f);
		}
	}
	PY += S(10.f);
	TextBlock(TEXT("Fotos der Teammitglieder werden automatisch mitgeschickt. Doppelte Aufnahmen desselben Beweises zählen einmal (beste Qualität)."), RX, PY, RW, 12.f, SUHUDStyle::TerminalDim, true, 3);
	const TArray<FGuid> Ids = ReportSelection.Array();
	Button(FString::Printf(TEXT("BERICHT ABSCHICKEN (%d Fotos)"), Ids.Num()), RX, Y + Height - S(60.f), RW, S(48.f), [PC, Ids]()
	{
		PC->ServerSubmitReport(Ids);
	}, MM->IsWaitingForReport());
}

void ASUHUD::DrawReportResult(float X, float Y, float Width, float Height)
{
	ASUPlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	const FSUReportResult& R = PC->GetLastReport();
	Text(R.bAccepted ? TEXT("> BERICHT ANGENOMMEN") : TEXT("> BERICHT ABGELEHNT"), X, Y, 22.f, R.bAccepted ? SUHUDStyle::Terminal : SUHUDStyle::Bad, 0, true, false, true);
	float CY = Y + S(40.f);
	if (!R.Message.IsEmpty())
	{
		CY += TextBlock(R.Message.ToString(), X, CY, Width * 0.6f, 15.f, SUHUDStyle::Terminal, true) + S(10.f);
	}
	const float LW = FMath::Min(Width * 0.6f, S(760.f));
	for (const FSUReportLine& L : R.Lines)
	{
		Text(L.Label.ToString(), X, CY, 15.f, SUHUDStyle::Terminal, 0, false, false, true);
		Text(SUFormatMoney(L.AmountCents), X + LW, CY, 15.f, SUHUDStyle::Terminal, 2, false, false, true);
		CY += S(24.f);
	}
	Rect(X, CY + S(4.f), LW, 1.f, SUHUDStyle::TerminalDim);
	CY += S(14.f);
	Text(TEXT("AUSGEZAHLT"), X, CY, 18.f, SUHUDStyle::Terminal, 0, true, false, true);
	Text(SUFormatMoney(R.TotalCents), X + LW, CY, 18.f, SUHUDStyle::Accent, 2, true, false, true);
	CY += S(34.f);
	if (!R.Rating.IsEmpty())
	{
		Text(FString::Printf(TEXT("Bewertung der Zentrale: %s"), *R.Rating.ToString()), X, CY, 16.f, SUHUDStyle::Terminal, 0, false, false, true);
		CY += S(28.f);
	}
	for (const FText& Missing : R.Missing)
	{
		Text(FString::Printf(TEXT("Nicht dokumentiert: %s"), *Missing.ToString()), X, CY, 14.f, SUHUDStyle::Accent, 0, false, false, true);
		CY += S(22.f);
	}
	Button(TEXT("OK"), X, Y + Height - S(60.f), S(200.f), S(46.f), [this, PC]()
	{
		bShowReportResult = false;
		PC->ClearReportToShow();
		ReportSelection.Reset();
	});
}

void ASUHUD::DrawTerminalAccount(float X, float Y, float Width, float Height)
{
	const ASUGameState* GS = ASUGameState::Get(this);
	if (!GS)
	{
		return;
	}
	Text(FString::Printf(TEXT("> TEAMKONTO: %s"), *SUFormatMoney(GS->GetMoneyCents())), X, Y, 20.f, SUHUDStyle::Terminal, 0, true, false, true);
	Text(TEXT("> LETZTE BUCHUNGEN"), X, Y + S(44.f), 16.f, SUHUDStyle::Terminal, 0, true, false, true);
	const TArray<FSUTransaction>& T = GS->GetTransactions();
	float CY = Y + S(76.f);
	const float LW = FMath::Min(Width, S(820.f));
	for (int32 I = T.Num() - 1; I >= 0 && CY < Y + Height - S(30.f); --I)
	{
		Text(SUFormatTime(T[I].DayTimeHours), X, CY, 14.f, SUHUDStyle::TerminalDim, 0, false, false, true);
		Text(T[I].Label.ToString(), X + S(90.f), CY, 14.f, SUHUDStyle::Terminal, 0, false, false, true);
		Text(SUFormatMoney(T[I].AmountCents), X + LW, CY, 14.f, T[I].AmountCents < 0 ? SUHUDStyle::Accent : SUHUDStyle::Terminal, 2, false, false, true);
		CY += S(22.f);
	}
	if (T.Num() == 0)
	{
		Text(TEXT("> Keine Buchungen."), X, CY, 14.f, SUHUDStyle::TerminalDim, 0, false, false, true);
	}
}

void ASUHUD::DrawTerminalArchive(float X, float Y, float Width, float Height)
{
	const ASUMissionManager* MM = ASUMissionManager::Get(this);
	const ASUGameState* GS = ASUGameState::Get(this);
	Text(TEXT("> ABGESCHLOSSENE EINSÄTZE"), X, Y, 16.f, SUHUDStyle::Terminal, 0, true, false, true);
	float CY = Y + S(32.f);
	if (MM)
	{
		for (const FName& Id : MM->GetCompletedMissions())
		{
			const FSUMissionDef* Def = FSUMissionDatabase::Find(Id);
			Text(FString::Printf(TEXT("[x] %s"), Def ? *Def->Title.ToString() : *Id.ToString()), X + S(10.f), CY, 15.f, SUHUDStyle::Terminal, 0, false, false, true);
			CY += S(24.f);
		}
		if (MM->GetCompletedMissions().Num() == 0)
		{
			Text(TEXT("> Noch keine."), X + S(10.f), CY, 15.f, SUHUDStyle::TerminalDim, 0, false, false, true);
			CY += S(24.f);
		}
	}
	CY += S(20.f);
	if (GS && GS->GetUnpaidFuelIncidents() > 0)
	{
		Text(FString::Printf(TEXT("> VERMERK: %d x Tankstelle ohne Bezahlung verlassen."), GS->GetUnpaidFuelIncidents()), X, CY, 15.f, SUHUDStyle::Accent, 0, false, false, true);
		CY += S(26.f);
	}
	Text(TEXT("> Interne Notiz: Meldungen über verschwundene Personen im Nordwald häufen sich."), X, CY, 14.f, SUHUDStyle::TerminalDim, 0, false, false, true);
}

// ====================================================================================== shop

void ASUHUD::DrawShop()
{
	ASUPlayerController* PC = GetPC();
	ASUShopCounter* Counter = PC ? Cast<ASUShopCounter>(PC->GetScreenContext()) : nullptr;
	const ASUGameState* GS = ASUGameState::Get(this);
	if (!Counter || !GS)
	{
		return;
	}
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.f, 0.f, 0.f, 0.65f));
	DrawScreenTitle(TEXT("Pine Ridge Gas & Mart – Kasse"), TEXT("[Esc] schließen"));
	Text(FString::Printf(TEXT("Teamkonto: %s"), *SUFormatMoney(GS->GetMoneyCents())), W() - S(60.f), S(48.f), 18.f, SUHUDStyle::Text, 2, true);
	const float X = S(60.f);
	float Y = S(110.f);
	const float RowW = FMath::Min(S(720.f), W() * 0.5f);
	Text(TEXT("Waren"), X, Y, 18.f, SUHUDStyle::Accent, 0, true);
	Y += S(34.f);
	for (const FName& Id : ASUShopCounter::GetShopItems())
	{
		const FSUItemDef* Def = FSUItemDatabase::Find(Id);
		if (!Def)
		{
			continue;
		}
		const bool bAfford = GS->GetMoneyCents() >= Def->PriceCents;
		Text(Def->Name.ToString(), X, Y + S(8.f), 16.f, SUHUDStyle::Text);
		Text(SUFormatMoney(Def->PriceCents), X + RowW - S(170.f), Y + S(8.f), 16.f, bAfford ? SUHUDStyle::Text : SUHUDStyle::Bad, 2);
		Button(TEXT("Kaufen"), X + RowW - S(150.f), Y, S(150.f), S(38.f), [PC, Counter, Id]() { PC->ServerShopBuy(Counter, Id); }, bAfford, 15.f);
		Y += S(46.f);
	}
	// fuel and repair
	const float FX = X + RowW + S(60.f);
	float FY = S(110.f);
	Text(TEXT("Tankrechnung"), FX, FY, 18.f, SUHUDStyle::Accent, 0, true);
	FY += S(34.f);
	const int32 Due = Counter->GetOpenFuelCents();
	if (Due > 0)
	{
		Text(FString::Printf(TEXT("%.1f l Super – %s"), Counter->GetOpenFuelLiters(), *SUFormatMoney(Due)), FX, FY, 16.f, SUHUDStyle::Text);
		FY += S(30.f);
		Button(TEXT("Benzin bezahlen"), FX, FY, S(260.f), S(42.f), [PC, Counter]() { PC->ServerShopPayFuel(Counter); }, GS->GetMoneyCents() >= Due);
		FY += S(56.f);
	}
	else
	{
		Text(TEXT("Keine offene Rechnung."), FX, FY, 15.f, SUHUDStyle::Dim);
		FY += S(40.f);
	}
	Text(TEXT("Werkstatt"), FX, FY, 18.f, SUHUDStyle::Accent, 0, true);
	FY += S(34.f);
	const ASUVehicle* V = Counter->GetRepairableVehicle();
	if (!V)
	{
		TextBlock(TEXT("Den Wagen auf den Hof der Tankstelle fahren, um ihn reparieren zu lassen."), FX, FY, S(420.f), 15.f, SUHUDStyle::Dim);
	}
	else
	{
		const int32 Cost = Counter->GetRepairCostCents(V);
		Text(FString::Printf(TEXT("Motorzustand: %d %%"), FMath::RoundToInt(V->GetEngineHealth())), FX, FY, 16.f, SUHUDStyle::Text);
		FY += S(30.f);
		if (Cost > 0)
		{
			Button(FString::Printf(TEXT("Reparieren lassen (%s)"), *SUFormatMoney(Cost)), FX, FY, S(320.f), S(42.f), [PC, Counter]() { PC->ServerShopRepair(Counter); }, GS->GetMoneyCents() >= Cost);
		}
		else
		{
			Text(TEXT("Der Wagen ist in Ordnung."), FX, FY, 15.f, SUHUDStyle::Dim);
		}
	}
}

// ====================================================================================== document

void ASUHUD::DrawDocument()
{
	ASUPlayerController* PC = GetPC();
	const ASUDocument* Doc = PC ? Cast<ASUDocument>(PC->GetScreenContext()) : nullptr;
	if (!Doc)
	{
		return;
	}
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.f, 0.f, 0.f, 0.7f));
	const bool bPhone = Doc->Look == ESUDocumentLook::Phone;
	const float PW = bPhone ? S(420.f) : FMath::Min(S(760.f), W() - S(120.f));
	const float PH = H() - S(140.f);
	const float PX = W() * 0.5f - PW * 0.5f;
	const float PY = S(70.f);
	FLinearColor Paper = FLinearColor(0.86f, 0.82f, 0.72f, 1.f);
	FLinearColor Ink = FLinearColor(0.12f, 0.1f, 0.08f, 1.f);
	if (bPhone)
	{
		Paper = FLinearColor(0.04f, 0.05f, 0.07f, 1.f);
		Ink = FLinearColor(0.85f, 0.88f, 0.92f, 1.f);
	}
	else if (Doc->Look == ESUDocumentLook::Board)
	{
		Paper = FLinearColor(0.55f, 0.42f, 0.28f, 1.f);
	}
	Rect(PX, PY, PW, PH, Paper);
	Text(Doc->Title.ToString(), PX + S(40.f), PY + S(30.f), 24.f, Ink, 0, true, false);
	const float LineH = S(17.f) * 1.45f;
	const TArray<FString> Lines = WrapLines(Doc->Body.ToString(), PW - S(80.f), Font(17.f));
	const int32 Visible = FMath::Max(1, FMath::FloorToInt((PH - S(120.f)) / LineH));
	const int32 First = FMath::Clamp(FMath::FloorToInt(Scroll / LineH), 0, FMath::Max(0, Lines.Num() - Visible));
	float Y = PY + S(80.f);
	for (int32 I = First; I < Lines.Num() && I < First + Visible; ++I)
	{
		Text(Lines[I], PX + S(40.f), Y, 17.f, Ink, 0, false, false);
		Y += LineH;
	}
	if (Lines.Num() > Visible)
	{
		Text(TEXT("Mausrad: weiterlesen"), PX + PW - S(30.f), PY + PH - S(34.f), 12.f, Ink, 2, false, false);
	}
	FString Hint = TEXT("[Esc] weglegen");
	if (!Doc->TakeItemId.IsNone())
	{
		Hint += TEXT("   ·   [Q] am Gegenstand: mitnehmen");
	}
	Text(Hint, W() * 0.5f, PY + PH + S(18.f), 14.f, SUHUDStyle::Dim, 1);
}

// ====================================================================================== gallery

void ASUHUD::DrawPhotoDetails(const FSUPhotoMeta& Meta, float X, float Y, float Width, float Height, bool bAllowDelete)
{
	USUPhotoStorage* Storage = USUPhotoStorage::Get(this);
	UTexture2D* Tex = Storage ? Storage->GetTexture(Meta.PhotoId) : nullptr;
	const float ImgW = Width;
	const float ImgH = FMath::Min(Height * 0.7f, ImgW * 9.f / 16.f);
	Image(Tex, X, Y, ImgW, ImgH);
	if (!Tex)
	{
		Text(TEXT("Bilddatei nicht auf diesem Rechner"), X + ImgW * 0.5f, Y + ImgH * 0.5f, 16.f, SUHUDStyle::Dim, 1);
	}
	float CY = Y + ImgH + S(16.f);
	Text(PhotoTitle(Meta), X, CY, 16.f, SUHUDStyle::Text, 0, true);
	FString Flags;
	if (Meta.bFlash) { Flags += TEXT("Blitz  "); }
	if (Meta.bBlurred) { Flags += TEXT("verwackelt  "); }
	if (Meta.bSubmitted) { Flags += TEXT("bereits gemeldet"); }
	Text(FString::Printf(TEXT("Zoom %.1fx   %s"), Meta.ZoomFactor, *Flags), X + Width, CY, 14.f, SUHUDStyle::Dim, 2);
	CY += S(30.f);
	if (Meta.Hits.Num() == 0)
	{
		Text(TEXT("Keine Beweise auf diesem Foto erkannt."), X, CY, 15.f, SUHUDStyle::Dim);
	}
	for (const FSUEvidenceHit& Hit : Meta.Hits)
	{
		const bool bUsable = Hit.Quality >= SULogic::MinEvidenceQuality;
		Text(FString::Printf(TEXT("%s – %s"), *Hit.DisplayName.ToString(), *QualityText(Hit.Quality)), X, CY, 15.f, bUsable ? SUHUDStyle::Good : SUHUDStyle::Dim);
		Text(FString::Printf(TEXT("%.0f m · sichtbar %d %%"), Hit.Distance / 100.f, FMath::RoundToInt(Hit.Visibility * 100.f)), X + Width, CY, 13.f, SUHUDStyle::Dim, 2);
		CY += S(24.f);
	}
	if (bAllowDelete && !Meta.bSubmitted)
	{
		ASUPlayerController* PC = GetPC();
		const FGuid Id = Meta.PhotoId;
		Button(TEXT("Foto löschen"), X, Y + Height - S(44.f), S(200.f), S(40.f), [this, PC, Id]()
		{
			if (PC)
			{
				PC->ServerDeletePhoto(Id);
			}
			if (USUPhotoStorage* St = USUPhotoStorage::Get(this))
			{
				St->DeletePhoto(Id);
			}
			SelectedIndex = INDEX_NONE;
		});
	}
}

void ASUHUD::DrawGallery()
{
	ASUPlayerController* PC = GetPC();
	const ASUPlayerState* PS = PC ? PC->GetPlayerState<ASUPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.f, 0.f, 0.f, 0.85f));
	const TArray<FSUPhotoMeta>& Photos = PS->GetPhotos();
	DrawScreenTitle(FString::Printf(TEXT("Fotos (%d)"), Photos.Num()), SelectedIndex == INDEX_NONE ? TEXT("Klick: Foto ansehen · Mausrad: blättern · [Esc]/[P] schließen") : TEXT("[Esc] zurück zur Übersicht"));
	if (Photos.IsValidIndex(SelectedIndex))
	{
		const float DW = FMath::Min(W() - S(120.f), (H() - S(280.f)) * 16.f / 9.f);
		DrawPhotoDetails(Photos[SelectedIndex], W() * 0.5f - DW * 0.5f, S(100.f), DW, H() - S(160.f), true);
		Button(TEXT("‹"), S(20.f), H() * 0.4f, S(44.f), S(80.f), [this]() { SelectedIndex = FMath::Max(0, SelectedIndex - 1); }, SelectedIndex > 0, 28.f);
		const int32 Num = Photos.Num();
		Button(TEXT("›"), W() - S(64.f), H() * 0.4f, S(44.f), S(80.f), [this, Num]() { SelectedIndex = FMath::Min(Num - 1, SelectedIndex + 1); }, SelectedIndex < Num - 1, 28.f);
		return;
	}
	if (Photos.Num() == 0)
	{
		Text(TEXT("Noch keine Fotos. Kamera mit [C] ausrüsten, [RMT] Sucher, [LMT] auslösen."), S(60.f), S(110.f), 16.f, SUHUDStyle::Dim);
		return;
	}
	USUPhotoStorage* Storage = USUPhotoStorage::Get(this);
	const float TW = S(256.f);
	const float TH = S(144.f);
	const float Gap = S(16.f);
	const int32 Cols = FMath::Max(1, FMath::FloorToInt((W() - S(120.f) + Gap) / (TW + Gap)));
	const float RowH = TH + S(48.f);
	const int32 FirstRow = FMath::FloorToInt(Scroll / RowH);
	const int32 VisibleRows = FMath::Max(1, FMath::FloorToInt((H() - S(160.f)) / RowH));
	for (int32 I = FirstRow * Cols; I < Photos.Num() && I < (FirstRow + VisibleRows) * Cols; ++I)
	{
		const int32 Local = I - FirstRow * Cols;
		const float X = S(60.f) + (Local % Cols) * (TW + Gap);
		const float Y = S(100.f) + (Local / Cols) * RowH;
		const int32 Index = I;
		Button(TEXT(""), X - S(4.f), Y - S(4.f), TW + S(8.f), RowH - S(8.f), [this, Index]() { SelectedIndex = Index; });
		const FSUPhotoMeta& M = Photos[I];
		Image(Storage ? Storage->GetTexture(M.PhotoId) : nullptr, X, Y, TW, TH);
		const float Best = M.BestQuality();
		Text(PhotoTitle(M), X, Y + TH + S(6.f), 12.f, SUHUDStyle::Text);
		Text(Best >= SULogic::MinEvidenceQuality ? QualityText(Best) : FString(TEXT("–")), X + TW, Y + TH + S(6.f), 12.f, Best >= SULogic::MinEvidenceQuality ? SUHUDStyle::Good : SUHUDStyle::Dim, 2);
		if (M.bSubmitted)
		{
			Text(TEXT("gemeldet"), X + S(6.f), Y + S(6.f), 11.f, SUHUDStyle::Accent);
		}
	}
}

// ====================================================================================== trunk

void ASUHUD::DrawTrunk()
{
	ASUPlayerController* PC = GetPC();
	ASUVehicle* V = PC ? Cast<ASUVehicle>(PC->GetScreenContext()) : nullptr;
	ASUCharacter* Char = GetChar();
	if (!V || !Char || !V->GetTrunk() || !Char->GetInventory())
	{
		return;
	}
	if (FVector::Dist(V->GetActorLocation(), Char->GetActorLocation()) > 600.f)
	{
		PC->CloseScreen();
		return;
	}
	Rect(0.f, 0.f, W(), H(), FLinearColor(0.f, 0.f, 0.f, 0.65f));
	DrawScreenTitle(TEXT("Kofferraum"), TEXT("Klick: Gegenstand umlagern · [Esc] schließen"));
	auto Column = [&](const FString& Title, const USUInventoryComponent* Inv, float X, bool bToTrunk)
	{
		float Y = S(110.f);
		Text(FString::Printf(TEXT("%s  (%d/%d)"), *Title, Inv->GetSlots().Num(), Inv->MaxSlots), X, Y, 18.f, SUHUDStyle::Accent, 0, true);
		Y += S(36.f);
		for (const FSUInventorySlot& Slot : Inv->GetSlots())
		{
			if (Slot.Count <= 0)
			{
				continue;
			}
			const FSUItemDef* Def = FSUItemDatabase::Find(Slot.ItemId);
			const FName Id = Slot.ItemId;
			const bool bAllowed = !(Def && Def->bQuestItem && bToTrunk);
			const FString Label = FString::Printf(TEXT("%s%s   %s"), Def ? *Def->Name.ToString() : *Id.ToString(), Slot.Count > 1 ? *FString::Printf(TEXT(" ×%d"), Slot.Count) : TEXT(""), bToTrunk ? TEXT("→") : TEXT("←"));
			Button(Label, X, Y, S(420.f), S(38.f), [PC, V, Id, bToTrunk]() { PC->ServerTrunkTransfer(V, Id, bToTrunk); }, bAllowed, 15.f);
			Y += S(44.f);
		}
	};
	Column(TEXT("Rucksack"), Char->GetInventory(), S(60.f), true);
	Column(TEXT("Kofferraum"), V->GetTrunk(), FMath::Max(S(560.f), W() * 0.5f), false);
}
