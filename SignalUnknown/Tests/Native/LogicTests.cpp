// Native unit tests for the engine-independent game logic in Source/SignalUnknown/Logic.
// Build & run:  Tests/Native/run_tests.sh   (needs only a C++20 compiler)
#include "../../Source/SignalUnknown/Logic/SUWorldLayout.h"
#include "../../Source/SignalUnknown/Logic/SUEvidenceLogic.h"
#include "../../Source/SignalUnknown/Logic/SUEconomyLogic.h"
#include "../../Source/SignalUnknown/Logic/SUVehicleLogic.h"
#include "../../Source/SignalUnknown/Logic/SUDirectorLogic.h"
#include "../../Source/SignalUnknown/Logic/SUObjectiveLogic.h"
#include "../../Source/SignalUnknown/Logic/SUDsp.h"
#include "../../Source/SignalUnknown/Logic/SUModelData.h"
#include "../../Source/SignalUnknown/Logic/SUManorData.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>

using namespace SULogic;

static int GFailures = 0;
static int GChecks = 0;

#define EXPECT(Cond) do { ++GChecks; if (!(Cond)) { ++GFailures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); } } while (0)
#define EXPECT_NEAR(A, B, Eps) EXPECT(std::fabs((A) - (B)) <= (Eps))

static void TestWorldLayout()
{
	std::printf("[world layout]\n");
	const WorldLayout L1(7071u);
	const WorldLayout L2(7071u);
	// determinism: every co-op machine must build the same world
	for (double X = -1900.0; X <= 1900.0; X += 237.0)
	{
		for (double Y = -1900.0; Y <= 1900.0; Y += 211.0)
		{
			EXPECT(L1.Height(X, Y) == L2.Height(X, Y));
			EXPECT(L1.TreeDensity(X, Y) == L2.TreeDensity(X, Y));
		}
	}
	EXPECT(L1.Roads.size() == 5);
	for (const Road& R : L1.Roads)
	{
		EXPECT(R.Samples.size() > 10);
		double MaxGrade = 0.0;
		for (size_t I = 1; I < R.Samples.size(); ++I)
		{
			const double Ds = R.Samples[I].Along - R.Samples[I - 1].Along;
			EXPECT(Ds > 0.5 && Ds < 6.0);
			MaxGrade = std::max(MaxGrade, std::fabs(R.Samples[I].Z - R.Samples[I - 1].Z) / Ds);
			EXPECT(IsFiniteNumber(R.Samples[I].Z));
		}
		EXPECT(MaxGrade < 0.13);
		// the road surface is above the water table everywhere
		for (const RoadSample& S : R.Samples)
		{
			EXPECT(S.Z >= L1.TheLake.WaterZ + 1.0);
		}
		// terrain under the road centre is slightly below the road surface
		for (size_t I = 0; I < R.Samples.size(); I += 25)
		{
			const RoadSample& S = R.Samples[I];
			const double H = L1.Height(S.P.X, S.P.Y);
			EXPECT(H <= S.Z + 0.01);
			EXPECT(H >= S.Z - 0.6);
		}
	}
	// roads connect: the tower access road ends on the main road
	const int Main = L1.FindRoad("MainRoad");
	const int Access = L1.FindRoad("TowerAccess");
	EXPECT(Main >= 0 && Access >= 0);
	const RoadSample End = L1.Roads[static_cast<size_t>(Access)].Samples.back();
	const RoadQuery Q = L1.NearestRoad(End.P.X, End.P.Y, 20.0);
	EXPECT(Q.RoadIndex >= 0);
	const RoadQuery QMain = L1.NearestRoad(End.P.X + 0.0, End.P.Y + 0.0, 20.0);
	EXPECT(QMain.Distance < 10.0);

	// building pads are flat
	for (const SitePad& Pad : L1.Pads)
	{
		const double H0 = L1.Height(Pad.Center.X, Pad.Center.Y);
		const double H1 = L1.Height(Pad.Center.X + Pad.Radius * 0.5, Pad.Center.Y);
		// pads next to roads may be partly covered by the road itself
		EXPECT(std::fabs(H0 - H1) < 0.8);
		EXPECT(L1.TreeDensity(Pad.Center.X, Pad.Center.Y) == 0.0);
	}
	EXPECT(L1.FindPad("Tower") != nullptr);
	EXPECT(L1.FindPad("Forsthaus") != nullptr);
	EXPECT(L1.FindPad("GasStation") != nullptr);
	EXPECT(L1.FindPad("M02Car") != nullptr);
	// the tower stands on the highest point of the core area
	const double TowerZ = L1.Height(0.0, 0.0);
	EXPECT(TowerZ > L1.Height(-600.0, 0.0) + 20.0);
	// lake really is water, the tower is not
	EXPECT(L1.IsWater(L1.TheLake.Center.X, L1.TheLake.Center.Y));
	EXPECT(!L1.IsWater(0.0, 0.0));
	// no trees on the road
	const RoadSample Mid = L1.Roads[static_cast<size_t>(Main)].Samples[300];
	EXPECT(L1.TreeDensity(Mid.P.X, Mid.P.Y) == 0.0);
	// some forest exists
	int Forest = 0;
	for (double X = -1800.0; X <= 1800.0; X += 50.0)
	{
		for (double Y = -1800.0; Y <= 1800.0; Y += 50.0)
		{
			if (L1.TreeDensity(X, Y) > 0.3) { ++Forest; }
		}
	}
	EXPECT(Forest > 500);
}

