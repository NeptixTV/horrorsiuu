#include "GasStation/SUFuelPump.h"
#include "SignalUnknown.h"
#include "Vehicle/SUVehicle.h"
#include "Player/SUCharacter.h"
#include "Core/SUGameState.h"
#include "Core/SUEventSubsystem.h"
#include "Core/SUDeveloperSettings.h"
#include "Logic/SUEconomyLogic.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "CableComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	const FVector HolsterLocal(36.f, 28.f, 112.f);
	const FVector OutletLocal(31.f, 28.f, 168.f);
}

ASUFuelPump::ASUFuelPump()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;

	HolsterZone = CreateDefaultSubobject<UBoxComponent>(TEXT("HolsterZone"));
	HolsterZone->SetupAttachment(SceneRoot);
	HolsterZone->SetRelativeLocation(HolsterLocal + FVector(6.f, 0.f, -6.f));
	HolsterZone->SetBoxExtent(FVector(14.f, 22.f, 30.f));
	HolsterZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HolsterZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	HolsterZone->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HolsterZone->SetGenerateOverlapEvents(false);
}

void ASUFuelPump::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUFuelPump, PumpNumber);
	DOREPLIFETIME(ASUFuelPump, NozzleState);
	DOREPLIFETIME(ASUFuelPump, Holder);
	DOREPLIFETIME(ASUFuelPump, Vehicle);
	DOREPLIFETIME(ASUFuelPump, bPumping);
	DOREPLIFETIME(ASUFuelPump, BillLiters);
	DOREPLIFETIME(ASUFuelPump, PriceCentsPerLiter);
}

void ASUFuelPump::BuildGeometry()
{
	if (HasAuthority())
	{
		PriceCentsPerLiter = GetDefault<USUGameplaySettings>()->FuelPriceCentsPerLiter;
	}

	// PLACEHOLDER pump housing
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 95.f), FVector(56.f, 92.f, 190.f), ESUMat::MetalWhite);
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 202.f), FVector(62.f, 98.f, 26.f), ESUMat::GasRed);
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 6.f), FVector(66.f, 100.f, 12.f), ESUMat::MetalDark);
	AddPart(SceneRoot, ESUShape::Cube, FVector(29.f, -18.f, 145.f), FVector(2.f, 50.f, 44.f), ESUMat::Screen, FRotator::ZeroRotator, false);
	AddPart(SceneRoot, ESUShape::Cube, FVector(31.f, HolsterLocal.Y, HolsterLocal.Z - 4.f), FVector(10.f, 16.f, 24.f), ESUMat::MetalDark);
	AddPart(SceneRoot, ESUShape::Cylinder, OutletLocal, FVector(7.f, 7.f, 10.f), ESUMat::Black, FRotator(90.f, 0.f, 0.f), false);
	AddText(SceneRoot, FVector(32.f, 0.f, 203.f), FRotator::ZeroRotator, FString::Printf(TEXT("%d"), PumpNumber), 20.f, FColor::White);
	AddText(SceneRoot, FVector(-32.f, 0.f, 203.f), FRotator(0.f, 180.f, 0.f), FString::Printf(TEXT("%d"), PumpNumber), 20.f, FColor::White);
	AddText(SceneRoot, FVector(29.5f, -18.f, 116.f), FRotator::ZeroRotator,
		FString::Printf(TEXT("SUPER  %s/l"), *SUFormatMoney(PriceCentsPerLiter)), 4.5f, FColor(40, 40, 40));

	DisplayLiters = AddText(SceneRoot, FVector(30.5f, -18.f, 157.f), FRotator::ZeroRotator, TEXT(""), 7.f, FColor(120, 255, 140));
	DisplayPrice = AddText(SceneRoot, FVector(30.5f, -18.f, 145.f), FRotator::ZeroRotator, TEXT(""), 7.f, FColor(120, 255, 140));
	DisplayStatus = AddText(SceneRoot, FVector(30.5f, -18.f, 133.f), FRotator::ZeroRotator, TEXT(""), 3.6f, FColor(255, 210, 120));

	// nozzle (moved every frame to holster / hand / filler neck)
	NozzleRoot = NewObject<USceneComponent>(this, TEXT("NozzleRoot"));
	NozzleRoot->SetMobility(EComponentMobility::Movable);
	NozzleRoot->SetupAttachment(SceneRoot);
	NozzleRoot->SetUsingAbsoluteLocation(true);
	NozzleRoot->SetUsingAbsoluteRotation(true);
	NozzleRoot->RegisterComponent();
	AddPart(NozzleRoot, ESUShape::Cube, FVector(0.f, 0.f, 0.f), FVector(16.f, 5.f, 9.f), ESUMat::Black, FRotator::ZeroRotator, false);
	AddPart(NozzleRoot, ESUShape::Cube, FVector(-2.f, 0.f, -7.f), FVector(10.f, 4.f, 6.f), ESUMat::GasRed, FRotator(20.f, 0.f, 0.f), false);
	AddPart(NozzleRoot, ESUShape::Cylinder, FVector(16.f, 0.f, -2.f), FVector(2.6f, 2.6f, 18.f), ESUMat::MetalDark, FRotator(-80.f, 0.f, 0.f), false);

	Hose = NewObject<UCableComponent>(this, TEXT("Hose"));
	Hose->SetupAttachment(SceneRoot);
	Hose->SetRelativeLocation(OutletLocal);
	Hose->CableLength = 260.f;
	Hose->NumSegments = 14;
	Hose->CableWidth = 3.2f;
	Hose->NumSides = 6;
	Hose->SolverIterations = 4;
	Hose->bEnableStiffness = true;
	Hose->EndLocation = FVector(-9.f, 0.f, 0.f);
	if (USUAssetLibrary* Lib = Assets())
	{
		Hose->SetMaterial(0, Lib->GetMaterial(ESUMat::Black));
	}
	Hose->RegisterComponent();
	Hose->SetAttachEndToComponent(NozzleRoot);

	UpdateNozzleTransform();
	UpdateDisplay();
}

