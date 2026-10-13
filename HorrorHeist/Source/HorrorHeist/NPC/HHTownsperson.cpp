#include "NPC/HHTownsperson.h"
#include "NPC/HHNpcBody.h"
#include "Data/HHCharacterDefinition.h"
#include "Heist/HHHeistGameState.h"
#include "Player/HHCosmeticComponent.h"
#include "World/HHTimeOfDay.h"
#include "World/HHSwing.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AHHTownsperson::AHHTownsperson()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f;
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Cosmetics = CreateDefaultSubobject<UHHCosmeticComponent>(TEXT("Cosmetics"));
}

float AHHTownsperson::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : GetWorld()->GetTimeSeconds();
}

void AHHTownsperson::BeginPlay()
{
	Super::BeginPlay();
	BaseYaw = GetActorRotation().Yaw;
	Root->SetRelativeScale3D(FVector(FMath::Clamp(BodyScale, 0.3f, 1.5f)));
	if (const UHHCharacterDefinition* Definition = HHNpcBody::Setup(Body, Cosmetics, CosmeticIds))
	{
		UAnimSequence* Anim = nullptr;
		if (Pose == TEXT("Walk"))
		{
			Anim = Definition->Walk.LoadSynchronous();
		}
		else if (Pose == TEXT("Run"))
		{
			Anim = Definition->Run.LoadSynchronous();
		}
		else if (Pose != TEXT("Idle"))
		{
			Anim = Definition->FindExtraAnim(Pose);
		}
		if (!Anim)
		{
			Anim = Definition->Idle.LoadSynchronous();
		}
		HHNpcBody::PlayLoop(Body, Anim);
		bAnimating = Anim != nullptr;
	}
	GatherPath();
	if (IsWalking() && bAnimating)
	{
		// Feet match the ground: authored speed scaled by body size.
		Body->SetPlayRate(WalkSpeed / FMath::Max(1.f, AnimSpeed * FMath::Clamp(BodyScale, 0.3f, 1.5f)));
	}
	// Desynchronise identical idles; people who just stand there need no fast tick.
	if (bAnimating && !IsWalking())
	{
		Body->SetPosition(FMath::FRandRange(0.f, 3.f), false);
	}
	if (!IsWalking())
	{
		SetActorTickInterval(bWatchPlayers ? 0.05f : 1.f);
	}
	// Riding a swing: hang from its seat and go with it.
	if (!SwingName.IsNone())
	{
		for (TActorIterator<AHHSwing> It(GetWorld()); It; ++It)
		{
			if (It->GetFName() == SwingName || It->ActorHasTag(SwingName))
			{
				AttachToComponent(It->GetSeat(), FAttachmentTransformRules::KeepWorldTransform);
				break;
			}
		}
	}
	UpdatePresence();
}

void AHHTownsperson::GatherPath()
{
	PathPoints.Reset();
	PathLengths.Reset();
	PathTotal = 0.f;
	if (PathTag.IsNone())
	{
		return;
	}
	TMap<int32, FVector> ByIndex;
	const FString Prefix = PathTag.ToString() + TEXT("_");
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			const FString Text = Tag.ToString();
			if (Text.StartsWith(Prefix) && Text.Mid(Prefix.Len()).IsNumeric())
			{
				ByIndex.Add(FCString::Atoi(*Text.Mid(Prefix.Len())), It->GetActorLocation());
			}
		}
	}
	for (int32 Index = 0; ByIndex.Contains(Index); ++Index)
	{
		PathPoints.Add(ByIndex[Index]);
	}
	if (PathPoints.Num() < 2)
	{
		PathPoints.Reset();
		return;
	}
	for (int32 Index = 0; Index < PathPoints.Num(); ++Index)
	{
		const float Length = FVector::Dist(PathPoints[Index], PathPoints[(Index + 1) % PathPoints.Num()]);
		PathLengths.Add(Length);
		PathTotal += Length;
	}
}

void AHHTownsperson::SetSceneActive(bool bActive)
{
	bSceneActive = bActive;
	UpdatePresence();
}

void AHHTownsperson::UpdatePresence()
{
	bool bShow = true;
	if (!SceneTag.IsNone())
	{
		bShow = bSceneActive;
	}
	if (bHubOnly && AHHHeistGameState::IsJobWorld(this))
	{
		bShow = false;
	}
	else if (const AHHTimeOfDay* Clock = AHHTimeOfDay::Get(this))
	{
		if (VisibleWhen == EHHIntelTime::DayOnly)
		{
			bShow &= !Clock->IsNight();
		}
		else if (VisibleWhen == EHHIntelTime::NightOnly)
		{
			bShow &= Clock->IsNight();
		}
	}
	if (bShow != bShown)
	{
		bShown = bShow;
		SetActorHiddenInGame(!bShow);
	}
}

void AHHTownsperson::UpdateWalk()
{
	if (!IsWalking() || PathTotal <= 1.f)
	{
		return;
	}
	float Along = FMath::Fmod(GetServerTime() * WalkSpeed + PathOffset * PathTotal, PathTotal);
	for (int32 Index = 0; Index < PathPoints.Num(); ++Index)
	{
		if (Along <= PathLengths[Index] || Index == PathPoints.Num() - 1)
		{
			const FVector A = PathPoints[Index];
			const FVector B = PathPoints[(Index + 1) % PathPoints.Num()];
			const float Alpha = PathLengths[Index] > 0.f ? FMath::Clamp(Along / PathLengths[Index], 0.f, 1.f) : 0.f;
			const FVector Direction = (B - A).GetSafeNormal2D();
			// Round the corners a little: blend into the next segment's heading near its start.
			SetActorLocationAndRotation(FMath::Lerp(A, B, Alpha), FRotator(0.f, Direction.Rotation().Yaw, 0.f));
			return;
		}
		Along -= PathLengths[Index];
	}
}

void AHHTownsperson::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CheckTimer -= DeltaSeconds;
	if (CheckTimer <= 0.f)
	{
		CheckTimer = 1.f;
		UpdatePresence();
	}
	if (!bShown)
	{
		return;
	}
	if (IsWalking())
	{
		UpdateWalk();
		return;
	}
	if (bWatchPlayers)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		float TargetYaw = BaseYaw;
		if (Pawn && FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) < FMath::Square(600.f))
		{
			const float Wanted = (Pawn->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
			TargetYaw = BaseYaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(BaseYaw, Wanted), -60.f, 60.f);
		}
		const float Current = GetActorRotation().Yaw;
		const float Step = FMath::Clamp(FMath::FindDeltaAngleDegrees(Current, TargetYaw), -40.f * DeltaSeconds, 40.f * DeltaSeconds);
		if (!FMath::IsNearlyZero(Step, 0.001f))
		{
			SetActorRotation(FRotator(0.f, Current + Step, 0.f));
		}
	}
}
