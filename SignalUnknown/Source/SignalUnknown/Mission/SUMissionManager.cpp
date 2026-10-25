#include "Mission/SUMissionManager.h"
#include "SignalUnknown.h"
#include "Mission/SUMissionDatabase.h"
#include "Core/SUEventSubsystem.h"
#include "Core/SUGameState.h"
#include "Core/SUGameMode.h"
#include "Core/SUPlayerState.h"
#include "Core/SUPlayerController.h"
#include "Core/SUDeveloperSettings.h"
#include "Core/SUItemDatabase.h"
#include "Player/SUCharacter.h"
#include "Player/SUInventoryComponent.h"
#include "Base/SUBaseDevices.h"
#include "World/SUWorldBuilder.h"
#include "Logic/SUObjectiveLogic.h"
#include "Logic/SUEconomyLogic.h"
#include "Logic/SUEvidenceLogic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ASUMissionManager::ASUMissionManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ASUMissionManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUMissionManager, ActiveMissionId);
	DOREPLIFETIME(ASUMissionManager, Phase);
	DOREPLIFETIME(ASUMissionManager, Objectives);
	DOREPLIFETIME(ASUMissionManager, CompletedMissions);
	DOREPLIFETIME(ASUMissionManager, DocumentedEvidence);
	DOREPLIFETIME(ASUMissionManager, ReportedEvidence);
	DOREPLIFETIME(ASUMissionManager, Transmission);
}

ASUMissionManager* ASUMissionManager::Get(const UObject* WorldContext)
{
	const ASUGameState* GS = ASUGameState::Get(WorldContext);
	return GS ? GS->MissionManager.Get() : nullptr;
}

void ASUMissionManager::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		if (USUEventSubsystem* Events = GetWorld()->GetSubsystem<USUEventSubsystem>())
		{
			Events->OnGameEvent.AddDynamic(this, &ASUMissionManager::HandleGameEvent);
		}
		NextChatterTime = Now() + FMath::FRandRange(150.0, 260.0);
	}
}

double ASUMissionManager::Now() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : 0.0;
}

const FSUMissionDef* ASUMissionManager::GetActiveDef() const
{
	return ActiveMissionId.IsNone() ? nullptr : FSUMissionDatabase::Find(ActiveMissionId);
}

// ------------------------------------------------------------------------------------ objectives

namespace
{
	void ToLogic(const FSUMissionDef& Def, const TArray<FSUObjectiveState>& States, std::vector<SULogic::ObjectiveSpec>& OutSpecs, std::vector<SULogic::ObjectiveProgress>& OutProgress)
	{
		OutSpecs.clear();
		OutProgress.clear();
		for (int32 I = 0; I < Def.Objectives.Num(); ++I)
		{
			SULogic::ObjectiveSpec S;
			S.Stage = Def.Objectives[I].Stage;
			S.Required = FMath::Max(1, Def.Objectives[I].Required);
			S.bOptional = Def.Objectives[I].bOptional;
			OutSpecs.push_back(S);
			SULogic::ObjectiveProgress P;
			if (States.IsValidIndex(I))
			{
				P.Count = States[I].Count;
				P.bDone = States[I].bDone;
			}
			OutProgress.push_back(P);
		}
	}
}

bool ASUMissionManager::IsObjectiveActive(int32 Index) const
{
	const FSUMissionDef* Def = GetActiveDef();
	if (!Def || Phase != ESUMissionPhase::Active)
	{
		return false;
	}
	std::vector<SULogic::ObjectiveSpec> Specs;
	std::vector<SULogic::ObjectiveProgress> Progress;
	ToLogic(*Def, Objectives, Specs, Progress);
	return SULogic::IsObjectiveActive(Specs, Progress, static_cast<size_t>(Index));
}

int32 ASUMissionManager::GetCurrentStage() const
{
	const FSUMissionDef* Def = GetActiveDef();
	if (!Def)
	{
		return INDEX_NONE;
	}
	std::vector<SULogic::ObjectiveSpec> Specs;
	std::vector<SULogic::ObjectiveProgress> Progress;
	ToLogic(*Def, Objectives, Specs, Progress);
	return SULogic::CurrentObjectiveStage(Specs, Progress);
}

