// SIGNAL UNKNOWN – local photo files (PNG) and thumbnails.
//
// Photos are real in-game captures. They are written to
//   <Project>/Saved/SignalUnknown/Photos/<SaveSlot>/<PhotoId>.png
// on the machine of the player who took them. Metadata (evidence hits) lives in the save game.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include <atomic>
#include "SUPhotoStorage.generated.h"

class UTexture2D;

UCLASS()
class SIGNALUNKNOWN_API USUPhotoStorage : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static USUPhotoStorage* Get(const UObject* WorldContext);

	FString GetDirectory() const;
	FString GetPhotoPath(const FGuid& PhotoId) const;

	/** Stores a captured image (BGRA pixels). Creates the thumbnail now, writes the PNG asynchronously. */
	void StorePhoto(const FGuid& PhotoId, TArray<FColor>&& Pixels, int32 Width, int32 Height);

	/** Texture for display; loads the PNG on demand. Null if the file is missing (photo of another player). */
	UTexture2D* GetTexture(const FGuid& PhotoId);

	bool HasFile(const FGuid& PhotoId) const;
	void DeletePhoto(const FGuid& PhotoId);

	/** Number of PNG writes still in flight. */
	int32 GetPendingWrites() const { return PendingWrites.load(); }

private:
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UTexture2D>> Textures;

	/** Missing files are only looked up once. */
	TSet<FGuid> MissingFiles;

	std::atomic<int32> PendingWrites{0};

	static UTexture2D* MakeTexture(const TArray<FColor>& Pixels, int32 Width, int32 Height);
	void TrimCache();
};