static void TestEvidence()
{
	std::printf("[evidence]\n");
	PhotoCamera Cam;
	Cam.Position = Vec3(0, 0, 0);
	Cam.Forward = Vec3(1, 0, 0);
	Cam.Right = Vec3(0, 1, 0);
	Cam.Up = Vec3(0, 0, 1);
	Cam.TanHalfFovX = std::tan(70.0 * 0.5 * Pi / 180.0);
	Cam.TanHalfFovY = Cam.TanHalfFovX * 9.0 / 16.0;

	auto Box = [](Vec3 C, Vec3 E, Vec3 Out[8])
	{
		int K = 0;
		for (int SX = -1; SX <= 1; SX += 2)
			for (int SY = -1; SY <= 1; SY += 2)
				for (int SZ = -1; SZ <= 1; SZ += 2)
					Out[K++] = Vec3(C.X + SX * E.X, C.Y + SY * E.Y, C.Z + SZ * E.Z);
	};

	Vec3 Corners[8];
	// a 1 m object 3 m in front, centred
	Box(Vec3(300, 0, 0), Vec3(50, 50, 50), Corners);
	FrameMetrics M = MeasureFrame(ProjectCorners(Corners, Cam));
	EXPECT(M.Coverage > 0.03 && M.Coverage < 0.5);
	EXPECT_NEAR(M.InFrameFraction, 1.0, 1e-6);
	EXPECT(M.CenterOffset < 0.05);

	EvidenceShot Shot;
	Shot.Frame = M;
	Shot.Visibility = 1.0;
	Shot.Distance = 300.0;
	const double Good = EvidenceQuality(Shot);
	EXPECT(Good > 0.75);

	// behind the camera -> nothing
	Box(Vec3(-300, 0, 0), Vec3(50, 50, 50), Corners);
	M = MeasureFrame(ProjectCorners(Corners, Cam));
	EXPECT(M.Coverage == 0.0);
	Shot.Frame = M;
	EXPECT(EvidenceQuality(Shot) == 0.0);

	// far away and tiny -> not recognisable
	Box(Vec3(6000, 0, 0), Vec3(10, 10, 10), Corners);
	Shot.Frame = MeasureFrame(ProjectCorners(Corners, Cam));
	Shot.Distance = 6000.0;
	EXPECT(EvidenceQuality(Shot) == 0.0);

	// mostly outside the frame -> weak
	Box(Vec3(300, 260, 0), Vec3(50, 50, 50), Corners);
	Shot.Frame = MeasureFrame(ProjectCorners(Corners, Cam));
	Shot.Distance = 400.0;
	EXPECT(Shot.Frame.InFrameFraction < 0.5);
	EXPECT(EvidenceQuality(Shot) < Good);

	// occluded
	Box(Vec3(300, 0, 0), Vec3(50, 50, 50), Corners);
	Shot.Frame = MeasureFrame(ProjectCorners(Corners, Cam));
	Shot.Distance = 300.0;
	Shot.Visibility = 0.1;
	EXPECT(EvidenceQuality(Shot) == 0.0);

	// blurred and dark photos are worth less
	Shot.Visibility = 1.0;
	Shot.bBlurred = true;
	const double Blurred = EvidenceQuality(Shot);
	EXPECT(Blurred < Good * 0.5);
	Shot.bBlurred = false;
	Shot.bLit = false;
	EXPECT(EvidenceQuality(Shot) < Good * 0.5);

	// object surrounding the camera (corners behind) still produces a bounded result
	Box(Vec3(30, 0, 0), Vec3(100, 100, 100), Corners);
	M = MeasureFrame(ProjectCorners(Corners, Cam));
	EXPECT(M.Coverage <= 1.0 && M.Coverage > 0.9);

	EXPECT(std::string(QualityLabel(0.9)) == "Sehr gut");
	EXPECT(std::string(QualityLabel(0.05)) == "Unbrauchbar");
	EXPECT(IsShotBlurred(80.0, 0.0, 1.0));
	EXPECT(!IsShotBlurred(10.0, 50.0, 1.0));
	EXPECT(IsShotBlurred(20.0, 0.0, 4.0));
}

