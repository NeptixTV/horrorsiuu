#include "UI/Widgets/SHHInventoryPanel.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Heist/HHHeistGameState.h"
#include "Player/HHInventoryComponent.h"
#include "Player/HHPlayerController.h"
#include "Player/HHPlayerState.h"
#include "Data/HHEquipmentDefinition.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Data/HHLootDefinition.h"
#include "Settings/HHInputSubsystem.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHInventoryPanel"

SHHInventoryPanel::~SHHInventoryPanel()
{
	if (UHHInventoryComponent* Inventory = BoundInventory.Get())
	{
		Inventory->OnInventoryChanged.Remove(InventoryHandle);
	}
}

UHHInventoryComponent* SHHInventoryPanel::GetInventory() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	const AHHPlayerController* PC = LobbyHUD ? LobbyHUD->GetHHController() : nullptr;
	const AHHPlayerState* State = PC ? PC->GetHHPlayerState() : nullptr;
	return State ? State->GetInventory() : nullptr;
}

void SHHInventoryPanel::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;
	if (UHHInventoryComponent* Inventory = GetInventory())
	{
		BoundInventory = Inventory;
		InventoryHandle = Inventory->OnInventoryChanged.AddLambda([this](UHHInventoryComponent*) { Rebuild(); });
	}
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;

	TSharedRef<SWidget> Header = HHUI::PanelHeader(FText::FromString(TEXT("TAB")), LOCTEXT("Title", "YOUR BAG"),
		LOCTEXT("Subtitle", "Five slots. The gear you brought takes room too - drop what you don't need."));

	TSharedRef<SWidget> Body = SNew(SScrollBox)
		.ScrollBarThickness(FVector2D(2.0, 2.0))
		+ SScrollBox::Slot()
		[
			SAssignNew(Rows, SVerticalBox)
		];

	TSharedRef<SWidget> Footer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text_Lambda([WeakHUD, this]()
			{
				const UHHInventoryComponent* Inventory = GetInventory();
				const AHHLobbyHUD* LobbyHUD = WeakHUD.Get();
				const AHHHeistGameState* Heist = LobbyHUD ? LobbyHUD->GetHeistState() : nullptr;
				const int32 Carried = Inventory ? Inventory->GetCarriedLootValue() : 0;
				return Heist
					? FText::Format(LOCTEXT("Totals", "On you: {0}.  In the van: {1} of {2}.  Loot only counts once it is in the van."),
						HHText::Money(Carried), HHText::Money(Heist->GetSecuredValue()), HHText::Money(Heist->GetMinimumHaul()))
					: FText::Format(LOCTEXT("TotalsHub", "On you: {0}."), HHText::Money(Carried));
			})
			.Font(FHHStyle::Font(EHHFont::Body, 13.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)
		[
			SNew(SHHKeyHint).Key(LOCTEXT("Esc", "ESC")).Label(LOCTEXT("Back", "Back"))
		];

	ChildSlot
	[
		HHUI::PanelFrame(Header, Body, Footer)
	];
	Rebuild();
}

