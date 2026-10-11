#include "UI/Widgets/SHHHeistHud.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Heist/HHHeistGameState.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Player/HHInventoryComponent.h"
#include "Data/HHItemRegistrySubsystem.h"
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

	static FText StageName(int32 Stage)
	{
		switch (Stage)
		{
		case 1:		return LOCTEXT("Stage1", "uneasy");
		case 2:		return LOCTEXT("Stage2", "disturbed");
		case 3:		return LOCTEXT("Stage3", "hostile");
		case 4:		return LOCTEXT("Stage4", "it showed itself");
		default:	return LOCTEXT("Stage0", "quiet");
		}
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
	];
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
				return State ? FText::Format(LOCTEXT("AtVan", "AT THE VAN  {0}/{1}"), FText::AsNumber(State->GetCrewAtVan()), FText::AsNumber(State->GetCrewTotal())) : FText::GetEmpty();
			})
			.Font(FHHStyle::Font(EHHFont::CondensedSemi, 12.f, 240))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
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
			return State && (State->GetAlert() > 1.f || State->IsPoliceCalled()) && State->GetHeistPhase() != EHHHeistPhase::Ended
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
				.Visibility_Lambda([this]() { const AHHHeistGameState* State = GetState(); return State && State->IsPoliceCalled() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(FHHStyle::WithAlpha(FHHStyle::Danger(), 0.6f + 0.4f * FMath::Abs(FMath::Sin(static_cast<float>(Now) * 4.f)))); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					const AHHHeistGameState* State = GetState();
					if (State && State->IsPoliceCalled())
					{
						return FText::Format(LOCTEXT("Police", "POLICE ARRIVE IN {0}"), HHHud::Duration(State->GetPoliceRemaining()));
					}
					return LOCTEXT("Neighbours", "THE NEIGHBOURS");
				})
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 13.f, 300))
				.ColorAndOpacity_Lambda([this]()
				{
					const AHHHeistGameState* State = GetState();
					return FSlateColor(State && (State->IsPoliceCalled() || State->GetAlert() > 60.f) ? FHHStyle::Danger() : FHHStyle::TextDim());
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
				const UHHLootDefinition* Loot = Registry ? Registry->FindLoot(Inventory->GetSelected().ItemId) : nullptr;
				const FText Name = Inventory->GetSlotName(Inventory->GetSelectedIndex());
				return Loot && Loot->bHeavy ? FText::Format(LOCTEXT("HeavyName", "{0}  -  heavy"), Name) : Name;
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
						.Text_Lambda([WeakMember]() { const AHHPlayerState* M = WeakMember.Get(); return M && M->IsInsideHouse() ? LOCTEXT("Inside", "INSIDE") : FText::GetEmpty(); })
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

// ---------------------------------------------------------------------------------------
// Summary

TSharedRef<SWidget> SHHHeistHud::MakeSummary(const FHHMissionSummary& Summary, TWeakObjectPtr<AHHLobbyHUD> OwnerHUD)
{
	const UHHItemRegistrySubsystem* Registry = OwnerHUD.IsValid() ? UHHItemRegistrySubsystem::Get(OwnerHUD.Get()) : nullptr;
	const UHHMissionDefinition* Job = Registry ? Registry->FindMission(Summary.MissionId) : nullptr;
	const bool bGood = Summary.Outcome == EHHMissionOutcome::Success;
	const bool bBad = Summary.Outcome == EHHMissionOutcome::Caught || Summary.Outcome == EHHMissionOutcome::Abandoned;
	const FLinearColor OutcomeColor = bGood ? FHHStyle::Ready() : (bBad ? FHHStyle::Danger() : FHHStyle::Accent());

	auto Stat = [](const FText& Label, const FText& Value, const FLinearColor& Color)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Label).Font(FHHStyle::Font(EHHFont::CondensedSemi, 11.f, 260)).ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Value).Font(FHHStyle::Font(EHHFont::MonoMedium, 22.f)).ColorAndOpacity(FSlateColor(Color))
			];
	};

	TSharedRef<SVerticalBox> LootList = SNew(SVerticalBox);
	const int32 Shown = FMath::Min(Summary.Loot.Num(), 9);
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		const FHHLootRecord& Record = Summary.Loot[Index];
		const UHHItemDefinition* Item = Registry ? Registry->FindItem(Record.ItemId) : nullptr;
		LootList->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(Item ? Item->DisplayName : FText::FromName(Record.ItemId)).Font(FHHStyle::Font(EHHFont::Body, 14.f)).ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(16.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString(Record.StolenBy)).Font(FHHStyle::Font(EHHFont::Condensed, 12.f)).ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(16.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(90.f).HAlign(HAlign_Right)
				[
					SNew(STextBlock).Text(HHText::Money(Record.Value)).Font(FHHStyle::Font(EHHFont::Mono, 13.f)).ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
				]
			]
		];
	}
	if (Summary.Loot.Num() > Shown)
	{
		LootList->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("MoreLoot", "... and {0} more"), FText::AsNumber(Summary.Loot.Num() - Shown)))
			.Font(FHHStyle::Font(EHHFont::Condensed, 12.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
		];
	}
	if (Summary.Loot.Num() == 0)
	{
		LootList->AddSlot().AutoHeight()
		[
			SNew(STextBlock).Text(LOCTEXT("NoLoot", "Nothing made it into the van.")).Font(FHHStyle::Font(EHHFont::Typewriter, 14.f)).ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		];
	}

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(FHHStyle::Brush("HH.White")).ColorAndOpacity(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.82f)))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(760.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Job ? Job->DisplayName.ToUpper() : LOCTEXT("JobOver", "THE JOB")).Font(FHHStyle::Font(EHHFont::Condensed, 14.f, 400)).ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock).Text(HHText::OutcomeName(Summary.Outcome).ToUpper()).Font(FHHStyle::Font(EHHFont::Display, 54.f, 200)).ColorAndOpacity(FSlateColor(OutcomeColor))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[Stat(LOCTEXT("SumHaul", "IN THE VAN"), HHText::Money(Summary.SecuredValue), FHHStyle::Text())]
					+ SHorizontalBox::Slot().FillWidth(1.f)[Stat(LOCTEXT("SumPayout", "YOUR CUT"), HHText::Money(Summary.PayoutPerPlayer), FHHStyle::Accent())]
					+ SHorizontalBox::Slot().FillWidth(1.f)[Stat(LOCTEXT("SumXP", "REPUTATION"), FText::Format(LOCTEXT("XPFmt", "+{0} XP"), FText::AsNumber(Summary.XPPerPlayer)), FHHStyle::Text())]
					+ SHorizontalBox::Slot().FillWidth(1.f)[Stat(LOCTEXT("SumTime", "TIME INSIDE"), HHHud::Duration(Summary.DurationSeconds), FHHStyle::Text())]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("HouseState", "The house was {0} when you left."), HHHud::StageName(Summary.HauntStage)))
					.Font(FHHStyle::Font(EHHFont::Typewriter, 15.f))
					.ColorAndOpacity(FSlateColor(Summary.HauntStage >= 3 ? FHHStyle::Danger() : FHHStyle::TextDim()))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Visibility(Summary.LostValue > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					.Text(FText::Format(LOCTEXT("Lost", "Left behind: {0}"), HHText::Money(Summary.LostValue)))
					.Font(FHHStyle::Font(EHHFont::Body, 14.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 24.f, 0.f, 8.f)
				[
					HHUI::SectionLabel(LOCTEXT("TheHaul", "THE HAUL"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					LootList
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 28.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Returning", "Back to the hideout shortly..."))
					.Font(FHHStyle::Font(EHHFont::Condensed, 13.f, 200))
					.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