static void TestEconomy()
{
	std::printf("[economy]\n");
	std::vector<PayRequirement> Reqs(4);
	Reqs[0].Bonus = 60; Reqs[0].bRequired = true;
	Reqs[1].Bonus = 60; Reqs[1].bRequired = true;
	Reqs[2].Bonus = 40; Reqs[2].bRequired = true;
	Reqs[3].Bonus = 0; Reqs[3].bRare = true; Reqs[3].RareBonus = 150;

	// nothing submitted -> reduced base only
	PayResult R = ComputeReportPayout(200, 100, Reqs, {});
	EXPECT(R.Total == 70);
	EXPECT(!R.bAllRequiredFound);
	EXPECT(std::string(ReportRating(R)) == "Unvollständig");

	// perfect report
	std::vector<SubmittedHit> Hits = {{0, 1.0}, {1, 1.0}, {2, 1.0}};
	R = ComputeReportPayout(200, 100, Reqs, Hits);
	EXPECT(R.bAllRequiredFound);
	EXPECT(R.Total == 200 + 60 + 60 + 40 + 100);
	EXPECT(std::string(ReportRating(R)) == "Hervorragend");

	// duplicate photos of the same evidence only count once (best one)
	Hits.push_back({0, 0.3});
	Hits.push_back({0, 0.9});
	PayResult R2 = ComputeReportPayout(200, 100, Reqs, Hits);
	EXPECT(R2.Total == R.Total);

	// rare discovery
	Hits.push_back({3, 0.4});
	R2 = ComputeReportPayout(200, 100, Reqs, Hits);
	EXPECT(R2.Total == R.Total + 150);

	// unusable quality does not count
	R2 = ComputeReportPayout(200, 100, Reqs, {{0, 0.05}});
	EXPECT(R2.RequiredFound == 0);

	// evidence already reported earlier pays nothing again
	Reqs[0].bAlreadyReported = true;
	R2 = ComputeReportPayout(200, 100, Reqs, {{0, 1.0}});
	EXPECT(R2.EvidenceAmounts[0] == 0);

	// invalid indices are ignored
	R2 = ComputeReportPayout(200, 100, Reqs, {{-1, 1.0}, {99, 1.0}});
	EXPECT(R2.RequiredFound == 0);

	EXPECT(FuelCostCents(10.0, 179) == 1790);
	EXPECT(FuelCostCents(-3.0, 179) == 0);
}

static void TestVehicle()
{
	std::printf("[vehicle]\n");
	FuelModel F;
	EXPECT(FuelFlowLitersPerSecond(F, false, 1.0, 30.0) == 0.0);
	const double Idle = FuelFlowLitersPerSecond(F, true, 0.0, 0.0);
	EXPECT(Idle > 0.0);
	const double Cruise = FuelFlowLitersPerSecond(F, true, 0.3, 60.0 / 3.6);
	const double Full = FuelFlowLitersPerSecond(F, true, 1.0, 60.0 / 3.6);
	EXPECT(Full > Cruise && Cruise > Idle);
	// realistic-ish consumption for an old SUV: 15..40 l/100km at 60 km/h
	const double Per100 = Cruise / (60.0 / 3.6) * 1000.0 * 100.0;
	EXPECT(Per100 > 15.0 && Per100 < 40.0);
	EXPECT(EstimatedRangeKm(F, 70.0) > 150.0);
	EXPECT(EstimatedRangeKm(F, 0.0) == 0.0);

	EngineModel E;
	EXPECT(EngineDriveForce(E, 1.0, 0.0, 1.0) > 0.0);
	EXPECT(EngineDriveForce(E, 1.0, E.TopSpeedMps + 1.0, 1.0) == 0.0);
	EXPECT(EngineDriveForce(E, -1.0, 0.0, 1.0) < 0.0);
	EXPECT(EngineDriveForce(E, 1.0, 30.0, 1.0) < EngineDriveForce(E, 1.0, 5.0, 1.0));
	EXPECT(EngineDriveForce(E, 1.0, 20.0, 0.0) < EngineDriveForce(E, 1.0, 20.0, 1.0));
	EXPECT(ResistanceForce(30.0, 1800.0) < 0.0);
	EXPECT(ResistanceForce(-5.0, 1800.0) > 0.0);

	WheelInput W;
	W.NormalLoadN = 4500.0;
	W.Mu = 1.0;
	W.LatVelMps = 3.0;
	WheelOutput O = ComputeWheelForces(W);
	EXPECT(O.LatForceN < 0.0);
	EXPECT(std::sqrt(O.LatForceN * O.LatForceN + O.LongForceN * O.LongForceN) <= W.Mu * W.NormalLoadN + 1e-6);
	// in the air no forces
	W.NormalLoadN = 0.0;
	O = ComputeWheelForces(W);
	EXPECT(O.LatForceN == 0.0 && O.LongForceN == 0.0);
	// braking never pushes backwards beyond stopping
	W.NormalLoadN = 4500.0;
	W.LatVelMps = 0.0;
	W.LongVelMps = 0.01;
	W.BrakeForceN = 10000.0;
	O = ComputeWheelForces(W);
	EXPECT(O.LongForceN <= 0.0);
	EXPECT(std::fabs(O.LongForceN) <= 0.01 * W.MassShareKg / W.Dt + 1e-6);
	// low friction surface slips earlier
	W.LongVelMps = 20.0;
	W.Mu = 0.3;
	O = ComputeWheelForces(W);
	EXPECT(O.bSlipping);
	EXPECT(SuspensionForce(-0.1, 1.0, 50000.0, 4000.0) == 0.0);
	EXPECT(SuspensionForce(0.1, 0.0, 50000.0, 4000.0) > 0.0);
}

