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

	FText OutcomeName(EHHMissionOutcome Outcome)
	{
		switch (Outcome)
		{
		case EHHMissionOutcome::Success:		return LOCTEXT("OutcomeSuccess", "Clean getaway");
		case EHHMissionOutcome::Partial:		return LOCTEXT("OutcomePartial", "Got away light");
		case EHHMissionOutcome::Caught:			return LOCTEXT("OutcomeCaught", "Busted");
		case EHHMissionOutcome::Abandoned:		return LOCTEXT("OutcomeAbandoned", "Job abandoned");
		case EHHMissionOutcome::HotGetaway:		return LOCTEXT("OutcomeHot", "Hot getaway");
		case EHHMissionOutcome::ChaseBusted:	return LOCTEXT("OutcomeChaseBusted", "Boxed in");
		default:								return FText::GetEmpty();
		}
	}

	FText OutcomeLine(EHHMissionOutcome Outcome)
	{
		switch (Outcome)
		{
		case EHHMissionOutcome::Success:		return LOCTEXT("LineSuccess", "In and out. Nobody saw a thing.");
		case EHHMissionOutcome::Partial:		return LOCTEXT("LinePartial", "You got away - with less than the client asked for.");
		case EHHMissionOutcome::Caught:			return LOCTEXT("LineCaught", "Blue lights in the driveway. The night ended in a cell.");
		case EHHMissionOutcome::Abandoned:		return LOCTEXT("LineAbandoned", "Some houses aren't worth it. The crew walked away.");
		case EHHMissionOutcome::HotGetaway:		return LOCTEXT("LineHot", "The police were called, but the van made it. The goods are hot - the fence takes more.");
		case EHHMissionOutcome::ChaseBusted:	return LOCTEXT("LineChase", "The patrol car cut the van off. Everything in it is evidence now.");
		default:								return FText::GetEmpty();
		}
	}

	bool IsGetaway(EHHMissionOutcome Outcome)
	{
		return Outcome == EHHMissionOutcome::Success || Outcome == EHHMissionOutcome::Partial || Outcome == EHHMissionOutcome::HotGetaway;
	}

	FText CrewFateName(EHHCrewFate Fate)
	{
		switch (Fate)
		{
		case EHHCrewFate::Escaped:		return LOCTEXT("FateEscaped", "Made it");
		case EHHCrewFate::LeftBehind:	return LOCTEXT("FateLeft", "Left behind");
		case EHHCrewFate::Caught:		return LOCTEXT("FateCaught", "Arrested");
		case EHHCrewFate::Gone:			return LOCTEXT("FateGone", "Disconnected");
		default:						return FText::GetEmpty();
		}
	}

	FText IntelKindName(EHHIntelKind Kind)
	{
		switch (Kind)
		{
		case EHHIntelKind::Owner:		return LOCTEXT("IntelOwner", "The owners");
		case EHHIntelKind::Valuable:	return LOCTEXT("IntelValuable", "Valuables");
		case EHHIntelKind::Entry:		return LOCTEXT("IntelEntry", "Ways in");
		case EHHIntelKind::Security:	return LOCTEXT("IntelSecurity", "Security");
		default:						return LOCTEXT("IntelNote", "Notes");
		}
	}

	FText ClockTime(float Hours)
	{
		const float Wrapped = FMath::Fmod(FMath::Fmod(Hours, 24.f) + 24.f, 24.f);
		const int32 TotalMinutes = FMath::FloorToInt(Wrapped * 60.f);
		const int32 Hour = (TotalMinutes / 60) % 24;
		const int32 Minute = TotalMinutes % 60;
		return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Hour, Minute));
	}
}

#undef LOCTEXT_NAMESPACE
