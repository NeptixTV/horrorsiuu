#include "Player/HHPlayerState.h"
#include "Player/HHInventoryComponent.h"
#include "Net/UnrealNetwork.h"

AHHPlayerState::AHHPlayerState()
{
	// Changes call ForceNetUpdate(), so the low default update frequency is fine.
	Inventory = CreateDefaultSubobject<UHHInventoryComponent>(TEXT("Inventory"));
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
	DOREPLIFETIME(AHHPlayerState, Psyche);
	DOREPLIFETIME(AHHPlayerState, bInsideHouse);
	DOREPLIFETIME(AHHPlayerState, StashedValue);
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
	return CrewColorForSlot(CrewSlot);
}

FLinearColor AHHPlayerState::CrewColorForSlot(int32 Slot)
{
	static const FLinearColor Colors[] =
	{
		FLinearColor::FromSRGBColor(FColor(214, 168, 92)),	// sodium amber
		FLinearColor::FromSRGBColor(FColor(110, 160, 160)),	// verdigris
		FLinearColor::FromSRGBColor(FColor(186, 104, 82)),	// brick
		FLinearColor::FromSRGBColor(FColor(150, 134, 186))	// faded violet
	};
	return Colors[FMath::Abs(Slot) % UE_ARRAY_COUNT(Colors)];
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

void AHHPlayerState::SetPsyche(float NewPsyche)
{
	const float Clamped = FMath::Clamp(NewPsyche, 0.f, 100.f);
	if (HasAuthority() && !FMath::IsNearlyEqual(Clamped, Psyche, 0.05f))
	{
		Psyche = Clamped;
	}
}

void AHHPlayerState::SetInsideHouse(bool bNewInside)
{
	if (HasAuthority() && bInsideHouse != bNewInside)
	{
		bInsideHouse = bNewInside;
		ForceNetUpdate();
	}
}

void AHHPlayerState::AddStashedValue(int32 Amount)
{
	if (HasAuthority() && Amount != 0)
	{
		StashedValue += Amount;
		ForceNetUpdate();
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