static void TestDirector()
{
	std::printf("[horror director]\n");
	DirectorContext C;
	C.Darkness = 1.0;
	C.StoryStage = 3;
	C.Intensity = 1.0;
	DirectorState S;

	// scripted sequences block everything
	C.bInScriptedSequence = true;
	EXPECT(HorrorTriggerChance(C, S) == 0.0);
	C.bInScriptedSequence = false;

	// the director can always decide to do nothing
	C.Now = 1000.0;
	const double Chance = HorrorTriggerChance(C, S);
	EXPECT(Chance > 0.0 && Chance < 1.0);
	DirectorDecision D = DecideHorrorEvent(C, S, 0.999, 0.5);
	EXPECT(D.Event == HorrorEvent::None);

	// cooldowns and tension are respected
	FastRng Rng(42u);
	int Fired = 0;
	int Large = 0;
	std::vector<double> LastFire(static_cast<size_t>(HorrorEvent::Count), -1e9);
	for (int Step = 0; Step < 4000; ++Step)
	{
		C.Now = 2000.0 + Step * 25.0;
		C.bInVehicle = (Step / 40) % 2 == 0;
		C.bOnRoad = C.bInVehicle;
		C.VehicleSpeedKmh = C.bInVehicle ? 70.0 : 0.0;
		C.bRadioNearby = true;
		C.bLightNearby = true;
		C.bBuildingNearby = !C.bInVehicle;
		C.bVehicleNearbyOnFoot = !C.bInVehicle;
		DecayHorrorTension(S, 25.0);
		const DirectorDecision Dec = DecideHorrorEvent(C, S, Rng.Next01(), Rng.Next01());
		if (Dec.Event != HorrorEvent::None)
		{
			const auto& Spec = HorrorCatalog()[static_cast<size_t>(Dec.Event)];
			EXPECT(C.Now - LastFire[static_cast<size_t>(Dec.Event)] >= Spec.CooldownSeconds);
			LastFire[static_cast<size_t>(Dec.Event)] = C.Now;
			if (Spec.bNeedsVehicleMoving) { EXPECT(C.bInVehicle); }
			if (Spec.bNeedsOnFoot) { EXPECT(!C.bInVehicle); }
			if (Spec.Size == HorrorEventSize::Large) { ++Large; }
			++Fired;
		}
	}
	// not every evaluation fires, but something happens over ~28 hours of play
	EXPECT(Fired > 20);
	EXPECT(Fired < 2000);
	EXPECT(Large > 0);

	// story gating: at stage 0 no medium/large events
	DirectorState S2;
	DirectorContext C2 = C;
	C2.StoryStage = 0;
	for (int Step = 0; Step < 500; ++Step)
	{
		C2.Now = 5000.0 + Step * 30.0;
		const DirectorDecision Dec = DecideHorrorEvent(C2, S2, Rng.Next01(), Rng.Next01());
		if (Dec.Event != HorrorEvent::None)
		{
			EXPECT(HorrorCatalog()[static_cast<size_t>(Dec.Event)].MinStoryStage == 0);
		}
	}

	// safe zone strongly reduces chances
	DirectorState S3;
	C.bInSafeZone = false;
	const double Outside = HorrorTriggerChance(C, S3);
	C.bInSafeZone = true;
	EXPECT(HorrorTriggerChance(C, S3) < Outside * 0.5);
	// intensity 0 disables the director
	C.Intensity = 0.0;
	EXPECT(HorrorTriggerChance(C, S3) == 0.0);
}