bool ASUMissionManager::IsWaitingForReport() const
{
	const FSUMissionDef* Def = GetActiveDef();
	if (!Def || Phase != ESUMissionPhase::Active)
	{
		return false;
	}
	const int32 Stage = GetCurrentStage();
	for (const FSUObjectiveDef& O : Def->Objectives)
	{
		if (O.EventName == SUEvent::ReportSubmitted && O.Stage == Stage)
		{
			return true;
		}
	}
	return false;
}

bool ASUMissionManager::AdvanceObjectives(FName EventName, FName Param)
{
	const FSUMissionDef* Def = GetActiveDef();
	if (!Def || Phase != ESUMissionPhase::Active)
	{
		return false;
	}
	std::vector<SULogic::ObjectiveSpec> Specs;
	std::vector<SULogic::ObjectiveProgress> Progress;
	ToLogic(*Def, Objectives, Specs, Progress);

	bool bChanged = false;
	for (int32 I = 0; I < Def->Objectives.Num(); ++I)
	{
		const FSUObjectiveDef& O = Def->Objectives[I];
		if (O.EventName != EventName || (!O.EventParam.IsNone() && O.EventParam != Param))
		{
			continue;
		}
		const int32 Before = Progress[static_cast<size_t>(I)].Count;
		const bool bJustDone = SULogic::AdvanceObjective(Specs, Progress, static_cast<size_t>(I), 1);
		if (Progress[static_cast<size_t>(I)].Count != Before)
		{
			bChanged = true;
			Objectives[I].Count = Progress[static_cast<size_t>(I)].Count;
			Objectives[I].bDone = Progress[static_cast<size_t>(I)].bDone;
			if (bJustDone && !O.bHidden)
			{
				NotifyAll(FText::FromString(FString::Printf(TEXT("Erledigt: %s"), *O.Text.ToString())), FLinearColor(0.6f, 0.95f, 0.6f));
			}
		}
	}
	if (bChanged)
	{
		OnRep_State();
	}
	return bChanged;
}

void ASUMissionManager::AutoCompleteObjectives()
{
	// "pick up the camera" is already done if someone carries one
	const FSUMissionDef* Def = GetActiveDef();
	if (!Def)
	{
		return;
	}
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		for (const FSUObjectiveDef& O : Def->Objectives)
		{
			if (O.EventName != SUEvent::ItemPicked)
			{
				continue;
			}
			for (TActorIterator<ASUCharacter> It(GetWorld()); It; ++It)
			{
				if (It->GetInventory() && It->GetInventory()->HasItem(O.EventParam))
				{
					AdvanceObjectives(SUEvent::ItemPicked, O.EventParam);
					break;
				}
			}
		}
	}
}

void ASUMissionManager::HandleGameEvent(FName EventName, FName Param, AActor* Instigator)
{
	AdvanceObjectives(EventName, Param);
}

bool ASUMissionManager::GetMarker(FVector& OutLocation, FText& OutLabel) const
{
	const ASUWorldBuilder* Builder = ASUWorldBuilder::Get(this);
	if (!Builder)
	{
		return false;
	}
	if (Phase == ESUMissionPhase::Incoming || Phase == ESUMissionPhase::Briefing || Phase == ESUMissionPhase::Offered)
	{
		OutLocation = Builder->GetPointLocation(TEXT("TowerRadio"));
		OutLabel = SUText(TEXT("Funkgerät"));
		return !OutLocation.IsZero();
	}
	const FSUMissionDef* Def = GetActiveDef();
	if (!Def || Phase != ESUMissionPhase::Active)
	{
		return false;
	}
	for (int32 I = 0; I < Def->Objectives.Num(); ++I)
	{
		if (IsObjectiveActive(I) && !Def->Objectives[I].MarkerSite.IsNone() && !Def->Objectives[I].bOptional)
		{
			OutLocation = Builder->GetPointLocation(Def->Objectives[I].MarkerSite);
			OutLabel = Def->Objectives[I].Text;
			return !OutLocation.IsZero();
		}
	}
	OutLocation = Builder->GetPointLocation(Def->TargetSite);
	OutLabel = Def->Title;
	return !OutLocation.IsZero();
}

// ------------------------------------------------------------------------------------ flow

void ASUMissionManager::QueueMission(FName MissionId, float DelaySeconds)
{
	if (!HasAuthority() || !FSUMissionDatabase::Find(MissionId))
	{
		return;
	}
	QueuedMissionId = MissionId;
	QueueTime = Now() + FMath::Max(0.f, DelaySeconds);
}

