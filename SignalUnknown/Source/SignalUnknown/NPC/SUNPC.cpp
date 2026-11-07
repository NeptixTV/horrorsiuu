#include "NPC/SUNPC.h"
#include "SignalUnknown.h"
#include "Player/SUCharacter.h"
#include "Core/SUGameState.h"
#include "Core/SUPlayerController.h"
#include "GasStation/SUFuelPump.h"
#include "GasStation/SUShopCounter.h"
#include "World/SUAssetLibrary.h"
#include "Audio/SUProceduralAudioComponent.h"
#include "Core/SUGameUserSettings.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ASUNPC::ASUNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(32.f, 90.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 240.f, 0.f);
	Move->MaxWalkSpeed = 120.f;
	Move->bUseRVOAvoidance = false;
}

void ASUNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUNPC, NPCRole);
	DOREPLIFETIME(ASUNPC, DisplayName);
	DOREPLIFETIME(ASUNPC, bDaytimeOnly);
	DOREPLIFETIME(ASUNPC, bAway);
}

void ASUNPC::BeginPlay()
{
	Super::BeginPlay();
	BuildBody();
	LastLocation = GetActorLocation();
	if (GetNetMode() != NM_DedicatedServer)
	{
		VoiceAudio = NewObject<USUProceduralAudioComponent>(this);
		VoiceAudio->SetupAttachment(GetCapsuleComponent());
		VoiceAudio->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
		VoiceAudio->ConfigureSound(ESUSoundKind::RadioVoice, SUAudioCategory::Voice, 0.7f, 1500.f);
		VoiceAudio->RegisterComponent();
		VoiceAudio->StartSound();
		VoiceAudio->SetSoundParam(SULogic::Dsp::Param::B, NPCRole == ESUNPCRole::Clerk ? 1.2f : 0.9f);
		VoiceAudio->SetSoundParam(SULogic::Dsp::Param::C, 1.f);
		VoiceAudio->SetSoundParam(SULogic::Dsp::Param::D, 0.f);

		FootstepAudio = NewObject<USUProceduralAudioComponent>(this);
		FootstepAudio->SetupAttachment(GetCapsuleComponent());
		FootstepAudio->SetRelativeLocation(FVector(0.f, 0.f, -85.f));
		FootstepAudio->ConfigureSound(ESUSoundKind::Footstep, SUAudioCategory::Effects, 0.5f, 1200.f);
		FootstepAudio->RegisterComponent();
		FootstepAudio->StartSound();
	}
	if (HasAuthority())
	{
		UpdateDaytime();
	}
	OnRep_Away();
}

void ASUNPC::BuildBody()
{
	USUAssetLibrary* Lib = USUAssetLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	auto Part = [this, Lib](ESUShape Shape, const FVector& Loc, const FVector& Size, ESUMat Mat, const FRotator& Rot = FRotator::ZeroRotator) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(GetCapsuleComponent());
		C->SetStaticMesh(Lib->GetMesh(Shape));
		C->SetMaterial(0, Lib->GetMaterial(Mat));
		C->SetRelativeTransform(FTransform(Rot, Loc, Size / 100.f));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		BodyParts.Add(C);
		return C;
	};
	// PLACEHOLDER figure (to be replaced by a skeletal mesh with animations)
	const ESUMat Shirt = NPCRole == ESUNPCRole::Clerk ? ESUMat::GasRed : ESUMat::FabricBlue;
	const ESUMat Trousers = NPCRole == ESUNPCRole::Clerk ? ESUMat::CarDark : ESUMat::Cloth;
	Part(ESUShape::Cube, FVector(0.f, 0.f, 22.f), FVector(26.f, 40.f, 62.f), Shirt);                 // torso
	Part(ESUShape::Cube, FVector(0.f, 0.f, -10.f), FVector(24.f, 38.f, 16.f), Trousers);            // hips
	Part(ESUShape::Sphere, FVector(0.f, 0.f, 70.f), FVector(22.f, 20.f, 26.f), ESUMat::Skin);       // head
	Part(ESUShape::Sphere, FVector(-2.f, 0.f, 79.f), FVector(23.f, 21.f, 14.f), NPCRole == ESUNPCRole::Clerk ? ESUMat::WoodDark : ESUMat::MetalDark); // hair
	Part(ESUShape::Cube, FVector(10.f, -5.f, 72.f), FVector(2.f, 3.f, 3.f), ESUMat::Black);         // eyes
	Part(ESUShape::Cube, FVector(10.f, 5.f, 72.f), FVector(2.f, 3.f, 3.f), ESUMat::Black);
	if (NPCRole == ESUNPCRole::Clerk)
	{
		Part(ESUShape::Cube, FVector(13.5f, -10.f, 36.f), FVector(1.f, 10.f, 4.f), ESUMat::White);  // name badge
	}
	LegLeft = Part(ESUShape::Cylinder, FVector(0.f, -10.f, -52.f), FVector(13.f, 13.f, 76.f), Trousers);
	LegRight = Part(ESUShape::Cylinder, FVector(0.f, 10.f, -52.f), FVector(13.f, 13.f, 76.f), Trousers);
	ArmLeft = Part(ESUShape::Cylinder, FVector(0.f, -26.f, 22.f), FVector(10.f, 10.f, 60.f), Shirt);
	ArmRight = Part(ESUShape::Cylinder, FVector(0.f, 26.f, 22.f), FVector(10.f, 10.f, 60.f), Shirt);
}

