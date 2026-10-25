// SIGNAL UNKNOWN – mission data structures.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/SUTypes.h"
#include "SUMissionTypes.generated.h"

/** Objective that completes when a matching gameplay event is posted. */
USTRUCT(BlueprintType)
struct FSUObjectiveDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText Text;

	/** Objectives of the same stage are active at the same time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 Stage = 0;

	/** Event that advances the objective (see SUEvent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName EventName;

	/** Required event parameter. None = any. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName EventParam;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 Required = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bOptional = false;

	/** Not listed in the HUD (used for things the player should not know in advance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bHidden = false;

	/** Site shown as marker on the map / compass while this objective is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName MarkerSite;
};

/** Evidence the dispatcher expects in the report. */
USTRUCT(BlueprintType)
struct FSUEvidenceRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName EvidenceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 BonusCents = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bRequired = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bRare = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 RareBonusCents = 0;
};

USTRUCT(BlueprintType)
struct FSUMissionDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText Summary;

	/** Main location of the mission (site id of the world builder). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName TargetSite;

	/** Radio call that offers the mission. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FSUSpeechLine> Briefing;

	/** Reply after the player confirmed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FSUSpeechLine> AcceptLines;

	/** Radio after the report was accepted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FSUSpeechLine> CompletionLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FSUObjectiveDef> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FSUEvidenceRequirement> Evidence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 BasePayCents = 20000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 CompletionBonusCents = 5000;

	/** Mission offered after this one (None = none yet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName NextMissionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 StoryStageOnAccept = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 StoryStageOnComplete = 0;

	const FSUEvidenceRequirement* FindEvidence(FName EvidenceId) const
	{
		for (const FSUEvidenceRequirement& Req : Evidence)
		{
			if (Req.EvidenceId == EvidenceId)
			{
				return &Req;
			}
		}
		return nullptr;
	}
};

USTRUCT(BlueprintType)
struct FSUObjectiveState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	bool bDone = false;
};

USTRUCT(BlueprintType)
struct FSUReportLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 AmountCents = 0;
};

USTRUCT(BlueprintType)
struct FSUReportResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName MissionId;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FText Rating;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FText Message;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	TArray<FSUReportLine> Lines;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	TArray<FText> Missing;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 TotalCents = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 PhotosUsed = 0;
};