void ASUMissionManager::StartIncoming(FName MissionId)
{
	ActiveMissionId = MissionId;
	Phase = ESUMissionPhase::Incoming;
	Objectives.Reset();
	DocumentedEvidence.Reset();
	OnRep_State();
	NotifyAll(SUText(TEXT("Das Funkgerät im Turm rauscht …")), FLinearColor(0.9f, 0.85f, 0.6f));
}

void ASUMissionManager::AnswerRadio(AActor* By)
{
	if (!HasAuthority() || Phase != ESUMissionPhase::Incoming)
	{
		return;
	}
	Phase = ESUMissionPhase::Briefing;
	PlayTransmission(ESUTransmissionKind::Briefing, ActiveMissionId, 0);
	USUEventSubsystem::Post(this, SUEvent::RadioAnswered, ActiveMissionId, By);
	OnRep_State();
}

void ASUMissionManager::AcceptOfferedMission(AActor* By)
{
	const FSUMissionDef* Def = GetActiveDef();
	if (!HasAuthority() || !Def || Phase != ESUMissionPhase::Offered)
	{
		return;
	}
	Phase = ESUMissionPhase::Active;
	Objectives.Reset();
	for (const FSUObjectiveDef& O : Def->Objectives)
	{
		FSUObjectiveState S;
		S.Id = O.Id;
		Objectives.Add(S);
	}
	PlayTransmission(ESUTransmissionKind::Accept, ActiveMissionId, 0);
	if (ASUGameState* GS = ASUGameState::Get(this))
	{
		GS->SetStoryStage(Def->StoryStageOnAccept);
	}
	OnRep_State();
	NotifyAll(FText::FromString(FString::Printf(TEXT("Neuer Einsatz: %s"), *Def->Title.ToString())), FLinearColor(1.f, 0.85f, 0.5f));
	USUEventSubsystem::Post(this, SUEvent::MissionAccepted, ActiveMissionId, By);
	AutoCompleteObjectives();
}

void ASUMissionManager::RepeatLastTransmission()
{
	if (!HasAuthority() || IsTransmitting() || ActiveMissionId.IsNone())
	{
		return;
	}
	if (Phase == ESUMissionPhase::Offered || Phase == ESUMissionPhase::Active)
	{
		PlayTransmission(ESUTransmissionKind::Briefing, ActiveMissionId, 0);
	}
}

bool ASUMissionManager::PlayStrangeTransmission()
{
	if (!HasAuthority() || IsTransmitting() || Phase == ESUMissionPhase::Incoming)
	{
		return false;
	}
	const int32 Count = FSUMissionDatabase::StrangeTransmissions().Num();
	PlayTransmission(ESUTransmissionKind::Strange, NAME_None, FMath::RandRange(0, FMath::Max(0, Count - 1)));
	return true;
}

void ASUMissionManager::PlayTransmission(ESUTransmissionKind Kind, FName MissionId, int32 Index)
{
	Transmission.Kind = Kind;
	Transmission.MissionId = MissionId;
	Transmission.Index = Index;
	Transmission.StartServerTime = Now();
	Transmission.Serial++;
}

const TArray<FSUSpeechLine>* ASUMissionManager::GetTransmissionLines(const FSURadioTransmission& T)
{
	switch (T.Kind)
	{
	case ESUTransmissionKind::Briefing:
	case ESUTransmissionKind::Accept:
	case ESUTransmissionKind::Completion:
	{
		const FSUMissionDef* Def = FSUMissionDatabase::Find(T.MissionId);
		if (!Def)
		{
			return nullptr;
		}
		return T.Kind == ESUTransmissionKind::Briefing ? &Def->Briefing : (T.Kind == ESUTransmissionKind::Accept ? &Def->AcceptLines : &Def->CompletionLines);
	}
	case ESUTransmissionKind::Chatter:
	case ESUTransmissionKind::Strange:
	{
		// single line transmissions are looked up by index; store them in a stable array
		static TArray<FSUSpeechLine> Single;
		const TArray<FSUSpeechLine>& Source = T.Kind == ESUTransmissionKind::Chatter ? FSUMissionDatabase::Chatter() : FSUMissionDatabase::StrangeTransmissions();
		if (!Source.IsValidIndex(T.Index))
		{
			return nullptr;
		}
		Single.Reset();
		Single.Add(Source[T.Index]);
		return &Single;
	}
	case ESUTransmissionKind::EndOfContent:
		return &FSUMissionDatabase::EndOfContentLines();
	default:
		return nullptr;
	}
}

