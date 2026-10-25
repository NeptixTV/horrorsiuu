// SIGNAL UNKNOWN – per player state: photo metadata (owner only) and simple stats.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Evidence/SUPhotoTypes.h"
#include "SUPlayerState.generated.h"

DECLARE_MULTICAST_DELEGATE(FSUPhotosChangedSignature);

UCLASS()
class SIGNALUNKNOWN_API ASUPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ASUPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<FSUPhotoMeta>& GetPhotos() const { return Photos; }
	const FSUPhotoMeta* FindPhoto(const FGuid& Id) const;

	/** Server: store metadata of a new photo. */
	void AddPhoto(const FSUPhotoMeta& Meta);
	/** Server: delete a photo (frees camera storage). */
	bool RemovePhoto(const FGuid& Id);
	/** Server: mark photos as used in a report. */
	void MarkSubmitted(const TArray<FGuid>& Ids);
	/** Server: replace all photos (loading a save). */
	void SetPhotos(const TArray<FSUPhotoMeta>& InPhotos);

	int32 CountPhotosOnCamera() const;

	FSUPhotosChangedSignature OnPhotosChanged;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	int32 MissionsCompleted = 0;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Photos)
	TArray<FSUPhotoMeta> Photos;

	UFUNCTION()
	void OnRep_Photos();
};
