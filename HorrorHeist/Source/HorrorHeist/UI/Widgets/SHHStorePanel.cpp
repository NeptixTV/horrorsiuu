#include "UI/Widgets/SHHStorePanel.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Data/HHCosmeticDefinition.h"
#include "Data/HHEquipmentDefinition.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Progression/HHProfileSubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHStorePanel"

SHHStorePanel::~SHHStorePanel()
{
	if (UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr)
	{
		Profile->OnProfileChanged.Remove(ProfileHandle);
	}
}

void SHHStorePanel::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;
	if (UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr)
	{
		ProfileHandle = Profile->OnProfileChanged.AddSP(this, &SHHStorePanel::Refresh);
	}

	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;

	TSharedRef<SWidget> Header = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				HHUI::PanelHeader(FText::FromString(TEXT("04")), LOCTEXT("Title", "THE FENCE"),
					LOCTEXT("Subtitle", "No receipts. No refunds. No questions."))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(20.f, 8.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Wallet", "IN YOUR POCKET"))
					.Font(FHHStyle::Font(EHHFont::CondensedSemi, 10.f, 300))
					.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text_Lambda([WeakHUD]()
					{
						const UHHProfileSubsystem* Profile = WeakHUD.IsValid() ? UHHProfileSubsystem::Get(WeakHUD.Get()) : nullptr;
						return HHText::Money(Profile ? Profile->GetCash() : 0);
					})
					.Font(FHHStyle::Font(EHHFont::MonoMedium, 24.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::Accent()))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
		[
			SNew(SHHTabBar)
			.Tabs({ LOCTEXT("All", "EVERYTHING"), LOCTEXT("Gear", "GEAR"), LOCTEXT("Clothes", "CLOTHES"), LOCTEXT("Faces", "MASKS & HATS"), LOCTEXT("Extras", "EXTRAS") })
			.Selected_Lambda([this]() { return Category; })
			.OnSelected_Lambda([this](int32 Index) { Category = Index; RebuildGrid(); })
		];

	TSharedRef<SWidget> Body = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.62f).Padding(0.f, 0.f, 22.f, 0.f)
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
		+ SHorizontalBox::Slot().FillWidth(0.38f)
		[
			SAssignNew(Details, SBox)
		];

	TSharedRef<SWidget> Footer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Hint", "Cash comes from jobs. Development builds: console 'HHGiveCash 5000'."))
			.Font(FHHStyle::Font(EHHFont::Body, 13.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHHKeyHint).Key(LOCTEXT("Esc", "ESC")).Label(LOCTEXT("Back", "Back"))
		];

	ChildSlot
	[
		HHUI::PanelFrame(Header, Body, Footer)
	];

	Refresh();
}

void SHHStorePanel::Refresh()
{
	RebuildGrid();
	RebuildDetails();
}

bool SHHStorePanel::PassesFilter(const UHHItemDefinition* Item) const
{
	const UHHCosmeticDefinition* Cosmetic = Cast<UHHCosmeticDefinition>(Item);
	switch (Category)
	{
	case 1: return Item->IsA<UHHEquipmentDefinition>();
	case 2: return Cosmetic && (Cosmetic->Slot == EHHCosmeticSlot::Top || Cosmetic->Slot == EHHCosmeticSlot::Jacket
		|| Cosmetic->Slot == EHHCosmeticSlot::Pants || Cosmetic->Slot == EHHCosmeticSlot::Shoes || Cosmetic->Slot == EHHCosmeticSlot::Gloves);
	case 3: return Cosmetic && (Cosmetic->Slot == EHHCosmeticSlot::Mask || Cosmetic->Slot == EHHCosmeticSlot::Hat || Cosmetic->Slot == EHHCosmeticSlot::Hair);
	case 4: return Cosmetic && (Cosmetic->Slot == EHHCosmeticSlot::Backpack || Cosmetic->Slot == EHHCosmeticSlot::Accessory || Cosmetic->Slot == EHHCosmeticSlot::Body);
	default: return true;
	}
}

void SHHStorePanel::RebuildGrid()
{
	Grid->ClearChildren();
	const UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	if (!Profile || !Registry)
	{
		return;
	}

	TArray<const UHHItemDefinition*> Items = Registry->GetStoreItems();
	Items.Sort([](const UHHItemDefinition& A, const UHHItemDefinition& B)
	{
		if (A.UnlockLevel != B.UnlockLevel)
		{
			return A.UnlockLevel < B.UnlockLevel;
		}
		return A.Price < B.Price;
	});

	for (const UHHItemDefinition* Item : Items)
	{
		if (!PassesFilter(Item))
		{
			continue;
		}
		const FName Id = Item->GetItemId();
		const bool bOwned = Profile->IsOwned(Id);
		const bool bLevelLocked = Profile->GetLevel() < Item->UnlockLevel;
		const FText Badge = bOwned ? LOCTEXT("Owned", "OWNED")
			: (bLevelLocked ? FText::Format(LOCTEXT("Lv", "LV {0}"), FText::AsNumber(Item->UnlockLevel)) : HHText::Money(Item->Price));

		Grid->AddSlot()
		[
			SNew(SHHItemCard)
			.Item(Item)
			.Icon(Registry->GetIcon(Item))
			.Size(118.f)
			.IsSelected_Lambda([this, Id]() { return Selected == Id; })
			.IsEquipped(false)
			.IsLocked(bLevelLocked && !bOwned)
			.Badge(Badge)
			.OnClicked_Lambda([this, Id]() { Selected = Id; RebuildDetails(); })
		];
	}
}

