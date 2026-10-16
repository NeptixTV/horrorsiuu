#include "UI/Widgets/SHHBankPanel.h"
#include "UI/Widgets/SHHWidgets.h"
#include "UI/HHLobbyHUD.h"
#include "UI/HHStyle.h"
#include "Core/HHTypes.h"
#include "Progression/HHProfileSubsystem.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HHBankPanel"

UHHProfileSubsystem* SHHBankPanel::GetProfile() const
{
	return HUD.IsValid() ? UHHProfileSubsystem::Get(HUD.Get()) : nullptr;
}

void SHHBankPanel::Deposit(int64 Amount)
{
	UHHProfileSubsystem* Profile = GetProfile();
	if (!Profile)
	{
		return;
	}
	const int64 Wanted = Amount < 0 ? Profile->GetCash() : Amount;
	if (Wanted > 0 && Profile->Deposit(Wanted))
	{
		HHUI::PlayUISound(EHHUISound::Purchase);
	}
	else
	{
		HHUI::PlayUISound(EHHUISound::Error);
	}
}

void SHHBankPanel::Withdraw(int64 Amount)
{
	UHHProfileSubsystem* Profile = GetProfile();
	if (!Profile)
	{
		return;
	}
	const int64 Wanted = Amount < 0 ? Profile->GetSavings() : Amount;
	if (Wanted > 0 && Profile->Withdraw(Wanted))
	{
		HHUI::PlayUISound(EHHUISound::Purchase);
	}
	else
	{
		HHUI::PlayUISound(EHHUISound::Error);
	}
}

TSharedRef<SWidget> SHHBankPanel::MakeBalance(const FText& Label, TAttribute<FText> Value, const FLinearColor& Color)
{
	return SNew(SBorder)
		.BorderImage(FHHStyle::Brush("HH.Card"))
		.Padding(FMargin(18.f, 14.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FHHStyle::Font(EHHFont::CondensedSemi, 11.f, 300))
				.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(Value)
				.Font(FHHStyle::Font(EHHFont::MonoMedium, 30.f))
				.ColorAndOpacity(FSlateColor(Color))
			]
		];
}

TSharedRef<SWidget> SHHBankPanel::MakeAmountRow(bool bDeposit)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	const int64 Amounts[] = { 100, 500, 1000, -1 };
	for (const int64 Amount : Amounts)
	{
		const FText Label = Amount < 0
			? (bDeposit ? LOCTEXT("DepositAll", "ALL OF IT") : LOCTEXT("WithdrawAll", "EVERYTHING"))
			: HHText::Money(Amount);
		Row->AddSlot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SHHActionButton)
			.Text(Label)
			.Kind(Amount < 0 ? EHHButtonKind::Primary : EHHButtonKind::Secondary)
			.MinWidth(110.f)
			.ClickSound(EHHUISound::Confirm)
			.IsEnabled_Lambda([this, bDeposit, Amount]()
			{
				const UHHProfileSubsystem* Profile = GetProfile();
				const int64 Available = Profile ? (bDeposit ? Profile->GetCash() : Profile->GetSavings()) : 0;
				return Available > 0 && (Amount < 0 || Available >= Amount);
			})
			.OnClicked_Lambda([this, bDeposit, Amount]()
			{
				if (bDeposit)
				{
					Deposit(Amount);
				}
				else
				{
					Withdraw(Amount);
				}
			})
		];
	}
	return Row;
}

void SHHBankPanel::Construct(const FArguments& InArgs)
{
	HUD = InArgs._HUD;

	TSharedRef<SWidget> Header = HHUI::PanelHeader(FText::FromString(TEXT("$")), LOCTEXT("Title", "HOLLOWMERE SAVINGS & LOAN"),
		LOCTEXT("Subtitle", "Est. 1902. Your money, safe in the vault."));

	auto SectionTitle = [](const FText& Text)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FHHStyle::Font(EHHFont::CondensedSemi, 13.f, 200))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()));
	};

	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 10.f, 0.f)
			[
				MakeBalance(LOCTEXT("Pocket", "IN YOUR POCKET"), TAttribute<FText>::CreateLambda([this]()
				{
					const UHHProfileSubsystem* Profile = GetProfile();
					return HHText::Money(Profile ? Profile->GetCash() : 0);
				}), FHHStyle::Text())
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(10.f, 0.f, 0.f, 0.f)
			[
				MakeBalance(LOCTEXT("Account", "IN THE ACCOUNT"), TAttribute<FText>::CreateLambda([this]()
				{
					const UHHProfileSubsystem* Profile = GetProfile();
					return HHText::Money(Profile ? Profile->GetSavings() : 0);
				}), FHHStyle::Accent())
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text_Lambda([this]()
			{
				const UHHProfileSubsystem* Profile = GetProfile();
				if (!Profile)
				{
					return FText::GetEmpty();
				}
				return FText::Format(LOCTEXT("Interest", "Day {0} in Hollowmere.  Interest after the next job night: {1} (2% a night, at most {2})."),
					FText::AsNumber(Profile->GetDay()), HHText::Money(Profile->GetNextInterest()), HHText::Money(UHHProfileSubsystem::MaxInterestPerNight));
			})
			.Font(FHHStyle::Font(EHHFont::Typewriter, 14.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::Paper()))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 24.f, 0.f, 8.f)
		[
			SectionTitle(LOCTEXT("DepositTitle", "DEPOSIT  (pocket -> account)"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeAmountRow(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 8.f)
		[
			SectionTitle(LOCTEXT("WithdrawTitle", "WITHDRAW  (account -> pocket)"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeAmountRow(false)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(LOCTEXT("Why", "Why bother? If the police pick you up, bail and a lawyer take a quarter of the cash in your pocket - "
				"the account is never touched. Shops in town and the Fence only take cash, so withdraw before you go shopping."))
			.Font(FHHStyle::Font(EHHFont::Body, 14.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextDim()))
		];

	TSharedRef<SWidget> Footer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Hours", "Teller windows open by day. The ATM outside never sleeps."))
			.Font(FHHStyle::Font(EHHFont::Body, 13.f))
			.ColorAndOpacity(FSlateColor(FHHStyle::TextFaint()))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)
		[
			SNew(SHHKeyHint).Key(LOCTEXT("Esc", "ESC")).Label(LOCTEXT("Back", "Back"))
		];

	ChildSlot
	[
		HHUI::PanelFrame(Header, Body, Footer)
	];
}

#undef LOCTEXT_NAMESPACE
