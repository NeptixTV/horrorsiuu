// SIGNAL UNKNOWN – world layout of the first playable region (engine independent).
//
// Units: meters. Axis convention matches Unreal: +X = north, +Y = east, +Z = up.
// Yaw (degrees) follows Unreal as well: 0 = north, 90 = east.
//
// The layout is fully deterministic for a given seed. The Unreal side (ASUWorldBuilder)
// turns it into terrain meshes, road meshes, forests and building sites on every machine,
// which keeps co-op clients in sync without replicating any static geometry.
#pragma once

#include "SUMathLite.h"
#include "SUNoise.h"

#include <vector>
#include <cstring>

namespace SULogic
{
	enum class RoadSurface : uint8_t
	{
		Asphalt,
		Gravel,
		Dirt
	};

	struct RoadSample
	{
		Vec2 P;
		double Z = 0.0;
		Vec2 Tangent;
		double Along = 0.0;
	};

	struct Road
	{
		const char* Name = "";
		RoadSurface Surface = RoadSurface::Asphalt;
		double Width = 7.0;
		std::vector<Vec2> Control;
		std::vector<RoadSample> Samples;
		double Length = 0.0;
	};

	struct SitePad
	{
		const char* Id = "";
		Vec2 Center;
		double Radius = 10.0;
		double Blend = 10.0;
		double Z = 0.0;
		double FacingYawDeg = 0.0;
	};

	struct Lake
	{
		Vec2 Center;
		double Radius = 450.0;
		double Depth = 12.0;
		double WaterZ = 0.0;
	};

	struct RoadQuery
	{
		int RoadIndex = -1;
		int SegmentIndex = -1;
		double Distance = 1e9;
		double RoadZ = 0.0;
		double SegmentT = 0.0;
		Vec2 Closest;
		Vec2 Tangent;
	};

	class WorldLayout
	{
	public:
		static constexpr double CoreHalfSize = 2000.0;   // playable square (meters)
		static constexpr double WorldHalfSize = 4200.0;  // visible terrain incl. mountains
		static constexpr double RoadSampleSpacing = 4.0;
		static constexpr double RoadShoulder = 11.0;

		explicit WorldLayout(uint32_t InSeed = 7071u)
			: Seed(InSeed)
		{
			Build();
		}

		uint32_t GetSeed() const { return Seed; }

		// ---------------------------------------------------------------- terrain
		double NaturalHeight(double X, double Y) const
		{
			double H = 2.0;
			H += Fbm2D(X / 900.0, Y / 900.0, Seed, 4) * 14.0;
			H += Fbm2D(X / 230.0, Y / 230.0, Seed + 7u, 3) * 2.6;

			// the lookout hill
			const double D2 = X * X + Y * Y;
			H += 48.0 * std::exp(-D2 / (2.0 * 230.0 * 230.0));

			// a smaller wooded ridge between the tower and the north road
			const double RX = X - 520.0;
			const double RY = Y + 420.0;
			H += 18.0 * std::exp(-(RX * RX) / (2.0 * 420.0 * 420.0) - (RY * RY) / (2.0 * 160.0 * 160.0));

			// mountains outside the playable core, with an eastern valley towards the town
			const double M = std::max(std::fabs(X), std::fabs(Y));
			double MountainMask = SmoothStep(1850.0, 3100.0, M);
			const double ValleyX = X + 470.0;
			const double Valley = SmoothStep(1500.0, 2300.0, Y) * std::exp(-(ValleyX * ValleyX) / (2.0 * 430.0 * 430.0));
			MountainMask *= (1.0 - 0.93 * Valley);
			H += MountainMask * (230.0 + 600.0 * Ridged2D(X / 1300.0, Y / 1300.0, Seed + 19u, 5));
			return H;
		}

