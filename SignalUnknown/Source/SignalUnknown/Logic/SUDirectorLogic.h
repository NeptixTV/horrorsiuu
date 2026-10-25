// SIGNAL UNKNOWN – Horror Director decision logic (engine independent).
//
// The director is evaluated at irregular intervals. Each evaluation first decides IF anything
// should happen at all (it often decides "nothing"), then picks one eligible event by weight.
// Tension rises with every event and decays over time, so busy phases are followed by calm ones.
#pragma once

#include "SUMathLite.h"

#include <array>

namespace SULogic
{
	enum class HorrorEvent : int
	{
		None = -1,
		DistantSound = 0,
		LightFlicker,
		RadioInterference,
		TreeWatcher,
		RoadsideFigure,
		StrangeRadioMessage,
		CarWontStart,
		BuildingSounds,
		HunterChase,
		Count
	};

	enum class HorrorEventSize : int
	{
		Small = 0,
		Medium = 1,
		Large = 2
	};

	struct HorrorEventSpec
	{
		HorrorEvent Id = HorrorEvent::None;
		HorrorEventSize Size = HorrorEventSize::Small;
		double Weight = 1.0;
		double CooldownSeconds = 60.0;
		int MinStoryStage = 0;
		double MinDarkness = 0.0;     // 0 = day allowed, 1 = only deep night
		bool bAllowedInSafeZone = false;
		bool bNeedsVehicleMoving = false;
		bool bNeedsOnFoot = false;
		bool bNeedsRadio = false;
		bool bNeedsLight = false;
		bool bNeedsBuildingNearby = false;
		bool bNeedsVehicleNearby = false;
		bool bNeedsOutdoors = false;
		double MaxTension = 1.0;      // only fire while the tension is below this
		double TensionAdded = 0.1;
	};

	struct DirectorContext
	{
		double Now = 0.0;              // seconds
		double Darkness = 0.0;         // 0 = bright day, 1 = night
		double Rain = 0.0;             // 0..1
		int StoryStage = 0;
		double Intensity = 1.0;        // user setting 0..2
		int NumPlayers = 1;
		bool bInSafeZone = false;      // at the base
		bool bInScriptedSequence = false;
		bool bInCriticalInteraction = false;
		bool bInVehicle = false;
		double VehicleSpeedKmh = 0.0;
		bool bOnRoad = false;
		bool bIndoors = false;
		bool bRadioNearby = false;     // a radio/car radio/walkie that could be disturbed
		bool bLightNearby = false;     // powered lamp or flashlight on
		bool bBuildingNearby = false;
		bool bVehicleNearbyOnFoot = false;
		bool bCreatureActive = false;  // a creature is already spawned
	};

	struct DirectorState
	{
		double Tension = 0.0;
		double LastEventTime = -1e9;
		std::array<double, static_cast<size_t>(HorrorEvent::Count)> LastById{};
		int EventsFired = 0;
		int Evaluations = 0;

		DirectorState() { LastById.fill(-1e9); }
	};

	struct DirectorDecision
	{
		HorrorEvent Event = HorrorEvent::None;
		double Chance = 0.0;
		int EligibleCount = 0;
	};

