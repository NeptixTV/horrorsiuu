#include "Interaction/SUDocument.h"
#include "Core/SUEventSubsystem.h"
#include "Core/SUItemDatabase.h"
#include "Core/SUPlayerController.h"
#include "Player/SUCharacter.h"
#include "Player/SUInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ASUDocument::ASUDocument()
{
}

void ASUDocument::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUDocument, Title);
	DOREPLIFETIME(ASUDocument, Body);
	DOREPLIFETIME(ASUDocument, DocumentId);
	DOREPLIFETIME(ASUDocument, TakeItemId);
	DOREPLIFETIME(ASUDocument, Look);
}

void ASUDocument::BuildGeometry()
{
	UStaticMeshComponent* Part = nullptr;
	switch (Look)
	{
	case ESUDocumentLook::Notebook:
		Part = AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 1.f), FVector(20.f, 14.f, 2.f), ESUMat::WoodDark);
		AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 2.2f), FVector(19.f, 13.f, 0.4f), ESUMat::Paper, FRotator::ZeroRotator, false);
		break;
	case ESUDocumentLook::Phone:
		Part = AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 0.5f), FVector(14.f, 7.f, 1.f), ESUMat::Black);
		AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 1.05f), FVector(12.5f, 6.f, 0.1f), ESUMat::Screen, FRotator::ZeroRotator, false);
		break;
	case ESUDocumentLook::Board:
		// cork board on a wall: origin = centre, +X = facing direction
		Part = AddPart(SceneRoot, ESUShape::Cube, FVector(-1.f, 0.f, 0.f), FVector(2.f, 90.f, 60.f), ESUMat::WoodLight);
		AddPart(SceneRoot, ESUShape::Cube, FVector(0.5f, -20.f, 8.f), FVector(0.3f, 18.f, 22.f), ESUMat::Paper, FRotator(0.f, 0.f, 4.f), false);
		AddPart(SceneRoot, ESUShape::Cube, FVector(0.5f, 18.f, -6.f), FVector(0.3f, 22.f, 16.f), ESUMat::Paper, FRotator(0.f, 0.f, -3.f), false);
		AddPart(SceneRoot, ESUShape::Cube, FVector(0.5f, 2.f, 16.f), FVector(0.3f, 12.f, 12.f), ESUMat::Yellow, FRotator(0.f, 0.f, 7.f), false);
		break;
	default:
		Part = AddPart(SceneRoot, ESUShape::Cube, FVector(0.f, 0.f, 0.2f), FVector(21.f, 15.f, 0.4f), ESUMat::Paper);
		break;
	}
	if (Part && Look != ESUDocumentLook::Board)
	{
		Part->SetCollisionResponseToAllChannels(ECR_Ignore);
		Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

bool ASUDocument::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	OutPrompt.Primary = FText::FromString(FString::Printf(TEXT("Lesen: %s"), *Title.ToString()));
	OutPrompt.bPrimary = true;
	if (!TakeItemId.IsNone())
	{
		OutPrompt.Secondary = SUText(TEXT("Einstecken"));
		OutPrompt.bSecondary = true;
	}
	return true;
}

void ASUDocument::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	if (!User)
	{
		return;
	}
	if (!DocumentId.IsNone())
	{
		USUEventSubsystem::Post(this, SUEvent::ItemInspected, DocumentId, User);
	}
	if (Option == ESUInteractOption::Secondary && !TakeItemId.IsNone())
	{
		if (User->GetInventory() && User->GetInventory()->AddItem(TakeItemId, 1) > 0)
		{
			User->NotifyOwner(FText::FromString(FString::Printf(TEXT("+ %s"), *FSUItemDatabase::GetName(TakeItemId).ToString())));
			USUEventSubsystem::Post(this, SUEvent::ItemPicked, TakeItemId, User);
			Destroy();
		}
		return;
	}
	if (ASUPlayerController* PC = Cast<ASUPlayerController>(User->GetController()))
	{
		PC->ClientOpenScreen(ESUScreen::Document, this);
	}
	Super::Interact(User, Component, Option);
}
