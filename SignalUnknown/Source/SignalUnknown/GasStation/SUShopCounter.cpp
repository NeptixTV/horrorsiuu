#include "GasStation/SUShopCounter.h"
#include "SignalUnknown.h"
#include "GasStation/SUFuelPump.h"
#include "NPC/SUNPC.h"
#include "Vehicle/SUVehicle.h"
#include "Player/SUCharacter.h"
#include "Player/SUInventoryComponent.h"
#include "Core/SUGameState.h"
#include "Core/SUPlayerController.h"
#include "Core/SUEventSubsystem.h"
#include "Core/SUItemDatabase.h"
#include "Net/UnrealNetwork.h"

ASUShopCounter::ASUShopCounter()
{
}

void ASUShopCounter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUShopCounter, Pumps);
	DOREPLIFETIME(ASUShopCounter, Clerk);
}

void ASUShopCounter::BuildGeometry()
{
	// PLACEHOLDER cash register on the counter
	AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 9.f), FVector(36.f, 40.f, 18.f), ESUMat::MetalDark);
	AddPart(SceneRoot, ESUShape::Cube, FVector(-6.f, 0.f, 28.f), FVector(6.f, 30.f, 20.f), ESUMat::Screen, FRotator(0.f, 0.f, 0.f), false);
	AddPart(SceneRoot, ESUShape::Cube, FVector(12.f, 0.f, 19.f), FVector(14.f, 30.f, 3.f), ESUMat::Black, FRotator(-12.f, 0.f, 0.f), false);
	AddText(SceneRoot, FVector(-2.5f, 0.f, 30.f), FRotator::ZeroRotator, TEXT("KASSE"), 4.f, FColor(120, 255, 140));
	// price list behind the counter
	AddPart(SceneRoot, ESUShape::Cube, FVector(-190.f, 70.f, 200.f), FVector(4.f, 150.f, 90.f), ESUMat::White, FRotator::ZeroRotator, false);
	FString Board;
	for (FName Id : GetShopItems())
	{
		if (const FSUItemDef* Def = FSUItemDatabase::Find(Id))
		{
			Board += FString::Printf(TEXT("%s  %s\n"), *Def->Name.ToString(), *SUFormatMoney(Def->PriceCents));
		}
	}
	AddText(SceneRoot, FVector(-187.f, 70.f, 200.f), FRotator::ZeroRotator, Board, 6.f, FColor(30, 30, 30));
}

const TArray<FName>& ASUShopCounter::GetShopItems()
{
	static const TArray<FName> Items = {SUItem::Battery, SUItem::Bandage, SUItem::Coffee, SUItem::Snack, SUItem::Drink, SUItem::Flashlight, SUItem::Toolkit, SUItem::Camera};
	return Items;
}

int32 ASUShopCounter::GetOpenFuelCents() const
{
	int32 Sum = 0;
	for (const TObjectPtr<ASUFuelPump>& P : Pumps)
	{
		if (P)
		{
			Sum += P->GetBillCents();
		}
	}
	return Sum;
}

float ASUShopCounter::GetOpenFuelLiters() const
{
	float Sum = 0.f;
	for (const TObjectPtr<ASUFuelPump>& P : Pumps)
	{
		if (P)
		{
			Sum += P->GetBillLiters();
		}
	}
	return Sum;
}

ASUVehicle* ASUShopCounter::GetRepairableVehicle() const
{
	const ASUGameState* GS = ASUGameState::Get(this);
	ASUVehicle* V = GS ? GS->MainVehicle.Get() : nullptr;
	if (!V || FVector::Dist2D(V->GetActorLocation(), GetActorLocation()) > 4000.f)
	{
		return nullptr;
	}
	return V;
}

int32 ASUShopCounter::GetRepairCostCents(const ASUVehicle* Vehicle) const
{
	if (!Vehicle)
	{
		return 0;
	}
	// 0,90 $ per percent of engine damage, 15 $ call-out of the mechanic
	const float Missing = FMath::Max(0.f, 100.f - Vehicle->GetEngineHealth());
	return Missing < 0.5f ? 0 : 1500 + FMath::RoundToInt(Missing * 90.f);
}

bool ASUShopCounter::IsClerkPresent() const
{
	return Clerk && !Clerk->IsAway() && FVector::Dist2D(Clerk->GetActorLocation(), GetActorLocation()) < 260.f;
}

bool ASUShopCounter::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	OutPrompt.Primary = SUText(TEXT("Kasse – einkaufen / bezahlen"));
	OutPrompt.bPrimary = true;
	const int32 Due = GetOpenFuelCents();
	if (Due > 0)
	{
		OutPrompt.Info = FText::FromString(FString::Printf(TEXT("Offene Tankrechnung: %s"), *SUFormatMoney(Due)));
	}
	else if (!IsClerkPresent())
	{
		OutPrompt.Info = SUText(TEXT("Niemand an der Kasse"));
	}
	return true;
}

void ASUShopCounter::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	if (!User)
	{
		return;
	}
	if (Clerk)
	{
		if (!IsClerkPresent())
		{
			Clerk->Summon();
			User->NotifyOwner(FText::FromString(FString::Printf(TEXT("„Moment, ich komme!“ – %s ist auf dem Weg zur Kasse."), *Clerk->DisplayName.ToString())));
			if (USUProceduralAudioComponent* Bell = GetOrCreateSound(ESUSoundKind::Impact, 0.5f, 1500.f))
			{
				Bell->TriggerSound(1, 0.3f);
			}
			return;
		}
		Clerk->FaceActor(User, 4.f);
		Clerk->Summon();
	}
	if (ASUPlayerController* PC = Cast<ASUPlayerController>(User->GetController()))
	{
		PC->ClientOpenScreen(ESUScreen::Shop, this);
	}
	Super::Interact(User, Component, Option);
}

