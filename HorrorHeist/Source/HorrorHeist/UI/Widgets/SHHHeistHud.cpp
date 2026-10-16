#include "UI/Widgets/SHHHeistHud.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Heist/HHHeistGameState.h"
#include "Heist/HHEscapeZone.h"
#include "NPC/HHNeighbourNPC.h"
#include "Player/HHCharacter.h"
#include "Vehicles/HHVanVehicle.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Player/HHInventoryComponent.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHEquipmentDefinition.h"
#include "Data/HHLootDefinition.h"
#include "Data/HHMissionDefinition.h"
#include "Settings/HHInputSubsystem.h"
#include "World/HHTimeOfDay.h"
#include "Core/HHDeveloperSettings.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Styling/StyleDefaults.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHHeistHud"

namespace HHHud
{
	/** Thin horizontal meter (0..1). */
	static TSharedRef<SWidget> Meter(TAttribute<float> Value, TAttribute<FSlateColor> Color, float Width, float Height)
	{
		return SNew(SBox)
			.WidthOverride(Width)
			.HeightOverride(Height)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Value]() { return FMath::Max(FMath::Clamp(Value.Get(), 0.f, 1.f), 0.001f); }))
				[
					SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(Color)
				]
				+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Value]() { return FMath::Max(1.f - FMath::Clamp(Value.Get(), 0.f, 1.f), 0.001f); }))
				[
					SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.1f)))
				]
			];
	}

	static TSharedRef<SWidget> Label(const TAttribute<FText>& Text, float Size = 11.f, const FLinearColor& Color = FHHStyle::TextFaint())
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FHHStyle::Font(EHHFont::CondensedSemi, Size, 260))
			.ColorAndOpacity(FSlateColor(Color))
			.ShadowOffset(FVector2D(0.0, 1.0))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	}

	static FText Duration(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::RoundToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
	}
}