// ------------------------------------------------------------------------------------ prompts

bool ASUFuelPump::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	if (!User)
	{
		return false;
	}
	OutPrompt.Info = FText::FromString(FString::Printf(TEXT("Zapfsäule %d – %s/l"), PumpNumber, *SUFormatMoney(PriceCentsPerLiter)));
	if (NozzleState == ESUNozzleState::Holstered)
	{
		if (HasOpenBill())
		{
			OutPrompt.Info = FText::FromString(FString::Printf(TEXT("Gesperrt – bitte %s an der Kasse bezahlen"), *SUFormatMoney(GetBillCents())));
			return true;
		}
		if (User->IsSeated() || User->GetHeldNozzlePump())
		{
			return true;
		}
		OutPrompt.Primary = SUText(TEXT("Zapfpistole nehmen"));
		OutPrompt.bPrimary = true;
		return true;
	}
	if (NozzleState == ESUNozzleState::Held && Holder == User)
	{
		OutPrompt.Primary = SUText(TEXT("Zapfpistole einhängen"));
		OutPrompt.bPrimary = true;
		return true;
	}
	OutPrompt.Info = SUText(TEXT("Zapfsäule wird gerade benutzt"));
	return true;
}

void ASUFuelPump::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	if (!User || Option != ESUInteractOption::Primary)
	{
		return;
	}
	if (NozzleState == ESUNozzleState::Holstered)
	{
		TakeNozzle(User);
	}
	else if (NozzleState == ESUNozzleState::Held && Holder == User)
	{
		ReturnNozzle();
	}
	Super::Interact(User, Component, Option);
}

// ------------------------------------------------------------------------------------ server actions

