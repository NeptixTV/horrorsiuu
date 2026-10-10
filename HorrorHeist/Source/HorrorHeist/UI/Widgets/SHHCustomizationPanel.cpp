#include "UI/Widgets/SHHCustomizationPanel.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Interaction/HHPreviewStation.h"
#include "Data/HHCosmeticDefinition.h"
#include "Data/HHItemRegistrySubsystem.h"
#include "Progression/HHProfileSubsystem.h"
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

#define LOCTEXT_NAMESPACE "HHCustomizationPanel"

/** Transparent area over the mannequin: drag to rotate, wheel to zoom. */
class SHHPreviewDragArea : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHPreviewDragArea) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		HUD = InArgs._HUD;
		TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
		auto Rotate = [WeakHUD](float Degrees)
		{
			const AHHLobbyHUD* LobbyHUD = WeakHUD.Get();
			if (AHHPreviewStation* Station = LobbyHUD ? LobbyHUD->GetPreviewStation() : nullptr)
			{
				Station->AddYaw(Degrees);
			}
		};

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 44.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&FHHStyle::Button("HH.Button.Secondary"))
					.ContentPadding(FMargin(12.f, 8.f))
					.OnClicked_Lambda([Rotate]() { HHUI::PlayUISound(EHHUISound::Click); Rotate(-45.f); return FReply::Handled(); })
					[
						SNew(SImage).Image(FHHStyle::Brush("HH.Icon.ChevronLeft")).DesiredSizeOverride(FVector2D(14.0, 14.0))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DragHint", "DRAG TO TURN  \u00B7  WHEEL TO ZOOM"))
					.Font(FHHStyle::Font(EHHFont::Condensed, 12.f, 300))
					.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
					.ShadowOffset(FVector2D(0.0, 1.0))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&FHHStyle::Button("HH.Button.Secondary"))
					.ContentPadding(FMargin(12.f, 8.f))
					.OnClicked_Lambda([Rotate]() { HHUI::PlayUISound(EHHUISound::Click); Rotate(45.f); return FReply::Handled(); })
					[
						SNew(SImage).Image(FHHStyle::Brush("HH.Icon.ChevronRight")).DesiredSizeOverride(FVector2D(14.0, 14.0))
					]
				]
			]
		];
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return HasMouseCapture() ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled();
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (HasMouseCapture())
		{
			const AHHLobbyHUD* LobbyHUD = HUD.Get();
			if (AHHPreviewStation* Station = LobbyHUD ? LobbyHUD->GetPreviewStation() : nullptr)
			{
				Station->AddYaw(MouseEvent.GetCursorDelta().X * 0.45f);
			}
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		const AHHLobbyHUD* LobbyHUD = HUD.Get();
		if (AHHPreviewStation* Station = LobbyHUD ? LobbyHUD->GetPreviewStation() : nullptr)
		{
			Station->AddZoom(-MouseEvent.GetWheelDelta());
		}
		return FReply::Handled();
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		return FCursorReply::Cursor(HasMouseCapture() ? EMouseCursor::GrabHandClosed : EMouseCursor::GrabHand);
	}

private:
	TWeakObjectPtr<AHHLobbyHUD> HUD;
};

// ---------------------------------------------------------------------------------------

SHHCustomizationPanel::~SHHCustomizationPanel()
{
	if (UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr)
	{
		Profile->OnProfileChanged.Remove(ProfileHandle);
	}
}

AHHPreviewStation* SHHCustomizationPanel::GetStation() const
{
	const AHHLobbyHUD* LobbyHUD = HUD.Get();
	return LobbyHUD ? LobbyHUD->GetPreviewStation() : nullptr;
}

void SHHCustomizationPanel::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;

	if (UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr)
	{
		ProfileHandle = Profile->OnProfileChanged.AddSP(this, &SHHCustomizationPanel::Refresh);
		Selected = Profile->GetCosmetics().Get(SelectedSlot);
	}

	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(SlotTabs, SWrapBox)
			.UseAllottedSize(true)
			.InnerSlotPadding(FVector2D(6.0, 6.0))
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 18.f, 0.f, 0.f)
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
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
		[
			SAssignNew(Details, SBox)
			.MinDesiredHeight(120.f)
		];

	TSharedRef<SWidget> Footer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Hint", "Cosmetics never change how you play. They only change how you are remembered."))
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
			HHUI::PanelHeader(FText::FromString(TEXT("03")), LOCTEXT("Title", "THE MIRROR"),
				LOCTEXT("Subtitle", "Dress for the job. Nobody sees your face if you're smart.")),
			Body, Footer,
			SNew(SHHPreviewDragArea).HUD(HUD),
			0.5f)
	];

	Refresh();
	if (AHHPreviewStation* Station = GetStation())
	{
		Station->FocusSlot(SelectedSlot);
	}
}