float ASUMissionManager::GetLinesDuration(const TArray<FSUSpeechLine>& Lines)
{
	float Total = 0.f;
	for (const FSUSpeechLine& L : Lines)
	{
		Total += L.GetDuration() + 0.6f;
	}
	return Total;
}

bool ASUMissionManager::IsTransmitting() const
{
	const TArray<FSUSpeechLine>* Lines = GetTransmissionLines(Transmission);
	if (!Lines)
	{
		return false;
	}
	const double Elapsed = Now() - Transmission.StartServerTime;
	return Elapsed >= 0.0 && Elapsed < GetLinesDuration(*Lines);
}

bool ASUMissionManager::GetTransmissionLine(FSUSpeechLine& OutLine, float& OutLineTime) const
{
	const TArray<FSUSpeechLine>* Lines = GetTransmissionLines(Transmission);
	if (!Lines)
	{
		return false;
	}
	float T = static_cast<float>(Now() - Transmission.StartServerTime);
	if (T < 0.f)
	{
		return false;
	}
	for (const FSUSpeechLine& L : *Lines)
	{
		const float D = L.GetDuration();
		if (T < D)
		{
			OutLine = L;
			OutLineTime = T;
			return true;
		}
		T -= D + 0.6f;
		if (T < 0.f)
		{
			return false; // short pause between lines
		}
	}
	return false;
}

void ASUMissionManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	const double T = Now();

	if (!QueuedMissionId.IsNone() && T >= QueueTime && (Phase == ESUMissionPhase::None || Phase == ESUMissionPhase::Completed) && !IsTransmitting())
	{
		const FName Id = QueuedMissionId;
		QueuedMissionId = NAME_None;
		StartIncoming(Id);
	}

	if (Phase == ESUMissionPhase::Briefing && !IsTransmitting())
	{
		Phase = ESUMissionPhase::Offered;
		OnRep_State();
	}

	if (PendingCompletionTime > 0.0 && T >= PendingCompletionTime && !IsTransmitting())
	{
		PendingCompletionTime = -1.0;
		PlayTransmission(ESUTransmissionKind::Completion, ActiveMissionId, 0);
	}

	if (bEndOfContentPending && QueuedMissionId.IsNone() && PendingCompletionTime < 0.0 && !IsTransmitting() && T > QueueTime + 30.0)
	{
		bEndOfContentPending = false;
		PlayTransmission(ESUTransmissionKind::EndOfContent, NAME_None, 0);
		NotifyAll(SUText(TEXT("Ende der Inhalte dieser Version – weitere Einsätze folgen in späteren Updates. Die Welt bleibt frei erkundbar.")), FLinearColor(0.8f, 0.8f, 1.f));
	}

	// background chatter of other posts when nothing important is going on
	if (T >= NextChatterTime)
	{
		NextChatterTime = T + FMath::FRandRange(200.0, 420.0);
		if (!IsTransmitting() && Phase != ESUMissionPhase::Incoming && Phase != ESUMissionPhase::Briefing && Phase != ESUMissionPhase::Offered)
		{
			const int32 Count = FSUMissionDatabase::Chatter().Num();
			PlayTransmission(ESUTransmissionKind::Chatter, NAME_None, FMath::RandRange(0, FMath::Max(0, Count - 1)));
		}
	}
}

void ASUMissionManager::OnRep_State()
{
}

void ASUMissionManager::NotifyAll(const FText& Text, const FLinearColor& Color) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ASUPlayerController* PC = Cast<ASUPlayerController>(It->Get()))
		{
			PC->ClientNotify(Text, Color);
		}
	}
}

// ------------------------------------------------------------------------------------ photos & reports

