// SIGNAL UNKNOWN – photo and evidence data.
#pragma once

#include "CoreMinimal.h"
#include "SUPhotoTypes.generated.h"

/** One recognised evidence object inside a photo. */
USTRUCT(BlueprintType)
struct FSUEvidenceHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Evidence")
	FName EvidenceId;

	UPROPERTY(BlueprintReadOnly, Category = "Evidence")
	FName Category;

	UPROPERTY(BlueprintReadOnly, Category = "Evidence")
	FText DisplayName;

	/** 0..1, see SULogic::EvidenceQuality. */
	UPROPERTY(BlueprintReadOnly, Category = "Evidence")
	float Quality = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Evidence")
	float Coverage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Evidence")
	float Visibility = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Evidence")
	float Distance = 0.f;
};

/** Metadata of a photo. The image itself is a PNG on the photographer's machine. */
USTRUCT(BlueprintType)
struct FSUPhotoMeta
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	FGuid PhotoId;

	/** Mission that was active when the photo was taken (None outside missions). */
	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	FName MissionId;

	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	TArray<FSUEvidenceHit> Hits;

	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	FDateTime TakenAt;

	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	float DayTimeHours = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	float ZoomFactor = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	bool bFlash = false;

	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	bool bBlurred = false;

	/** Already used in a submitted report. */
	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	bool bSubmitted = false;

	/** File name inside the photo folder of the save slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Photo")
	FString FileName;

	float BestQuality() const
	{
		float Best = 0.f;
		for (const FSUEvidenceHit& Hit : Hits)
		{
			Best = FMath::Max(Best, Hit.Quality);
		}
		return Best;
	}
};