void ASUFuelPump::TakeNozzle(ASUCharacter* By)
{
	if (!HasAuthority() || !By || NozzleState != ESUNozzleState::Holstered || By->IsSeated() || By->IsDead() || By->GetHeldNozzlePump())
	{
		return;
	}
	if (HasOpenBill())
	{
		By->NotifyOwner(SUText(TEXT("Die Säule ist gesperrt. Erst drinnen bezahlen.")), FLinearColor(1.f, 0.75f, 0.4f));
		return;
	}
	NozzleState = ESUNozzleState::Held;
	Holder = By;
	By->SetHeldNozzlePump(this);
	if (USUProceduralAudioComponent* Click = GetOrCreateSound(ESUSoundKind::Click, 0.6f, 800.f))
	{
		Click->TriggerSound(2, 1.f);
	}
}

void ASUFuelPump::InsertNozzle(ASUVehicle* InVehicle, ASUCharacter* By)
{
	if (!HasAuthority() || !InVehicle || NozzleState != ESUNozzleState::Held || Holder != By)
	{
		return;
	}
	if (!InVehicle->IsFuelCapOpen() || InVehicle->GetInsertedNozzlePump())
	{
		return;
	}
	if (FVector::Dist(InVehicle->GetFuelCapLocation(), GetOutletLocation()) > HoseReach)
	{
		By->NotifyOwner(SUText(TEXT("Der Schlauch reicht nicht bis zum Tank. Näher an die Säule fahren.")), FLinearColor(1.f, 0.75f, 0.4f));
		return;
	}
	ClearHolder();
	NozzleState = ESUNozzleState::InVehicle;
	Vehicle = InVehicle;
	InVehicle->SetInsertedNozzlePump(this);
	if (USUProceduralAudioComponent* Click = GetOrCreateSound(ESUSoundKind::Click, 0.6f, 800.f))
	{
		Click->TriggerSound(2, 1.f);
	}
}

void ASUFuelPump::TakeNozzleFromVehicle(ASUCharacter* By)
{
	if (!HasAuthority() || !By || NozzleState != ESUNozzleState::InVehicle || By->GetHeldNozzlePump() || By->IsSeated())
	{
		return;
	}
	StopPumping(false);
	if (Vehicle)
	{
		Vehicle->SetInsertedNozzlePump(nullptr);
	}
	Vehicle = nullptr;
	NozzleState = ESUNozzleState::Held;
	Holder = By;
	By->SetHeldNozzlePump(this);
}

void ASUFuelPump::ReturnNozzle()
{
	if (!HasAuthority())
	{
		return;
	}
	StopPumping(false);
	ClearHolder();
	if (Vehicle)
	{
		Vehicle->SetInsertedNozzlePump(nullptr);
		Vehicle = nullptr;
	}
	if (NozzleState != ESUNozzleState::Holstered)
	{
		NozzleState = ESUNozzleState::Holstered;
		if (USUProceduralAudioComponent* Click = GetOrCreateSound(ESUSoundKind::Click, 0.6f, 800.f))
		{
			Click->TriggerSound(1, 1.f);
		}
	}
	UpdateNozzleTransform();
}

void ASUFuelPump::ForceReturnNozzle(bool bByVehicle)
{
	if (!HasAuthority() || NozzleState == ESUNozzleState::Holstered)
	{
		return;
	}
	if (bByVehicle && Vehicle)
	{
		for (int32 Seat = 0; Seat < ASUVehicle::NumSeats; ++Seat)
		{
			if (ASUCharacter* C = Vehicle->GetOccupant(Seat))
			{
				C->NotifyOwner(SUText(TEXT("Die Zapfpistole steckte noch im Tank! Der Schlauch ist zurückgeschnellt.")), FLinearColor(1.f, 0.6f, 0.3f));
			}
		}
		if (USUProceduralAudioComponent* Hit = GetOrCreateSound(ESUSoundKind::Impact, 1.f, 2500.f))
		{
			Hit->TriggerSound(1, 0.9f);
		}
	}
	else if (Holder)
	{
		Holder->NotifyOwner(SUText(TEXT("Der Schlauch reicht nicht weiter – die Zapfpistole ist zurück an der Säule.")), FLinearColor(1.f, 0.75f, 0.4f));
	}
	ReturnNozzle();
}