void SHHHeistHud::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;
	SlotBrushes.SetNum(UHHInventoryComponent::NumSlots);

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)

		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(36.f, 30.f, 0.f, 0.f))
		[
			BuildObjectives()
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 30.f, 36.f, 0.f))
		[
			BuildTopRight()
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.f, 30.f, 0.f, 0.f))
		[
			BuildAlert()
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(36.f, 120.f, 0.f, 0.f))
		[
			SAssignNew(CrewBox, SVerticalBox)
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(36.f, 0.f, 0.f, 34.f))
		[
			BuildPsyche()
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 26.f))
		[
			BuildHotbar()
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 36.f, 150.f))
		[
			BuildNoiseMeter()
		]
		// Arrested: you watch the rest of the job from the back of the patrol car.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this]()
			{
				const AHHLobbyHUD* LobbyHUD = HUD.Get();
				const AHHCharacter* Character = LobbyHUD && LobbyHUD->GetHHController() ? Cast<AHHCharacter>(LobbyHUD->GetHHController()->GetPawn()) : nullptr;
				const AHHHeistGameState* State = GetState();
				return Character && Character->IsArrested() && State && State->GetHeistPhase() != EHHHeistPhase::Ended ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Arrested", "ARRESTED"))
				.Font(FHHStyle::Font(EHHFont::Display, 54.f, 300))
				.ColorAndOpacity(FSlateColor(FHHStyle::Danger()))
				.ShadowOffset(FVector2D(0.0, 2.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ArrestedBody", "In the back of the patrol car. The crew can still get away with the van."))
				.Font(FHHStyle::Font(EHHFont::Typewriter, 16.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
				.ShadowOffset(FVector2D(0.0, 1.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
			]
		]
		// Down on the floor / dead and watching the others.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.f, 150.f, 0.f, 0.f))
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this]()
			{
				const AHHCharacter* Character = GetOwnCharacter();
				const AHHHeistGameState* State = GetState();
				return Character && Character->IsIncapacitated() && State && State->GetHeistPhase() != EHHHeistPhase::Ended
					? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					const AHHCharacter* Character = GetOwnCharacter();
					return Character && Character->IsDead() ? LOCTEXT("Dead", "DEAD") : LOCTEXT("Downed", "DOWN");
				})
				.Font(FHHStyle::Font(EHHFont::Display, 54.f, 300))
				.ColorAndOpacity(FSlateColor(FHHStyle::Danger()))
				.ShadowOffset(FVector2D(0.0, 2.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Text_Lambda([this]()
				{
					const AHHCharacter* Character = GetOwnCharacter();
					if (!Character)
					{
						return FText::GetEmpty();
					}
					if (Character->IsDowned())
					{
						return FText::Format(LOCTEXT("DownedBody", "{0} s. Crawl towards your crew - one of them has to kneel next to you and hold {1}."),
							FText::AsNumber(FMath::CeilToInt(Character->GetBleedOutRemaining())), GetKeyText(TEXT("Interact")));
					}
					const AHHLobbyHUD* LobbyHUD = HUD.Get();
					const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
					const APawn* Watched = PC ? PC->GetSpectated() : nullptr;
					const APlayerState* WatchedState = Watched ? Watched->GetPlayerState() : nullptr;
					if (!WatchedState)
					{
						return LOCTEXT("DeadAlone", "Nobody left to watch. The house is quiet now.");
					}
					return FText::Format(LOCTEXT("DeadBody", "Watching {0}. {1}: next crew member. Your gear stays in the house."),
						FText::FromString(WatchedState->GetPlayerName()), GetKeyText(TEXT("Interact")));
				})
				.Font(FHHStyle::Font(EHHFont::Typewriter, 16.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
				.ShadowOffset(FVector2D(0.0, 1.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
			]
		]
	];
}

AHHCharacter* SHHHeistHud::GetOwnCharacter() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
	return PC ? Cast<AHHCharacter>(PC->GetPawn()) : nullptr;
}

FText SHHHeistHud::GetKeyText(const TCHAR* Binding) const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
	const UHHInputSubsystem* Input = PC ? UHHInputSubsystem::Get(PC) : nullptr;
	return Input ? Input->GetKeyDisplayText(FName(Binding)) : FText::FromString(TEXT("E"));
}

AHHHeistGameState* SHHHeistHud::GetState() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	return LobbyHUD ? LobbyHUD->GetHeistState() : nullptr;
}

AHHPlayerState* SHHHeistHud::GetMyState() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
	return PC ? PC->GetHHPlayerState() : nullptr;
}

UHHInventoryComponent* SHHHeistHud::GetInventory() const
{
	const AHHPlayerState* State = GetMyState();
	return State ? State->GetInventory() : nullptr;
}

// ---------------------------------------------------------------------------------------
// Objectives (top left)

TSharedRef<SWidget> SHHHeistHud::BuildObjectives()
{
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	auto Mission = [WeakHUD]() -> const UHHMissionDefinition*
	{
		const AHHLobbyHUD* LobbyHUD = WeakHUD.Get();
		const AHHHeistGameState* State = LobbyHUD ? LobbyHUD->GetHeistState() : nullptr;
		const UHHItemRegistrySubsystem* Registry = LobbyHUD ? UHHItemRegistrySubsystem::Get(LobbyHUD) : nullptr;
		return State && Registry ? Registry->FindMission(State->GetSelectedMission()) : nullptr;
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(2.f)
			[
				SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(FHHStyle::WithAlpha(FHHStyle::Accent(), 0.8f)))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([Mission]() { const UHHMissionDefinition* M = Mission(); return M ? M->DisplayName.ToUpper() : LOCTEXT("TheJob", "THE JOB"); })
				.Font(FHHStyle::Font(EHHFont::Display, 18.f, 120))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
				.ShadowOffset(FVector2D(0.0, 1.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 10.f)
			[
				SNew(STextBlock)
				.Text_Lambda([Mission]() { const UHHMissionDefinition* M = Mission(); return M ? M->Address : FText::GetEmpty(); })
				.Font(FHHStyle::Font(EHHFont::Typewriter, 13.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
				.ShadowOffset(FVector2D(0.0, 1.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(ObjectiveBox, SVerticalBox)
			]
		];
}

void SHHHeistHud::RebuildObjectives()
{
	const AHHHeistGameState* State = GetState();
	if (!State || !ObjectiveBox.IsValid())
	{
		return;
	}
	ObjectiveBox->ClearChildren();
	for (const FHHObjective& Objective : State->GetObjectives())
	{
		const bool bDone = Objective.bDone;
		const bool bUrgent = Objective.Id == TEXT("Police");
		const FLinearColor Color = bUrgent ? FHHStyle::Danger() : (bDone ? FHHStyle::TextFaint() : (Objective.bOptional ? FHHStyle::TextDim() : FHHStyle::Text()));
		ObjectiveBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(14.f).HeightOverride(14.f)
				[
					bDone
					? StaticCastSharedRef<SWidget>(SNew(SImage).Image(FHHStyle::Brush("HH.Icon.Check")).ColorAndOpacity(FSlateColor(FHHStyle::Accent())))
					: StaticCastSharedRef<SWidget>(SNew(SBorder).BorderImage(FHHStyle::Brush("HH.Outline")).Padding(0.f))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Objective.Text)
				.Font(FHHStyle::Font(bUrgent ? EHHFont::BodySemi : EHHFont::BodyMedium, 14.f))
				.ColorAndOpacity(FSlateColor(Color))
				.ShadowOffset(FVector2D(0.0, 1.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f))
			]
		];
	}
}

// ---------------------------------------------------------------------------------------
// Clock + haul (top right)

TSharedRef<SWidget> SHHHeistHud::BuildTopRight()
{
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	auto Clock = [WeakHUD]() -> const AHHTimeOfDay* { return WeakHUD.IsValid() ? AHHTimeOfDay::Get(WeakHUD.Get()) : nullptr; };

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image_Lambda([Clock]() { const AHHTimeOfDay* C = Clock(); return FHHStyle::Brush(C && C->GetDaylight() > 0.4f ? "HH.Icon.Sun" : "HH.Icon.Moon"); })
				.DesiredSizeOverride(FVector2D(18.0, 18.0))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([Clock]() { const AHHTimeOfDay* C = Clock(); return C ? HHText::ClockTime(C->GetTimeOfDay()) : FText::GetEmpty(); })
				.Font(FHHStyle::Font(EHHFont::MonoMedium, 20.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
				.ShadowOffset(FVector2D(0.0, 1.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 16.f, 0.f, 0.f)
		[
			HHHud::Label(LOCTEXT("InTheVan", "IN THE VAN"))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				const AHHHeistGameState* State = GetState();
				return State ? FText::Format(LOCTEXT("Haul", "{0} / {1}"), HHText::Money(State->GetSecuredValue()), HHText::Money(State->GetMinimumHaul())) : FText::GetEmpty();
			})
			.Font(FHHStyle::Font(EHHFont::MonoMedium, 16.f))
			.ColorAndOpacity_Lambda([this]()
			{
				const AHHHeistGameState* State = GetState();
				return FSlateColor(State && State->GetSecuredValue() >= State->GetMinimumHaul() ? FHHStyle::Ready() : FHHStyle::Accent());
			})
			.ShadowOffset(FVector2D(0.0, 1.0))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 10.f, 0.f, 0.f)
		[
			HHHud::Label(LOCTEXT("Carrying", "YOU CARRY"))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				const UHHInventoryComponent* Inventory = GetInventory();
				if (!Inventory)
				{
					return FText::GetEmpty();
				}
				int32 Used = 0;
				for (const FHHInventorySlot& Slot : Inventory->GetSlots())
				{
					Used += Slot.IsEmpty() ? 0 : 1;
				}
				return FText::Format(LOCTEXT("Carry", "{0}   ({1}/{2} slots)"), HHText::Money(Inventory->GetCarriedLootValue()),
					FText::AsNumber(Used), FText::AsNumber(UHHInventoryComponent::NumSlots));
			})
			.Font(FHHStyle::Font(EHHFont::Mono, 14.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			.ShadowOffset(FVector2D(0.0, 1.0))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]()
			{
				const AHHHeistGameState* State = GetState();
				return State && (State->GetHeistPhase() == EHHHeistPhase::Escape || State->IsPoliceCalled() || State->GetCrewAtVan() > 0)
					? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			.Text_Lambda([this]()
			{
				const AHHHeistGameState* State = GetState();
				return State ? FText::Format(LOCTEXT("AtVan", "IN THE VAN  {0}/{1}"), FText::AsNumber(State->GetCrewAtVan()), FText::AsNumber(State->GetCrewTotal())) : FText::GetEmpty();
			})
			.Font(FHHStyle::Font(EHHFont::CondensedSemi, 12.f, 240))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		]
		// Where the van has to go, and how far it is.
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return GetState() && AHHEscapeZone::Find(HUD.Get(), GetState()->GetSafehouse()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.Text_Lambda([this]()
			{
				const AHHHeistGameState* State = GetState();
				const AHHEscapeZone* Zone = State ? AHHEscapeZone::Find(HUD.Get(), State->GetSafehouse()) : nullptr;
				const AHHLobbyHUD* LobbyHUD = HUD.Get();
				const APawn* Pawn = LobbyHUD && LobbyHUD->GetHHController() ? LobbyHUD->GetHHController()->GetPawn() : nullptr;
				if (!Zone || !Pawn)
				{
					return FText::GetEmpty();
				}
				const FVector Delta = Zone->GetActorLocation() - Pawn->GetActorLocation();
				// Compass in town terms: north is +Y in the plans, which is -Y in the engine.
				static const TCHAR* Points[] = { TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW"), TEXT("N"), TEXT("NE") };
				const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
				const int32 Sector = (FMath::RoundToInt(Angle / 45.f) % 8 + 8) % 8;
				return FText::Format(LOCTEXT("Getaway", "GETAWAY  {0}  -  {1} M {2}"), Zone->GetDisplayName().ToUpper(),
					FText::AsNumber(FMath::RoundToInt(Delta.Size2D() / 100.f)), FText::FromString(Points[Sector]));
			})
			.Font(FHHStyle::Font(EHHFont::CondensedSemi, 12.f, 240))
			.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
			.ShadowOffset(FVector2D(0.0, 1.0))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		];
}

// ---------------------------------------------------------------------------------------
// Alert / police (top centre)

TSharedRef<SWidget> SHHHeistHud::BuildAlert()
{
	return SNew(SVerticalBox)
		.Visibility_Lambda([this]()
		{
			const AHHHeistGameState* State = GetState();
			return State && (State->GetAlert() > 1.f || State->IsPoliceCalled() || State->GetPoliceState() != EHHPoliceState::None) && State->GetHeistPhase() != EHHHeistPhase::Ended
				? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FHHStyle::Brush("HH.Icon.Siren"))
				.DesiredSizeOverride(FVector2D(16.0, 16.0))
				.Visibility_Lambda([this]() { const AHHHeistGameState* State = GetState(); return State && (State->IsPoliceCalled() || State->GetPoliceState() != EHHPoliceState::None) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(FHHStyle::WithAlpha(FHHStyle::Danger(), 0.6f + 0.4f * FMath::Abs(FMath::Sin(static_cast<float>(Now) * 4.f)))); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					const AHHHeistGameState* State = GetState();
					if (!State)
					{
						return FText::GetEmpty();
					}
					switch (State->GetPoliceState())
					{
					case EHHPoliceState::Patrol:	return LOCTEXT("PolicePatrol", "PATROL CAR IN THE STREET - STAY OUT OF SIGHT");
					case EHHPoliceState::AtHouse:	return LOCTEXT("PoliceAtHouse", "POLICE AT THE HOUSE");
					case EHHPoliceState::Chasing:	return LOCTEXT("PoliceChase", "POLICE ON YOUR TAIL - LOSE THEM");
					case EHHPoliceState::Searching:	return LOCTEXT("PoliceSearch", "POLICE SEARCHING THE AREA");
					default:						break;
					}
					if (State->IsPoliceCalled())
					{
						return State->GetPoliceRemaining() > 0.f
							? FText::Format(LOCTEXT("Police", "POLICE ARRIVE IN {0}"), HHHud::Duration(State->GetPoliceRemaining()))
							: LOCTEXT("PoliceAny", "POLICE ANY MOMENT");
					}
					return LOCTEXT("Neighbours", "THE NEIGHBOURS");
				})
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 13.f, 300))
				.ColorAndOpacity_Lambda([this]()
				{
					const AHHHeistGameState* State = GetState();
					return FSlateColor(State && (State->IsPoliceCalled() || State->GetPoliceState() != EHHPoliceState::None || State->GetAlert() > 60.f) ? FHHStyle::Danger() : FHHStyle::TextDim());
				})
				.ShadowOffset(FVector2D(0.0, 1.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f, 0.f, 0.f)
		[
			HHHud::Meter(
				TAttribute<float>::CreateLambda([this]() { const AHHHeistGameState* State = GetState(); return State ? State->GetAlert() / 100.f : 0.f; }),
				TAttribute<FSlateColor>::CreateLambda([this]()
				{
					const AHHHeistGameState* State = GetState();
					const float Alert = State ? State->GetAlert() / 100.f : 0.f;
					return FSlateColor(FMath::Lerp(FHHStyle::Accent(), FHHStyle::Danger(), Alert));
				}),
				220.f, 3.f)
		]
		// The neighbour is out looking: how close he is to recognising someone.
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this]()
			{
				const AHHNeighbourNPC* Npc = HUD.IsValid() ? AHHNeighbourNPC::Get(HUD.Get()) : nullptr;
				return Npc && Npc->IsOut() && Npc->GetState() != EHHNeighbourState::Fleeing ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					const AHHNeighbourNPC* Npc = HUD.IsValid() ? AHHNeighbourNPC::Get(HUD.Get()) : nullptr;
					return Npc ? FText::Format(LOCTEXT("NeighbourLooking", "{0} IS LOOKING AROUND"), Npc->GetDisplayName().ToUpper()) : FText::GetEmpty();
				})
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 12.f, 260))
				.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
				.ShadowOffset(FVector2D(0.0, 1.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f, 0.f, 0.f)
			[
				HHHud::Meter(
					TAttribute<float>::CreateLambda([this]() { const AHHNeighbourNPC* Npc = HUD.IsValid() ? AHHNeighbourNPC::Get(HUD.Get()) : nullptr; return Npc ? Npc->GetSuspicion() : 0.f; }),
					TAttribute<FSlateColor>(FSlateColor(FHHStyle::Danger())), 140.f, 3.f)
			]
		];
}

// ---------------------------------------------------------------------------------------
// Psyche (bottom left)

TSharedRef<SWidget> SHHHeistHud::BuildPsyche()
{
	auto Psyche = [this]() { const AHHPlayerState* State = GetMyState(); return State ? State->GetPsyche() / 100.f : 1.f; };
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FHHStyle::Brush("HH.Icon.Brain"))
			.DesiredSizeOverride(FVector2D(30.0, 30.0))
			.ColorAndOpacity_Lambda([this, Psyche]()
			{
				const float Value = Psyche();
				const float Pulse = Value < 0.3f ? 0.55f + 0.45f * FMath::Abs(FMath::Sin(static_cast<float>(Now) * (2.f + (0.3f - Value) * 12.f))) : 1.f;
				return FSlateColor(FHHStyle::WithAlpha(FMath::Lerp(FHHStyle::Danger(), FHHStyle::TextDim(), FMath::Clamp(Value * 2.f, 0.f, 1.f)), Pulse));
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				HHHud::Label(LOCTEXT("Psyche", "PSYCHE"), 13.f, FHHStyle::Text())
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f)
			[
				HHHud::Meter(TAttribute<float>::CreateLambda(Psyche),
					TAttribute<FSlateColor>::CreateLambda([Psyche]() { return FSlateColor(FMath::Lerp(FHHStyle::Danger(), FHHStyle::Text(), FMath::Clamp(Psyche() * 1.6f, 0.f, 1.f))); }),
					260.f, 4.f)
			]
		];
}

// ---------------------------------------------------------------------------------------
// Hotbar (bottom)

TSharedRef<SWidget> SHHHeistHud::BuildSlot(int32 Index)
{
	auto IsSelected = [this, Index]() { const UHHInventoryComponent* Inventory = GetInventory(); return Inventory && Inventory->GetSelectedIndex() == Index; };
	return SNew(SBox)
		.WidthOverride(66.f)
		.HeightOverride(66.f)
		[
			SNew(SBorder)
			.BorderImage_Lambda([IsSelected]() { return FHHStyle::Brush(IsSelected() ? "HH.CardSelected" : "HH.Card"); })
			.Padding(FMargin(0.f))
			[
				SNew(SOverlay)
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(6.f)
				[
					SNew(SImage)
					.Image_Lambda([this, Index]() { return SlotBrushes.IsValidIndex(Index) && SlotBrushes[Index].IsValid() ? SlotBrushes[Index].Get() : nullptr; })
					.DesiredSizeOverride(FVector2D(50.0, 50.0))
				]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(5.f, 2.f))
				[
					SNew(STextBlock)
					.Text(FText::AsNumber(Index + 1))
					.Font(FHHStyle::Font(EHHFont::Mono, 10.f))
					.ColorAndOpacity_Lambda([IsSelected]() { return FSlateColor(IsSelected() ? FHHStyle::Accent() : FHHStyle::TextFaint()); })
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(4.f, 2.f))
				[
					SNew(STextBlock)
					.Text_Lambda([this, Index]()
					{
						const UHHInventoryComponent* Inventory = GetInventory();
						const FHHInventorySlot& Slot = Inventory ? Inventory->GetSlot(Index) : FHHInventorySlot();
						return Slot.IsLoot() ? HHText::Money(Slot.Value) : FText::GetEmpty();
					})
					.Font(FHHStyle::Font(EHHFont::Mono, 10.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
					.ShadowOffset(FVector2D(0.0, 1.0))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
				]
			]
		];
}

TSharedRef<SWidget> SHHHeistHud::BuildHotbar()
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < UHHInventoryComponent::NumSlots; ++Index)
	{
		Row->AddSlot().AutoWidth().Padding(4.f, 0.f)[BuildSlot(Index)];
	}

	return SNew(SVerticalBox)
		.Visibility_Lambda([this]() { const UHHInventoryComponent* Inventory = GetInventory(); return Inventory && Inventory->IsEnabled() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				const UHHInventoryComponent* Inventory = GetInventory();
				if (!Inventory || Inventory->GetSelected().IsEmpty())
				{
					return LOCTEXT("EmptyHands", "Empty hands");
				}
				const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
				const FHHInventorySlot& Selected = Inventory->GetSelected();
				const UHHLootDefinition* Loot = Registry ? Registry->FindLoot(Selected.ItemId) : nullptr;
				const FText Name = Inventory->GetSlotName(Inventory->GetSelectedIndex());
				if (Loot)
				{
					return Loot->bHeavy ? FText::Format(LOCTEXT("HeavyName", "{0}  -  heavy"), Name) : Name;
				}
				// Gear: charges left and the key that uses it.
				const UHHEquipmentDefinition* Equipment = Registry && Selected.Kind == EHHInventoryKind::Equipment ? Registry->FindEquipment(Selected.ItemId) : nullptr;
				if (!Equipment)
				{
					return Name;
				}
				static const TSet<FName> Usable = { TEXT("Light"), TEXT("Walkie"), TEXT("Salt"), TEXT("Camera") };
				FText Line = Name;
				if (Equipment->GetStat(TEXT("Charges")) > 0.f || Equipment->GetStat(TEXT("Shots")) > 0.f)
				{
					Line = FText::Format(LOCTEXT("ChargesLeft", "{0}  -  {1} left"), Line, FText::AsNumber(Selected.Value));
				}
				if (Usable.Contains(Equipment->ToolTag))
				{
					Line = FText::Format(LOCTEXT("UseKey", "{0}   [{1}] use"), Line, GetKeyText(TEXT("UseItem")));
				}
				return Line;
			})
			.Font(FHHStyle::Font(EHHFont::BodyMedium, 14.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			.ShadowOffset(FVector2D(0.0, 1.0))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			Row
		];
}

TSharedRef<SWidget> SHHHeistHud::BuildNoiseMeter()
{
	TSharedRef<SHorizontalBox> Segments = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Segments->AddSlot().AutoWidth().Padding(2.f, 0.f)
		[
			SNew(SBox).WidthOverride(26.f).HeightOverride(14.f)
			[
				SNew(SBorder)
				.BorderImage(FHHStyle::Brush("HH.White"))
				.BorderBackgroundColor_Lambda([this, Index]()
				{
					const float Reading = GetMeterReading();
					const FLinearColor Lit = Index >= 4 ? FHHStyle::Danger() : (Index >= 2 ? FHHStyle::Accent() : FHHStyle::Ready());
					return FSlateColor(Reading > Index + 0.15f ? Lit : FLinearColor(0.08f, 0.08f, 0.08f, 0.8f));
				})
			]
		];
	}
	return SNew(SBorder)
		.BorderImage(FHHStyle::Brush("HH.Card"))
		.Padding(FMargin(12.f, 10.f))
		.Visibility_Lambda([this]() { return GetHeldTool() == TEXT("NoiseMeter") ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Meter", "NOISE METER"))
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 11.f, 260))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 6.f)
			[
				Segments
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					const float Reading = GetMeterReading();
					if (Reading >= 4.8f)
					{
						return LOCTEXT("MeterHunt", "PINNED - GET OUT OR HIDE");
					}
					if (Reading >= 3.f)
					{
						return LOCTEXT("MeterNear", "Something is very close");
					}
					if (Reading >= 1.5f)
					{
						return LOCTEXT("MeterSome", "Something moved nearby");
					}
					return LOCTEXT("MeterQuiet", "Quiet");
				})
				.Font(FHHStyle::Font(EHHFont::Typewriter, 13.f))
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetMeterReading() >= 4.8f ? FHHStyle::Danger() : FHHStyle::Text()); })
			]
		];
}

