#include "Evidence/SUPhotoStorage.h"
#include "SignalUnknown.h"
#include "Core/SUGameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"

USUPhotoStorage* USUPhotoStorage::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<USUPhotoStorage>() : nullptr;
}

FString USUPhotoStorage::GetDirectory() const
{
	const USUGameInstance* GI = Cast<USUGameInstance>(GetGameInstance());
	const FString Slot = GI ? GI->GetSaveSlot() : FString(TEXT("Default"));
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SignalUnknown"), TEXT("Photos"), Slot);
}

FString USUPhotoStorage::GetPhotoPath(const FGuid& PhotoId) const
{
	return FPaths::Combine(GetDirectory(), PhotoId.ToString(EGuidFormats::Digits) + TEXT(".png"));
}

bool USUPhotoStorage::HasFile(const FGuid& PhotoId) const
{
	return IFileManager::Get().FileExists(*GetPhotoPath(PhotoId));
}

UTexture2D* USUPhotoStorage::MakeTexture(const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		return nullptr;
	}
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}
	void* Data = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

void USUPhotoStorage::StorePhoto(const FGuid& PhotoId, TArray<FColor>&& Pixels, int32 Width, int32 Height)
{
	if (Pixels.Num() != Width * Height || Width <= 0)
	{
		UE_LOG(LogSignalUnknown, Warning, TEXT("StorePhoto: invalid image"));
		return;
	}
	// thumbnail (quarter size) for the gallery, available immediately
	const int32 TW = FMath::Max(1, Width / 4);
	const int32 TH = FMath::Max(1, Height / 4);
	TArray<FColor> Thumb;
	Thumb.SetNumUninitialized(TW * TH);
	for (int32 Y = 0; Y < TH; ++Y)
	{
		for (int32 X = 0; X < TW; ++X)
		{
			int32 R = 0, G = 0, B = 0;
			for (int32 SY = 0; SY < 4; ++SY)
			{
				for (int32 SX = 0; SX < 4; ++SX)
				{
					const FColor& C = Pixels[FMath::Min(Height - 1, Y * 4 + SY) * Width + FMath::Min(Width - 1, X * 4 + SX)];
					R += C.R;
					G += C.G;
					B += C.B;
				}
			}
			Thumb[Y * TW + X] = FColor(static_cast<uint8>(R / 16), static_cast<uint8>(G / 16), static_cast<uint8>(B / 16), 255);
		}
	}
	if (UTexture2D* Texture = MakeTexture(Thumb, TW, TH))
	{
		Textures.Add(PhotoId, Texture);
		TrimCache();
	}
	MissingFiles.Remove(PhotoId);

	// PNG encoding is slow -> background thread
	IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName(TEXT("ImageWrapper")));
	const FString Path = GetPhotoPath(PhotoId);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	++PendingWrites;
	TWeakObjectPtr<USUPhotoStorage> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [Image = MoveTemp(Pixels), Width, Height, Path, &Module, WeakThis]()
	{
		TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
		bool bOk = false;
		if (Wrapper.IsValid() && Wrapper->SetRaw(Image.GetData(), Image.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			const TArray64<uint8> Png = Wrapper->GetCompressed(100);
			bOk = FFileHelper::SaveArrayToFile(Png, *Path);
		}
		if (!bOk)
		{
			UE_LOG(LogSignalUnknown, Error, TEXT("Could not write photo %s"), *Path);
		}
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (USUPhotoStorage* Self = WeakThis.Get())
			{
				--Self->PendingWrites;
			}
		});
	});
}

UTexture2D* USUPhotoStorage::GetTexture(const FGuid& PhotoId)
{
	if (TObjectPtr<UTexture2D>* Found = Textures.Find(PhotoId))
	{
		return *Found;
	}
	if (MissingFiles.Contains(PhotoId))
	{
		return nullptr;
	}
	const FString Path = GetPhotoPath(PhotoId);
	UTexture2D* Texture = IFileManager::Get().FileExists(*Path) ? FImageUtils::ImportFileAsTexture2D(Path) : nullptr;
	if (!Texture)
	{
		MissingFiles.Add(PhotoId);
		return nullptr;
	}
	Textures.Add(PhotoId, Texture);
	TrimCache();
	return Texture;
}

void USUPhotoStorage::DeletePhoto(const FGuid& PhotoId)
{
	Textures.Remove(PhotoId);
	IFileManager::Get().Delete(*GetPhotoPath(PhotoId), false, true, true);
}

void USUPhotoStorage::TrimCache()
{
	// keep memory bounded: full size textures loaded from disk are the expensive ones
	const int32 MaxTextures = 48;
	while (Textures.Num() > MaxTextures)
	{
		auto It = Textures.CreateIterator();
		It.RemoveCurrent();
	}
}