void ASUMissionManager::OnPhotoRegistered(ASUPlayerState* Photographer, const FSUPhotoMeta& Meta)
{
	if (!HasAuthority())
	{
		return;
	}
	ASUPlayerController* PC = Photographer ? Cast<ASUPlayerController>(Photographer->GetOwner()) : nullptr;
	const FSUMissionDef* Def = (Phase == ESUMissionPhase::Active) ? GetActiveDef() : nullptr;

	FString Message;
	for (const FSUEvidenceHit& Hit : Meta.Hits)
	{
		if (Hit.Quality < SULogic::MinEvidenceQuality)
		{
			continue;
		}
		const FSUEvidenceRequirement* Req = Def ? Def->FindEvidence(Hit.EvidenceId) : nullptr;
		const FString Label = UTF8_TO_TCHAR(SULogic::QualityLabel(Hit.Quality));
		if (!Req)
		{
			Message += FString::Printf(TEXT("%s (%s, nicht einsatzrelevant)  "), *Hit.DisplayName.ToString(), *Label);
			continue;
		}
		Message += FString::Printf(TEXT("Beweis: %s (%s)  "), *Req->DisplayName.ToString(), *Label);
		if (!DocumentedEvidence.Contains(Hit.EvidenceId))
		{
			DocumentedEvidence.Add(Hit.EvidenceId);
			USUEventSubsystem::Post(this, SUEvent::PhotoEvidence, Hit.EvidenceId, Photographer ? Photographer->GetPawn() : nullptr);
			USUEventSubsystem::Post(this, SUEvent::PhotoCategory, Req->Category, Photographer ? Photographer->GetPawn() : nullptr);
		}
	}
	if (PC)
	{
		if (Message.IsEmpty())
		{
			PC->ClientNotify(SUText(TEXT("Foto gespeichert – kein Beweis erkannt.")), FLinearColor(0.8f, 0.8f, 0.8f));
		}
		else
		{
			PC->ClientNotify(FText::FromString(FString(TEXT("Foto gespeichert. ")) + Message), FLinearColor(0.75f, 0.9f, 1.f));
		}
	}
}

FSUReportResult ASUMissionManager::EvaluatePhotos(const TArray<FSUPhotoMeta>& Photos, bool bForPreview) const
{
	FSUReportResult Result;
	const FSUMissionDef* Def = GetActiveDef();
	if (!Def)
	{
		Result.Message = SUText(TEXT("Kein aktiver Einsatz."));
		return Result;
	}
	Result.MissionId = Def->Id;
	Result.PhotosUsed = Photos.Num();

	std::vector<SULogic::PayRequirement> Reqs;
	for (const FSUEvidenceRequirement& E : Def->Evidence)
	{
		SULogic::PayRequirement R;
		R.Weight = E.Weight;
		R.Bonus = E.BonusCents;
		R.bRequired = E.bRequired;
		R.bRare = E.bRare;
		R.RareBonus = E.RareBonusCents;
		R.bAlreadyReported = ReportedEvidence.Contains(E.EvidenceId);
		Reqs.push_back(R);
	}
	std::vector<SULogic::SubmittedHit> Hits;
	for (const FSUPhotoMeta& P : Photos)
	{
		for (const FSUEvidenceHit& H : P.Hits)
		{
			const int32 Index = Def->Evidence.IndexOfByPredicate([&H](const FSUEvidenceRequirement& E) { return E.EvidenceId == H.EvidenceId; });
			if (Index != INDEX_NONE)
			{
				SULogic::SubmittedHit S;
				S.RequirementIndex = Index;
				S.Quality = H.Quality;
				Hits.push_back(S);
			}
		}
	}
	const SULogic::PayResult Pay = SULogic::ComputeReportPayout(Def->BasePayCents, Def->CompletionBonusCents, Reqs, Hits, SULogic::MinEvidenceQuality);

	auto AddLine = [&Result](const FString& Label, int32 Amount)
	{
		FSUReportLine L;
		L.Label = FText::FromString(Label);
		L.AmountCents = Amount;
		Result.Lines.Add(L);
	};
	AddLine(FString::Printf(TEXT("Grundvergütung (%d/%d Pflichtbeweise)"), Pay.RequiredFound, Pay.RequiredTotal), Pay.BaseAmount);
	for (int32 I = 0; I < Def->Evidence.Num(); ++I)
	{
		const FSUEvidenceRequirement& E = Def->Evidence[I];
		const double Q = Pay.BestQuality[static_cast<size_t>(I)];
		if (Q >= SULogic::MinEvidenceQuality)
		{
			if (Pay.EvidenceAmounts[static_cast<size_t>(I)] > 0)
			{
				AddLine(FString::Printf(TEXT("%s (%s)"), *E.DisplayName.ToString(), UTF8_TO_TCHAR(SULogic::QualityLabel(Q))), Pay.EvidenceAmounts[static_cast<size_t>(I)]);
			}
			if (E.bRare)
			{
				AddLine(FString::Printf(TEXT("Seltene Entdeckung: %s"), *E.DisplayName.ToString()), E.RareBonusCents);
			}
		}
		else if (E.bRequired && !ReportedEvidence.Contains(E.EvidenceId))
		{
			Result.Missing.Add(E.DisplayName);
		}
	}
	if (Pay.CompletenessBonus > 0)
	{
		AddLine(TEXT("Bonus: vollständiger Bericht"), Pay.CompletenessBonus);
	}
	Result.TotalCents = Pay.Total;
	Result.Rating = FText::FromString(UTF8_TO_TCHAR(SULogic::ReportRating(Pay)));
	Result.bAccepted = !bForPreview;
	return Result;
}

