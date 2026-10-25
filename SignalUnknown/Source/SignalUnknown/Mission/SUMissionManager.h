// SIGNAL UNKNOWN – mission state machine, objectives, radio transmissions, reports and payment.
// Spawned by the game mode on the server, replicated to all clients.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Mission/SUMissionTypes.h"
#include "Evidence/SUPhotoTypes.h"
#include "Core/SUSaveGame.h"
#include "SUMissionManager.generated.h"

class ASUPlayerController;
class ASUPlayerState;

UENUM()
enum class ESUTransmissionKind : uint8
{
	None,
	Briefing,
	Accept,
	Completion,
	Chatter,
	Strange,
	EndOfContent
};

USTRUCT()
struct FSURadioTransmission
{
	GENERATED_BODY()

	UPROPERTY() ESUTransmissionKind Kind = ESUTransmissionKind::None;
	UPROPERTY() FName MissionId;
	UPROPERTY() int32 Index = 0;
	UPROPERTY() double StartServerTime = 0.0;
	UPROPERTY() uint8 Serial = 0;
};

UCLASS()
class SIGNALUNKNOWN_API ASUMissionManager : public AInfo
{
	GENERATED_BODY()

public:
	ASUMissionManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static ASUMissionManager* Get(const UObject* WorldContext);

	// ---------------------------------------------------------------- state
	ESUMissionPhase GetPhase() const { return Phase; }
	FName GetActiveMissionId() const { return ActiveMissionId; }
	const FSUMissionDef* GetActiveDef() const;
	const TArray<FSUObjectiveState>& GetObjectiveStates() const { return Objectives; }
	bool IsObjectiveActive(int32 Index) const;
	int32 GetCurrentStage() const;
	bool IsWaitingForReport() const;
	bool IsEvidenceDocumented(FName EvidenceId) const { return DocumentedEvidence.Contains(EvidenceId); }
	bool WasEvidenceReported(FName EvidenceId) const { return ReportedEvidence.Contains(EvidenceId); }
	const TArray<FName>& GetCompletedMissions() const { return CompletedMissions; }
	/** Map / compass target for the current step. */
	bool GetMarker(FVector& OutLocation, FText& OutLabel) const;

	// ---------------------------------------------------------------- server API
	void QueueMission(FName MissionId, float DelaySeconds);
	bool CanAnswer() const { return Phase == ESUMissionPhase::Incoming; }
	bool CanAccept() const { return Phase == ESUMissionPhase::Offered; }
	void AnswerRadio(AActor* By);
	void AcceptOfferedMission(AActor* By);
	void RepeatLastTransmission();
	/** Horror director: a disturbing transmission on the radios (if nothing else is playing). */
	bool PlayStrangeTransmission();
	void OnPhotoRegistered(ASUPlayerState* Photographer, const FSUPhotoMeta& Meta);
	FSUReportResult SubmitReport(ASUPlayerController* Submitter, const TArray<FGuid>& PhotoIds);
	/** Builds a preview of the payment for the terminal (no money is booked). */
	FSUReportResult PreviewReport(const TArray<FSUPhotoMeta>& Photos) const;

	FSUMissionSave GetSaveData() const;
	void ApplySaveData(const FSUMissionSave& Data);

	// ---------------------------------------------------------------- radio playback (all machines)
	const FSURadioTransmission& GetTransmission() const { return Transmission; }
	bool IsTransmitting() const;
	/** Current line of the transmission and time since that line started. */
	bool GetTransmissionLine(FSUSpeechLine& OutLine, float& OutLineTime) const;
	static const TArray<FSUSpeechLine>* GetTransmissionLines(const FSURadioTransmission& T);
	static float GetLinesDuration(const TArray<FSUSpeechLine>& Lines);

private:
	UPROPERTY(ReplicatedUsing = OnRep_State) FName ActiveMissionId;
	UPROPERTY(ReplicatedUsing = OnRep_State) ESUMissionPhase Phase = ESUMissionPhase::None;
	UPROPERTY(ReplicatedUsing = OnRep_State) TArray<FSUObjectiveState> Objectives;
	UPROPERTY(Replicated) TArray<FName> CompletedMissions;
	UPROPERTY(Replicated) TArray<FName> DocumentedEvidence;
	UPROPERTY(Replicated) TArray<FName> ReportedEvidence;
	UPROPERTY(Replicated) FSURadioTransmission Transmission;

	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void HandleGameEvent(FName EventName, FName Param, AActor* Instigator);

	FName QueuedMissionId;
	double QueueTime = 0.0;
	double NextChatterTime = 0.0;
	double PendingCompletionTime = -1.0;
	bool bEndOfContentPending = false;

	double Now() const;
	void StartIncoming(FName MissionId);
	void PlayTransmission(ESUTransmissionKind Kind, FName MissionId, int32 Index);
	bool AdvanceObjectives(FName EventName, FName Param);
	void AutoCompleteObjectives();
	void NotifyAll(const FText& Text, const FLinearColor& Color) const;
	void CompleteMission();
	FSUReportResult EvaluatePhotos(const TArray<FSUPhotoMeta>& Photos, bool bForPreview) const;
};