		/** Final ground height including lake basin, building pads and roads. */
		double Height(double X, double Y) const
		{
			double H = HeightWithoutRoads(X, Y);

			const RoadQuery Q = NearestRoad(X, Y, MaxRoadInfluence);
			if (Q.RoadIndex >= 0)
			{
				const Road& R = Roads[static_cast<size_t>(Q.RoadIndex)];
				const double Core = R.Width * 0.5 + 1.2;
				const double Under = Q.RoadZ - 0.18;
				if (Q.Distance <= Core)
				{
					H = Under;
				}
				else
				{
					const double T = SmoothStep(Core, Core + RoadShoulder, Q.Distance);
					H = Lerp(Under, H, T);
				}
			}
			return H;
		}

		/** Unit normal approximation (Z up) via central differences. */
		Vec3 Normal(double X, double Y, double Step = 2.0) const
		{
			const double HX0 = Height(X - Step, Y);
			const double HX1 = Height(X + Step, Y);
			const double HY0 = Height(X, Y - Step);
			const double HY1 = Height(X, Y + Step);
			return Vec3(-(HX1 - HX0) / (2.0 * Step), -(HY1 - HY0) / (2.0 * Step), 1.0).Normalized();
		}

		/** 0 = flat, 1 = vertical. */
		double Slope(double X, double Y) const
		{
			return 1.0 - Normal(X, Y).Z;
		}

		bool IsWater(double X, double Y) const
		{
			return Height(X, Y) < TheLake.WaterZ - 0.05;
		}

		bool IsInsideCore(double X, double Y, double Margin = 0.0) const
		{
			return std::fabs(X) <= CoreHalfSize - Margin && std::fabs(Y) <= CoreHalfSize - Margin;
		}

		// ---------------------------------------------------------------- roads
		RoadQuery NearestRoad(double X, double Y, double MaxDistance) const
		{
			RoadQuery Best;
			const int CX = CellIndex(X);
			const int CY = CellIndex(Y);
			if (CX < 0 || CY < 0 || CX >= GridDim || CY >= GridDim)
			{
				return Best;
			}
			const std::vector<SegmentRef>& Cell = Grid[static_cast<size_t>(CY * GridDim + CX)];
			const Vec2 P(X, Y);
			for (const SegmentRef& Ref : Cell)
			{
				const Road& R = Roads[static_cast<size_t>(Ref.Road)];
				const RoadSample& A = R.Samples[static_cast<size_t>(Ref.Segment)];
				const RoadSample& B = R.Samples[static_cast<size_t>(Ref.Segment + 1)];
				double T = 0.0;
				const double D = DistanceToSegment(P, A.P, B.P, T);
				if (D < Best.Distance && D <= MaxDistance)
				{
					Best.RoadIndex = Ref.Road;
					Best.SegmentIndex = Ref.Segment;
					Best.Distance = D;
					Best.SegmentT = T;
					Best.RoadZ = Lerp(A.Z, B.Z, T);
					Best.Closest = A.P + (B.P - A.P) * T;
					Best.Tangent = (B.P - A.P).Normalized();
				}
			}
			return Best;
		}

		/** Point on a road at a distance along it (clamped). */
		RoadSample SampleRoadAt(int RoadIndex, double Along) const
		{
			const Road& R = Roads[static_cast<size_t>(RoadIndex)];
			if (R.Samples.empty())
			{
				return RoadSample();
			}
			if (Along <= 0.0)
			{
				return R.Samples.front();
			}
			for (size_t I = 1; I < R.Samples.size(); ++I)
			{
				if (R.Samples[I].Along >= Along)
				{
					const RoadSample& A = R.Samples[I - 1];
					const RoadSample& B = R.Samples[I];
					const double Span = B.Along - A.Along;
					const double T = Span > 1e-6 ? (Along - A.Along) / Span : 0.0;
					RoadSample S;
					S.P = A.P + (B.P - A.P) * T;
					S.Z = Lerp(A.Z, B.Z, T);
					S.Tangent = (B.P - A.P).Normalized();
					S.Along = Along;
					return S;
				}
			}
			return R.Samples.back();
		}