FSUReportResult ASUMissionManager::PreviewReport(const TArray<FSUPhotoMeta>& Photos) const
{
	return EvaluatePhotos(Photos, true);
}

FSUReportResult ASUMissionManager::SubmitReport(ASUPlayerController* Submitter, const TArray<FGuid>& PhotoIds)
{
	FSUReportResult Rejected;
	Rejected.bAccepted = false;
	if (!HasAuthority() || !Submitter)
	{
		return Rejected;
	}
	if (!IsWaitingForReport())
	{
		Rejected.Message = SUText(TEXT("Der Bericht kann erst nach Abschluss der Untersuchung und der Rückkehr zur Basis abgeschickt werden."));
		return Rejected;
	}
	// must be at a terminal
	const APawn* Pawn = Submitter->GetPawn();
	bool bAtTerminal = false;
	for (TActorIterator<ASUComputerTerminal> It(GetWorld()); It && Pawn; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) < 450.f)
		{
			bAtTerminal = true;
			break;
		}
	}
	if (!bAtTerminal)
	{
		Rejected.Message = SUText(TEXT("Berichte werden am Einsatzcomputer im Turm abgeschickt."));
		return Rejected;
	}

	TArray<FSUPhotoMeta> Photos;
	ASUPlayerState* SubmitterPS = Submitter->GetPlayerState<ASUPlayerState>();
	if (SubmitterPS)
	{
		for (const FGuid& Id : PhotoIds)
		{
			if (const FSUPhotoMeta* P = SubmitterPS->FindPhoto(Id))
			{
				if (!P->bSubmitted)
				{
					Photos.Add(*P);
				}
			}
		}
	}
	if (Photos.Num() == 0)
	{
		Rejected.Message = SUText(TEXT("Keine Fotos ausgewählt. Wähle die Beweisfotos für den Bericht aus."));
		return Rejected;
	}
	// photos of team mates taken during this mission are attached automatically
	for (APlayerState* PS : GetWorld()->GetGameState()->PlayerArray)
	{
		ASUPlayerState* Other = Cast<ASUPlayerState>(PS);
		if (!Other || Other == SubmitterPS)
		{
			continue;
		}
		TArray<FGuid> Used;
		for (const FSUPhotoMeta& P : Other->GetPhotos())
		{
			if (P.MissionId == ActiveMissionId && !P.bSubmitted)
			{
				Photos.Add(P);
				Used.Add(P.PhotoId);
			}
		}
		Other->MarkSubmitted(Used);
	}

	FSUReportResult Result = EvaluatePhotos(Photos, false);
	const FSUMissionDef* Def = GetActiveDef();
	if (!Def)
	{
		return Rejected;
	}
	Result.Message = FText::FromString(FString::Printf(TEXT("Bericht „%s“ angenommen. Bewertung: %s"), *Def->Title.ToString(), *Result.Rating.ToString()));

	if (ASUGameState* GS = ASUGameState::Get(this))
	{
		GS->AddMoney(Result.TotalCents, FText::FromString(FString::Printf(TEXT("Einsatz: %s"), *Def->Title.ToString())));
	}
	TArray<FGuid> SubmittedIds;
	for (const FSUPhotoMeta& P : Photos)
	{
		SubmittedIds.Add(P.PhotoId);
		for (const FSUEvidenceHit& H : P.Hits)
		{
			if (H.Quality >= SULogic::MinEvidenceQuality && Def->FindEvidence(H.EvidenceId))
			{
				ReportedEvidence.AddUnique(H.EvidenceId);
			}
		}
	}
	if (SubmitterPS)
	{
		SubmitterPS->MarkSubmitted(SubmittedIds);
	}
	USUEventSubsystem::Post(this, SUEvent::ReportSubmitted, ActiveMissionId, Submitter->GetPawn());
	CompleteMission();
	return Result;
}