void ASUFuelPump::SetPumping(bool bOn, ASUCharacter* By)
{
	if (!HasAuthority() || NozzleState != ESUNozzleState::InVehicle || !Vehicle)
	{
		return;
	}
	if (bOn == bPumping)
	{
		return;
	}
	if (bOn)
	{
		if (Vehicle->GetFuelLiters() >= Vehicle->GetFuelCapacity() - 0.05f)
		{
			if (By)
			{
				By->NotifyOwner(SUText(TEXT("Der Tank ist bereits voll.")));
			}
			return;
		}
		if (Vehicle->IsEngineRunning() && By)
		{
			By->NotifyOwner(SUText(TEXT("Tipp: Beim Tanken den Motor abstellen.")), FLinearColor(1.f, 0.85f, 0.5f));
		}
		bPumping = true;
		PumpUser = By;
		BilledVehicle = Vehicle;
	}
	else
	{
		StopPumping(false);
	}
}

void ASUFuelPump::StopPumping(bool bFull)
{
	if (!bPumping)
	{
		return;
	}
	bPumping = false;
	ASUCharacter* User = PumpUser.Get();
	if (bFull)
	{
		if (USUProceduralAudioComponent* Click = GetOrCreateSound(ESUSoundKind::Click, 0.6f, 800.f))
		{
			Click->TriggerSound(2, 1.2f);
		}
		if (User)
		{
			User->NotifyOwner(SUText(TEXT("Klack – die Zapfpistole schaltet ab, der Tank ist voll.")));
		}
	}
	if (RefuelEventLiters > 0.5f)
	{
		if (User)
		{
			User->NotifyOwner(FText::FromString(FString::Printf(TEXT("%.1f l getankt – offen: %s (Kasse im Shop)"), BillLiters, *SUFormatMoney(GetBillCents()))));
		}
		USUEventSubsystem::Post(this, SUEvent::Refueled, NAME_None, User);
	}
	RefuelEventLiters = 0.f;
}

int32 ASUFuelPump::GetBillCents() const
{
	return SULogic::FuelCostCents(BillLiters, PriceCentsPerLiter);
}

void ASUFuelPump::SettleBill()
{
	if (!HasAuthority())
	{
		return;
	}
	BillLiters = 0.f;
	BilledVehicle.Reset();
}

void ASUFuelPump::ClearHolder()
{
	if (Holder)
	{
		if (Holder->GetHeldNozzlePump() == this)
		{
			Holder->SetHeldNozzlePump(nullptr);
		}
		Holder = nullptr;
	}
}

// ------------------------------------------------------------------------------------ tick

FVector ASUFuelPump::GetHolsterLocation() const
{
	return GetActorTransform().TransformPosition(HolsterLocal);
}

FVector ASUFuelPump::GetOutletLocation() const
{
	return GetActorTransform().TransformPosition(OutletLocal);
}