		int FindRoad(const char* Name) const
		{
			for (size_t I = 0; I < Roads.size(); ++I)
			{
				if (std::strcmp(Roads[I].Name, Name) == 0)
				{
					return static_cast<int>(I);
				}
			}
			return -1;
		}

		const SitePad* FindPad(const char* Id) const
		{
			for (const SitePad& Pad : Pads)
			{
				if (std::strcmp(Pad.Id, Id) == 0)
				{
					return &Pad;
				}
			}
			return nullptr;
		}

		// ---------------------------------------------------------------- vegetation
		/** 0 = clearing / meadow, 1 = dense forest (the forest pattern without exclusions). */
		double ForestFactor(double X, double Y) const
		{
			return SmoothStep(-0.30, 0.35, Fbm2D(X / 420.0, Y / 420.0, Seed + 3u, 3));
		}

		/** Probability (0..1) that a tree stands at this point. 0 where trees are not allowed. */
		double TreeDensity(double X, double Y) const
		{
			if (std::fabs(X) > WorldHalfSize - 50.0 || std::fabs(Y) > WorldHalfSize - 50.0)
			{
				return 0.0;
			}
			const RoadQuery Q = NearestRoad(X, Y, MaxRoadInfluence);
			if (Q.RoadIndex >= 0)
			{
				const double Clear = Roads[static_cast<size_t>(Q.RoadIndex)].Width * 0.5 + 3.5;
				if (Q.Distance < Clear)
				{
					return 0.0;
				}
			}
			for (const SitePad& Pad : Pads)
			{
				const double D = (Vec2(X, Y) - Pad.Center).Length();
				if (D < Pad.Radius + 3.0)
				{
					return 0.0;
				}
			}
			const double LakeD = (Vec2(X, Y) - TheLake.Center).Length();
			if (LakeD < TheLake.Radius * 1.08)
			{
				return 0.0;
			}
			const double H = Height(X, Y);
			if (H > 520.0)
			{
				return 0.0; // tree line
			}
			if (Slope(X, Y, 3.0) > 0.42)
			{
				return 0.0;
			}

			double Density = 0.16 + 0.52 * ForestFactor(X, Y);
			// thinner forest higher up the mountains
			Density *= 1.0 - SmoothStep(300.0, 520.0, H) * 0.7;
			// a bit more open around the forest edge of roads (visibility from the car)
			if (Q.RoadIndex >= 0)
			{
				Density *= SmoothStep(4.0, MaxRoadInfluence, Q.Distance) * 0.6 + 0.4;
			}
			return Clamp01(Density);
		}

		double Slope(double X, double Y, double Step) const
		{
			return 1.0 - Normal(X, Y, Step).Z;
		}

		std::vector<Road> Roads;
		std::vector<SitePad> Pads;
		Lake TheLake;

	private:
		struct SegmentRef
		{
			int Road = 0;
			int Segment = 0;
		};

		static constexpr double CellSize = 64.0;
		static constexpr double MaxRoadInfluence = 3.5 + 1.2 + RoadShoulder + 1.0;
		static constexpr int GridDim = static_cast<int>((2.0 * WorldHalfSize) / CellSize) + 1;

		uint32_t Seed;
		std::vector<std::vector<SegmentRef>> Grid;

		static int CellIndex(double V)
		{
			return static_cast<int>(std::floor((V + WorldHalfSize) / CellSize));
		}