void SHHInventoryPanel::Rebuild()
{
	if (!Rows.IsValid())
	{
		return;
	}
	Rows->ClearChildren();
	Brushes.Reset();
	const UHHInventoryComponent* Inventory = GetInventory();
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	if (!Inventory || !Inventory->IsEnabled() || !Registry)
	{
		Rows->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoBag", "You carry a bag on jobs. In the hideout, pick your gear at the locker."))
			.Font(FHHStyle::Font(EHHFont::Typewriter, 15.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		];
		return;
	}

	for (int32 Index = 0; Index < UHHInventoryComponent::NumSlots; ++Index)
	{
		const FHHInventorySlot& Slot = Inventory->GetSlot(Index);
		const bool bSelected = Index == Inventory->GetSelectedIndex();
		const UHHEquipmentDefinition* Equipment = Slot.Kind == EHHInventoryKind::Equipment ? Registry->FindEquipment(Slot.ItemId) : nullptr;
		const UHHLootDefinition* Loot = Slot.Kind == EHHInventoryKind::Loot ? Registry->FindLoot(Slot.ItemId) : nullptr;

		FText Kind;
		FText Detail;
		FText Note;
		FLinearColor KindColor = FHHStyle::TextFaint();
		if (Equipment)
		{
			Kind = HHText::EquipmentSlotName(Equipment->Slot).ToUpper();
			KindColor = FHHStyle::Info();
			Detail = Equipment->Description;
			if (Equipment->GetStat(TEXT("Charges")) > 0.f || Equipment->GetStat(TEXT("Shots")) > 0.f)
			{
				Note = FText::Format(LOCTEXT("Charges", "{0} left"), FText::AsNumber(Slot.Value));
			}
		}
		else if (Loot)
		{
			Kind = LOCTEXT("LootKind", "LOOT");
			KindColor = FHHStyle::Accent();
			Detail = Loot->Description;
			Note = Loot->bHeavy ? FText::Format(LOCTEXT("Heavy", "{0}  -  heavy, slows you down"), HHText::Money(Slot.Value)) : HHText::Money(Slot.Value);
		}
		else
		{
			Kind = LOCTEXT("EmptyKind", "EMPTY");
			Detail = LOCTEXT("EmptyDetail", "Room for one more thing.");
		}

		TSharedPtr<FSlateBrush> Icon = HHUI::MakeTextureBrush(Inventory->GetSlotIcon(Index), FVector2D(64.0, 64.0));
		Brushes.Add(Icon);

		TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
		if (!Slot.IsEmpty())
		{
			if (!bSelected)
			{
				Buttons->AddSlot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SHHActionButton).Text(LOCTEXT("Hold", "HOLD")).Kind(EHHButtonKind::Secondary).MinWidth(90.f)
					.OnClicked_Lambda([this, Index]() { Hold(Index); })
				];
			}
			Buttons->AddSlot().AutoWidth()
			[
				SNew(SHHActionButton).Text(LOCTEXT("Drop", "DROP")).Kind(EHHButtonKind::Ghost).MinWidth(90.f)
				.OnClicked_Lambda([this, Index]() { Drop(Index); })
			];
		}

		Rows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(SBorder)
			.BorderImage(FHHStyle::Brush(bSelected ? "HH.CardSelected" : "HH.Card"))
			.Padding(FMargin(14.f, 12.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::AsNumber(Index + 1))
					.Font(FHHStyle::Font(EHHFont::MonoMedium, 18.f))
					.ColorAndOpacity(FSlateColor(bSelected ? FHHStyle::Accent() : FHHStyle::TextFaint()))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f, 0.f)
				[
					SNew(SBox).WidthOverride(64.f).HeightOverride(64.f)
					[
						Icon.IsValid() ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Icon.Get())) : StaticCastSharedRef<SWidget>(SNew(SBox))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Kind)
						.Font(FHHStyle::Font(EHHFont::CondensedSemi, 10.f, 260))
						.ColorAndOpacity(FSlateColor(KindColor))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(Slot.IsEmpty() ? LOCTEXT("Nothing", "-") : Inventory->GetSlotName(Index))
						.Font(FHHStyle::Font(EHHFont::BodyMedium, 17.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Detail)
						.AutoWrapText(true)
						.Font(FHHStyle::Font(EHHFont::Body, 13.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(Note)
						.Visibility(Note.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
						.Font(FHHStyle::Font(EHHFont::MonoMedium, 13.f))
						.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
				[
					Buttons
				]
			]
		];
	}
}

void SHHInventoryPanel::Hold(int32 Index)
{
	if (UHHInventoryComponent* Inventory = GetInventory())
	{
		Inventory->RequestSelect(Index);
		HHUI::PlayUISound(EHHUISound::InventorySelect);
		Rebuild();
	}
}

void SHHInventoryPanel::Drop(int32 Index)
{
	if (UHHInventoryComponent* Inventory = GetInventory())
	{
		// Select first: the server drops the selected slot (both requests arrive in order).
		Inventory->RequestSelect(Index);
		Inventory->RequestDrop();
		HHUI::PlayUISound(EHHUISound::Click);
	}
}

#undef LOCTEXT_NAMESPACE