	inline const std::array<HorrorEventSpec, static_cast<size_t>(HorrorEvent::Count)>& HorrorCatalog()
	{
		static const std::array<HorrorEventSpec, static_cast<size_t>(HorrorEvent::Count)> Catalog = []()
		{
			std::array<HorrorEventSpec, static_cast<size_t>(HorrorEvent::Count)> C{};
			auto Make = [](HorrorEvent Id, HorrorEventSize Size, double Weight, double Cooldown, int MinStory, double MinDark, double Tension)
			{
				HorrorEventSpec S;
				S.Id = Id;
				S.Size = Size;
				S.Weight = Weight;
				S.CooldownSeconds = Cooldown;
				S.MinStoryStage = MinStory;
				S.MinDarkness = MinDark;
				S.TensionAdded = Tension;
				return S;
			};

			HorrorEventSpec E = Make(HorrorEvent::DistantSound, HorrorEventSize::Small, 1.0, 90.0, 0, 0.0, 0.10);
			E.bNeedsOutdoors = true;
			C[static_cast<size_t>(E.Id)] = E;

			E = Make(HorrorEvent::LightFlicker, HorrorEventSize::Small, 0.8, 140.0, 1, 0.25, 0.12);
			E.bNeedsLight = true;
			E.bAllowedInSafeZone = true;
			C[static_cast<size_t>(E.Id)] = E;

			E = Make(HorrorEvent::RadioInterference, HorrorEventSize::Small, 0.7, 160.0, 1, 0.0, 0.08);
			E.bNeedsRadio = true;
			E.bAllowedInSafeZone = true;
			C[static_cast<size_t>(E.Id)] = E;

			E = Make(HorrorEvent::TreeWatcher, HorrorEventSize::Small, 0.6, 260.0, 1, 0.3, 0.18);
			E.bNeedsOutdoors = true;
			E.MaxTension = 0.7;
			C[static_cast<size_t>(E.Id)] = E;

			E = Make(HorrorEvent::RoadsideFigure, HorrorEventSize::Medium, 0.9, 420.0, 1, 0.35, 0.3);
			E.bNeedsVehicleMoving = true;
			E.MaxTension = 0.6;
			C[static_cast<size_t>(E.Id)] = E;

			E = Make(HorrorEvent::StrangeRadioMessage, HorrorEventSize::Medium, 0.6, 600.0, 1, 0.0, 0.2);
			E.bNeedsRadio = true;
			E.bNeedsVehicleMoving = true;
			E.MaxTension = 0.7;
			C[static_cast<size_t>(E.Id)] = E;

			E = Make(HorrorEvent::CarWontStart, HorrorEventSize::Medium, 0.5, 900.0, 2, 0.4, 0.2);
			E.bNeedsOnFoot = true;
			E.bNeedsVehicleNearby = true;
			C[static_cast<size_t>(E.Id)] = E;

			E = Make(HorrorEvent::BuildingSounds, HorrorEventSize::Medium, 0.7, 300.0, 1, 0.2, 0.22);
			E.bNeedsOnFoot = true;
			E.bNeedsBuildingNearby = true;
			E.MaxTension = 0.65;
			C[static_cast<size_t>(E.Id)] = E;

			E = Make(HorrorEvent::HunterChase, HorrorEventSize::Large, 0.45, 1500.0, 2, 0.6, 0.6);
			E.bNeedsVehicleMoving = true;
			E.MaxTension = 0.3;
			C[static_cast<size_t>(E.Id)] = E;
			return C;
		}();
		return Catalog;
	}

	inline bool IsHorrorEventEligible(const HorrorEventSpec& S, const DirectorContext& C, const DirectorState& St)
	{
		if (S.Id == HorrorEvent::None) { return false; }
		if (C.StoryStage < S.MinStoryStage) { return false; }
		if (C.Darkness < S.MinDarkness) { return false; }
		if (C.bInSafeZone && !S.bAllowedInSafeZone) { return false; }
		if (St.Tension > S.MaxTension) { return false; }
		if (C.Now - St.LastById[static_cast<size_t>(S.Id)] < S.CooldownSeconds) { return false; }
		if (S.bNeedsVehicleMoving && !(C.bInVehicle && C.bOnRoad && C.VehicleSpeedKmh > 25.0)) { return false; }
		if (S.bNeedsOnFoot && C.bInVehicle) { return false; }
		if (S.bNeedsRadio && !C.bRadioNearby) { return false; }
		if (S.bNeedsLight && !C.bLightNearby) { return false; }
		if (S.bNeedsBuildingNearby && !C.bBuildingNearby) { return false; }
		if (S.bNeedsVehicleNearby && !C.bVehicleNearbyOnFoot) { return false; }
		if (S.bNeedsOutdoors && C.bIndoors) { return false; }
		if (S.Size == HorrorEventSize::Large && (C.bCreatureActive || C.NumPlayers < 1)) { return false; }
		if ((S.Id == HorrorEvent::TreeWatcher || S.Id == HorrorEvent::RoadsideFigure) && C.bCreatureActive) { return false; }
		return true;
	}

