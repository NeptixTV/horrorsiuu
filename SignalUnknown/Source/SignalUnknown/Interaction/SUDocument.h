// SIGNAL UNKNOWN – readable documents (notes, pinboard, notebooks, phones).
#pragma once

#include "CoreMinimal.h"
#include "Interaction/SUInteractableActor.h"
#include "SUDocument.generated.h"

UENUM()
enum class ESUDocumentLook : uint8
{
	Note,
	Notebook,
	Phone,
	Board
};

UCLASS()
class SIGNALUNKNOWN_API ASUDocument : public ASUInteractableActor
{
	GENERATED_BODY()

public:
	ASUDocument();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const override;
	virtual void Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option) override;
	virtual float GetInteractionRange() const override { return 220.f; }

	UPROPERTY(Replicated) FText Title;
	UPROPERTY(Replicated) FText Body;
	/** Posted as Item.Inspected when read. */
	UPROPERTY(Replicated) FName DocumentId;
	/** If set, the document can be taken with Q as this inventory item. */
	UPROPERTY(Replicated) FName TakeItemId;
	UPROPERTY(Replicated) ESUDocumentLook Look = ESUDocumentLook::Note;

protected:
	virtual void BuildGeometry() override;
};