void ASUMissionManager::CompleteMission()
{
	const FSUMissionDef* Def = GetActiveDef();
	if (!Def)
	{
		return;
	}
	Phase = ESUMissionPhase::Completed;
	CompletedMissions.AddUnique(Def->Id);
	if (ASUGameState* GS = ASUGameState::Get(this))
	{
		GS->SetStoryStage(Def->StoryStageOnComplete);
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (ASUPlayerState* SUPS = Cast<ASUPlayerState>(PS))
			{
				SUPS->MissionsCompleted++;
			}
		}
	}
	PendingCompletionTime = Now() + 4.0;
	if (!Def->NextMissionId.IsNone() && !CompletedMissions.Contains(Def->NextMissionId))
	{
		QueueMission(Def->NextMissionId, GetDefault<USUGameplaySettings>()->NextMissionDelaySeconds);
	}
	else
	{
		bEndOfContentPending = true;
		QueueTime = Now();
	}
	OnRep_State();

	// autosave after every report
	if (ASUGameMode* GM = GetWorld()->GetAuthGameMode<ASUGameMode>())
	{
		FString Message;
		GM->SaveGameNow(Message);
	}
}

// ------------------------------------------------------------------------------------ save

FSUMissionSave ASUMissionManager::GetSaveData() const
{
	FSUMissionSave Data;
	Data.ActiveMissionId = ActiveMissionId;
	// an unanswered or running radio call is restored as "incoming" so nothing is lost
	Data.Phase = (Phase == ESUMissionPhase::Briefing || Phase == ESUMissionPhase::Offered) ? ESUMissionPhase::Incoming : Phase;
	Data.Objectives = Objectives;
	Data.CompletedMissions = CompletedMissions;
	Data.DocumentedEvidence = DocumentedEvidence;
	Data.ReportedEvidence = ReportedEvidence;
	Data.QueuedMissionId = QueuedMissionId;
	Data.QueuedDelay = QueuedMissionId.IsNone() ? 0.f : static_cast<float>(FMath::Max(0.0, QueueTime - Now()));
	return Data;
}

void ASUMissionManager::ApplySaveData(const FSUMissionSave& Data)
{
	if (!HasAuthority())
	{
		return;
	}
	ActiveMissionId = FSUMissionDatabase::Find(Data.ActiveMissionId) ? Data.ActiveMissionId : NAME_None;
	Phase = ActiveMissionId.IsNone() ? ESUMissionPhase::None : Data.Phase;
	CompletedMissions = Data.CompletedMissions;
	DocumentedEvidence = Data.DocumentedEvidence;
	ReportedEvidence = Data.ReportedEvidence;
	Objectives.Reset();
	if (const FSUMissionDef* Def = GetActiveDef())
	{
		// match by objective id so changed mission definitions keep as much progress as possible
		for (const FSUObjectiveDef& O : Def->Objectives)
		{
			FSUObjectiveState S;
			S.Id = O.Id;
			if (const FSUObjectiveState* Saved = Data.Objectives.FindByPredicate([&O](const FSUObjectiveState& X) { return X.Id == O.Id; }))
			{
				S.Count = FMath::Min(Saved->Count, FMath::Max(1, O.Required));
				S.bDone = Saved->bDone;
			}
			Objectives.Add(S);
		}
	}
	QueuedMissionId = NAME_None;
	if (!Data.QueuedMissionId.IsNone())
	{
		QueueMission(Data.QueuedMissionId, Data.QueuedDelay);
	}
	else if (Phase == ESUMissionPhase::Completed || Phase == ESUMissionPhase::None)
	{
		// make sure the story continues if the save was taken between missions
		for (const FSUMissionDef& M : FSUMissionDatabase::All())
		{
			if (!CompletedMissions.Contains(M.Id))
			{
				QueueMission(M.Id, 20.f);
				break;
			}
		}
	}
	PendingCompletionTime = -1.0;
	Transmission = FSURadioTransmission();
	OnRep_State();
}