static void TestObjectives()
{
	std::printf("[objectives]\n");
	std::vector<ObjectiveSpec> Specs = {
		{0, 1, false}, // answer radio
		{1, 1, false}, // take camera
		{1, 1, false}, // take flashlight
		{1, 1, true},  // optional
		{2, 2, false}, // two photos
	};
	std::vector<ObjectiveProgress> P(Specs.size());
	EXPECT(CurrentObjectiveStage(Specs, P) == 0);
	EXPECT(!IsObjectiveActive(Specs, P, 1));
	EXPECT(!AdvanceObjective(Specs, P, 1)); // not active yet
	EXPECT(AdvanceObjective(Specs, P, 0));
	EXPECT(CurrentObjectiveStage(Specs, P) == 1);
	EXPECT(IsObjectiveActive(Specs, P, 1) && IsObjectiveActive(Specs, P, 2) && IsObjectiveActive(Specs, P, 3));
	EXPECT(AdvanceObjective(Specs, P, 2));
	EXPECT(AdvanceObjective(Specs, P, 1));
	// optional one does not block
	EXPECT(CurrentObjectiveStage(Specs, P) == 2);
	EXPECT(IsObjectiveActive(Specs, P, 3));
	EXPECT(!AdvanceObjective(Specs, P, 4));
	EXPECT(AdvanceObjective(Specs, P, 4));
	EXPECT(AllRequiredObjectivesDone(Specs, P));
	EXPECT(!AdvanceObjective(Specs, P, 4)); // already done
}

static void TestDsp()
{
	std::printf("[dsp]\n");
	using namespace Dsp;
	for (int K = 0; K < static_cast<int>(SoundKind::Count); ++K)
	{
		const SoundKind Kind = static_cast<SoundKind>(K);
		std::unique_ptr<Generator> G = CreateGenerator(Kind);
		EXPECT(G != nullptr);
		if (!G)
		{
			continue;
		}
		const int Channels = IsStereoKind(Kind) ? 2 : 1;
		G->Prepare(48000.0f, Channels, static_cast<uint32_t>(K + 1));
		G->SetParam(Param::Gain, 1.0f);
		G->SetParam(Param::A, 1.0f);
		G->SetParam(Param::B, 1.0f);
		G->SetParam(Param::C, 1.0f);
		G->SetParam(Param::D, 0.5f);
		if (Kind == SoundKind::Engine)
		{
			G->SetParam(Param::A, 3200.0f);
			G->Trigger(0, 1.0f);
		}
		if (Kind == SoundKind::Music)
		{
			G->SetParam(Param::A, static_cast<float>(K % 3));
			G->SetParam(Param::D, 0.0f);
		}
		for (int T = 0; T < 8; ++T)
		{
			G->Trigger(T % 5, 1.0f);
		}
		std::vector<float> Buf(static_cast<size_t>(512 * Channels));
		double Energy = 0.0;
		bool bFinite = true;
		float Peak = 0.0f;
		for (int Block = 0; Block < 94 * 3; ++Block) // ~3 s
		{
			G->Render(Buf.data(), 512);
			for (float S : Buf)
			{
				if (!std::isfinite(S)) { bFinite = false; }
				Peak = std::max(Peak, std::fabs(S));
				Energy += static_cast<double>(S) * S;
			}
			if (Block % 30 == 0)
			{
				G->Trigger(Block % 5, 0.8f);
			}
		}
		EXPECT(bFinite);
		EXPECT(Peak <= 1.0f);
		if (Energy <= 1e-6)
		{
			std::printf("  silent generator kind %d\n", K);
		}
		EXPECT(Energy > 1e-6);

		// gain 0 -> silence (after the smoother settled)
		G->SetParam(Param::Gain, 0.0f);
		for (int Block = 0; Block < 40; ++Block)
		{
			G->Render(Buf.data(), 512);
		}
		float After = 0.0f;
		for (float S : Buf) { After = std::max(After, std::fabs(S)); }
		EXPECT(After < 1e-3f);
	}

	// trigger queue overflow is handled (no crash, drops extra)
	std::unique_ptr<Generator> G = CreateGenerator(SoundKind::Click);
	G->Prepare(44100.0f, 1, 3u);
	for (int I = 0; I < 200; ++I) { G->Trigger(0, 1.0f); }
	std::vector<float> Buf(256);
	G->Render(Buf.data(), 256);
	EXPECT(std::isfinite(Buf[10]));
}


// ------------------------------------------------------------------------------------ models
static std::string GModelDir;

static bool LoadFile(const std::string& Path, std::vector<uint8_t>& Out)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In)
	{
		return false;
	}
	Out.assign(std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>());
	return true;
}