		double HeightWithLake(double X, double Y) const
		{
			double H = NaturalHeight(X, Y);
			const double D = (Vec2(X, Y) - TheLake.Center).Length();
			const double R = TheLake.Radius;
			if (D < R * 1.6)
			{
				if (D < R)
				{
					const double K = D / R;
					const double Basin = TheLake.WaterZ - 0.6 - TheLake.Depth * (1.0 - K * K);
					H = std::min(H, Basin);
					if (D > R * 0.85)
					{
						// shallow beach right at the edge
						H = Lerp(H, TheLake.WaterZ - 0.6, SmoothStep(R * 0.85, R, D));
					}
				}
				else
				{
					// keep the shore above the water line so the lake stays contained
					const double ShoreMin = TheLake.WaterZ + 0.4 + (D - R) * 0.04;
					const double T = SmoothStep(R * 1.25, R * 1.6, D);
					H = Lerp(std::max(H, ShoreMin), H, T);
					if (D < R * 1.08)
					{
						H = Lerp(TheLake.WaterZ - 0.6, H, SmoothStep(R, R * 1.08, D));
					}
				}
			}
			return H;
		}

		double HeightWithoutRoads(double X, double Y) const
		{
			double H = HeightWithLake(X, Y);
			const Vec2 P(X, Y);
			for (const SitePad& Pad : Pads)
			{
				const double D = (P - Pad.Center).Length();
				if (D <= Pad.Radius)
				{
					H = Pad.Z;
				}
				else if (D < Pad.Radius + Pad.Blend)
				{
					H = Lerp(Pad.Z, H, SmoothStep(Pad.Radius, Pad.Radius + Pad.Blend, D));
				}
			}
			return H;
		}

		static Vec2 CatmullRom(const Vec2& P0, const Vec2& P1, const Vec2& P2, const Vec2& P3, double T)
		{
			const double T2 = T * T;
			const double T3 = T2 * T;
			return (P1 * 2.0 + (P2 - P0) * T + (P0 * 2.0 - P1 * 5.0 + P2 * 4.0 - P3) * T2 + (P1 * 3.0 - P0 - P2 * 3.0 + P3) * T3) * 0.5;
		}

		static std::vector<Vec2> Resample(const std::vector<Vec2>& Ctrl, double Spacing)
		{
			// dense curve first
			std::vector<Vec2> Dense;
			const size_t N = Ctrl.size();
			for (size_t I = 0; I + 1 < N; ++I)
			{
				const Vec2& P0 = Ctrl[I == 0 ? 0 : I - 1];
				const Vec2& P1 = Ctrl[I];
				const Vec2& P2 = Ctrl[I + 1];
				const Vec2& P3 = Ctrl[I + 2 < N ? I + 2 : N - 1];
				const int Steps = std::max(4, static_cast<int>((P2 - P1).Length() / 1.0));
				for (int S = 0; S < Steps; ++S)
				{
					Dense.push_back(CatmullRom(P0, P1, P2, P3, static_cast<double>(S) / Steps));
				}
			}
			Dense.push_back(Ctrl.back());

			// then equal spacing
			std::vector<Vec2> Out;
			Out.push_back(Dense.front());
			double Carry = 0.0;
			for (size_t I = 1; I < Dense.size(); ++I)
			{
				Vec2 A = Dense[I - 1];
				const Vec2 B = Dense[I];
				double SegLen = (B - A).Length();
				while (Carry + SegLen >= Spacing)
				{
					const double Need = Spacing - Carry;
					const Vec2 Dir = (B - A).Normalized();
					A = A + Dir * Need;
					Out.push_back(A);
					SegLen -= Need;
					Carry = 0.0;
				}
				Carry += SegLen;
			}
			if ((Out.back() - Dense.back()).Length() > Spacing * 0.3)
			{
				Out.push_back(Dense.back());
			}
			else
			{
				Out.back() = Dense.back();
			}
			return Out;
		}

		void AddRoad(const char* Name, RoadSurface Surface, double Width, std::vector<Vec2> Ctrl)
		{
			Road R;
			R.Name = Name;
			R.Surface = Surface;
			R.Width = Width;
			R.Control = std::move(Ctrl);
			Roads.push_back(std::move(R));
		}

