// SIGNAL UNKNOWN – mission objective progression (engine independent).
//
// Objectives are grouped into stages. All objectives of the lowest unfinished stage are active
// at the same time (e.g. "take the camera" and "take the flashlight" in any order). Optional
// objectives never block the next stage and stay available once their stage was reached.
#pragma once

#include <vector>
#include <climits>

namespace SULogic
{
	struct ObjectiveSpec
	{
		int Stage = 0;
		int Required = 1;
		bool bOptional = false;
	};

	struct ObjectiveProgress
	{
		int Count = 0;
		bool bDone = false;
	};

	/** Lowest stage that still has an unfinished required objective; INT_MAX when all are done. */
	inline int CurrentObjectiveStage(const std::vector<ObjectiveSpec>& Specs, const std::vector<ObjectiveProgress>& Progress)
	{
		int Stage = INT_MAX;
		for (size_t I = 0; I < Specs.size() && I < Progress.size(); ++I)
		{
			if (!Specs[I].bOptional && !Progress[I].bDone && Specs[I].Stage < Stage)
			{
				Stage = Specs[I].Stage;
			}
		}
		return Stage;
	}

	inline bool IsObjectiveActive(const std::vector<ObjectiveSpec>& Specs, const std::vector<ObjectiveProgress>& Progress, size_t Index)
	{
		if (Index >= Specs.size() || Index >= Progress.size() || Progress[Index].bDone)
		{
			return false;
		}
		return Specs[Index].Stage <= CurrentObjectiveStage(Specs, Progress);
	}

	/** Adds progress to an objective if it is active. Returns true if it just became done. */
	inline bool AdvanceObjective(const std::vector<ObjectiveSpec>& Specs, std::vector<ObjectiveProgress>& Progress, size_t Index, int Amount = 1)
	{
		if (!IsObjectiveActive(Specs, Progress, Index))
		{
			return false;
		}
		ObjectiveProgress& P = Progress[Index];
		P.Count += Amount;
		if (P.Count >= Specs[Index].Required)
		{
			P.Count = Specs[Index].Required;
			P.bDone = true;
			return true;
		}
		return false;
	}

	inline bool AllRequiredObjectivesDone(const std::vector<ObjectiveSpec>& Specs, const std::vector<ObjectiveProgress>& Progress)
	{
		return CurrentObjectiveStage(Specs, Progress) == INT_MAX;
	}
}
