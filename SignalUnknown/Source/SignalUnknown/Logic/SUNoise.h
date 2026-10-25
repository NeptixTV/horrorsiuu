// SIGNAL UNKNOWN – deterministic gradient noise (engine independent).
// The procedural world must be identical on every machine in a co-op session, so all
// randomness used for world generation goes through these seeded functions.
#pragma once

#include "SUMathLite.h"

namespace SULogic
{
	inline uint32_t HashU32(uint32_t X)
	{
		X ^= X >> 16;
		X *= 0x7FEB352Du;
		X ^= X >> 15;
		X *= 0x846CA68Bu;
		X ^= X >> 16;
		return X;
	}

	inline uint32_t Hash2D(int32_t IX, int32_t IY, uint32_t Seed)
	{
		return HashU32(static_cast<uint32_t>(IX) * 0x8DA6B343u ^ static_cast<uint32_t>(IY) * 0xD8163841u ^ Seed * 0xCB1AB31Fu);
	}

	/** Uniform value in [0,1) for an integer lattice point. */
	inline double Hash2D01(int32_t IX, int32_t IY, uint32_t Seed)
	{
		return (Hash2D(IX, IY, Seed) >> 8) * (1.0 / 16777216.0);
	}

	/** 2D gradient (Perlin style) noise, result roughly in [-1,1]. */
	inline double GradientNoise2D(double X, double Y, uint32_t Seed)
	{
		const double FX = std::floor(X);
		const double FY = std::floor(Y);
		const int32_t IX = static_cast<int32_t>(FX);
		const int32_t IY = static_cast<int32_t>(FY);
		const double DX = X - FX;
		const double DY = Y - FY;

		auto Grad = [Seed](int32_t GX, int32_t GY, double PX, double PY)
		{
			const uint32_t H = Hash2D(GX, GY, Seed);
			const double Angle = (H >> 8) * (1.0 / 16777216.0) * 2.0 * Pi;
			return std::cos(Angle) * PX + std::sin(Angle) * PY;
		};

		const double N00 = Grad(IX, IY, DX, DY);
		const double N10 = Grad(IX + 1, IY, DX - 1.0, DY);
		const double N01 = Grad(IX, IY + 1, DX, DY - 1.0);
		const double N11 = Grad(IX + 1, IY + 1, DX - 1.0, DY - 1.0);

		// quintic fade
		const double UX = DX * DX * DX * (DX * (DX * 6.0 - 15.0) + 10.0);
		const double UY = DY * DY * DY * (DY * (DY * 6.0 - 15.0) + 10.0);

		const double NX0 = Lerp(N00, N10, UX);
		const double NX1 = Lerp(N01, N11, UX);
		return Lerp(NX0, NX1, UY) * 1.41421356;
	}

	/** Fractal brownian motion; result roughly in [-1,1]. */
	inline double Fbm2D(double X, double Y, uint32_t Seed, int Octaves, double Lacunarity = 2.0, double Gain = 0.5)
	{
		double Sum = 0.0;
		double Amp = 1.0;
		double Norm = 0.0;
		double Freq = 1.0;
		for (int Octave = 0; Octave < Octaves; ++Octave)
		{
			Sum += GradientNoise2D(X * Freq, Y * Freq, Seed + static_cast<uint32_t>(Octave) * 101u) * Amp;
			Norm += Amp;
			Amp *= Gain;
			Freq *= Lacunarity;
		}
		return Norm > 0.0 ? Sum / Norm : 0.0;
	}

	/** Ridged multifractal noise in [0,1], used for mountain ridges. */
	inline double Ridged2D(double X, double Y, uint32_t Seed, int Octaves)
	{
		double Sum = 0.0;
		double Amp = 0.5;
		double Freq = 1.0;
		double Norm = 0.0;
		for (int Octave = 0; Octave < Octaves; ++Octave)
		{
			const double N = 1.0 - std::fabs(GradientNoise2D(X * Freq, Y * Freq, Seed + static_cast<uint32_t>(Octave) * 37u));
			Sum += N * N * Amp;
			Norm += Amp;
			Amp *= 0.5;
			Freq *= 2.1;
		}
		return Norm > 0.0 ? Clamp01(Sum / Norm) : 0.0;
	}
}