void SHHStorePanel::Buy(const UHHItemDefinition* Item)
{
	AHHLobbyHUD* LobbyHUD = HUD.Get();
	if (!LobbyHUD || !Item)
	{
		return;
	}
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	LobbyHUD->ShowConfirm(
		LOCTEXT("BuyTitle", "DEAL?"),
		FText::Format(LOCTEXT("BuyBody", "{0} for {1}."), Item->DisplayName, HHText::Money(Item->Price)),
		LOCTEXT("BuyConfirm", "PAY"),
		[WeakHUD, Item]()
		{
			AHHLobbyHUD* HUDPtr = WeakHUD.Get();
			UHHProfileSubsystem* Profile = HUDPtr ? UHHProfileSubsystem::Get(HUDPtr) : nullptr;
			if (Profile && Profile->Purchase(Item))
			{
				HHUI::PlayUISound(EHHUISound::Purchase);
				HUDPtr->ShowNotification(FText::Format(LOCTEXT("Bought", "{0} is yours. Find it in your locker or mirror."), Item->DisplayName), EHHNotifyType::Success);
			}
			else
			{
				HHUI::PlayUISound(EHHUISound::Error);
			}
		});
}

void SHHStorePanel::RebuildDetails()
{
	const UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	const UHHItemDefinition* Item = Registry ? Registry->FindItem(Selected) : nullptr;
	if (!Profile || !Item)
	{
		Details->SetContent(
			SNew(STextBlock)
			.Text(LOCTEXT("Pick", "\"See something you like?\""))
			.Font(FHHStyle::Font(EHHFont::Typewriter, 15.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim())));
		return;
	}

	TSharedPtr<FSlateBrush> Icon = HHUI::MakeTextureBrush(Registry->GetIcon(Item), FVector2D(200.0, 200.0));
	Brushes.Add(Icon);

	const EHHPurchaseCheck Check = Profile->CanPurchase(Item);
	TSharedRef<SWidget> Action = Check == EHHPurchaseCheck::Ok
		? StaticCastSharedRef<SWidget>(SNew(SHHActionButton)
			.Text(FText::Format(LOCTEXT("BuyFor", "BUY  {0}"), HHText::Money(Item->Price)))
			.Kind(EHHButtonKind::Primary)
			.MinWidth(200.f)
			.OnClicked_Lambda([this, Item]() { Buy(Item); }))
		: StaticCastSharedRef<SWidget>(SNew(STextBlock)
			.Text(UHHProfileSubsystem::DescribePurchaseCheck(Check, Item))
			.Font(FHHStyle::Font(EHHFont::CondensedSemi, 15.f, 120))
			.ColorAndOpacity(FSlateColor(Check == EHHPurchaseCheck::AlreadyOwned ? FHHStyle::Ready() : FHHStyle::TextDim())));

	TSharedRef<SVerticalBox> Stats = SNew(SVerticalBox);
	if (const UHHEquipmentDefinition* Equipment = Cast<UHHEquipmentDefinition>(Item))
	{
		for (const FHHItemStat& Stat : Equipment->Stats)
		{
			Stats->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				HHUI::StatBar(Stat.Label, Stat.Value, Stat.DisplayMax, Stat.bLowerIsBetter)
			];
		}
	}

	const UHHCosmeticDefinition* Cosmetic = Cast<UHHCosmeticDefinition>(Item);
	const UHHEquipmentDefinition* Equipment = Cast<UHHEquipmentDefinition>(Item);
	const FText Kind = Cosmetic ? HHText::CosmeticSlotName(Cosmetic->Slot) : (Equipment ? HHText::EquipmentSlotName(Equipment->Slot) : FText::GetEmpty());

	Details->SetContent(
		SNew(SScrollBox)
		.ScrollBarThickness(FVector2D(2.0, 2.0))
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(200.f).HeightOverride(200.f)
				[
					SNew(SImage).Image(Icon.Get())
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Kind.ToUpper())
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 11.f, 280))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Item->DisplayName)
				.AutoWrapText(true)
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
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Item->Theme)
					.Font(FHHStyle::Font(EHHFont::CondensedSemi, 11.f, 220))
					.ColorAndOpacity(FSlateColor(FHHStyle::AccentDim()))
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
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
			[
				Stats
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f).HAlign(HAlign_Left)
			[
				Action
			]
		]);
}

#undef LOCTEXT_NAMESPACE