float SHHHeistHud::GetMeterReading() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	return LobbyHUD ? LobbyHUD->GetNoiseMeterReading() : 0.f;
}

FName SHHHeistHud::GetHeldTool() const
{
	const UHHInventoryComponent* Inventory = GetInventory();
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	if (!Inventory || !Inventory->IsEnabled() || !Registry || Inventory->GetSelected().Kind != EHHInventoryKind::Equipment)
	{
		return NAME_None;
	}
	const UHHEquipmentDefinition* Equipment = Registry->FindEquipment(Inventory->GetSelected().ItemId);
	return Equipment ? Equipment->ToolTag : NAME_None;
}

void SHHHeistHud::RefreshSlots()
{
	const UHHInventoryComponent* Inventory = GetInventory();
	for (int32 Index = 0; Index < UHHInventoryComponent::NumSlots; ++Index)
	{
		UTexture2D* Icon = Inventory ? Inventory->GetSlotIcon(Index) : nullptr;
		SlotBrushes[Index] = Icon ? HHUI::MakeTextureBrush(Icon, FVector2D(50.0, 50.0)) : nullptr;
	}
}

// ---------------------------------------------------------------------------------------
// Crew (left)

void SHHHeistHud::RebuildCrew()
{
	const AHHHeistGameState* State = GetState();
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
	if (!State || !CrewBox.IsValid())
	{
		return;
	}
	CrewBox->ClearChildren();
	for (AHHPlayerState* Member : State->GetCrew())
	{
		TWeakObjectPtr<AHHPlayerState> WeakMember = Member;
		const bool bTalking = PC && PC->IsPlayerTalking(Member);
		CrewBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill)
			[
				SNew(SBox).WidthOverride(3.f)
				[
					SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(Member->GetCrewColor()))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Member->GetPlayerName()))
						.Font(FHHStyle::Font(EHHFont::BodyMedium, 13.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
						.ShadowOffset(FVector2D(0.0, 1.0))
						.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(SImage)
						.Image(FHHStyle::Brush("HH.Icon.Mic"))
						.DesiredSizeOverride(FVector2D(12.0, 12.0))
						.ColorAndOpacity(FSlateColor(FHHStyle::Ready()))
						.Visibility(bTalking ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text_Lambda([WeakMember]()
						{
							const AHHPlayerState* M = WeakMember.Get();
							const AHHCharacter* Body = M ? Cast<AHHCharacter>(M->GetPawn()) : nullptr;
							if (Body && Body->IsDead())
							{
								return LOCTEXT("CrewDead", "DEAD");
							}
							if (Body && Body->IsDowned())
							{
								return FText::Format(LOCTEXT("CrewDown", "DOWN {0}s"), FText::AsNumber(FMath::CeilToInt(Body->GetBleedOutRemaining())));
							}
							return M && M->IsInsideHouse() ? LOCTEXT("Inside", "INSIDE") : FText::GetEmpty();
						})
						.Font(FHHStyle::Font(EHHFont::CondensedSemi, 10.f, 200))
						.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					HHHud::Meter(
						TAttribute<float>::CreateLambda([WeakMember]() { const AHHPlayerState* M = WeakMember.Get(); return M ? M->GetPsyche() / 100.f : 0.f; }),
						TAttribute<FSlateColor>::CreateLambda([WeakMember]()
						{
							const AHHPlayerState* M = WeakMember.Get();
							return FSlateColor(FMath::Lerp(FHHStyle::Danger(), FHHStyle::TextDim(), FMath::Clamp(M ? M->GetPsyche() / 60.f : 0.f, 0.f, 1.f)));
						}),
						120.f, 2.f)
				]
			]
		];
	}
}