// ------------------------------------------------------------------------------------ interaction

bool ASUNPC::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	if (bAway)
	{
		return false;
	}
	OutPrompt.Primary = FText::FromString(FString::Printf(TEXT("Mit %s sprechen"), *DisplayName.ToString()));
	OutPrompt.bPrimary = true;
	if (NPCRole == ESUNPCRole::Clerk)
	{
		OutPrompt.Info = SUText(TEXT("Kassiererin – zum Bezahlen an die Kasse"));
	}
	return true;
}

void ASUNPC::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	if (!HasAuthority() || !User || bAway)
	{
		return;
	}
	FaceActor(User, 6.f);
	SayTo(User, PickLine(User));
}

FString ASUNPC::Greeting() const
{
	const ASUGameState* GS = ASUGameState::Get(this);
	const float H = GS ? GS->GetTimeOfDay() : 12.f;
	if (H >= 5.f && H < 11.f)
	{
		return TEXT("Morgen.");
	}
	if (H >= 11.f && H < 18.f)
	{
		return TEXT("Tag.");
	}
	return TEXT("'n Abend.");
}

FString ASUNPC::PickLine(const ASUCharacter* Listener)
{
	const ASUGameState* GS = ASUGameState::Get(this);
	const int32 Story = GS ? GS->GetStoryStage() : 0;
	++TalkCounter;

	if (NPCRole == ESUNPCRole::Clerk)
	{
		// open fuel bill first
		for (TActorIterator<ASUShopCounter> It(GetWorld()); It; ++It)
		{
			const int32 Due = It->GetOpenFuelCents();
			if (Due > 0)
			{
				return FString::Printf(TEXT("%s %.1f Liter, das macht %s. Bezahlen können Sie vorne an der Kasse."), *Greeting(), It->GetOpenFuelLiters(), *SUFormatMoney(Due));
			}
			break;
		}
		if (GS && GS->GetUnpaidFuelIncidents() > 0 && TalkCounter % 3 == 1)
		{
			return TEXT("Ich hab Ihr Kennzeichen notiert, als Sie letztes Mal ohne zu zahlen vom Hof sind. Nur dass Sie's wissen.");
		}
		static const TCHAR* Lines[] = {
			TEXT("Kaffee ist frisch. Benzin ist teuer. Wie immer."),
			TEXT("Hallberg war letzte Woche noch hier. Hat Batterien gekauft – eine ganze Kiste."),
			TEXT("Nachts kommt hier kaum noch jemand vorbei. Seit dem Sommer ist es still geworden."),
			TEXT("Sie sind die neuen Ranger oben auf dem Turm? Dann passen Sie gut auf sich auf."),
			TEXT("Wenn Sie draußen jemand ruft, den Sie nicht sehen – antworten Sie nicht. Hat meine Großmutter immer gesagt."),
		};
		static const TCHAR* LateLines[] = {
			TEXT("Über die Nordstraße fahr ich nicht mehr. Nicht nach Einbruch der Dunkelheit."),
			TEXT("Gestern stand einer da draußen an der Waldkante. Hat sich nicht bewegt. Stundenlang."),
		};
		if (Story >= 1 && TalkCounter % 2 == 0)
		{
			return LateLines[(TalkCounter / 2) % UE_ARRAY_COUNT(LateLines)];
		}
		return FString::Printf(TEXT("%s %s"), *Greeting(), Lines[TalkCounter % UE_ARRAY_COUNT(Lines)]);
	}

	static const TCHAR* ResidentLines[] = {
		TEXT("Der Förster? Hab ihn seit Tagen nicht gesehen. Sein Licht brennt auch nicht mehr."),
		TEXT("Meine Katze ist weg. Die dritte dieses Jahr."),
		TEXT("Nachts klopft es manchmal. Dreimal. Ich mach nicht auf."),
		TEXT("Sie sollten nicht so spät noch draußen sein. Hier ist es nach Sonnenuntergang nicht gut."),
	};
	if (Listener && Listener->GetVehicle() == nullptr && GS && GS->GetDarkness() > 0.6f)
	{
		return TEXT("Gehen Sie nach Hause. Bitte.");
	}
	return FString::Printf(TEXT("%s %s"), *Greeting(), ResidentLines[TalkCounter % UE_ARRAY_COUNT(ResidentLines)]);
}

