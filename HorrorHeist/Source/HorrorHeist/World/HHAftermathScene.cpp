#include "World/HHAftermathScene.h"
#include "Lobby/HHLobbyGameState.h"
#include "NPC/HHTownsperson.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHMissionDefinition.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "HHAftermath"

namespace HHAftermathTags
{
	static const TCHAR* Variants[] = { TEXT(""), TEXT("_Police"), TEXT("_Body"), TEXT("_Road"), TEXT("_Haunt") };

	static FName Make(FName Base, const TCHAR* Suffix)
	{
		return FName(*(Base.ToString() + Suffix));
	}
}

AHHAftermathScene::AHHAftermathScene()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakeLight = [this](const TCHAR* Name, const FLinearColor& Color, const FVector& Offset, float Intensity, float Radius)
	{
		UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(Offset);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(Intensity);
		Light->SetAttenuationRadius(Radius);
		Light->SetLightColor(Color);
		Light->SetCastShadows(false);
		Light->SetVisibility(false);
		return Light;
	};
	LightRed = MakeLight(TEXT("LightRed"), FLinearColor(1.f, 0.05f, 0.03f), FVector(0.f, -35.f, 0.f), 450.f, 1800.f);
	LightBlue = MakeLight(TEXT("LightBlue"), FLinearColor(0.05f, 0.2f, 1.f), FVector(0.f, 35.f, 0.f), 450.f, 1800.f);
	CameraFlash = MakeLight(TEXT("CameraFlash"), FLinearColor(0.95f, 0.97f, 1.f), FVector::ZeroVector, 3000.f, 900.f);

	Radio = CreateDefaultSubobject<UAudioComponent>(TEXT("Radio"));
	Radio->SetupAttachment(Root);
	Radio->bAutoActivate = false;
}

void AHHAftermathScene::BeginPlay()
{
	Super::BeginPlay();
	CameraFlash->SetRelativeLocation(FlashOffset);
	// Everything hidden until the lobby says otherwise (job maps never do).
	Apply(TSet<FName>());
	Refresh();
}

void AHHAftermathScene::Refresh()
{
	TSet<FName> Active;
	const AHHLobbyGameState* Lobby = GetWorld() ? GetWorld()->GetGameState<AHHLobbyGameState>() : nullptr;
	if (Lobby && !SceneTag.IsNone())
	{
		const FHHAftermath& Aftermath = Lobby->GetAftermath();
		if (Aftermath.IsActive() && Aftermath.MissionId == MissionId)
		{
			Active.Add(SceneTag);
			if (Aftermath.bPolice)
			{
				Active.Add(HHAftermathTags::Make(SceneTag, TEXT("_Police")));
			}
			if (Aftermath.bDeath)
			{
				Active.Add(HHAftermathTags::Make(SceneTag, TEXT("_Body")));
			}
			if (Aftermath.bRoadClosed)
			{
				Active.Add(HHAftermathTags::Make(SceneTag, TEXT("_Road")));
			}
			if (Aftermath.bHaunting && !Aftermath.bDeath)
			{
				Active.Add(HHAftermathTags::Make(SceneTag, TEXT("_Haunt")));
			}
		}
	}
	if (!bApplied || !Active.Includes(Shown) || !Shown.Includes(Active))
	{
		Apply(Active);
	}
}

void AHHAftermathScene::Apply(const TSet<FName>& Active)
{
	bApplied = true;
	Shown = Active;
	TSet<FName> All;
	for (const TCHAR* Suffix : HHAftermathTags::Variants)
	{
		All.Add(HHAftermathTags::Make(SceneTag, Suffix));
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this)
		{
			continue;
		}
		if (AHHTownsperson* Person = Cast<AHHTownsperson>(Actor))
		{
			if (All.Contains(Person->GetSceneTag()))
			{
				Person->SetSceneActive(Active.Contains(Person->GetSceneTag()));
			}
			continue;
		}
		for (const FName& Tag : Actor->Tags)
		{
			if (All.Contains(Tag))
			{
				const bool bShow = Active.Contains(Tag);
				Actor->SetActorHiddenInGame(!bShow);
				Actor->SetActorEnableCollision(bShow);
				break;
			}
		}
	}
	bPoliceLights = Active.Contains(HHAftermathTags::Make(SceneTag, TEXT("_Police")));
	bPhotographer = bPoliceLights;
	bRoadClosed = Active.Contains(HHAftermathTags::Make(SceneTag, TEXT("_Road")));
	LightRed->SetVisibility(false);
	LightBlue->SetVisibility(false);
	CameraFlash->SetVisibility(false);
	if (Radio->GetSound())
	{
		if (bPoliceLights && !Radio->IsPlaying())
		{
			Radio->Play();
		}
		else if (!bPoliceLights && Radio->IsPlaying())
		{
			Radio->Stop();
		}
	}
}

