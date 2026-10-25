// SIGNAL UNKNOWN – mission report payout (engine independent).
#pragma once

#include "SUMathLite.h"

#include <vector>

namespace SULogic
{
	struct PayRequirement
	{
		double Weight = 1.0;
		int Bonus = 0;           // paid in full for a perfect photo
		bool bRequired = false;  // needed for the full base pay
		bool bRare = false;      // rare discoveries pay a fixed extra
		int RareBonus = 0;
		bool bAlreadyReported = false; // documented in an earlier report – pays nothing again
	};

	/** One evidence hit inside a submitted photo. */
	struct SubmittedHit
	{
		int RequirementIndex = -1;
		double Quality = 0.0;
	};

	struct PayResult
	{
		int BaseAmount = 0;
		std::vector<int> EvidenceAmounts;   // per requirement
		std::vector<double> BestQuality;    // per requirement
		int RareAmount = 0;
		int CompletenessBonus = 0;
		int Total = 0;
		int RequiredFound = 0;
		int RequiredTotal = 0;
		double Score = 0.0;                 // weighted quality of required evidence (0..1)
		bool bAllRequiredFound = false;
	};

	inline PayResult ComputeReportPayout(int BasePay, int CompletionBonus, const std::vector<PayRequirement>& Reqs,
		const std::vector<SubmittedHit>& Hits, double MinQuality = 0.15)
	{
		PayResult R;
		const size_t N = Reqs.size();
		R.BestQuality.assign(N, 0.0);
		R.EvidenceAmounts.assign(N, 0);

		for (const SubmittedHit& Hit : Hits)
		{
			if (Hit.RequirementIndex < 0 || static_cast<size_t>(Hit.RequirementIndex) >= N)
			{
				continue;
			}
			const size_t I = static_cast<size_t>(Hit.RequirementIndex);
			if (Reqs[I].bAlreadyReported)
			{
				continue;
			}
			// several photos of the same evidence only count once – the best one
			R.BestQuality[I] = std::max(R.BestQuality[I], Clamp01(Hit.Quality));
		}

		double WeightSum = 0.0;
		double WeightedQuality = 0.0;
		for (size_t I = 0; I < N; ++I)
		{
			const PayRequirement& Req = Reqs[I];
			const double Q = R.BestQuality[I];
			const bool bFound = Q >= MinQuality;
			if (Req.bRequired)
			{
				++R.RequiredTotal;
				WeightSum += Req.Weight;
				if (bFound)
				{
					++R.RequiredFound;
					WeightedQuality += Req.Weight * Q;
				}
			}
			if (bFound)
			{
				R.EvidenceAmounts[I] = static_cast<int>(std::lround(Req.Bonus * (0.25 + 0.75 * Q)));
				if (Req.bRare)
				{
					R.RareAmount += Req.RareBonus;
				}
			}
		}

		const double FoundShare = R.RequiredTotal > 0 ? static_cast<double>(R.RequiredFound) / R.RequiredTotal : 1.0;
		R.BaseAmount = static_cast<int>(std::lround(BasePay * (0.35 + 0.65 * FoundShare)));
		R.Score = WeightSum > 0.0 ? WeightedQuality / WeightSum : 1.0;
		R.bAllRequiredFound = R.RequiredFound == R.RequiredTotal;
		if (R.bAllRequiredFound && R.Score >= 0.5)
		{
			R.CompletenessBonus = CompletionBonus;
		}

		R.Total = R.BaseAmount + R.RareAmount + R.CompletenessBonus;
		for (int Amount : R.EvidenceAmounts)
		{
			R.Total += Amount;
		}
		return R;
	}

	inline const char* ReportRating(const PayResult& R)
	{
		if (!R.bAllRequiredFound) { return "Unvollst\xC3\xA4ndig"; } // UTF-8 bytes, independent of the compiler charset
		if (R.Score >= 0.75) { return "Hervorragend"; }
		if (R.Score >= 0.5) { return "Gut"; }
		return "Ausreichend";
	}

	/** Cost in cents for a fuel amount. Using integer cents avoids rounding drift in the bank account. */
	inline int FuelCostCents(double Liters, int PricePerLiterCents)
	{
		return static_cast<int>(std::lround(std::max(0.0, Liters) * PricePerLiterCents));
	}
}
