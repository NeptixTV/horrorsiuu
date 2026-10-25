// SIGNAL UNKNOWN – photo evidence rating (engine independent).
//
// A photo only counts as evidence if the registered evidence object was really in the shot:
// projected into the frame, large enough, not occluded, close enough, lit and not blurred.
// The Unreal side gathers the raw measurements (projection of the evidence bounds,
// visibility traces, camera shake) and this file turns them into a quality value.
#pragma once

#include "SUMathLite.h"

namespace SULogic
{
	struct PhotoCamera
	{
		Vec3 Position;
		Vec3 Forward;
		Vec3 Right;
		Vec3 Up;
		double TanHalfFovX = 0.7;   // tan(horizontal fov / 2)
		double TanHalfFovY = 0.4;   // tan(vertical fov / 2)
		double NearPlane = 10.0;
	};

	/** Screen-space rectangle of a projected box in normalized device coords (-1..1). */
	struct FrameRect
	{
		bool bAnyInFront = false;
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;
	};

	struct FrameMetrics
	{
		double Coverage = 0.0;        // share of the frame covered by the visible part (0..1)
		double InFrameFraction = 0.0; // visible part / whole projected object (0..1)
		double CenterOffset = 1.0;    // 0 = centred, 1 = at the frame edge
	};

	struct EvidenceShot
	{
		FrameMetrics Frame;
		double Visibility = 0.0;      // share of unoccluded sample points (0..1)
		double Distance = 0.0;
		double MaxDistance = 2500.0;
		double MinCoverage = 0.004;   // below this the object is too small to recognise
		double IdealCoverage = 0.08;  // at/above this size counts fully
		bool bLit = true;
		bool bBlurred = false;
		double FocusError = 0.0;      // 0 sharp .. 1 completely out of focus
	};

	/** Projects the 8 corners of a box (given in world space) into the photo frame. */
	inline FrameRect ProjectCorners(const Vec3 Corners[8], const PhotoCamera& Cam)
	{
		FrameRect R;
		R.MinX = 1e9;
		R.MinY = 1e9;
		R.MaxX = -1e9;
		R.MaxY = -1e9;
		int InFront = 0;
		for (int I = 0; I < 8; ++I)
		{
			const Vec3 D = Corners[I] - Cam.Position;
			const double Depth = D.Dot(Cam.Forward);
			const double Side = D.Dot(Cam.Right);
			const double Vert = D.Dot(Cam.Up);
			double SX = 0.0;
			double SY = 0.0;
			if (Depth > Cam.NearPlane)
			{
				SX = Side / (Depth * Cam.TanHalfFovX);
				SY = Vert / (Depth * Cam.TanHalfFovY);
				++InFront;
			}
			else
			{
				// Corner behind the camera: push it far out in the direction it lies in, which
				// conservatively treats the object as extending past that frame edge.
				SX = Sign(Side) * 10.0;
				SY = Sign(Vert) * 10.0;
			}
			R.MinX = std::min(R.MinX, SX);
			R.MinY = std::min(R.MinY, SY);
			R.MaxX = std::max(R.MaxX, SX);
			R.MaxY = std::max(R.MaxY, SY);
		}
		R.bAnyInFront = InFront > 0;
		return R;
	}

	inline FrameMetrics MeasureFrame(const FrameRect& R)
	{
		FrameMetrics M;
		if (!R.bAnyInFront)
		{
			return M;
		}
		const double FullW = std::max(0.0, R.MaxX - R.MinX);
		const double FullH = std::max(0.0, R.MaxY - R.MinY);
		const double FullArea = FullW * FullH;

		const double CMinX = Clamp(R.MinX, -1.0, 1.0);
		const double CMaxX = Clamp(R.MaxX, -1.0, 1.0);
		const double CMinY = Clamp(R.MinY, -1.0, 1.0);
		const double CMaxY = Clamp(R.MaxY, -1.0, 1.0);
		const double ClipW = std::max(0.0, CMaxX - CMinX);
		const double ClipH = std::max(0.0, CMaxY - CMinY);
		const double ClipArea = ClipW * ClipH;

		M.Coverage = Clamp01(ClipArea / 4.0); // the frame is 2x2 in NDC
		M.InFrameFraction = FullArea > 1e-12 ? Clamp01(ClipArea / FullArea) : 0.0;
		if (ClipArea > 0.0)
		{
			const double CX = (CMinX + CMaxX) * 0.5;
			const double CY = (CMinY + CMaxY) * 0.5;
			M.CenterOffset = Clamp01(std::sqrt(CX * CX + CY * CY) / 1.41421356);
		}
		return M;
	}

	/** Quality 0..1. Zero means "not usable as evidence". */
	inline double EvidenceQuality(const EvidenceShot& Shot)
	{
		const FrameMetrics& F = Shot.Frame;
		if (F.Coverage < Shot.MinCoverage || F.InFrameFraction < 0.2)
		{
			return 0.0;
		}
		if (Shot.Visibility < 0.2)
		{
			return 0.0;
		}
		if (Shot.Distance > Shot.MaxDistance)
		{
			return 0.0;
		}

		const double Size = Clamp01((F.Coverage - Shot.MinCoverage) / std::max(1e-6, Shot.IdealCoverage - Shot.MinCoverage));
		const double SizeScore = 0.35 + 0.65 * std::sqrt(Size);
		const double FrameScore = std::pow(F.InFrameFraction, 0.7);
		const double CenterScore = 1.0 - 0.25 * F.CenterOffset;
		const double VisScore = Clamp01((Shot.Visibility - 0.2) / 0.6);
		const double DistScore = 1.0 - SmoothStep(Shot.MaxDistance * 0.6, Shot.MaxDistance, Shot.Distance);
		const double LightScore = Shot.bLit ? 1.0 : 0.3;
		const double BlurScore = Shot.bBlurred ? 0.4 : 1.0;
		const double FocusScore = 1.0 - 0.6 * Clamp01(Shot.FocusError);

		const double Q = SizeScore * FrameScore * CenterScore * VisScore * DistScore * LightScore * BlurScore * FocusScore;
		return Clamp01(Q);
	}

	/** Minimum quality for a photo to count as documented evidence. */
	constexpr double MinEvidenceQuality = 0.15;

	inline const char* QualityLabel(double Quality)
	{
		if (Quality >= 0.75) { return "Sehr gut"; }
		if (Quality >= 0.5) { return "Gut"; }
		if (Quality >= 0.3) { return "Brauchbar"; }
		if (Quality >= MinEvidenceQuality) { return "Schwach"; }
		return "Unbrauchbar";
	}

	/** Camera shake rating from angular speed (deg/s) and own speed (cm/s) at the shutter moment. */
	inline bool IsShotBlurred(double AngularSpeedDegPerSec, double LinearSpeedCmPerSec, double ZoomFactor)
	{
		// longer focal lengths magnify shake
		const double Angular = AngularSpeedDegPerSec * std::max(1.0, ZoomFactor);
		return Angular > 55.0 || LinearSpeedCmPerSec > 420.0;
	}
}