	/** Probability that this evaluation fires an event at all. */
	inline double HorrorTriggerChance(const DirectorContext& C, const DirectorState& St)
	{
		if (C.bInScriptedSequence || C.bInCriticalInteraction || C.Intensity <= 0.0)
		{
			return 0.0;
		}
		const double Story = 0.5 + 0.25 * std::min(C.StoryStage, 4);
		const double Dark = 0.35 + 0.65 * Clamp01(C.Darkness);
		const double Weather = 1.0 + 0.25 * Clamp01(C.Rain);
		const double Safe = C.bInSafeZone ? 0.25 : 1.0;
		const double Calm = std::pow(1.0 - Clamp01(St.Tension), 1.5);
		const double Recency = SmoothStep(30.0, 150.0, C.Now - St.LastEventTime);
		return Clamp(0.24 * Story * Dark * Weather * Safe * Calm * Recency * C.Intensity, 0.0, 0.85);
	}

	inline void DecayHorrorTension(DirectorState& St, double DeltaSeconds)
	{
		St.Tension = std::max(0.0, St.Tension - 0.0025 * std::max(0.0, DeltaSeconds));
	}

	/**
	 * Decide what (if anything) happens now. RollTrigger and RollPick are uniform randoms in [0,1).
	 * Updates the state when an event is chosen.
	 */
	inline DirectorDecision DecideHorrorEvent(const DirectorContext& C, DirectorState& St, double RollTrigger, double RollPick)
	{
		DirectorDecision D;
		++St.Evaluations;
		D.Chance = HorrorTriggerChance(C, St);
		if (RollTrigger >= D.Chance)
		{
			return D; // deliberate "nothing happens"
		}

		const auto& Catalog = HorrorCatalog();
		double Total = 0.0;
		std::array<double, static_cast<size_t>(HorrorEvent::Count)> Weights{};
		for (const HorrorEventSpec& S : Catalog)
		{
			if (!IsHorrorEventEligible(S, C, St))
			{
				continue;
			}
			double W = S.Weight;
			// quieter, smaller events in daylight; bigger ones at night
			if (S.Size == HorrorEventSize::Small) { W *= 1.2 - 0.4 * C.Darkness; }
			else { W *= 0.6 + 0.8 * C.Darkness; }
			// groups that are spread out are more vulnerable
			if (C.NumPlayers > 1 && S.Size != HorrorEventSize::Small) { W *= 1.1; }
			Weights[static_cast<size_t>(S.Id)] = W;
			Total += W;
			++D.EligibleCount;
		}
		if (Total <= 0.0)
		{
			return D;
		}

		double Pick = Clamp01(RollPick) * Total;
		for (const HorrorEventSpec& S : Catalog)
		{
			const double W = Weights[static_cast<size_t>(S.Id)];
			if (W <= 0.0)
			{
				continue;
			}
			if (Pick < W)
			{
				D.Event = S.Id;
				break;
			}
			Pick -= W;
		}
		if (D.Event == HorrorEvent::None)
		{
			// floating point edge case: take the last eligible one
			for (const HorrorEventSpec& S : Catalog)
			{
				if (Weights[static_cast<size_t>(S.Id)] > 0.0)
				{
					D.Event = S.Id;
				}
			}
		}

		const HorrorEventSpec& Chosen = Catalog[static_cast<size_t>(D.Event)];
		St.Tension = Clamp01(St.Tension + Chosen.TensionAdded);
		St.LastEventTime = C.Now;
		St.LastById[static_cast<size_t>(D.Event)] = C.Now;
		++St.EventsFired;
		return D;
	}

	inline const char* HorrorEventName(HorrorEvent E)
	{
		switch (E)
		{
		case HorrorEvent::DistantSound: return "DistantSound";
		case HorrorEvent::LightFlicker: return "LightFlicker";
		case HorrorEvent::RadioInterference: return "RadioInterference";
		case HorrorEvent::TreeWatcher: return "TreeWatcher";
		case HorrorEvent::RoadsideFigure: return "RoadsideFigure";
		case HorrorEvent::StrangeRadioMessage: return "StrangeRadioMessage";
		case HorrorEvent::CarWontStart: return "CarWontStart";
		case HorrorEvent::BuildingSounds: return "BuildingSounds";
		case HorrorEvent::HunterChase: return "HunterChase";
		default: return "None";
		}
	}
}