static void TestModelFile(const char* FileName, int ExpectedClips, bool bOptional)
{
	std::vector<uint8_t> Bytes;
	const bool bFound = LoadFile(GModelDir + FileName, Bytes);
	if (!bFound && bOptional)
	{
		std::printf("  %s not present (optional) - skipped\n", FileName);
		return;
	}
	EXPECT(bFound);
	if (!bFound)
	{
		return;
	}
	SUModel::File F;
	std::string Error;
	const bool bOk = SUModel::Parse(Bytes.data(), Bytes.size(), F, Error);
	if (!bOk)
	{
		std::printf("  %s: %s\n", FileName, Error.c_str());
	}
	EXPECT(bOk);
	EXPECT(!F.Meshes.empty());
	EXPECT(static_cast<int>(F.Clips.size()) == ExpectedClips);
	size_t Tris = 0;
	for (const SUModel::Mesh& M : F.Meshes)
	{
		EXPECT(!M.Sections.empty());
		for (const SUModel::Section& S : M.Sections)
		{
			EXPECT(S.NumVertices > 0 && S.Indices.size() % 3 == 0);
			Tris += S.Indices.size() / 3;
			// normals are unit length
			for (size_t V = 0; V < S.NumVertices; V += 97)
			{
				const float* N = &S.Normals[V * 3];
				EXPECT_NEAR(std::sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]), 1.0f, 0.02f);
			}
		}
	}
	for (const SUModel::Texture& T : F.Textures)
	{
		EXPECT(T.Size > 100 && T.Width > 0 && T.Height > 0);
		// PNG / JPEG signatures
		EXPECT(T.Encoding == 0 ? (T.Data[0] == 0x89 && T.Data[1] == 'P') : (T.Data[0] == 0xFF && T.Data[1] == 0xD8));
	}
	std::printf("  %-20s %3zu meshes %7zu tris %2zu textures %2zu joints %zu clips\n", FileName, F.Meshes.size(), Tris, F.Textures.size(), F.Joints.size(), F.Clips.size());

	if (!F.Clips.empty())
	{
		// the pipeline stored skinned reference positions: sampling + skinning must reproduce them
		EXPECT(!F.Checks.empty());
		SUModel::Pose Pose;
		std::vector<float> World, Skin;
		for (const SUModel::Check& C : F.Checks)
		{
			SUModel::SampleClip(F, static_cast<int>(C.ClipIndex), C.Time, Pose);
			SUModel::ComputeMatrices(F, Pose, World, Skin);
			float P[3];
			SUModel::SkinPosition(F.Meshes[0].Sections[0], C.Vertex, Skin, P);
			EXPECT_NEAR(P[0], C.Position[0], 0.05f);
			EXPECT_NEAR(P[1], C.Position[1], 0.05f);
			EXPECT_NEAR(P[2], C.Position[2], 0.05f);
		}
		// rest pose leaves the mesh untouched
		SUModel::RestPose(F, Pose);
		SUModel::ComputeMatrices(F, Pose, World, Skin);
		const SUModel::Section& S = F.Meshes[0].Sections[0];
		for (size_t V = 0; V < S.NumVertices; V += 211)
		{
			float P[3];
			SUModel::SkinPosition(S, V, Skin, P);
			EXPECT_NEAR(P[0], S.Positions[V * 3], 0.05f);
			EXPECT_NEAR(P[2], S.Positions[V * 3 + 2], 0.05f);
		}
		// every clip keeps the creature near its origin and above the ground, at every time
		std::vector<float> Pos, Nrm, Tan;
		for (size_t CI = 0; CI < F.Clips.size(); ++CI)
		{
			const SUModel::Clip& C = F.Clips[CI];
			EXPECT(C.Duration > 0.2f && C.NumFrames >= 2);
			for (int Step = 0; Step <= 4; ++Step)
			{
				SUModel::SampleClip(F, static_cast<int>(CI), C.Duration * static_cast<float>(Step) / 4.0f, Pose);
				SUModel::ComputeMatrices(F, Pose, World, Skin);
				SUModel::SkinSection(S, Skin, Pos, Nrm, Tan);
				float MinZ = 1e9f, MaxZ = -1e9f, MaxR = 0.0f;
				for (size_t V = 0; V < S.NumVertices; ++V)
				{
					MinZ = std::min(MinZ, Pos[V * 3 + 2]);
					MaxZ = std::max(MaxZ, Pos[V * 3 + 2]);
					MaxR = std::max(MaxR, std::fabs(Pos[V * 3]) + std::fabs(Pos[V * 3 + 1]));
					EXPECT(std::isfinite(Pos[V * 3]));
				}
				EXPECT(MinZ > -45.0f);
				EXPECT(MaxZ > 100.0f && MaxZ < 460.0f);
				EXPECT(MaxR < 520.0f);
			}
		}
		// the front marker is one metre ahead of the root: models face +X
		const int Front = F.FindJoint("SU_Front");
		EXPECT(Front >= 0);
		if (Front >= 0)
		{
			EXPECT_NEAR(F.Joints[static_cast<size_t>(Front)].RestT[0], 100.0f, 0.5f);
		}
		EXPECT(F.FindClip("Idle") >= 0 && F.FindClip("Walk") >= 0 && F.FindClip("Run") >= 0 && F.FindClip("Attack") >= 0 && F.FindClip("Scream") >= 0);
		EXPECT(F.GetMeta("RunSpeed", 0.0f) > 300.0f);
		// blending two poses stays normalised
		SUModel::Pose A, B;
		SUModel::SampleClip(F, 0, 0.3f, A);
		SUModel::SampleClip(F, 1, 0.2f, B);
		SUModel::BlendPose(A, B, 0.5f);
		for (size_t J = 0; J < F.Joints.size(); ++J)
		{
			const float* Q = &A.Q[J * 4];
			EXPECT_NEAR(Q[0] * Q[0] + Q[1] * Q[1] + Q[2] * Q[2] + Q[3] * Q[3], 1.0f, 1e-3f);
		}
	}
}