// ---------------------------------------------------------------------------------------
// Tick: rebuild lists when their content changed

void SHHHeistHud::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Now = InCurrentTime;

	if (const AHHHeistGameState* State = GetState())
	{
		int32 Signature = State->GetObjectives().Num();
		for (const FHHObjective& Objective : State->GetObjectives())
		{
			Signature = HashCombine(Signature, GetTypeHash(Objective.Text.ToString()));
			Signature = HashCombine(Signature, Objective.bDone ? 3 : 5);
		}
		if (Signature != ObjectiveSignature)
		{
			ObjectiveSignature = Signature;
			RebuildObjectives();
		}

		const AHHLobbyHUD* LobbyHUD = HUD.Get();
		const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
		int32 Crew = State->GetCrew().Num();
		for (const AHHPlayerState* Member : State->GetCrew())
		{
			Crew = HashCombine(Crew, GetTypeHash(Member->GetPlayerName()));
			Crew = HashCombine(Crew, (PC && PC->IsPlayerTalking(Member)) ? 11 : 7);
		}
		if (Crew != CrewSignature)
		{
			CrewSignature = Crew;
			RebuildCrew();
		}
	}

	if (const UHHInventoryComponent* Inventory = GetInventory())
	{
		int32 Signature = Inventory->GetSelectedIndex();
		for (const FHHInventorySlot& Slot : Inventory->GetSlots())
		{
			Signature = HashCombine(Signature, GetTypeHash(Slot.ItemId));
		}
		if (Signature != SlotSignature)
		{
			SlotSignature = Signature;
			RefreshSlots();
		}
	}
}

#undef LOCTEXT_NAMESPACE