void ASUNPC::SayTo(ASUCharacter* Listener, const FString& Text, float Duration)
{
	if (!HasAuthority() || !Listener)
	{
		return;
	}
	FSUSpeechLine Line;
	Line.Speaker = DisplayName;
	Line.Text = FText::FromString(Text);
	Line.Duration = Duration;
	if (ASUPlayerController* PC = Cast<ASUPlayerController>(Listener->GetController()))
	{
		PC->ClientSpeech(Line, false);
	}
	MulticastSpeak(Line.GetDuration());
}

void ASUNPC::MulticastSpeak_Implementation(float Duration)
{
	if (const UWorld* World = GetWorld())
	{
		SpeakUntil = World->GetTimeSeconds() + Duration;
	}
}

void ASUNPC::FaceActor(AActor* Target, float Seconds)
{
	FaceTarget = Target;
	FaceTimer = Seconds;
	WaitTimer = FMath::Max(WaitTimer, Seconds);
}

void ASUNPC::Summon()
{
	if (!HasAuthority() || WanderPoints.Num() == 0)
	{
		return;
	}
	RouteIndex = 0;
	RouteDirection = 1;
	SummonTimer = 30.f;
	WaitTimer = 0.f;
}

bool ASUNPC::IsAtHome(float Tolerance) const
{
	return WanderPoints.Num() > 0 && FVector::Dist2D(GetActorLocation(), WanderPoints[0]) < Tolerance;
}

// ------------------------------------------------------------------------------------ tick

void ASUNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		NextAwayCheck -= DeltaSeconds;
		if (NextAwayCheck <= 0.f)
		{
			NextAwayCheck = 5.f;
			UpdateDaytime();
		}
		if (!bAway)
		{
			UpdateRoute(DeltaSeconds);
		}
	}
	if (!bAway)
	{
		AnimateBody(DeltaSeconds);
	}
}

void ASUNPC::UpdateDaytime()
{
	if (!bDaytimeOnly)
	{
		return;
	}
	const ASUGameState* GS = ASUGameState::Get(this);
	const float H = GS ? GS->GetTimeOfDay() : 12.f;
	const bool bShouldBeAway = H < 8.f || H >= 19.f || (GS && GS->GetRain() > 0.6f);
	if (bShouldBeAway == bAway)
	{
		return;
	}
	if (bShouldBeAway && !IsAtHome(150.f))
	{
		// walk to the front door first
		RouteIndex = 0;
		WaitTimer = 0.f;
		SummonTimer = 10.f;
		return;
	}
	bAway = bShouldBeAway;
	if (!bAway && WanderPoints.Num() > 0)
	{
		SetActorLocation(WanderPoints[0], false, nullptr, ETeleportType::TeleportPhysics);
	}
	OnRep_Away();
}

void ASUNPC::OnRep_Away()
{
	SetActorHiddenInGame(bAway);
	SetActorEnableCollision(!bAway);
	GetCharacterMovement()->SetMovementMode(bAway ? MOVE_None : MOVE_Walking);
}