static void TestModels()
{
	std::printf("[models]\n");
	TestModelFile("Vegetation.sumodel", 0, false);
	TestModelFile("Wendigo.sumodel", 5, false);
	TestModelFile("Cyclops.sumodel", 5, false);
	TestModelFile("Manor.sumodel", 0, false);
	TestModelFile("Restricted/Tomb.sumodel", 0, true);

	// damaged files are rejected instead of crashing
	std::vector<uint8_t> Bytes;
	if (LoadFile(GModelDir + "Wendigo.sumodel", Bytes))
	{
		SUModel::File F;
		std::string Error;
		EXPECT(!SUModel::Parse(Bytes.data(), Bytes.size() / 2, F, Error));
		SUModel::File F2;
		Bytes[0] = 'X';
		EXPECT(!SUModel::Parse(Bytes.data(), Bytes.size(), F2, Error));
	}
	// every mesh the manor site asks for exists in the file
	if (LoadFile(GModelDir + "Manor.sumodel", Bytes))
	{
		SUModel::File F;
		std::string Error;
		EXPECT(SUModel::Parse(Bytes.data(), Bytes.size(), F, Error));
		for (const char* Name : SUManor::ManorMeshes) { EXPECT(F.FindMesh(Name) >= 0); }
		for (const char* Name : SUManor::UndergroundMeshes) { EXPECT(F.FindMesh(Name) >= 0); }
		EXPECT(F.FindMesh("SM_Manor_DoorL") >= 0 && F.FindMesh("SM_Manor_DoorR") >= 0);
	}
}

