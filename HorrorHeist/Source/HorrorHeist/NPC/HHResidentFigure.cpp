#include "NPC/HHResidentFigure.h"
#include "NPC/HHNpcBody.h"
#include "Data/HHCharacterDefinition.h"
#include "Heist/HHHeistGameState.h"
#include "Player/HHCosmeticComponent.h"
#include "World/HHTimeOfDay.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

AHHResidentFigure::AHHResidentFigure()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Cosmetics = CreateDefaultSubobject<UHHCosmeticComponent>(TEXT("Cosmetics"));
}

void AHHResidentFigure::BeginPlay()
{
	Super::BeginPlay();
	BaseYaw = GetActorRotation().Yaw;
	if (const UHHCharacterDefinition* Definition = HHNpcBody::Setup(Body, Cosmetics, CosmeticIds))
	{
		HHNpcBody::PlayLoop(Body, Definition->Idle.LoadSynchronous());
	}
	UpdatePresence();
}

void AHHResidentFigure::UpdatePresence()
{
	bool bShow = true;
	if (bHubOnly && AHHHeistGameState::IsJobWorld(this))
	{
		bShow = false;
	}
	else if (const AHHTimeOfDay* Clock = AHHTimeOfDay::Get(this))
	{
		if (VisibleWhen == EHHIntelTime::DayOnly)
		{
			bShow = !Clock->IsNight();
		}
		else if (VisibleWhen == EHHIntelTime::NightOnly)
		{
			bShow = Clock->IsNight();
		}
	}
	if (bShow != bShown)
	{
		bShown = bShow;
		SetActorHiddenInGame(!bShow);
	}

	// Who is she looking at? The closest local view on the street.
	Watched.Reset();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (bShown && Pawn && FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) < FMath::Square(NoticeRadius))
	{
		Watched = const_cast<APawn*>(Pawn);
	}
}

void AHHResidentFigure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CheckTimer -= DeltaSeconds;
	if (CheckTimer <= 0.f)
	{
		CheckTimer = 0.5f;
		UpdatePresence();
	}
	if (!bShown)
	{
		return;
	}

	float TargetYaw = BaseYaw;
	if (const AActor* Target = Watched.Get())
	{
		const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
		const float Wanted = ToTarget.Rotation().Yaw;
		TargetYaw = BaseYaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(BaseYaw, Wanted), -MaxTurn, MaxTurn);
	}
	const float Current = GetActorRotation().Yaw;
	const float Step = FMath::Clamp(FMath::FindDeltaAngleDegrees(Current, TargetYaw), -TurnSpeed * DeltaSeconds, TurnSpeed * DeltaSeconds);
	if (!FMath::IsNearlyZero(Step, 0.001f))
	{
		SetActorRotation(FRotator(0.f, Current + Step, 0.f));
	}
}