void ASUFuelPump::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		if (NozzleState == ESUNozzleState::Held)
		{
			if (!Holder || Holder->IsDead() || Holder->IsSeated() || Holder->GetHeldNozzlePump() != this)
			{
				ReturnNozzle();
			}
			else if (FVector::Dist(Holder->GetActorLocation(), GetOutletLocation()) > HoseReach + 90.f)
			{
				ForceReturnNozzle(false);
			}
		}
		else if (NozzleState == ESUNozzleState::InVehicle)
		{
			if (!Vehicle)
			{
				ReturnNozzle();
			}
			else if (FVector::Dist(Vehicle->GetFuelCapLocation(), GetOutletLocation()) > HoseReach + 80.f)
			{
				ForceReturnNozzle(true);
			}
		}

		if (bPumping && Vehicle)
		{
			const float Want = FlowLitersPerSecond * DeltaSeconds;
			const float Added = Vehicle->AddFuel(Want);
			BillLiters += Added;
			RefuelEventLiters += Added;
			if (Added < Want * 0.5f)
			{
				StopPumping(true);
			}
		}

		// driving away without paying
		ASUVehicle* Billed = BilledVehicle.Get();
		if (HasOpenBill() && NozzleState != ESUNozzleState::InVehicle && Billed
			&& FVector::Dist(Billed->GetActorLocation(), GetActorLocation()) > 3500.f && Billed->GetSpeedKmh() > 8.f)
		{
			if (ASUGameState* GS = ASUGameState::Get(this))
			{
				GS->AddUnpaidFuelIncident();
			}
			UE_LOG(LogSignalUnknown, Log, TEXT("Unpaid fuel at pump %d: %.1f l (%d ct)"), PumpNumber, BillLiters, GetBillCents());
			for (int32 Seat = 0; Seat < ASUVehicle::NumSeats; ++Seat)
			{
				if (ASUCharacter* C = Billed->GetOccupant(Seat))
				{
					C->NotifyOwner(SUText(TEXT("Ihr seid weggefahren, ohne das Benzin zu bezahlen. Das wurde notiert.")), FLinearColor(1.f, 0.45f, 0.35f));
				}
			}
			BillLiters = 0.f;
			BilledVehicle.Reset();
		}
	}

	if (USUProceduralAudioComponent* Flow = GetOrCreateSound(ESUSoundKind::Pump, 0.55f, 1500.f))
	{
		Flow->SetSoundParam(SULogic::Dsp::Param::A, bPumping ? 1.f : 0.f);
	}
	UpdateNozzleTransform();
	UpdateDisplay();
}

void ASUFuelPump::UpdateNozzleTransform()
{
	if (!NozzleRoot)
	{
		return;
	}
	FTransform T;
	if (NozzleState == ESUNozzleState::Held && Holder && Holder->GetHandAttachPoint())
	{
		const FTransform Hand = Holder->GetHandAttachPoint()->GetComponentTransform();
		T = FTransform(Hand.GetRotation() * FQuat(FRotator(-12.f, -8.f, 0.f)), Hand.GetLocation(), FVector::OneVector);
	}
	else if (NozzleState == ESUNozzleState::InVehicle && Vehicle)
	{
		// spout points into the filler neck on the left side of the car
		const FRotator Rot = (Vehicle->GetActorRotation().Quaternion() * FQuat(FRotator(-35.f, 90.f, 0.f))).Rotator();
		const FVector Dir = Rot.Vector();
		T = FTransform(Rot, Vehicle->GetFuelCapLocation() - Dir * 24.f + FVector(0.f, 0.f, 6.f), FVector::OneVector);
	}
	else
	{
		const FRotator Rot = (GetActorRotation().Quaternion() * FQuat(FRotator(-70.f, 0.f, 0.f))).Rotator();
		T = FTransform(Rot, GetHolsterLocation(), FVector::OneVector);
	}
	NozzleRoot->SetWorldTransform(T);
}

void ASUFuelPump::UpdateDisplay()
{
	if (!DisplayLiters)
	{
		return;
	}
	DisplayLiters->SetText(FText::FromString(FString::Printf(TEXT("%6.2f l"), BillLiters)));
	DisplayPrice->SetText(FText::FromString(SUFormatMoney(GetBillCents())));
	FString Status;
	if (bPumping)
	{
		Status = TEXT("TANKT …");
	}
	else if (NozzleState == ESUNozzleState::Holstered && HasOpenBill())
	{
		Status = TEXT("BITTE AN DER KASSE ZAHLEN");
	}
	else if (NozzleState == ESUNozzleState::Holstered)
	{
		Status = TEXT("BEREIT");
	}
	else
	{
		Status = TEXT("ZAPFPISTOLE ENTNOMMEN");
	}
	DisplayStatus->SetText(FText::FromString(Status));
}
