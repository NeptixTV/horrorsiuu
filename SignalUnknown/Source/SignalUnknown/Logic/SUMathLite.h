// SIGNAL UNKNOWN – engine-independent math helpers.
// Everything in Source/SignalUnknown/Logic is plain C++ (no Unreal headers) so it can be
// compiled and unit-tested without the engine (see Tests/Native).
#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace SULogic
{
	constexpr double Pi = 3.14159265358979323846;

	inline double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	inline double Clamp01(double V) { return Clamp(V, 0.0, 1.0); }
	inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	inline double SmoothStep(double E0, double E1, double X)
	{
		if (E1 == E0) { return X < E0 ? 0.0 : 1.0; }
		const double T = Clamp01((X - E0) / (E1 - E0));
		return T * T * (3.0 - 2.0 * T);
	}
	inline double Sign(double V) { return V > 0.0 ? 1.0 : (V < 0.0 ? -1.0 : 0.0); }
	inline bool IsFiniteNumber(double V) { return std::isfinite(V); }

	struct Vec2
	{
		double X = 0.0;
		double Y = 0.0;

		Vec2() = default;
		Vec2(double InX, double InY) : X(InX), Y(InY) {}

		Vec2 operator+(const Vec2& O) const { return Vec2(X + O.X, Y + O.Y); }
		Vec2 operator-(const Vec2& O) const { return Vec2(X - O.X, Y - O.Y); }
		Vec2 operator*(double S) const { return Vec2(X * S, Y * S); }
		double Dot(const Vec2& O) const { return X * O.X + Y * O.Y; }
		double LengthSq() const { return X * X + Y * Y; }
		double Length() const { return std::sqrt(LengthSq()); }
		Vec2 Normalized() const
		{
			const double L = Length();
			return L > 1e-9 ? Vec2(X / L, Y / L) : Vec2(0.0, 0.0);
		}
		Vec2 Perp() const { return Vec2(-Y, X); }
	};

	struct Vec3
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;

		Vec3() = default;
		Vec3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}

		Vec3 operator+(const Vec3& O) const { return Vec3(X + O.X, Y + O.Y, Z + O.Z); }
		Vec3 operator-(const Vec3& O) const { return Vec3(X - O.X, Y - O.Y, Z - O.Z); }
		Vec3 operator*(double S) const { return Vec3(X * S, Y * S, Z * S); }
		double Dot(const Vec3& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		Vec3 Cross(const Vec3& O) const { return Vec3(Y * O.Z - Z * O.Y, Z * O.X - X * O.Z, X * O.Y - Y * O.X); }
		double Length() const { return std::sqrt(X * X + Y * Y + Z * Z); }
		Vec3 Normalized() const
		{
			const double L = Length();
			return L > 1e-9 ? Vec3(X / L, Y / L, Z / L) : Vec3(0.0, 0.0, 0.0);
		}
	};

	/** Distance from P to segment AB; OutT receives the clamped segment parameter. */
	inline double DistanceToSegment(const Vec2& P, const Vec2& A, const Vec2& B, double& OutT)
	{
		const Vec2 AB = B - A;
		const double LenSq = AB.LengthSq();
		OutT = LenSq > 1e-12 ? Clamp01((P - A).Dot(AB) / LenSq) : 0.0;
		const Vec2 Closest = A + AB * OutT;
		return (P - Closest).Length();
	}

	/** Small deterministic RNG (xorshift32). Same sequence on every platform. */
	class FastRng
	{
	public:
		explicit FastRng(uint32_t Seed = 0x9E3779B9u) : State(Seed ? Seed : 0x9E3779B9u) {}

		uint32_t NextU32()
		{
			uint32_t S = State;
			S ^= S << 13;
			S ^= S >> 17;
			S ^= S << 5;
			State = S;
			return S;
		}
		/** Uniform in [0,1). */
		double Next01() { return (NextU32() >> 8) * (1.0 / 16777216.0); }
		/** Uniform in [-1,1). */
		double NextSigned() { return Next01() * 2.0 - 1.0; }
		double Range(double Lo, double Hi) { return Lo + (Hi - Lo) * Next01(); }
		int RangeInt(int Lo, int HiInclusive)
		{
			if (HiInclusive <= Lo) { return Lo; }
			return Lo + static_cast<int>(NextU32() % static_cast<uint32_t>(HiInclusive - Lo + 1));
		}

	private:
		uint32_t State;
	};
}