static void TestManorSite()
{
	std::printf("[manor site]\n");
	const WorldLayout L(7071u);
	const SitePad* Pad = L.FindPad("Manor");
	EXPECT(Pad != nullptr);
	if (!Pad)
	{
		return;
	}
	// the site must stay axis aligned (terrain hole) and inside the playable area
	EXPECT(std::fmod(Pad->FacingYawDeg, 90.0) == 0.0);
	EXPECT(L.IsInsideCore(Pad->Center.X, Pad->Center.Y, 150.0));
	EXPECT((Pad->Center - L.TheLake.Center).Length() > L.TheLake.Radius + 200.0);
	EXPECT(Pad->Z > L.TheLake.WaterZ + 2.0);
	EXPECT(Pad->Radius * 100.0 >= SUManor::PadRadius);
	// the plateau is exactly level, so nothing below it can poke through the ground
	for (double A = 0.0; A < 6.28; A += 0.2)
	{
		for (double R = 0.0; R <= Pad->Radius - 1.0; R += 8.0)
		{
			const double PX = Pad->Center.X + std::cos(A) * R, PY = Pad->Center.Y + std::sin(A) * R;
			if (L.NearestRoad(PX, PY, 14.0).RoadIndex >= 0)
			{
				continue;   // the drive in front of the house shapes its own bed
			}
			EXPECT_NEAR(L.Height(PX, PY), Pad->Z, 0.02);
		}
	}
	// the slope around the plateau stays walkable
	for (double A = 0.0; A < 6.28; A += 0.3)
	{
		const double R0 = Pad->Radius, R1 = Pad->Radius + Pad->Blend;
		const double H0 = L.Height(Pad->Center.X + std::cos(A) * R0, Pad->Center.Y + std::sin(A) * R0);
		const double H1 = L.Height(Pad->Center.X + std::cos(A) * R1, Pad->Center.Y + std::sin(A) * R1);
		EXPECT(std::fabs(H1 - H0) / Pad->Blend < 0.75);
	}
	// everything underground lies inside the plateau and below the surface
	auto InsidePad = [](const SUManor::Vec& P) {
		const float DX = P.X - SUManor::PadCentre.X, DY = P.Y - SUManor::PadCentre.Y;
		return std::sqrt(DX * DX + DY * DY) < SUManor::PadRadius - 300.0f;
	};
	for (const SUManor::Vec& N : SUManor::NavNodes)
	{
		EXPECT(InsidePad(N));
		EXPECT(N.Z < -250.0f);
		// ... and never under the drive, whose bed is cut into the plateau
		const double WX = Pad->Center.X - (N.Y - SUManor::PadCentre.Y) / 100.0, WY = Pad->Center.Y + (N.X - SUManor::PadCentre.X) / 100.0;
		EXPECT(L.NearestRoad(WX, WY, 14.0).RoadIndex < 0);
	}
	for (const SUManor::Zone& Z : SUManor::Zones)
	{
		const float CX = (Z.Lo.X + Z.Hi.X) * 0.5f - SUManor::PadCentre.X, CY = (Z.Lo.Y + Z.Hi.Y) * 0.5f - SUManor::PadCentre.Y;
		const float HX = (Z.Hi.X - Z.Lo.X) * 0.5f, HY = (Z.Hi.Y - Z.Lo.Y) * 0.5f;
		EXPECT(std::sqrt((std::fabs(CX) + HX) * (std::fabs(CX) + HX) + (std::fabs(CY) + HY) * (std::fabs(CY) + HY)) < SUManor::PadRadius + 100.0f);
		EXPECT(Z.Lo.X < Z.Hi.X && Z.Lo.Y < Z.Hi.Y && Z.Lo.Z < Z.Hi.Z);
	}
	for (const SUManor::Hide& H : SUManor::Hides) { EXPECT(InsidePad(H.Inside)); }
	EXPECT(SUManor::ShaftMaxX - SUManor::ShaftMinX > 100.0f && SUManor::ShaftMaxY - SUManor::ShaftMinY > 500.0f);

	// waypoint graph: connected, no long jumps, and the creature finds its way from the lair to the cellar breach
	SUModel::NavGraph G;
	const int Count = static_cast<int>(std::size(SUManor::NavNodes));
	G.SetNodeCount(Count);
	for (int I = 0; I < Count; ++I)
	{
		G.Nodes[static_cast<size_t>(I)] = {SUManor::NavNodes[I].X, SUManor::NavNodes[I].Y, SUManor::NavNodes[I].Z};
	}
	for (const SUManor::Edge& E : SUManor::NavEdges)
	{
		EXPECT(E.A >= 0 && E.B >= 0 && E.A < Count && E.B < Count);
		G.AddEdge(E.A, E.B);
		EXPECT(G.Distance(E.A, E.B) < 1500.0f);
		// no edge is steeper than a creature can walk
		const float DZ = std::fabs(G.Nodes[static_cast<size_t>(E.A)].Z - G.Nodes[static_cast<size_t>(E.B)].Z);
		EXPECT(DZ < 0.75f * G.Distance(E.A, E.B) + 1.0f || E.A >= SUManor::NavFirstTombNode || E.B >= SUManor::NavFirstTombNode);
	}
	const int Lair = G.Nearest(SUManor::CyclopsSpawn.X, SUManor::CyclopsSpawn.Y, SUManor::CyclopsSpawn.Z);
	std::vector<int> Path;
	for (int I = 0; I < Count; ++I)
	{
		EXPECT(G.FindPath(Lair, I, Path));
		EXPECT(!Path.empty() && Path.front() == Lair && Path.back() == I);
	}
	EXPECT(G.FindPath(Lair, 0, Path) && Path.size() > 12);
	EXPECT(!G.FindPath(Lair, Count + 5, Path));
	// anchors used by the mission exist
	auto HasAnchor = [](const char* Name) {
		for (const SUManor::Anchor& A : SUManor::Anchors) { if (std::string(A.Name) == Name) { return true; } }
		return false;
	};
	for (const char* Name : {"FrontDoor", "StudyDesk", "CellarStairsTop", "FuseBox", "Breach", "StoreTable", "CavernCentre", "Lair", "DigChamber", "TombDoor", "Sarcophagus"})
	{
		EXPECT(HasAnchor(Name));
	}
}

int main(int Argc, char** Argv)
{
	GModelDir = Argc > 1 ? std::string(Argv[1]) + "/" : std::string("Content/SUModels/");
	TestWorldLayout();
	TestEvidence();
	TestEconomy();
	TestVehicle();
	TestDirector();
	TestObjectives();
	TestDsp();
	TestModels();
	TestManorSite();
	std::printf("\n%d checks, %d failures\n", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