void SHHCustomizationPanel::Refresh()
{
	RebuildSlotTabs();
	RebuildGrid();
	RebuildDetails();
}

void SHHCustomizationPanel::SelectSlot(EHHCosmeticSlot Slot)
{
	SelectedSlot = Slot;
	if (const UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr)
	{
		Selected = Profile->GetCosmetics().Get(Slot);
	}
	if (AHHPreviewStation* Station = GetStation())
	{
		Station->ShowEquipped();
		Station->FocusSlot(Slot);
	}
	Refresh();
}

void SHHCustomizationPanel::RebuildSlotTabs()
{
	SlotTabs->ClearChildren();
	for (EHHCosmeticSlot Slot : TEnumRange<EHHCosmeticSlot>())
	{
		const bool bActive = Slot == SelectedSlot;
		SlotTabs->AddSlot()
		[
			SNew(SButton)
			.ButtonStyle(&FHHStyle::Button(bActive ? "HH.Button.Secondary" : "HH.Button.Ghost"))
			.ContentPadding(FMargin(12.f, 6.f))
			.OnHovered_Lambda([]() { HHUI::PlayUISound(EHHUISound::Hover); })
			.OnClicked_Lambda([this, Slot]()
			{
				HHUI::PlayUISound(EHHUISound::Click);
				SelectSlot(Slot);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(HHText::CosmeticSlotName(Slot).ToUpper())
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 12.f, 180))
				.ColorAndOpacity(FSlateColor(bActive ? FHHStyle::Accent() : FHHStyle::TextDim()))
			]
		];
	}
}

void SHHCustomizationPanel::RebuildGrid()
{
	Grid->ClearChildren();
	const UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	if (!Profile || !Registry)
	{
		return;
	}

	const FName Equipped = Profile->GetCosmetics().Get(SelectedSlot);

	if (UHHCosmeticDefinition::IsSlotOptional(SelectedSlot))
	{
		Grid->AddSlot()
		[
			SNew(SHHItemCard)
			.Item(nullptr)
			.Size(104.f)
			.EmptyLabel(LOCTEXT("None", "None"))
			.IsSelected_Lambda([this]() { return Selected.IsNone(); })
			.IsEquipped(Equipped.IsNone())
			.OnClicked_Lambda([this]() { Select(NAME_None); })
		];
	}

	for (const UHHCosmeticDefinition* Item : Registry->GetCosmeticsForSlot(SelectedSlot))
	{
		const FName Id = Item->GetItemId();
		const bool bOwned = Profile->IsOwned(Id);
		const bool bLevelLocked = Profile->GetLevel() < Item->UnlockLevel;
		const FText Badge = bOwned ? FText::GetEmpty()
			: (Item->bHiddenInStore ? LOCTEXT("Special", "EVENT")
			: (bLevelLocked ? FText::Format(LOCTEXT("Lv", "LV {0}"), FText::AsNumber(Item->UnlockLevel)) : HHText::Money(Item->Price)));

		Grid->AddSlot()
		[
			SNew(SHHItemCard)
			.Item(Item)
			.Icon(Registry->GetIcon(Item))
			.Size(104.f)
			.IsSelected_Lambda([this, Id]() { return Selected == Id; })
			.IsEquipped(Equipped == Id)
			.IsLocked(!bOwned)
			.Badge(Badge)
			.OnClicked_Lambda([this, Id]() { Select(Id); })
		];
	}
}

void SHHCustomizationPanel::Preview(const UHHCosmeticDefinition* Item)
{
	AHHPreviewStation* Station = GetStation();
	const UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
	if (!Station || !Profile)
	{
		return;
	}
	FHHCosmeticLoadout Loadout = Profile->GetCosmetics();
	Loadout.Set(SelectedSlot, Item ? Item->GetItemId() : NAME_None);
	Station->PreviewLoadout(Loadout);
}

void SHHCustomizationPanel::Select(FName ItemId)
{
	Selected = ItemId;
	UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	if (!Profile || !Registry)
	{
		return;
	}

	const UHHCosmeticDefinition* Item = Registry->FindCosmetic(ItemId);
	if (!Item)
	{
		// "None" on an optional slot.
		HHUI::PlayUISound(EHHUISound::Equip);
		Profile->ClearCosmeticSlot(SelectedSlot);
		if (AHHPreviewStation* Station = GetStation())
		{
			Station->ShowEquipped();
		}
		RebuildDetails();
		return;
	}

	if (Profile->IsOwned(ItemId))
	{
		// Owned: wear it right away (the crew sees it too).
		HHUI::PlayUISound(EHHUISound::Equip);
		if (AHHPreviewStation* Station = GetStation())
		{
			Station->ShowEquipped();
		}
		Profile->EquipCosmetic(Item);
	}
	else
	{
		Preview(Item);
		RebuildDetails();
	}
}