		/** Height of an already finished road at a point, or a fallback. */
		bool PinnedHeight(const Vec2& P, size_t SelfIndex, double& OutZ) const
		{
			for (size_t I = 0; I < SelfIndex; ++I)
			{
				const Road& Other = Roads[I];
				for (size_t S = 0; S + 1 < Other.Samples.size(); ++S)
				{
					double T = 0.0;
					const double D = DistanceToSegment(P, Other.Samples[S].P, Other.Samples[S + 1].P, T);
					if (D < 16.0)
					{
						OutZ = Lerp(Other.Samples[S].Z, Other.Samples[S + 1].Z, T);
						return true;
					}
				}
			}
			for (const SitePad& Pad : Pads)
			{
				if ((P - Pad.Center).Length() < Pad.Radius)
				{
					OutZ = Pad.Z;
					return true;
				}
			}
			return false;
		}

		void BuildRoadProfile(size_t Index)
		{
			Road& R = Roads[Index];
			const std::vector<Vec2> Pts = Resample(R.Control, RoadSampleSpacing);
			const size_t N = Pts.size();
			std::vector<double> Z(N);
			for (size_t I = 0; I < N; ++I)
			{
				Z[I] = HeightWithLake(Pts[I].X, Pts[I].Y);
			}

			// smooth the profile (moving average)
			for (int Pass = 0; Pass < 6; ++Pass)
			{
				std::vector<double> Tmp(N);
				for (size_t I = 0; I < N; ++I)
				{
					double Sum = 0.0;
					int Count = 0;
					for (int K = -8; K <= 8; ++K)
					{
						const long J = static_cast<long>(I) + K;
						if (J >= 0 && J < static_cast<long>(N))
						{
							Sum += Z[static_cast<size_t>(J)];
							++Count;
						}
					}
					Tmp[I] = Sum / Count;
				}
				Z.swap(Tmp);
			}

			double PinStart = 0.0;
			double PinEnd = 0.0;
			const bool bPinStart = PinnedHeight(Pts.front(), Index, PinStart);
			const bool bPinEnd = PinnedHeight(Pts.back(), Index, PinEnd);
			const double BlendLen = 18.0; // samples (~72 m)
			if (bPinStart)
			{
				for (size_t I = 0; I < N && I < static_cast<size_t>(BlendLen); ++I)
				{
					Z[I] = Lerp(PinStart, Z[I], SmoothStep(0.0, BlendLen, static_cast<double>(I)));
				}
			}
			if (bPinEnd)
			{
				for (size_t I = 0; I < N && I < static_cast<size_t>(BlendLen); ++I)
				{
					const size_t J = N - 1 - I;
					Z[J] = Lerp(PinEnd, Z[J], SmoothStep(0.0, BlendLen, static_cast<double>(I)));
				}
			}

			// Grade limit (max 9 %). Relaxation instead of a one-directional clamp, so cuts and fills
			// are split between neighbours and a single high point never lifts a whole road.
			const double MaxStep = 0.09 * RoadSampleSpacing;
			for (int Iter = 0; Iter < 4000; ++Iter)
			{
				bool bChanged = false;
				for (size_t I = 0; I + 1 < N; ++I)
				{
					const double D = Z[I + 1] - Z[I];
					const double Excess = std::fabs(D) - MaxStep;
					if (Excess <= 1e-4)
					{
						continue;
					}
					const bool bFixedA = (I == 0 && bPinStart);
					const bool bFixedB = (I + 1 == N - 1 && bPinEnd);
					const double Dir = Sign(D);
					if (bFixedA && bFixedB)
					{
						continue;
					}
					if (bFixedA)
					{
						Z[I + 1] -= Dir * Excess;
					}
					else if (bFixedB)
					{
						Z[I] += Dir * Excess;
					}
					else
					{
						Z[I] += Dir * Excess * 0.5;
						Z[I + 1] -= Dir * Excess * 0.5;
					}
					bChanged = true;
				}
				if (!bChanged)
				{
					break;
				}
			}

			const double MinZ = TheLake.WaterZ + 1.6;
			R.Samples.clear();
			R.Samples.reserve(N);
			double Along = 0.0;
			for (size_t I = 0; I < N; ++I)
			{
				RoadSample S;
				S.P = Pts[I];
				S.Z = std::max(Z[I], MinZ); // never below the water table (ponds, lake)
				const Vec2 Prev = Pts[I == 0 ? 0 : I - 1];
				const Vec2 Next = Pts[I + 1 < N ? I + 1 : N - 1];
				S.Tangent = (Next - Prev).Normalized();
				if (I > 0)
				{
					Along += (Pts[I] - Pts[I - 1]).Length();
				}
				S.Along = Along;
				R.Samples.push_back(S);
			}
			R.Length = Along;
		}

