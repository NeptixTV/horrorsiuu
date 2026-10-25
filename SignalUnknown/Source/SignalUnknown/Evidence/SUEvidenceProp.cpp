#include "Evidence/SUEvidenceProp.h"
#include "Evidence/SUEvidenceComponent.h"
#include "Net/UnrealNetwork.h"

ASUEvidenceProp::ASUEvidenceProp()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bStaticGeometry = false;
	SceneRoot->SetMobility(EComponentMobility::Movable);
	Evidence = CreateDefaultSubobject<USUEvidenceComponent>(TEXT("Evidence"));
	Evidence->SetupAttachment(SceneRoot);
	Evidence->bRequiresLight = true;
	Evidence->MaxDistance = 900.f;
}

void ASUEvidenceProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUEvidenceProp, EvidenceId);
	DOREPLIFETIME(ASUEvidenceProp, DisplayName);
	DOREPLIFETIME(ASUEvidenceProp, Category);
	DOREPLIFETIME(ASUEvidenceProp, Look);
	DOREPLIFETIME(ASUEvidenceProp, Seed);
}

ASUEvidenceProp* ASUEvidenceProp::SpawnProp(UWorld* World, FName InEvidenceId, const FText& InDisplayName, FName InCategory, ESUEvidencePropLook InLook, const FVector& Location, float Yaw, int32 InSeed)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const FTransform T(FRotator(0.f, Yaw, 0.f), Location);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ASUEvidenceProp* P = World->SpawnActor<ASUEvidenceProp>(ASUEvidenceProp::StaticClass(), T, Params);
	if (P)
	{
		P->EvidenceId = InEvidenceId;
		P->DisplayName = InDisplayName;
		P->Category = InCategory;
		P->Look = InLook;
		P->Seed = InSeed;
		P->FinishSpawning(T);
	}
	return P;
}

void ASUEvidenceProp::BeginPlay()
{
	Super::BeginPlay();
	Evidence->EvidenceId = EvidenceId;
	Evidence->DisplayName = DisplayName;
	Evidence->Category = Category;

	FRandomStream Rng(Seed);
	switch (Look)
	{
	case ESUEvidencePropLook::CreatureTracks:
	{
		// a line of long three-toed prints (PLACEHOLDER geometry instead of decals)
		for (int32 I = 0; I < 7; ++I)
		{
			const float X = I * 55.f;
			const float Y = (I % 2 == 0 ? -14.f : 14.f) + Rng.FRandRange(-4.f, 4.f);
			const float Yaw = Rng.FRandRange(-10.f, 10.f);
			for (int32 Toe = -1; Toe <= 1; ++Toe)
			{
				AddBox(FVector(X + 8.f, Y + Toe * 5.f, 0.5f), FVector(26.f, 3.5f, 1.f), ESUMat::Mud, FRotator(0.f, Yaw + Toe * 14.f, 0.f), false);
			}
			AddBox(FVector(X - 6.f, Y, 0.5f), FVector(10.f, 9.f, 1.f), ESUMat::Mud, FRotator(0.f, Yaw, 0.f), false);
		}
		Evidence->SetRelativeLocation(FVector(165.f, 0.f, 3.f));
		Evidence->BoxExtent = FVector(190.f, 35.f, 4.f);
		break;
	}
	case ESUEvidencePropLook::ClawMarks:
	{
		for (int32 I = 0; I < 4; ++I)
		{
			AddBox(FVector(0.f, -15.f + I * 10.f, 0.f), FVector(1.f, 2.f, 60.f), ESUMat::BloodDry, FRotator(0.f, 0.f, Rng.FRandRange(-12.f, -4.f)), false);
		}
		Evidence->BoxExtent = FVector(4.f, 30.f, 32.f);
		break;
	}
	case ESUEvidencePropLook::BloodDrops:
	{
		for (int32 I = 0; I < 12; ++I)
		{
			AddBox(FVector(Rng.FRandRange(-60.f, 60.f), Rng.FRandRange(-30.f, 30.f), 0.4f), FVector(Rng.FRandRange(3.f, 8.f), Rng.FRandRange(3.f, 7.f), 0.8f), ESUMat::Blood, FRotator(0.f, Rng.FRandRange(0.f, 180.f), 0.f), false);
		}
		Evidence->BoxExtent = FVector(65.f, 35.f, 4.f);
		break;
	}
	}
}