void ASUNPC::UpdateRoute(float DeltaSeconds)
{
	SummonTimer = FMath::Max(0.f, SummonTimer - DeltaSeconds);

	// turn to the player we talk to
	if (FaceTimer > 0.f)
	{
		FaceTimer -= DeltaSeconds;
		if (AActor* T = FaceTarget.Get())
		{
			const FRotator Yaw(0.f, (T->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), Yaw, DeltaSeconds, 5.f));
		}
		return;
	}
	if (WaitTimer > 0.f)
	{
		WaitTimer -= DeltaSeconds;
		return;
	}
	if (WanderPoints.Num() == 0)
	{
		return;
	}
	// a customer waits at the register: stay at home
	const bool bStayHome = SummonTimer > 0.f;
	const FVector Target = WanderPoints[FMath::Clamp(RouteIndex, 0, WanderPoints.Num() - 1)];
	FVector To = Target - GetActorLocation();
	To.Z = 0.f;
	if (To.Size() < 45.f)
	{
		if (bStayHome && RouteIndex == 0)
		{
			WaitTimer = 1.f;
			return;
		}
		// reached: pause at the ends of the route and at home, then continue
		const bool bEnd = RouteIndex == 0 || RouteIndex == WanderPoints.Num() - 1;
		WaitTimer = bEnd ? FMath::FRandRange(6.f, 16.f) : FMath::FRandRange(0.f, 1.5f);
		if (WanderPoints.Num() > 1)
		{
			if (RouteIndex + RouteDirection < 0 || RouteIndex + RouteDirection >= WanderPoints.Num())
			{
				RouteDirection = -RouteDirection;
			}
			RouteIndex += RouteDirection;
		}
		StuckTimer = 0.f;
		return;
	}
	AddMovementInput(To.GetSafeNormal(), 1.f);

	// direct steering can get stuck on furniture: give up on that point after a while
	const float Moved = FVector::Dist2D(GetActorLocation(), LastLocation);
	LastLocation = GetActorLocation();
	StuckTimer = Moved < 20.f * DeltaSeconds ? StuckTimer + DeltaSeconds : 0.f;
	if (StuckTimer > 3.f)
	{
		StuckTimer = 0.f;
		RouteDirection = -RouteDirection;
		RouteIndex = FMath::Clamp(RouteIndex + RouteDirection, 0, WanderPoints.Num() - 1);
		WaitTimer = 1.f;
	}
}

void ASUNPC::AnimateBody(float DeltaSeconds)
{
	const float Speed = GetVelocity().Size2D();
	if (Speed > 10.f)
	{
		WalkPhase += DeltaSeconds * Speed / 22.f;
	}
	else
	{
		WalkPhase = FMath::FInterpTo(WalkPhase, FMath::RoundToFloat(WalkPhase / UE_PI) * UE_PI, DeltaSeconds, 6.f);
	}
	const float Swing = FMath::Sin(WalkPhase) * FMath::Clamp(Speed / 120.f, 0.f, 1.f) * 24.f;
	auto Pivot = [](UStaticMeshComponent* C, const FVector& Hip, float Pitch, float Length)
	{
		if (C)
		{
			// rotate around the hip / shoulder (top end of the cylinder)
			const FRotator R(Pitch, 0.f, 0.f);
			C->SetRelativeLocationAndRotation(Hip + R.RotateVector(FVector(0.f, 0.f, -Length * 0.5f)), R);
		}
	};
	Pivot(LegLeft, FVector(0.f, -10.f, -14.f), Swing, 76.f);
	Pivot(LegRight, FVector(0.f, 10.f, -14.f), -Swing, 76.f);
	Pivot(ArmLeft, FVector(0.f, -26.f, 50.f), -Swing * 0.7f, 60.f);
	Pivot(ArmRight, FVector(0.f, 26.f, 50.f), Swing * 0.7f, 60.f);

	if (FootstepAudio && Speed > 30.f)
	{
		StepTimer -= DeltaSeconds;
		if (StepTimer <= 0.f)
		{
			StepTimer = 0.55f;
			FootstepAudio->TriggerSound(NPCRole == ESUNPCRole::Clerk ? 3 : 0, 0.6f);
		}
	}
	if (VoiceAudio)
	{
		const UWorld* World = GetWorld();
		const bool bSpeaking = World && World->GetTimeSeconds() < SpeakUntil;
		VoiceAudio->SetSoundParam(SULogic::Dsp::Param::A, bSpeaking ? 1.f : 0.f);
	}
}