bool ASUShopCounter::ValidateCustomer(ASUPlayerController* PC, ASUCharacter*& OutChar) const
{
	OutChar = PC ? Cast<ASUCharacter>(PC->GetPawn()) : nullptr;
	if (!HasAuthority() || !OutChar || OutChar->IsDead())
	{
		return false;
	}
	if (FVector::Dist(OutChar->GetActorLocation(), GetActorLocation()) > ServiceDistance)
	{
		PC->Notify(SUText(TEXT("Du bist zu weit von der Kasse entfernt.")), FLinearColor(1.f, 0.7f, 0.4f));
		return false;
	}
	if (Clerk && !IsClerkPresent())
	{
		PC->Notify(SUText(TEXT("Niemand an der Kasse.")), FLinearColor(1.f, 0.7f, 0.4f));
		return false;
	}
	return true;
}

void ASUShopCounter::PlayRegisterSound()
{
	if (USUProceduralAudioComponent* Click = GetOrCreateSound(ESUSoundKind::Click, 0.7f, 900.f))
	{
		Click->TriggerSound(0, 1.f);
		Click->TriggerSound(2, 0.8f);
	}
}

void ASUShopCounter::Buy(ASUPlayerController* PC, FName ItemId)
{
	ASUCharacter* Char = nullptr;
	if (!ValidateCustomer(PC, Char) || !GetShopItems().Contains(ItemId))
	{
		return;
	}
	const FSUItemDef* Def = FSUItemDatabase::Find(ItemId);
	ASUGameState* GS = ASUGameState::Get(this);
	if (!Def || !GS)
	{
		return;
	}
	if (!Char->GetInventory()->CanAdd(ItemId, 1))
	{
		PC->Notify(SUText(TEXT("Kein Platz mehr im Inventar.")), FLinearColor(1.f, 0.7f, 0.4f));
		return;
	}
	if (!GS->TrySpend(Def->PriceCents, FText::FromString(FString::Printf(TEXT("Tankstelle: %s"), *Def->Name.ToString()))))
	{
		PC->Notify(SUText(TEXT("Nicht genug Geld auf dem Teamkonto.")), FLinearColor(1.f, 0.5f, 0.4f));
		return;
	}
	Char->GetInventory()->AddItem(ItemId, 1, 1.f);
	PlayRegisterSound();
	PC->Notify(FText::FromString(FString::Printf(TEXT("Gekauft: %s (%s)"), *Def->Name.ToString(), *SUFormatMoney(Def->PriceCents))));
	USUEventSubsystem::Post(this, SUEvent::ItemPicked, ItemId, Char);
}

void ASUShopCounter::PayFuel(ASUPlayerController* PC)
{
	ASUCharacter* Char = nullptr;
	if (!ValidateCustomer(PC, Char))
	{
		return;
	}
	ASUGameState* GS = ASUGameState::Get(this);
	const int32 Due = GetOpenFuelCents();
	if (!GS || Due <= 0)
	{
		PC->Notify(SUText(TEXT("Es ist keine Tankrechnung offen.")));
		return;
	}
	const float Liters = GetOpenFuelLiters();
	if (!GS->TrySpend(Due, FText::FromString(FString::Printf(TEXT("Benzin %.1f l"), Liters))))
	{
		PC->Notify(SUText(TEXT("Nicht genug Geld auf dem Teamkonto, um das Benzin zu bezahlen.")), FLinearColor(1.f, 0.5f, 0.4f));
		return;
	}
	for (const TObjectPtr<ASUFuelPump>& P : Pumps)
	{
		if (P)
		{
			P->SettleBill();
		}
	}
	PlayRegisterSound();
	PC->Notify(FText::FromString(FString::Printf(TEXT("Benzin bezahlt: %.1f l für %s"), Liters, *SUFormatMoney(Due))));
	if (Clerk)
	{
		Clerk->SayTo(Char, TEXT("Danke. Gute Fahrt – und bleiben Sie auf den Hauptstraßen."));
	}
	USUEventSubsystem::Post(this, SUEvent::FuelPaid, NAME_None, Char);
}

void ASUShopCounter::RepairVehicle(ASUPlayerController* PC)
{
	ASUCharacter* Char = nullptr;
	if (!ValidateCustomer(PC, Char))
	{
		return;
	}
	ASUGameState* GS = ASUGameState::Get(this);
	ASUVehicle* V = GetRepairableVehicle();
	if (!GS || !V)
	{
		PC->Notify(SUText(TEXT("Der Wagen muss auf dem Hof der Tankstelle stehen.")), FLinearColor(1.f, 0.7f, 0.4f));
		return;
	}
	const int32 Cost = GetRepairCostCents(V);
	if (Cost <= 0)
	{
		PC->Notify(SUText(TEXT("Am Wagen ist nichts zu reparieren.")));
		return;
	}
	if (V->IsEngineRunning())
	{
		PC->Notify(SUText(TEXT("Erst den Motor abstellen.")), FLinearColor(1.f, 0.7f, 0.4f));
		return;
	}
	if (!GS->TrySpend(Cost, SUText(TEXT("Reparatur Fahrzeug"))))
	{
		PC->Notify(SUText(TEXT("Nicht genug Geld für die Reparatur.")), FLinearColor(1.f, 0.5f, 0.4f));
		return;
	}
	V->RepairBy(100.f);
	PlayRegisterSound();
	PC->Notify(FText::FromString(FString::Printf(TEXT("Wagen repariert (%s). Motorzustand 100 %%."), *SUFormatMoney(Cost))));
}
