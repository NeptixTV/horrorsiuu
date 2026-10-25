// SIGNAL UNKNOWN – vehicle model helpers (engine independent, SI units: m, s, kg, N).
#pragma once

#include "SUMathLite.h"

namespace SULogic
{
	struct FuelModel
	{
		double CapacityLiters = 70.0;
		double IdleLitersPerHour = 1.3;
		double BaseLitersPerKm = 0.15;
		double LoadLitersPerKm = 0.22; // extra at full throttle
	};

	/** Fuel flow in liters per second. */
	inline double FuelFlowLitersPerSecond(const FuelModel& M, bool bEngineRunning, double Throttle01, double SpeedMps)
	{
		if (!bEngineRunning)
		{
			return 0.0;
		}
		const double Throttle = Clamp01(Throttle01);
		const double Idle = M.IdleLitersPerHour / 3600.0;
		const double PerKm = M.BaseLitersPerKm + M.LoadLitersPerKm * Throttle;
		return Idle + std::fabs(SpeedMps) / 1000.0 * PerKm;
	}

	/** Remaining range in km at a steady speed with moderate throttle. */
	inline double EstimatedRangeKm(const FuelModel& M, double FuelLiters, double SpeedKmh = 60.0)
	{
		const double Mps = SpeedKmh / 3.6;
		const double Flow = FuelFlowLitersPerSecond(M, true, 0.3, Mps);
		if (Flow <= 0.0)
		{
			return 0.0;
		}
		return FuelLiters / Flow * Mps / 1000.0;
	}

	struct EngineModel
	{
		double MaxDriveForceN = 9000.0;  // traction limit at low speed
		double PowerWatts = 95000.0;
		double TopSpeedMps = 41.0;       // ~148 km/h governor
		double ReverseTopSpeedMps = 6.0;
		double EngineBrakeN = 900.0;
	};

	/** Drive force (N) for a throttle value; negative throttle = reverse. */
	inline double EngineDriveForce(const EngineModel& M, double Throttle, double ForwardSpeedMps, double HealthFactor)
	{
		const double Health = Clamp(HealthFactor, 0.0, 1.0);
		if (Throttle > 0.0)
		{
			if (ForwardSpeedMps >= M.TopSpeedMps)
			{
				return 0.0;
			}
			const double PowerLimited = M.PowerWatts * (0.35 + 0.65 * Health) / std::max(ForwardSpeedMps, 2.0);
			return Throttle * std::min(M.MaxDriveForceN, PowerLimited);
		}
		if (Throttle < 0.0)
		{
			if (-ForwardSpeedMps >= M.ReverseTopSpeedMps)
			{
				return 0.0;
			}
			return Throttle * M.MaxDriveForceN * 0.55;
		}
		return 0.0;
	}

	/** Aerodynamic drag + rolling resistance (N), opposing the motion. */
	inline double ResistanceForce(double ForwardSpeedMps, double MassKg)
	{
		const double Drag = 0.5 * 1.2 * 0.48 * 2.9 * ForwardSpeedMps * std::fabs(ForwardSpeedMps);
		const double Rolling = 0.015 * MassKg * 9.81 * Clamp(ForwardSpeedMps / 0.5, -1.0, 1.0);
		return -(Drag + Rolling);
	}

	struct WheelInput
	{
		double NormalLoadN = 0.0;   // suspension force pressing the tyre onto the ground
		double LongVelMps = 0.0;    // contact patch velocity along the wheel heading
		double LatVelMps = 0.0;     // contact patch velocity sideways
		double DriveForceN = 0.0;   // from the engine (already split per wheel)
		double BrakeForceN = 0.0;   // magnitude
		double Mu = 1.0;            // surface friction coefficient
		double MassShareKg = 450.0; // share of the vehicle mass carried by this wheel
		double Dt = 1.0 / 60.0;
		double LateralGrip = 0.85;  // 0..1 portion of lateral velocity removed per step
	};

	struct WheelOutput
	{
		double LongForceN = 0.0;
		double LatForceN = 0.0;
		bool bSlipping = false;
	};

	/** Simple friction-circle tyre model, stable at any frame rate. */
	inline WheelOutput ComputeWheelForces(const WheelInput& In)
	{
		WheelOutput Out;
		if (In.NormalLoadN <= 0.0 || In.Dt <= 0.0)
		{
			return Out;
		}
		const double Dt = Clamp(In.Dt, 1.0 / 240.0, 1.0 / 20.0);
		const double MaxF = std::max(0.0, In.Mu) * In.NormalLoadN;

		// lateral: remove (part of) the sideways sliding velocity
		double Lat = -In.LatVelMps * In.MassShareKg / Dt * Clamp01(In.LateralGrip);

		// longitudinal: engine + brake (brake never reverses the motion)
		double Long = In.DriveForceN;
		if (In.BrakeForceN > 0.0)
		{
			const double StopForce = std::fabs(In.LongVelMps) * In.MassShareKg / Dt;
			const double Brake = std::min(In.BrakeForceN, StopForce);
			Long += -Sign(In.LongVelMps) * Brake;
		}

		const double Total = std::sqrt(Long * Long + Lat * Lat);
		if (Total > MaxF && Total > 1e-9)
		{
			const double Scale = MaxF / Total;
			Long *= Scale;
			Lat *= Scale;
			Out.bSlipping = true;
		}
		Out.LongForceN = Long;
		Out.LatForceN = Lat;
		return Out;
	}

	/** Spring + damper suspension force (N) for a compression in meters and its rate (m/s). */
	inline double SuspensionForce(double CompressionM, double CompressionRateMps, double StiffnessNpm, double DampingNsPm)
	{
		if (CompressionM <= 0.0)
		{
			return 0.0;
		}
		const double F = CompressionM * StiffnessNpm + CompressionRateMps * DampingNsPm;
		return std::max(0.0, F);
	}
}
