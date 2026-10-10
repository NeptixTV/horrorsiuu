#include "World/HHPreviewMannequin.h"
#include "Player/HHCosmeticComponent.h"
#include "Player/HHCharacterAnimInstance.h"
#include "Core/HHGameData.h"
#include "Data/HHCharacterDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

AHHPreviewMannequin::AHHPreviewMannequin()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Cosmetics = CreateDefaultSubobject<UHHCosmeticComponent>(TEXT("Cosmetics"));
}

void AHHPreviewMannequin::BeginPlay()
{
	Super::BeginPlay();

	const UHHGameData* GameData = UHHGameData::Get(this);
	const UHHCharacterDefinition* Definition = GameData ? GameData->DefaultCharacter.LoadSynchronous() : nullptr;
	if (Definition)
	{
		if (USkeletalMesh* Mesh = Definition->BodyMesh.LoadSynchronous())
		{
			Body->SetSkeletalMeshAsset(Mesh);
		}
		Body->SetRelativeRotation(FRotator(0.f, Definition->MeshYawOffset, 0.f));
		Body->SetAnimInstanceClass(UHHCharacterAnimInstance::StaticClass());
		if (UHHCharacterAnimInstance* AnimInstance = Cast<UHHCharacterAnimInstance>(Body->GetAnimInstance()))
		{
			AnimInstance->InitializeFromDefinition(Definition);
			AnimInstance->SetPreviewMode(true);
		}
	}
	Cosmetics->SetBodyMesh(Body);
}

void AHHPreviewMannequin::ShowLoadout(const FHHCosmeticLoadout& Loadout)
{
	Cosmetics->ApplyLoadout(Loadout);
}
