#include "Core/SUPlayerState.h"
#include "Net/UnrealNetwork.h"

ASUPlayerState::ASUPlayerState()
{
}

void ASUPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ASUPlayerState, Photos, COND_OwnerOnly);
	DOREPLIFETIME(ASUPlayerState, MissionsCompleted);
}

const FSUPhotoMeta* ASUPlayerState::FindPhoto(const FGuid& Id) const
{
	return Photos.FindByPredicate([&Id](const FSUPhotoMeta& P) { return P.PhotoId == Id; });
}

void ASUPlayerState::AddPhoto(const FSUPhotoMeta& Meta)
{
	if (!HasAuthority())
	{
		return;
	}
	Photos.Add(Meta);
	OnRep_Photos();
}

bool ASUPlayerState::RemovePhoto(const FGuid& Id)
{
	if (!HasAuthority())
	{
		return false;
	}
	const int32 Removed = Photos.RemoveAll([&Id](const FSUPhotoMeta& P) { return P.PhotoId == Id; });
	if (Removed > 0)
	{
		OnRep_Photos();
	}
	return Removed > 0;
}

void ASUPlayerState::MarkSubmitted(const TArray<FGuid>& Ids)
{
	if (!HasAuthority())
	{
		return;
	}
	for (FSUPhotoMeta& P : Photos)
	{
		if (Ids.Contains(P.PhotoId))
		{
			P.bSubmitted = true;
		}
	}
	OnRep_Photos();
}

void ASUPlayerState::SetPhotos(const TArray<FSUPhotoMeta>& InPhotos)
{
	if (!HasAuthority())
	{
		return;
	}
	Photos = InPhotos;
	OnRep_Photos();
}

int32 ASUPlayerState::CountPhotosOnCamera() const
{
	return Photos.Num();
}

void ASUPlayerState::OnRep_Photos()
{
	OnPhotosChanged.Broadcast();
}
