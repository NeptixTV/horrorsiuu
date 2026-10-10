#include "UI/Widgets/SHHLoadoutPanel.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Data/HHEquipmentDefinition.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Progression/HHProfileSubsystem.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHLoadoutPanel"

SHHLoadoutPanel::~SHHLoadoutPanel()
{
	if (UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr)
	{
		Profile->OnProfileChanged.Remove(ProfileHandle);
	}
}

void SHHLoadoutPanel::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;

	if (UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr)
	{
		ProfileHandle = Profile->OnProfileChanged.AddSP(this, &SHHLoadoutPanel::Refresh);
		Inspected = Profile->GetEquipment().Get(SelectedSlot);
	}

	TSharedRef<SWidget> Body = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.3f).Padding(0.f, 0.f, 22.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				HHUI::SectionLabel(LOCTEXT("Slots", "LOADOUT"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(SlotList, SVerticalBox)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(0.38f).Padding(0.f, 0.f, 22.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				HHUI::SectionLabel(LOCTEXT("Gear", "IN THE LOCKER"))
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SScrollBox)
				.ScrollBarThickness(FVector2D(2.0, 2.0))
				+ SScrollBox::Slot()
				[
					SAssignNew(Grid, SWrapBox)
					.UseAllottedSize(true)
					.InnerSlotPadding(FVector2D(10.0, 10.0))
				]
			]
		]
		+ SHorizontalBox::Slot().FillWidth(0.32f)
		[
			SAssignNew(Details, SBox)
		];

	TSharedRef<SWidget> Footer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Hint", "Gear changes are shared with your crew immediately."))
			.Font(FHHStyle::Font(EHHFont::Body, 13.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHHKeyHint).Key(LOCTEXT("Esc", "ESC")).Label(LOCTEXT("Back", "Back"))
		];

	ChildSlot
	[
		HHUI::PanelFrame(
			HHUI::PanelHeader(FText::FromString(TEXT("02")), LOCTEXT("Title", "THE LOCKER"),
				LOCTEXT("Subtitle", "Bring what you need. Leave what makes noise.")),
			Body, Footer)
	];

	Refresh();
}

void SHHLoadoutPanel::Refresh()
{
	RebuildSlots();
	RebuildGrid();
	RebuildDetails();
}

void SHHLoadoutPanel::SelectSlot(EHHEquipmentSlot Slot)
{
	SelectedSlot = Slot;
	if (const UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr)
	{
		Inspected = Profile->GetEquipment().Get(Slot);
	}
	Refresh();
}

void SHHLoadoutPanel::Inspect(FName ItemId)
{
	Inspected = ItemId;
	RebuildDetails();
}

void SHHLoadoutPanel::RebuildSlots()
{
	SlotList->ClearChildren();
	const UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	if (!Profile || !Registry)
	{
		return;
	}

	for (EHHEquipmentSlot Slot : TEnumRange<EHHEquipmentSlot>())
	{
		const UHHEquipmentDefinition* Item = Registry->FindEquipment(Profile->GetEquipment().Get(Slot));
		TSharedPtr<FSlateBrush> Icon = HHUI::MakeTextureBrush(Item ? Registry->GetIcon(Item) : nullptr, FVector2D(40.0, 40.0));
		Brushes.Add(Icon);

		SlotList->AddSlot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SButton)
			.ButtonStyle(&FHHStyle::Button("HH.Button.Card"))
			.ContentPadding(FMargin(0.f))
			.OnHovered_Lambda([]() { HHUI::PlayUISound(EHHUISound::Hover); })
			.OnClicked_Lambda([this, Slot]()
			{
				HHUI::PlayUISound(EHHUISound::Click);
				SelectSlot(Slot);
				return FReply::Handled();
			})
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(10.f)
					[
						SNew(SBox).WidthOverride(40.f).HeightOverride(40.f)
						[
							SNew(SImage).Image(Icon.Get())
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(4.f, 0.f, 10.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(HHText::EquipmentSlotName(Slot).ToUpper())
							.Font(FHHStyle::Font(EHHFont::CondensedSemi, 11.f, 220))
							.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(Item ? Item->DisplayName : LOCTEXT("Empty", "Empty"))
							.Font(FHHStyle::Font(EHHFont::BodyMedium, 14.f))
							.ColorAndOpacity(FSlateColor(Item ? FHHStyle::Text() : FHHStyle::TextFaint()))
						]
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FHHStyle::Brush("HH.OutlineAccent"))
					.Visibility(SelectedSlot == Slot ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
				]
			]
		];
	}
}