void AHHAftermathScene::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshTimer -= DeltaSeconds;
	if (RefreshTimer <= 0.f)
	{
		RefreshTimer = 1.f;
		Refresh();
	}
	if (bPoliceLights)
	{
		FlashTimer += DeltaSeconds;
		if (FlashTimer > 0.3f)
		{
			FlashTimer = 0.f;
			bFlashPhase = !bFlashPhase;
			LightRed->SetVisibility(bFlashPhase);
			LightBlue->SetVisibility(!bFlashPhase);
		}
	}
	if (bPhotographer)
	{
		if (FlashOff > 0.f)
		{
			FlashOff -= DeltaSeconds;
			if (FlashOff <= 0.f)
			{
				CameraFlash->SetVisibility(false);
			}
		}
		NextPhoto -= DeltaSeconds;
		if (NextPhoto <= 0.f)
		{
			NextPhoto = FMath::FRandRange(3.5f, 7.f);
			FlashOff = 0.08f;
			CameraFlash->SetVisibility(true);
			if (FlashSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, FlashSound, CameraFlash->GetComponentLocation(), 0.6f);
			}
		}
	}
}

bool AHHAftermathScene::IsNodeClosed(const UObject* WorldContextObject, FName NodeId)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || NodeId.IsNone())
	{
		return false;
	}
	for (TActorIterator<AHHAftermathScene> It(World); It; ++It)
	{
		// Police at work in the street (or a full road block): the town's traffic keeps away.
		if ((It->bRoadClosed || It->bPoliceLights) && It->ClosedNodes.Contains(NodeId))
		{
			return true;
		}
	}
	return false;
}

FText AHHAftermathScene::Headline(const UObject* WorldContextObject, const FHHMissionSummary& Summary)
{
	const FHHAftermath Aftermath = FHHAftermath::FromSummary(Summary);
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(WorldContextObject);
	const UHHMissionDefinition* Mission = Registry ? Registry->FindMission(Summary.MissionId) : nullptr;
	const FText Place = Mission ? Mission->Address.ToUpper() : LOCTEXT("SomeHouse", "A HOLLOWMERE HOME");
	bool bCaught = false;
	for (const FHHCrewRecord& Record : Summary.Crew)
	{
		bCaught |= Record.Fate == EHHCrewFate::Caught;
	}
	if (Aftermath.bDeath)
	{
		return FText::Format(LOCTEXT("HeadDeath", "BURGLAR FOUND DEAD AT {0}"), Place);
	}
	if (Summary.Outcome == EHHMissionOutcome::ChaseBusted)
	{
		return LOCTEXT("HeadChase", "NIGHT CHASE ENDS IN ARRESTS");
	}
	if (bCaught)
	{
		return FText::Format(LOCTEXT("HeadCaught", "ARREST AFTER BREAK-IN AT {0}"), Place);
	}
	if (Aftermath.bPolice)
	{
		return FText::Format(LOCTEXT("HeadPolice", "POLICE HUNT BURGLARS AFTER {0} BREAK-IN"), Place);
	}
	if (Aftermath.bNoticed)
	{
		return Aftermath.bHaunting
			? FText::Format(LOCTEXT("HeadBurgledHaunt", "{0} BURGLED - OWNERS BLAME 'SOMETHING ELSE'"), Place)
			: FText::Format(LOCTEXT("HeadBurgled", "{0} BURGLED IN THE NIGHT"), Place);
	}
	if (Aftermath.bHaunting)
	{
		return LOCTEXT("HeadHaunt", "'SOMETHING IS IN THE HOUSE'");
	}
	return LOCTEXT("HeadQuiet", "QUIET NIGHT IN HOLLOWMERE");
}

FText AHHAftermathScene::Story(const UObject* WorldContextObject, const FHHMissionSummary& Summary)
{
	const FHHAftermath Aftermath = FHHAftermath::FromSummary(Summary);
	const UHHItemRegistrySubsystem* Registry = UHHItemRegistrySubsystem::Get(WorldContextObject);
	const UHHMissionDefinition* Mission = Registry ? Registry->FindMission(Summary.MissionId) : nullptr;
	const FText Place = Mission ? Mission->Address : LOCTEXT("SomeHouseLower", "a house in town");
	if (Aftermath.bDeath)
	{
		return FText::Format(LOCTEXT("StoryDeath", "Police were called to {0} at dawn, where an unidentified intruder was found dead. "
			"\"No wounds, no struggle,\" said an officer. \"The face, though.\" The street remains closed."), Place);
	}
	if (Summary.Outcome == EHHMissionOutcome::ChaseBusted)
	{
		return LOCTEXT("StoryChase", "A van fleeing a break-in was boxed in by a patrol car after a chase through town. "
			"Its passengers are being questioned.");
	}
	if (Aftermath.bPolice)
	{
		return FText::Format(LOCTEXT("StoryPolice", "Officers spent the morning at {0}, photographing footprints and dusting doors. "
			"Neighbours are asked to report anyone \"loitering after dark\"."), Place);
	}
	if (Aftermath.bNoticed)
	{
		return Aftermath.bHaunting
			? FText::Format(LOCTEXT("StoryBurgledHaunt", "The owners of {0} say valuables vanished overnight - \"and the doors were all locked from the inside.\" "
				"They no longer sleep upstairs."), Place)
			: FText::Format(LOCTEXT("StoryBurgled", "The owners of {0} woke to find valuables missing. No one heard a thing."), Place);
	}
	if (Aftermath.bHaunting)
	{
		return FText::Format(LOCTEXT("StoryHaunt", "Residents of {0} report voices, slamming doors and \"someone standing at the end of the hall\". "
			"Father Doyle has been asked to visit."), Place);
	}
	return LOCTEXT("StoryQuiet", "Nothing to report. Bake sale at St. Agnes on Sunday.");
}

#undef LOCTEXT_NAMESPACE