		void IndexRoad(int RoadIndex)
		{
			const Road& R = Roads[static_cast<size_t>(RoadIndex)];
			const double Pad = MaxRoadInfluence + 1.0;
			for (size_t S = 0; S + 1 < R.Samples.size(); ++S)
			{
				const Vec2 A = R.Samples[S].P;
				const Vec2 B = R.Samples[S + 1].P;
				const int X0 = std::max(0, CellIndex(std::min(A.X, B.X) - Pad));
				const int X1 = std::min(GridDim - 1, CellIndex(std::max(A.X, B.X) + Pad));
				const int Y0 = std::max(0, CellIndex(std::min(A.Y, B.Y) - Pad));
				const int Y1 = std::min(GridDim - 1, CellIndex(std::max(A.Y, B.Y) + Pad));
				for (int CY = Y0; CY <= Y1; ++CY)
				{
					for (int CX = X0; CX <= X1; ++CX)
					{
						SegmentRef Ref;
						Ref.Road = RoadIndex;
						Ref.Segment = static_cast<int>(S);
						Grid[static_cast<size_t>(CY * GridDim + CX)].push_back(Ref);
					}
				}
			}
		}

		/** Pad next to a road: placed Offset meters to the left (+) or right (-) of the road at ArcDistance. */
		void AddRoadsidePad(const char* Id, int RoadIndex, double ArcDistance, double Offset, double Radius, double Blend)
		{
			const RoadSample S = SampleRoadAt(RoadIndex, ArcDistance);
			// Unreal axes seen from above: facing +X (north) the right hand side is +Y (east),
			// so for a travel direction T the right vector is (-T.Y, T.X).
			const Vec2 Right(-S.Tangent.Y, S.Tangent.X);
			SitePad P;
			P.Id = Id;
			P.Center = S.P + Right * (-Offset);
			P.Radius = Radius;
			P.Blend = Blend;
			P.Z = S.Z;
			// face the road
			const Vec2 ToRoad = (S.P - P.Center).Normalized();
			P.FacingYawDeg = std::atan2(ToRoad.Y, ToRoad.X) * 180.0 / Pi;
			Pads.push_back(P);
		}

		/** Arc distance on a road closest to a point. */
		double ArcDistanceNear(int RoadIndex, const Vec2& P) const
		{
			const Road& R = Roads[static_cast<size_t>(RoadIndex)];
			double Best = 1e18;
			double BestAlong = 0.0;
			for (const RoadSample& S : R.Samples)
			{
				const double D = (S.P - P).LengthSq();
				if (D < Best)
				{
					Best = D;
					BestAlong = S.Along;
				}
			}
			return BestAlong;
		}

