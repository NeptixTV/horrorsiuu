#include "Core/HHTypes.h"

#define LOCTEXT_NAMESPACE "HHTypes"

namespace HHText
{
	FText CosmeticSlotName(EHHCosmeticSlot Slot)
	{
		switch (Slot)
		{
		case EHHCosmeticSlot::Body:			return LOCTEXT("SlotBody", "Skin Tone");
		case EHHCosmeticSlot::Hair:			return LOCTEXT("SlotHair", "Hair");
		case EHHCosmeticSlot::Hat:			return LOCTEXT("SlotHat", "Headwear");
		case EHHCosmeticSlot::Mask:			return LOCTEXT("SlotMask", "Mask");
		case EHHCosmeticSlot::Top:			return LOCTEXT("SlotTop", "Top");
		case EHHCosmeticSlot::Jacket:		return LOCTEXT("SlotJacket", "Jacket");
		case EHHCosmeticSlot::Gloves:		return LOCTEXT("SlotGloves", "Gloves");
		case EHHCosmeticSlot::Pants:		return LOCTEXT("SlotPants", "Pants");
		case EHHCosmeticSlot::Shoes:		return LOCTEXT("SlotShoes", "Shoes");
		case EHHCosmeticSlot::Backpack:		return LOCTEXT("SlotBack", "Back");
		case EHHCosmeticSlot::Accessory:	return LOCTEXT("SlotAccessory", "Accessory");
		default:							return FText::GetEmpty();
		}
	}

	FText EquipmentSlotName(EHHEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EHHEquipmentSlot::Light:	return LOCTEXT("EqLight", "Light");
		case EHHEquipmentSlot::Entry:	return LOCTEXT("EqEntry", "Entry Tool");
		case EHHEquipmentSlot::Utility:	return LOCTEXT("EqUtility", "Utility");
		case EHHEquipmentSlot::Gadget:	return LOCTEXT("EqGadget", "Gadget");
		case EHHEquipmentSlot::Bag:		return LOCTEXT("EqBag", "Bag");
		default:						return FText::GetEmpty();
		}
	}

	FText RarityName(EHHItemRarity Rarity)
	{
		switch (Rarity)
		{
		case EHHItemRarity::Common:		return LOCTEXT("RarityCommon", "Common");
		case EHHItemRarity::Uncommon:	return LOCTEXT("RarityUncommon", "Uncommon");
		case EHHItemRarity::Rare:		return LOCTEXT("RarityRare", "Rare");
		case EHHItemRarity::Epic:		return LOCTEXT("RarityEpic", "Epic");
		case EHHItemRarity::Legendary:	return LOCTEXT("RarityLegendary", "Legendary");
		default:						return FText::GetEmpty();
		}
	}

	FText DifficultyName(EHHMissionDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case EHHMissionDifficulty::Quiet:		return LOCTEXT("DiffQuiet", "Quiet");
		case EHHMissionDifficulty::Uneasy:		return LOCTEXT("DiffUneasy", "Uneasy");
		case EHHMissionDifficulty::Restless:	return LOCTEXT("DiffRestless", "Restless");
		case EHHMissionDifficulty::Malevolent:	return LOCTEXT("DiffMalevolent", "Malevolent");
		default:								return FText::GetEmpty();
		}
	}

	FLinearColor RarityColor(EHHItemRarity Rarity)
	{
		switch (Rarity)
		{
		case EHHItemRarity::Common:		return FLinearColor::FromSRGBColor(FColor(150, 147, 140));
		case EHHItemRarity::Uncommon:	return FLinearColor::FromSRGBColor(FColor(112, 152, 104));
		case EHHItemRarity::Rare:		return FLinearColor::FromSRGBColor(FColor(92, 132, 178));
		case EHHItemRarity::Epic:		return FLinearColor::FromSRGBColor(FColor(146, 108, 184));
		case EHHItemRarity::Legendary:	return FLinearColor::FromSRGBColor(FColor(214, 164, 82));
		default:						return FLinearColor::White;
		}
	}

	FLinearColor DifficultyColor(EHHMissionDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case EHHMissionDifficulty::Quiet:		return FLinearColor::FromSRGBColor(FColor(140, 160, 128));
		case EHHMissionDifficulty::Uneasy:		return FLinearColor::FromSRGBColor(FColor(205, 170, 96));
		case EHHMissionDifficulty::Restless:	return FLinearColor::FromSRGBColor(FColor(204, 118, 72));
		case EHHMissionDifficulty::Malevolent:	return FLinearColor::FromSRGBColor(FColor(178, 52, 46));
		default:								return FLinearColor::White;
		}
	}

	FText Money(int64 Amount)
	{
		FNumberFormattingOptions Options;
		Options.SetUseGrouping(true);
		Options.SetMaximumFractionalDigits(0);
		const FText Number = FText::AsNumber(FMath::Abs(Amount), &Options);
		return Amount < 0
			? FText::Format(LOCTEXT("MoneyNegative", "-${0}"), Number)
			: FText::Format(LOCTEXT("Money", "${0}"), Number);
	}
}

#undef LOCTEXT_NAMESPACE
