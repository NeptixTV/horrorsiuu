#include "Interaction/SUPickupItem.h"
#include "Core/SUItemDatabase.h"
#include "Core/SUEventSubsystem.h"
#include "Player/SUCharacter.h"
#include "Player/SUInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ASUPickupItem::ASUPickupItem()
{
}

void ASUPickupItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUPickupItem, ItemId);
	DOREPLIFETIME(ASUPickupItem, Count);
	DOREPLIFETIME(ASUPickupItem, Charge);
}

ASUPickupItem* ASUPickupItem::SpawnItem(UWorld* World, FName InItemId, int32 InCount, float InCharge, const FTransform& Transform, AActor* AttachTo)
{
	if (!World || World->GetNetMode() == NM_Client || !FSUItemDatabase::Find(InItemId))
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ASUPickupItem* Item = World->SpawnActor<ASUPickupItem>(ASUPickupItem::StaticClass(), Transform, Params);
	if (!Item)
	{
		return nullptr;
	}
	Item->ItemId = InItemId;
	Item->Count = FMath::Max(1, InCount);
	Item->Charge = InCharge;
	Item->FinishSpawning(Transform);
	if (AttachTo)
	{
		Item->AttachToActor(AttachTo, FAttachmentTransformRules::KeepWorldTransform);
	}
	return Item;
}

void ASUPickupItem::BuildGeometry()
{
	const FSUItemDef* Def = FSUItemDatabase::Find(ItemId);
	if (!Def)
	{
		return;
	}
	USUAssetLibrary* Lib = Assets();
	const FVector Size = Def->Size;
	UStaticMeshComponent* Body = nullptr;
	switch (Def->Shape)
	{
	case ESUItemShape::Cylinder:
		Body = AddPart(SceneRoot, ESUShape::Cylinder, FVector(0.f, 0.f, Size.Z * 0.5f), Size, ESUMat::Black);
		break;
	case ESUItemShape::Sphere:
		Body = AddPart(SceneRoot, ESUShape::Sphere, FVector(0.f, 0.f, Size.Z * 0.5f), Size, ESUMat::Black);
		break;
	default:
		Body = AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, Size.Z * 0.5f), Size, ESUMat::Black);
		break;
	}
	if (Body && Lib)
	{
		Body->SetMaterial(0, Lib->GetColorMaterial(Def->Color));
		// items should not block the player but must be hit by the interaction trace
		Body->SetCollisionResponseToAllChannels(ECR_Ignore);
		Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	// small details that make devices recognisable
	if (ItemId == SUItem::Camera)
	{
		AddPart(SceneRoot, ESUShape::Cylinder, FVector(Size.X * 0.5f + 4.f, 0.f, Size.Z * 0.45f), FVector(7.f, 7.f, 9.f), ESUMat::Black, FRotator(90.f, 0.f, 0.f), false);
		AddPart(SceneRoot, ESUShape::Cube, FVector(-2.f, 0.f, Size.Z + 1.f), FVector(4.f, 4.f, 2.f), ESUMat::MetalDark, FRotator::ZeroRotator, false);
	}
	else if (ItemId == SUItem::Flashlight)
	{
		AddPart(SceneRoot, ESUShape::Cylinder, FVector(0.f, 0.f, Size.Z), FVector(7.f, 7.f, 5.f), ESUMat::MetalDark, FRotator::ZeroRotator, false);
	}
	else if (ItemId == SUItem::RangerNotebook)
	{
		AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, Size.Z + 0.2f), FVector(Size.X - 1.f, Size.Y - 1.f, 0.4f), ESUMat::Paper, FRotator::ZeroRotator, false);
	}
}

bool ASUPickupItem::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	const FSUItemDef* Def = FSUItemDatabase::Find(ItemId);
	if (!Def)
	{
		return false;
	}
	FString Name = Def->Name.ToString();
	if (Count > 1)
	{
		Name += FString::Printf(TEXT(" (%d)"), Count);
	}
	OutPrompt.Primary = FText::FromString(FString::Printf(TEXT("%s aufheben"), *Name));
	OutPrompt.bPrimary = true;
	return true;
}

void ASUPickupItem::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	if (!User || !User->GetInventory())
	{
		return;
	}
	const int32 Added = User->GetInventory()->AddItem(ItemId, Count, Charge);
	if (Added <= 0)
	{
		User->NotifyOwner(SUText(TEXT("Kein Platz im Inventar.")));
		return;
	}
	User->NotifyOwner(FText::FromString(FString::Printf(TEXT("+ %s"), *FSUItemDatabase::GetName(ItemId).ToString())));
	USUEventSubsystem::Post(this, SUEvent::ItemPicked, ItemId, User);
	Count -= Added;
	Super::Interact(User, Component, Option);
	if (Count <= 0)
	{
		Destroy();
	}
}