		void Build()
		{
			Grid.assign(static_cast<size_t>(GridDim * GridDim), std::vector<SegmentRef>());

			TheLake.Center = Vec2(-1320.0, -1240.0);
			TheLake.Radius = 470.0;
			TheLake.Depth = 12.0;
			TheLake.WaterZ = NaturalHeight(TheLake.Center.X, TheLake.Center.Y) - 3.0;

			// ---- pads that define their own height
			{
				SitePad Tower;
				Tower.Id = "Tower";
				Tower.Center = Vec2(-6.0, 10.0);
				Tower.Radius = 36.0;
				Tower.Blend = 26.0;
				Tower.Z = HeightWithLake(0.0, 0.0) - 0.8;
				Tower.FacingYawDeg = 153.4; // local +X points at the start of the access road
				Pads.push_back(Tower);

				SitePad Forsthaus;
				Forsthaus.Id = "Forsthaus";
				Forsthaus.Center = Vec2(1446.0, -534.0);
				Forsthaus.Radius = 30.0;
				Forsthaus.Blend = 22.0;
				Forsthaus.Z = HeightWithLake(1446.0, -534.0);
				Forsthaus.FacingYawDeg = 239.0; // local +X points at the driveway
				Pads.push_back(Forsthaus);

				SitePad Town;
				Town.Id = "Town";
				Town.Center = Vec2(-470.0, 3360.0);
				Town.Radius = 300.0;
				Town.Blend = 160.0;
				Town.Z = HeightWithLake(-470.0, 3360.0);
				Town.FacingYawDeg = 270.0;
				Pads.push_back(Town);
			}

			// ---- roads (order matters: later roads pin their ends onto earlier ones)
			AddRoad("MainRoad", RoadSurface::Asphalt, 7.0, {
				Vec2(-690, -2160), Vec2(-680, -2000), Vec2(-720, -1700), Vec2(-790, -1300), Vec2(-760, -900),
				Vec2(-700, -500), Vec2(-662, -150), Vec2(-650, 150), Vec2(-690, 550), Vec2(-660, 950),
				Vec2(-600, 1250), Vec2(-560, 1550), Vec2(-540, 1850), Vec2(-520, 2300), Vec2(-490, 2800),
				Vec2(-470, 3150)});
			AddRoad("TowerAccess", RoadSurface::Gravel, 4.6, {
				Vec2(-26, 20), Vec2(-80, 92), Vec2(-200, 150), Vec2(-330, 122), Vec2(-460, 40),
				Vec2(-560, -38), Vec2(-659, -100)});
			AddRoad("NorthRoad", RoadSurface::Gravel, 5.4, {
				Vec2(-748, -700), Vec2(-560, -760), Vec2(-300, -830), Vec2(0, -860), Vec2(300, -800),
				Vec2(620, -880), Vec2(900, -820), Vec2(1150, -760), Vec2(1350, -700), Vec2(1600, -780),
				Vec2(1800, -860), Vec2(2090, -915)});
			AddRoad("ForsthausDrive", RoadSurface::Dirt, 4.0, {
				Vec2(1351, -700), Vec2(1384, -640), Vec2(1418, -580), Vec2(1440, -545)});

			for (size_t I = 0; I < Roads.size(); ++I)
			{
				BuildRoadProfile(I);
				IndexRoad(static_cast<int>(I));
			}

			// ---- pads that sit next to roads
			const int Main = FindRoad("MainRoad");
			if (Main >= 0)
			{
				AddRoadsidePad("GasStation", Main, ArcDistanceNear(Main, Vec2(-600, 1250)), 34.0, 30.0, 16.0);
				const double Houses[6][2] = {
					{1480.0, 30.0}, {1630.0, 32.0}, {1780.0, 30.0},
					{1530.0, -30.0}, {1690.0, -32.0}, {1840.0, -30.0}};
				static const char* HouseIds[6] = {"House1", "House2", "House3", "House4", "House5", "House6"};
				for (int H = 0; H < 6; ++H)
				{
					const Vec2 Approx(-560.0, Houses[H][0]);
					AddRoadsidePad(HouseIds[H], Main, ArcDistanceNear(Main, Approx), Houses[H][1], 13.0, 10.0);
				}
			}
			const int North = FindRoad("NorthRoad");
			if (North >= 0)
			{
				// pull-out where the abandoned car of mission 2 stands
				AddRoadsidePad("M02Car", North, ArcDistanceNear(North, Vec2(1700, -820)), 9.0, 9.0, 8.0);
			}
		}
	};
}
