#include "Vehicles/HHGarageDoor.h"
#include "Heist/HHHeistGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "HHGarageDoor"

AHHGarageDoor::AHHGarageDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bAlwaysRelevant = true;
	Prompt = LOCTEXT("Open", "Open the garage door");

	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(Root);
	Frame->SetCollisionProfileName(TEXT("BlockAll"));
}

void AHHGarageDoor::BeginPlay()
{
	Super::BeginPlay();
	BaseScale = Mesh->GetRelativeScale3D();
	BaseLocation = Mesh->GetRelativeLocation();
	if (const UStaticMesh* DoorMesh = Mesh->GetStaticMesh())
	{
		// The roll-up door mesh stands on its bottom edge (origin at the floor).
		DoorHeight = FMath::Max(50.f, static_cast<float>(DoorMesh->GetBounds().GetBox().Max.Z));
	}
	Alpha = bOpen ? 1.f : 0.f;
	ApplyAlpha();
}

void AHHGarageDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHHGarageDoor, bOpen);
}

AHHGarageDoor* AHHGarageDoor::FindNearest(const UObject* WorldContextObject, const FVector& Location, float Range)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	AHHGarageDoor* Best = nullptr;
	float BestDistance = FMath::Square(Range);
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AHHGarageDoor> It(World); It; ++It)
	{
		const float Distance = FVector::DistSquared(It->GetActorLocation(), Location);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	return Best;
}

bool AHHGarageDoor::CanOperate(FText& OutReason) const
{
	if (bHubOnly && AHHHeistGameState::IsJobWorld(this))
	{
		OutReason = LOCTEXT("JobLocked", "Locked from the inside. Nobody is home tonight.");
		return false;
	}
	return bInteractionEnabled;
}

FText AHHGarageDoor::GetInteractionPrompt(AActor* Interactor) const
{
	return bOpen ? LOCTEXT("Close", "Close the garage door") : Prompt;
}

FText AHHGarageDoor::GetInteractionDetail(AActor* Interactor) const
{
	FText Reason;
	if (!CanOperate(Reason))
	{
		return Reason;
	}
	return LOCTEXT("Remote", "Also works from the driver's seat (G)");
}

bool AHHGarageDoor::CanInteract(AActor* Interactor) const
{
	FText Reason;
	return CanOperate(Reason);
}

void AHHGarageDoor::Interact(AActor* Interactor)
{
	if (HasAuthority() && CanInteract(Interactor))
	{
		Toggle();
	}
}

void AHHGarageDoor::Toggle()
{
	if (!HasAuthority())
	{
		return;
	}
	bOpen = !bOpen;
	OnRep_Open();
	ForceNetUpdate();
}

void AHHGarageDoor::OnRep_Open()
{
	SetActorTickEnabled(true);
	if (MotorSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, MotorSound, GetActorLocation() + FVector(0.f, 0.f, DoorHeight * 0.8f));
	}
}

void AHHGarageDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Target = bOpen ? 1.f : 0.f;
	Alpha = FMath::FInterpConstantTo(Alpha, Target, DeltaSeconds, 1.f / FMath::Max(TravelSeconds, 0.1f));
	ApplyAlpha();
	if (FMath::IsNearlyEqual(Alpha, Target))
	{
		SetActorTickEnabled(false);
	}
}

void AHHGarageDoor::ApplyAlpha()
{
	// Squash towards the top edge: what is left of the door hangs under the drum.
	const float Remaining = FMath::Max(0.03f, 1.f - Alpha * 0.97f);
	Mesh->SetRelativeScale3D(FVector(BaseScale.X, BaseScale.Y, BaseScale.Z * Remaining));
	Mesh->SetRelativeLocation(BaseLocation + FVector(0.f, 0.f, DoorHeight * BaseScale.Z * (1.f - Remaining)));
}

#undef LOCTEXT_NAMESPACE
