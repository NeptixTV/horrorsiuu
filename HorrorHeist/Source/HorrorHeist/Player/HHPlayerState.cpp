#include "Player/HHPlayerState.h"
#include "Net/UnrealNetwork.h"

AHHPlayerState::AHHPlayerState()
{
	// Changes call ForceNetUpdate(), so the low default update frequency is fine.
}

void AHHPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHHPlayerState, bReady);
	DOREPLIFETIME(AHHPlayerState, Cosmetics);
	DOREPLIFETIME(AHHPlayerState, Equipment);
	DOREPLIFETIME(AHHPlayerState, PlayerLevel);
	DOREPLIFETIME(AHHPlayerState, bCrewLeader);
	DOREPLIFETIME(AHHPlayerState, CrewSlot);
}

FHHOnPlayerStateEvent& AHHPlayerState::OnAnyCrewChange()
{
	static FHHOnPlayerStateEvent Delegate;
	return Delegate;
}

void AHHPlayerState::BeginPlay()
{
	Super::BeginPlay();
	BroadcastCrewChange();
}

void AHHPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	BroadcastCrewChange();
}

void AHHPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (AHHPlayerState* Target = Cast<AHHPlayerState>(PlayerState))
	{
		// Survives seamless travel into a job and back.
		Target->Cosmetics = Cosmetics;
		Target->Equipment = Equipment;
		Target->PlayerLevel = PlayerLevel;
		Target->bCrewLeader = bCrewLeader;
		Target->CrewSlot = CrewSlot;
	}
}

FLinearColor AHHPlayerState::GetCrewColor() const
{
	static const FLinearColor Colors[] =
	{
		FLinearColor::FromSRGBColor(FColor(214, 168, 92)),	// sodium amber
		FLinearColor::FromSRGBColor(FColor(110, 160, 160)),	// verdigris
		FLinearColor::FromSRGBColor(FColor(186, 104, 82)),	// brick
		FLinearColor::FromSRGBColor(FColor(150, 134, 186))	// faded violet
	};
	return Colors[FMath::Abs(CrewSlot) % UE_ARRAY_COUNT(Colors)];
}

void AHHPlayerState::SetReady(bool bNewReady)
{
	if (HasAuthority() && bReady != bNewReady)
	{
		bReady = bNewReady;
		OnRep_Ready();
		ForceNetUpdate();
	}
}

void AHHPlayerState::SetCosmetics(const FHHCosmeticLoadout& NewCosmetics)
{
	if (HasAuthority() && Cosmetics != NewCosmetics)
	{
		Cosmetics = NewCosmetics;
		OnRep_Cosmetics();
		ForceNetUpdate();
	}
}

void AHHPlayerState::SetEquipment(const FHHEquipmentLoadout& NewEquipment)
{
	if (HasAuthority() && Equipment != NewEquipment)
	{
		Equipment = NewEquipment;
		OnRep_Equipment();
		ForceNetUpdate();
	}
}

void AHHPlayerState::SetPlayerLevel(int32 NewLevel)
{
	if (HasAuthority() && PlayerLevel != NewLevel)
	{
		PlayerLevel = FMath::Max(1, NewLevel);
		OnRep_CrewInfo();
	}
}

void AHHPlayerState::SetCrewLeader(bool bNewLeader)
{
	if (HasAuthority() && bCrewLeader != bNewLeader)
	{
		bCrewLeader = bNewLeader;
		OnRep_CrewInfo();
	}
}

void AHHPlayerState::SetCrewSlot(int32 NewSlot)
{
	if (HasAuthority() && CrewSlot != NewSlot)
	{
		CrewSlot = NewSlot;
		OnRep_CrewInfo();
	}
}

void AHHPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	BroadcastCrewChange();
}

void AHHPlayerState::OnRep_Ready()
{
	BroadcastCrewChange();
}

void AHHPlayerState::OnRep_Cosmetics()
{
	OnCosmeticsChanged.Broadcast(this);
}

void AHHPlayerState::OnRep_Equipment()
{
	OnEquipmentChanged.Broadcast(this);
}

void AHHPlayerState::OnRep_CrewInfo()
{
	BroadcastCrewChange();
}

void AHHPlayerState::BroadcastCrewChange()
{
	OnAnyCrewChange().Broadcast(this);
}