void SHHLoadoutPanel::RebuildGrid()
{
	Grid->ClearChildren();
	const UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	if (!Profile || !Registry)
	{
		return;
	}

	const FName Equipped = Profile->GetEquipment().Get(SelectedSlot);

	// "Nothing" card: leave the slot empty.
	Grid->AddSlot()
	[
		SNew(SHHItemCard)
		.Item(nullptr)
		.Size(112.f)
		.EmptyLabel(LOCTEXT("Nothing", "Nothing"))
		.IsSelected_Lambda([this]() { return Inspected.IsNone(); })
		.IsEquipped(Equipped.IsNone())
		.OnClicked_Lambda([this]() { Inspect(NAME_None); })
	];

	for (const UHHEquipmentDefinition* Item : Registry->GetEquipmentForSlot(SelectedSlot))
	{
		const FName Id = Item->GetItemId();
		const bool bOwned = Profile->IsOwned(Id);
		const bool bLevelLocked = Profile->GetLevel() < Item->UnlockLevel;
		const FText Badge = bOwned ? FText::GetEmpty()
			: (bLevelLocked ? FText::Format(LOCTEXT("Lv", "LV {0}"), FText::AsNumber(Item->UnlockLevel)) : HHText::Money(Item->Price));

		Grid->AddSlot()
		[
			SNew(SHHItemCard)
			.Item(Item)
			.Icon(Registry->GetIcon(Item))
			.Size(112.f)
			.IsSelected_Lambda([this, Id]() { return Inspected == Id; })
			.IsEquipped(Equipped == Id)
			.IsLocked(!bOwned)
			.Badge(Badge)
			.OnClicked_Lambda([this, Id]() { Inspect(Id); })
		];
	}
}

void SHHLoadoutPanel::RebuildDetails()
{
	UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	if (!Profile || !Registry)
	{
		Details->SetContent(SNullWidget::NullWidget);
		return;
	}

	const UHHEquipmentDefinition* Item = Registry->FindEquipment(Inspected);
	const FName Equipped = Profile->GetEquipment().Get(SelectedSlot);
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	const EHHEquipmentSlot Slot = SelectedSlot;

	if (!Item)
	{
		Details->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EmptyHandsTitle", "Empty-handed"))
				.Font(FHHStyle::Font(EHHFont::Display, 24.f, 60))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EmptyHandsBody", "Lighter, quieter, and completely unprepared."))
				.AutoWrapText(true)
				.Font(FHHStyle::Font(EHHFont::Typewriter, 14.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f).HAlign(HAlign_Left)
			[
				SNew(SHHActionButton)
				.Text(Equipped.IsNone() ? LOCTEXT("SlotEmpty", "SLOT EMPTY") : LOCTEXT("ClearSlot", "LEAVE SLOT EMPTY"))
				.Kind(EHHButtonKind::Secondary)
				.ClickSound(EHHUISound::Equip)
				.OnClicked_Lambda([WeakHUD, Slot]()
				{
					if (UHHProfileSubsystem* P = WeakHUD.IsValid() ? UHHProfileSubsystem::Get(WeakHUD.Get()) : nullptr)
					{
						P->ClearEquipmentSlot(Slot);
					}
				})
			]);
		return;
	}

	const FName Id = Item->GetItemId();
	const bool bOwned = Profile->IsOwned(Id);
	TSharedPtr<FSlateBrush> Icon = HHUI::MakeTextureBrush(Registry->GetIcon(Item), FVector2D(180.0, 180.0));
	Brushes.Add(Icon);

	TSharedRef<SVerticalBox> Stats = SNew(SVerticalBox);
	for (const FHHItemStat& Stat : Item->Stats)
	{
		Stats->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[
			HHUI::StatBar(Stat.Label, Stat.Value, Stat.DisplayMax, Stat.bLowerIsBetter)
		];
	}

	FText ActionText;
	bool bCanEquip = false;
	if (Equipped == Id)
	{
		ActionText = LOCTEXT("Equipped", "EQUIPPED");
	}
	else if (bOwned)
	{
		ActionText = LOCTEXT("Equip", "TAKE IT");
		bCanEquip = true;
	}
	else
	{
		ActionText = FText::Format(LOCTEXT("BuyAtFence", "FROM THE FENCE  {0}"), HHText::Money(Item->Price));
	}

	Details->SetContent(
		SNew(SScrollBox)
		.ScrollBarThickness(FVector2D(2.0, 2.0))
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(180.f).HeightOverride(180.f)
				[
					SNew(SImage).Image(Icon.Get())
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Item->DisplayName)
				.Font(FHHStyle::Font(EHHFont::Display, 24.f, 60))
				.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					HHUI::Pill(HHText::RarityName(Item->Rarity).ToUpper(), HHText::RarityColor(Item->Rarity))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Item->Description)
				.AutoWrapText(true)
				.Font(FHHStyle::Font(EHHFont::Body, 14.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Item->FlavorText)
				.AutoWrapText(true)
				.Font(FHHStyle::Font(EHHFont::Typewriter, 13.f))
				.ColorAndOpacity(FSlateColor(FHHStyle::AccentDim()))
				.Visibility(Item->FlavorText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 20.f, 0.f, 0.f)
			[
				Stats
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f).HAlign(HAlign_Left)
			[
				SNew(SHHActionButton)
				.Text(ActionText)
				.Kind(bCanEquip ? EHHButtonKind::Primary : EHHButtonKind::Secondary)
				.MinWidth(180.f)
				.ClickSound(bCanEquip ? EHHUISound::Equip : EHHUISound::Click)
				.OnClicked_Lambda([WeakHUD, Item, bCanEquip, bOwned]()
				{
					AHHLobbyHUD* LobbyHUD = WeakHUD.Get();
					UHHProfileSubsystem* P = LobbyHUD ? UHHProfileSubsystem::Get(LobbyHUD) : nullptr;
					if (!P)
					{
						return;
					}
					if (bCanEquip)
					{
						P->EquipItem(Item);
					}
					else if (!bOwned)
					{
						LobbyHUD->OpenScreen(EHHLobbyScreen::Store);
					}
				})
			]
		]);
}

#undef LOCTEXT_NAMESPACE
