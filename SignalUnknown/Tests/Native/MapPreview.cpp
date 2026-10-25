// Renders a top-down preview of the procedural world layout (PPM image) so the layout can be
// reviewed without the engine:   ./MapPreview out.ppm [size]
#include "../../Source/SignalUnknown/Logic/SUWorldLayout.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace SULogic;

int main(int Argc, char** Argv)
{
	const char* Path = Argc > 1 ? Argv[1] : "map_preview.ppm";
	const int Size = Argc > 2 ? std::atoi(Argv[2]) : 700;
	const WorldLayout Layout;

	const double Half = WorldLayout::WorldHalfSize;
	std::vector<unsigned char> Img(static_cast<size_t>(Size * Size * 3));
	for (int PY = 0; PY < Size; ++PY)
	{
		for (int PX = 0; PX < Size; ++PX)
		{
			// screen up = north (+X), screen right = east (+Y)
			const double North = Half - (PY + 0.5) / Size * 2.0 * Half;
			const double East = -Half + (PX + 0.5) / Size * 2.0 * Half;
			const double H = Layout.Height(North, East);
			const Vec3 Nrm = Layout.Normal(North, East, 8.0);
			const double Shade = Clamp01(0.55 + 0.45 * Nrm.Dot(Vec3(0.5, -0.5, 0.7).Normalized()));
			double R = 0.25, G = 0.38, B = 0.2;
			if (H > 300.0) { R = 0.45; G = 0.43; B = 0.4; }
			if (H > 650.0) { R = 0.9; G = 0.9; B = 0.92; }
			const double Trees = Layout.TreeDensity(North, East);
			G += (1.0 - Trees) * 0.12;
			R += (1.0 - Trees) * 0.1;
			if (Layout.IsWater(North, East)) { R = 0.1; G = 0.2; B = 0.4; }
			const RoadQuery Q = Layout.NearestRoad(North, East, 8.0);
			if (Q.RoadIndex >= 0 && Q.Distance < Layout.Roads[static_cast<size_t>(Q.RoadIndex)].Width * 0.5 + 2.0)
			{
				R = 0.85; G = 0.75; B = 0.5;
			}
			for (const SitePad& Pad : Layout.Pads)
			{
				if ((Vec2(North, East) - Pad.Center).Length() < Pad.Radius)
				{
					R = 0.8; G = 0.2; B = 0.2;
				}
			}
			if (!Layout.IsInsideCore(North, East) && Layout.IsInsideCore(North, East, -12.0))
			{
				R = 1.0; G = 1.0; B = 0.0;
			}
			unsigned char* Px = &Img[static_cast<size_t>((PY * Size + PX) * 3)];
			Px[0] = static_cast<unsigned char>(Clamp01(R * Shade) * 255.0);
			Px[1] = static_cast<unsigned char>(Clamp01(G * Shade) * 255.0);
			Px[2] = static_cast<unsigned char>(Clamp01(B * Shade) * 255.0);
		}
	}

	FILE* F = std::fopen(Path, "wb");
	if (!F)
	{
		std::fprintf(stderr, "cannot write %s\n", Path);
		return 1;
	}
	std::fprintf(F, "P6\n%d %d\n255\n", Size, Size);
	std::fwrite(Img.data(), 1, Img.size(), F);
	std::fclose(F);

	for (const Road& Rd : Layout.Roads)
	{
		std::printf("road %-14s len %7.1f m  z %6.1f .. %6.1f\n", Rd.Name, Rd.Length, Rd.Samples.front().Z, Rd.Samples.back().Z);
	}
	for (const SitePad& Pad : Layout.Pads)
	{
		std::printf("pad  %-12s at (%7.1f, %7.1f) z %6.1f yaw %6.1f\n", Pad.Id, Pad.Center.X, Pad.Center.Y, Pad.Z, Pad.FacingYawDeg);
	}
	std::printf("lake water z %.2f\n", Layout.TheLake.WaterZ);
	return 0;
}