void SHHCustomizationPanel::Buy(const UHHCosmeticDefinition* Item)
{
	AHHLobbyHUD* LobbyHUD = HUD.Get();
	if (!LobbyHUD || !Item)
	{
		return;
	}
	TWeakObjectPtr<AHHLobbyHUD> WeakHUD = HUD;
	LobbyHUD->ShowConfirm(
		LOCTEXT("BuyTitle", "PAY THE FENCE?"),
		FText::Format(LOCTEXT("BuyBody", "{0} for {1}. Cash only, no receipts."), Item->DisplayName, HHText::Money(Item->Price)),
		LOCTEXT("BuyConfirm", "BUY"),
		[WeakHUD, Item]()
		{
			AHHLobbyHUD* HUDPtr = WeakHUD.Get();
			UHHProfileSubsystem* Profile = HUDPtr ? UHHProfileSubsystem::Get(HUDPtr) : nullptr;
			if (Profile && Profile->Purchase(Item))
			{
				HHUI::PlayUISound(EHHUISound::Purchase);
				HUDPtr->ShowNotification(FText::Format(LOCTEXT("Bought", "{0} is yours."), Item->DisplayName), EHHNotifyType::Success);
				Profile->EquipCosmetic(Item);
			}
			else if (HUDPtr)
			{
				HHUI::PlayUISound(EHHUISound::Error);
			}
		});
}

void SHHCustomizationPanel::RebuildDetails()
{
	const UHHProfileSubsystem* Profile = HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
	const UHHItemRegistrySubsystem* Registry = HUD.IsValid() ? UHHItemRegistrySubsystem::Get(HUD.Get()) : nullptr;
	const UHHCosmeticDefinition* Item = Registry ? Registry->FindCosmetic(Selected) : nullptr;
	if (!Profile || !Item)
	{
		Details->SetContent(SNullWidget::NullWidget);
		return;
	}

	const bool bOwned = Profile->IsOwned(Item->GetItemId());
	const bool bEquipped = Profile->GetCosmetics().Get(SelectedSlot) == Item->GetItemId();
	const EHHPurchaseCheck Check = Profile->CanPurchase(Item);

	TSharedRef<SWidget> Action = SNullWidget::NullWidget;
	if (bEquipped)
	{
		Action = HHUI::Pill(LOCTEXT("Wearing", "WEARING"), FHHStyle::Ready());
	}
	else if (!bOwned)
	{
		if (Check == EHHPurchaseCheck::Ok)
		{
			Action = SNew(SHHActionButton)
				.Text(FText::Format(LOCTEXT("BuyFor", "BUY  {0}"), HHText::Money(Item->Price)))
				.Kind(EHHButtonKind::Primary)
				.MinWidth(170.f)
				.OnClicked_Lambda([this, Item]() { Buy(Item); });
		}
		else
		{
			Action = SNew(STextBlock)
				.Text(UHHProfileSubsystem::DescribePurchaseCheck(Check, Item))
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 14.f, 120))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()));
		}
	}

	Details->SetContent(
		SNew(SBorder)
		.BorderImage(FHHStyle::Brush("HH.Card"))
		.Padding(FMargin(18.f, 14.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Item->DisplayName)
						.Font(FHHStyle::Font(EHHFont::Display, 21.f, 60))
						.ColorAndOpacity(FSlateColor(FHHStyle::Text()))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
					[
						HHUI::Pill(HHText::RarityName(Item->Rarity).ToUpper(), HHText::RarityColor(Item->Rarity))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(Item->Theme)
					.Font(FHHStyle::Font(EHHFont::CondensedSemi, 11.f, 260))
					.ColorAndOpacity(FSlateColor(FHHStyle::AccentDim()))
					.Visibility(Item->Theme.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(Item->FlavorText.IsEmpty() ? Item->Description : Item->FlavorText)
					.AutoWrapText(true)
					.Font(FHHStyle::Font(EHHFont::Typewriter, 13.f))
					.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(18.f, 0.f, 0.f, 0.f)
			[
				Action
			]
		]);
}

#undef LOCTEXT_NAMESPACE
