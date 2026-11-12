// SIGNAL UNKNOWN – model files (.sumodel), skeleton animation and CPU skinning.
//
// Engine independent (std only) so it is covered by the native tests, which load the real model
// files of the project. The files are written by Tools/AssetPipeline/pack.py; all data is already
// in Unreal space (cm, +Z up, +X forward).
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace SUModel
{
	struct Texture
	{
		std::string Name;
		uint8_t Kind = 0;       // 0 colour (sRGB), 1 normal map
		uint8_t Encoding = 0;   // 0 PNG, 1 JPEG
		uint32_t Width = 0;
		uint32_t Height = 0;
		const uint8_t* Data = nullptr;   // points into the file buffer
		uint32_t Size = 0;
	};

	struct Material
	{
		std::string Name;
		int32_t BaseTexture = -1;
		int32_t NormalTexture = -1;
		float Color[4] = {1.f, 1.f, 1.f, 1.f};
		float Roughness = 0.9f;
		float Metallic = 0.f;
		bool bMasked = false;
		float Cutoff = 0.5f;
		bool bTwoSided = false;
	};

	struct Section
	{
		int32_t MaterialIndex = -1;
		uint32_t NumVertices = 0;
		std::vector<float> Positions;   // 3 per vertex
		std::vector<float> Normals;     // 3
		std::vector<float> Tangents;    // 4 (xyz + sign of the bitangent)
		std::vector<float> UVs;         // 2
		std::vector<uint32_t> Indices;
		std::vector<uint8_t> Joints;    // 4 (skinned meshes)
		std::vector<float> Weights;     // 4
	};

	struct Mesh
	{
		std::string Name;
		bool bSkinned = false;
		float Min[3] = {0.f, 0.f, 0.f};
		float Max[3] = {0.f, 0.f, 0.f};
		std::vector<Section> Sections;
	};

	struct Joint
	{
		std::string Name;
		int32_t Parent = -1;
		float RestT[3] = {0.f, 0.f, 0.f};
		float RestQ[4] = {0.f, 0.f, 0.f, 1.f};
		float InvBind[12] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};   // 3x4, row major
	};

	struct Clip
	{
		std::string Name;
		float Duration = 0.f;
		bool bLoop = true;
		uint32_t NumFrames = 0;
		std::vector<float> Rotations;      // joint major: [joint][frame][4]
		std::vector<float> Translations;   // [joint][frame][3]
	};

	struct Check
	{
		uint32_t ClipIndex = 0;
		float Time = 0.f;
		uint32_t Vertex = 0;
		float Position[3] = {0.f, 0.f, 0.f};
	};

	struct Meta
	{
		std::string Key;
		float Value = 0.f;
	};

	struct File
	{
		std::vector<Texture> Textures;
		std::vector<Material> Materials;
		std::vector<Mesh> Meshes;
		std::vector<Joint> Joints;
		std::vector<Clip> Clips;
		std::vector<Check> Checks;
		std::vector<Meta> MetaValues;

		int FindMesh(const char* Name) const
		{
			for (size_t I = 0; I < Meshes.size(); ++I)
			{
				if (Meshes[I].Name == Name) { return static_cast<int>(I); }
			}
			return -1;
		}

		int FindClip(const char* Name) const
		{
			for (size_t I = 0; I < Clips.size(); ++I)
			{
				if (Clips[I].Name == Name) { return static_cast<int>(I); }
			}
			return -1;
		}

		int FindJoint(const char* Name) const
		{
			for (size_t I = 0; I < Joints.size(); ++I)
			{
				if (Joints[I].Name == Name) { return static_cast<int>(I); }
			}
			return -1;
		}

		float GetMeta(const char* Key, float Default) const
		{
			for (const Meta& M : MetaValues)
			{
				if (M.Key == Key) { return M.Value; }
			}
			return Default;
		}
	};

	// ------------------------------------------------------------------------------------ reading
	class Reader
	{
	public:
		Reader(const uint8_t* InData, size_t InSize) : Data(InData), Size(InSize) {}

		bool Ok() const { return !bFailed; }
		size_t Tell() const { return Pos; }

		template <typename T>
		T Read()
		{
			T Value{};
			if (Pos + sizeof(T) > Size)
			{
				bFailed = true;
				return Value;
			}
			std::memcpy(&Value, Data + Pos, sizeof(T));
			Pos += sizeof(T);
			return Value;
		}

		std::string ReadString()
		{
			const uint16_t Len = Read<uint16_t>();
			if (bFailed || Pos + Len > Size)
			{
				bFailed = true;
				return std::string();
			}
			std::string Out(reinterpret_cast<const char*>(Data + Pos), Len);
			Pos += Len;
			return Out;
		}

		template <typename T>
		void ReadArray(std::vector<T>& Out, size_t Count)
		{
			const size_t Bytes = Count * sizeof(T);
			if (bFailed || Count > Size || Pos + Bytes > Size)
			{
				bFailed = true;
				return;
			}
			Out.resize(Count);
			if (Bytes > 0)
			{
				std::memcpy(Out.data(), Data + Pos, Bytes);
			}
			Pos += Bytes;
		}

		const uint8_t* Skip(size_t Bytes)
		{
			if (bFailed || Pos + Bytes > Size)
			{
				bFailed = true;
				return nullptr;
			}
			const uint8_t* P = Data + Pos;
			Pos += Bytes;
			return P;
		}

	private:
		const uint8_t* Data;
		size_t Size;
		size_t Pos = 0;
		bool bFailed = false;
	};

	/** Parses a .sumodel buffer. Texture data keeps pointing into the buffer, so it must stay alive. */
	inline bool Parse(const uint8_t* Data, size_t Size, File& Out, std::string& OutError)
	{
		Reader R(Data, Size);
		const uint32_t Magic = R.Read<uint32_t>();
		const uint32_t Version = R.Read<uint32_t>();
		if (!R.Ok() || Magic != 0x444D5553u)   // 'SUMD'
		{
			OutError = "not a .sumodel file";
			return false;
		}
		if (Version != 1)
		{
			OutError = "unsupported .sumodel version";
			return false;
		}
		const uint32_t NumTextures = R.Read<uint32_t>();
		for (uint32_t I = 0; I < NumTextures && R.Ok(); ++I)
		{
			Texture T;
			T.Name = R.ReadString();
			T.Kind = R.Read<uint8_t>();
			T.Encoding = R.Read<uint8_t>();
			T.Width = R.Read<uint32_t>();
			T.Height = R.Read<uint32_t>();
			T.Size = R.Read<uint32_t>();
			T.Data = R.Skip(T.Size);
			Out.Textures.push_back(T);
		}
		const uint32_t NumMaterials = R.Read<uint32_t>();
		for (uint32_t I = 0; I < NumMaterials && R.Ok(); ++I)
		{
			Material M;
			M.Name = R.ReadString();
			M.BaseTexture = R.Read<int32_t>();
			M.NormalTexture = R.Read<int32_t>();
			for (float& C : M.Color) { C = R.Read<float>(); }
			M.Roughness = R.Read<float>();
			M.Metallic = R.Read<float>();
			M.bMasked = R.Read<uint8_t>() != 0;
			M.Cutoff = R.Read<float>();
			M.bTwoSided = R.Read<uint8_t>() != 0;
			Out.Materials.push_back(M);
		}
		const uint32_t NumMeshes = R.Read<uint32_t>();
		for (uint32_t I = 0; I < NumMeshes && R.Ok(); ++I)
		{
			Mesh M;
			M.Name = R.ReadString();
			M.bSkinned = R.Read<uint8_t>() != 0;
			for (float& V : M.Min) { V = R.Read<float>(); }
			for (float& V : M.Max) { V = R.Read<float>(); }
			const uint32_t NumSections = R.Read<uint32_t>();
			for (uint32_t S = 0; S < NumSections && R.Ok(); ++S)
			{
				Section Sec;
				Sec.MaterialIndex = R.Read<int32_t>();
				Sec.NumVertices = R.Read<uint32_t>();
				const uint32_t NumIndices = R.Read<uint32_t>();
				const size_t N = Sec.NumVertices;
				R.ReadArray(Sec.Positions, N * 3);
				R.ReadArray(Sec.Normals, N * 3);
				R.ReadArray(Sec.Tangents, N * 4);
				R.ReadArray(Sec.UVs, N * 2);
				R.ReadArray(Sec.Indices, NumIndices);
				if (M.bSkinned)
				{
					R.ReadArray(Sec.Joints, N * 4);
					R.ReadArray(Sec.Weights, N * 4);
				}
				M.Sections.push_back(std::move(Sec));
			}
			Out.Meshes.push_back(std::move(M));
		}
		const uint32_t NumJoints = R.Read<uint32_t>();
		for (uint32_t I = 0; I < NumJoints && R.Ok(); ++I)
		{
			Joint J;
			J.Name = R.ReadString();
			J.Parent = R.Read<int32_t>();
			for (float& V : J.RestT) { V = R.Read<float>(); }
			for (float& V : J.RestQ) { V = R.Read<float>(); }
			for (float& V : J.InvBind) { V = R.Read<float>(); }
			Out.Joints.push_back(J);
		}
		const uint32_t NumClips = R.Read<uint32_t>();
		for (uint32_t I = 0; I < NumClips && R.Ok(); ++I)
		{
			Clip C;
			C.Name = R.ReadString();
			C.Duration = R.Read<float>();
			C.bLoop = R.Read<uint8_t>() != 0;
			C.NumFrames = R.Read<uint32_t>();
			const size_t Frames = C.NumFrames;
			C.Rotations.resize(0);
			C.Translations.resize(0);
			if (R.Ok() && Frames < Size)
			{
				C.Rotations.reserve(Out.Joints.size() * Frames * 4);
				C.Translations.reserve(Out.Joints.size() * Frames * 3);
				std::vector<float> Tmp;
				for (size_t J = 0; J < Out.Joints.size() && R.Ok(); ++J)
				{
					R.ReadArray(Tmp, Frames * 4);
					C.Rotations.insert(C.Rotations.end(), Tmp.begin(), Tmp.end());
					R.ReadArray(Tmp, Frames * 3);
					C.Translations.insert(C.Translations.end(), Tmp.begin(), Tmp.end());
				}
			}
			Out.Clips.push_back(std::move(C));
		}
		const uint32_t NumChecks = R.Read<uint32_t>();
		for (uint32_t I = 0; I < NumChecks && R.Ok(); ++I)
		{
			Check C;
			C.ClipIndex = R.Read<uint32_t>();
			C.Time = R.Read<float>();
			C.Vertex = R.Read<uint32_t>();
			for (float& V : C.Position) { V = R.Read<float>(); }
			Out.Checks.push_back(C);
		}
		const uint32_t NumMeta = R.Read<uint32_t>();
		for (uint32_t I = 0; I < NumMeta && R.Ok(); ++I)
		{
			Meta M;
			M.Key = R.ReadString();
			M.Value = R.Read<float>();
			Out.MetaValues.push_back(M);
		}
		if (!R.Ok())
		{
			OutError = "file is truncated or damaged";
			return false;
		}
		// sanity: indices and joints must be in range
		for (const Mesh& M : Out.Meshes)
		{
			for (const Section& Sec : M.Sections)
			{
				if (Sec.MaterialIndex >= static_cast<int32_t>(Out.Materials.size()))
				{
					OutError = "material index out of range";
					return false;
				}
				for (uint32_t Index : Sec.Indices)
				{
					if (Index >= Sec.NumVertices)
					{
						OutError = "vertex index out of range";
						return false;
					}
				}
				for (uint8_t JointIndex : Sec.Joints)
				{
					if (JointIndex >= Out.Joints.size())
					{
						OutError = "joint index out of range";
						return false;
					}
				}
			}
		}
		for (size_t I = 0; I < Out.Joints.size(); ++I)
		{
			if (Out.Joints[I].Parent >= static_cast<int32_t>(I))
			{
				OutError = "joints are not ordered parent first";
				return false;
			}
		}
		return true;
	}

	// ------------------------------------------------------------------------------------ animation
	/** Local pose: rotation (x, y, z, w) and translation per joint. */
	struct Pose
	{
		std::vector<float> Q;   // 4 per joint
		std::vector<float> T;   // 3 per joint
	};

	inline void RestPose(const File& F, Pose& Out)
	{
		const size_t N = F.Joints.size();
		Out.Q.resize(N * 4);
		Out.T.resize(N * 3);
		for (size_t J = 0; J < N; ++J)
		{
			std::memcpy(&Out.Q[J * 4], F.Joints[J].RestQ, sizeof(float) * 4);
			std::memcpy(&Out.T[J * 3], F.Joints[J].RestT, sizeof(float) * 3);
		}
	}

	inline void Nlerp(const float* A, const float* B, float Alpha, float* Out)
	{
		const float Dot = A[0] * B[0] + A[1] * B[1] + A[2] * B[2] + A[3] * B[3];
		const float Sign = Dot < 0.f ? -1.f : 1.f;
		float R[4];
		float LenSq = 0.f;
		for (int K = 0; K < 4; ++K)
		{
			R[K] = A[K] + (B[K] * Sign - A[K]) * Alpha;
			LenSq += R[K] * R[K];
		}
		const float Inv = LenSq > 1e-12f ? 1.f / std::sqrt(LenSq) : 1.f;
		for (int K = 0; K < 4; ++K)
		{
			Out[K] = R[K] * Inv;
		}
	}

	/** Samples a clip at Time (seconds). Looping clips wrap, others hold the last frame. */
	inline void SampleClip(const File& F, int ClipIndex, float Time, Pose& Out)
	{
		const size_t N = F.Joints.size();
		if (ClipIndex < 0 || ClipIndex >= static_cast<int>(F.Clips.size()) || F.Clips[static_cast<size_t>(ClipIndex)].NumFrames < 2)
		{
			RestPose(F, Out);
			return;
		}
		const Clip& C = F.Clips[static_cast<size_t>(ClipIndex)];
		Out.Q.resize(N * 4);
		Out.T.resize(N * 3);
		const size_t Frames = C.NumFrames;
		float T = Time;
		if (C.bLoop && C.Duration > 0.f)
		{
			T = std::fmod(T, C.Duration);
			if (T < 0.f) { T += C.Duration; }
		}
		T = T < 0.f ? 0.f : (T > C.Duration ? C.Duration : T);
		const float FramePos = C.Duration > 0.f ? T / C.Duration * static_cast<float>(Frames - 1) : 0.f;
		size_t F0 = static_cast<size_t>(FramePos);
		if (F0 >= Frames - 1) { F0 = Frames - 2; }
		const float Alpha = FramePos - static_cast<float>(F0);
		for (size_t J = 0; J < N; ++J)
		{
			const float* Q0 = &C.Rotations[(J * Frames + F0) * 4];
			Nlerp(Q0, Q0 + 4, Alpha, &Out.Q[J * 4]);
			const float* T0 = &C.Translations[(J * Frames + F0) * 3];
			for (int K = 0; K < 3; ++K)
			{
				Out.T[J * 3 + static_cast<size_t>(K)] = T0[K] + (T0[K + 3] - T0[K]) * Alpha;
			}
		}
	}

	/** InOut = lerp(InOut, Other, Alpha) per joint. */
	inline void BlendPose(Pose& InOut, const Pose& Other, float Alpha)
	{
		const size_t N = InOut.Q.size() / 4;
		if (Other.Q.size() != InOut.Q.size())
		{
			return;
		}
		for (size_t J = 0; J < N; ++J)
		{
			float Q[4];
			Nlerp(&InOut.Q[J * 4], &Other.Q[J * 4], Alpha, Q);
			std::memcpy(&InOut.Q[J * 4], Q, sizeof(Q));
			for (size_t K = 0; K < 3; ++K)
			{
				InOut.T[J * 3 + K] += (Other.T[J * 3 + K] - InOut.T[J * 3 + K]) * Alpha;
			}
		}
	}

	/** 3x4 affine matrices, row major: p' = M[0..2] * p + M[3] per row. */
	inline void MulAffine(const float* A, const float* B, float* Out)
	{
		for (int R = 0; R < 3; ++R)
		{
			for (int C = 0; C < 4; ++C)
			{
				Out[R * 4 + C] = A[R * 4 + 0] * B[0 * 4 + C] + A[R * 4 + 1] * B[1 * 4 + C] + A[R * 4 + 2] * B[2 * 4 + C] + (C == 3 ? A[R * 4 + 3] : 0.f);
			}
		}
	}

	inline void ComposeAffine(const float* Q, const float* T, float* Out)
	{
		const float X = Q[0], Y = Q[1], Z = Q[2], W = Q[3];
		Out[0] = 1.f - 2.f * (Y * Y + Z * Z); Out[1] = 2.f * (X * Y - Z * W);       Out[2] = 2.f * (X * Z + Y * W);       Out[3] = T[0];
		Out[4] = 2.f * (X * Y + Z * W);       Out[5] = 1.f - 2.f * (X * X + Z * Z); Out[6] = 2.f * (Y * Z - X * W);       Out[7] = T[1];
		Out[8] = 2.f * (X * Z - Y * W);       Out[9] = 2.f * (Y * Z + X * W);       Out[10] = 1.f - 2.f * (X * X + Y * Y); Out[11] = T[2];
	}

	/** World matrices (12 floats per joint) and skin matrices (world * inverse bind) for a pose. */
	inline void ComputeMatrices(const File& F, const Pose& P, std::vector<float>& OutWorld, std::vector<float>& OutSkin)
	{
		const size_t N = F.Joints.size();
		OutWorld.resize(N * 12);
		OutSkin.resize(N * 12);
		float Local[12];
		for (size_t J = 0; J < N; ++J)
		{
			ComposeAffine(&P.Q[J * 4], &P.T[J * 3], Local);
			const int32_t Parent = F.Joints[J].Parent;
			if (Parent >= 0)
			{
				MulAffine(&OutWorld[static_cast<size_t>(Parent) * 12], Local, &OutWorld[J * 12]);
			}
			else
			{
				std::memcpy(&OutWorld[J * 12], Local, sizeof(Local));
			}
			MulAffine(&OutWorld[J * 12], F.Joints[J].InvBind, &OutSkin[J * 12]);
		}
	}

	/** Skins one vertex position. */
	inline void SkinPosition(const Section& Sec, size_t Vertex, const std::vector<float>& Skin, float* Out)
	{
		const float* P = &Sec.Positions[Vertex * 3];
		Out[0] = Out[1] = Out[2] = 0.f;
		for (size_t K = 0; K < 4; ++K)
		{
			const float W = Sec.Weights[Vertex * 4 + K];
			if (W <= 0.f)
			{
				continue;
			}
			const float* M = &Skin[static_cast<size_t>(Sec.Joints[Vertex * 4 + K]) * 12];
			Out[0] += W * (M[0] * P[0] + M[1] * P[1] + M[2] * P[2] + M[3]);
			Out[1] += W * (M[4] * P[0] + M[5] * P[1] + M[6] * P[2] + M[7]);
			Out[2] += W * (M[8] * P[0] + M[9] * P[1] + M[10] * P[2] + M[11]);
		}
	}

	/** Skins positions, normals and tangents of a section (arrays of 3 floats per vertex). */
	inline void SkinSection(const Section& Sec, const std::vector<float>& Skin, std::vector<float>& OutPositions, std::vector<float>& OutNormals, std::vector<float>& OutTangents)
	{
		const size_t N = Sec.NumVertices;
		OutPositions.resize(N * 3);
		OutNormals.resize(N * 3);
		OutTangents.resize(N * 3);
		for (size_t V = 0; V < N; ++V)
		{
			float M[12] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
			for (size_t K = 0; K < 4; ++K)
			{
				const float W = Sec.Weights[V * 4 + K];
				if (W <= 0.f)
				{
					continue;
				}
				const float* J = &Skin[static_cast<size_t>(Sec.Joints[V * 4 + K]) * 12];
				for (int E = 0; E < 12; ++E)
				{
					M[E] += W * J[E];
				}
			}
			const float* P = &Sec.Positions[V * 3];
			const float* Nr = &Sec.Normals[V * 3];
			const float* Tg = &Sec.Tangents[V * 4];
			for (size_t R = 0; R < 3; ++R)
			{
				OutPositions[V * 3 + R] = M[R * 4] * P[0] + M[R * 4 + 1] * P[1] + M[R * 4 + 2] * P[2] + M[R * 4 + 3];
				OutNormals[V * 3 + R] = M[R * 4] * Nr[0] + M[R * 4 + 1] * Nr[1] + M[R * 4 + 2] * Nr[2];
				OutTangents[V * 3 + R] = M[R * 4] * Tg[0] + M[R * 4 + 1] * Tg[1] + M[R * 4 + 2] * Tg[2];
			}
		}
	}

	// ------------------------------------------------------------------------------------ waypoint graph
	/** Small undirected graph for creatures that move through tunnels. */
	struct NavGraph
	{
		struct Node { float X, Y, Z; };
		std::vector<Node> Nodes;
		std::vector<std::vector<int>> Links;

		void SetNodeCount(int Count)
		{
			Nodes.resize(static_cast<size_t>(Count));
			Links.assign(static_cast<size_t>(Count), {});
		}

		void AddEdge(int A, int B)
		{
			if (A < 0 || B < 0 || A >= static_cast<int>(Nodes.size()) || B >= static_cast<int>(Nodes.size()) || A == B)
			{
				return;
			}
			Links[static_cast<size_t>(A)].push_back(B);
			Links[static_cast<size_t>(B)].push_back(A);
		}

		int Nearest(float X, float Y, float Z, float ZWeight = 2.f) const
		{
			int Best = -1;
			float BestD = 1e30f;
			for (size_t I = 0; I < Nodes.size(); ++I)
			{
				const float DX = Nodes[I].X - X, DY = Nodes[I].Y - Y, DZ = (Nodes[I].Z - Z) * ZWeight;
				const float D = DX * DX + DY * DY + DZ * DZ;
				if (D < BestD)
				{
					BestD = D;
					Best = static_cast<int>(I);
				}
			}
			return Best;
		}

		float Distance(int A, int B) const
		{
			const Node& P = Nodes[static_cast<size_t>(A)];
			const Node& Q = Nodes[static_cast<size_t>(B)];
			return std::sqrt((P.X - Q.X) * (P.X - Q.X) + (P.Y - Q.Y) * (P.Y - Q.Y) + (P.Z - Q.Z) * (P.Z - Q.Z));
		}

		/** Shortest path (Dijkstra). OutPath starts with From and ends with To; empty if unreachable. */
		bool FindPath(int From, int To, std::vector<int>& OutPath) const
		{
			OutPath.clear();
			const int N = static_cast<int>(Nodes.size());
			if (From < 0 || To < 0 || From >= N || To >= N)
			{
				return false;
			}
			std::vector<float> Dist(static_cast<size_t>(N), 1e30f);
			std::vector<int> Prev(static_cast<size_t>(N), -1);
			std::vector<bool> Done(static_cast<size_t>(N), false);
			Dist[static_cast<size_t>(From)] = 0.f;
			for (int Step = 0; Step < N; ++Step)
			{
				int U = -1;
				float Best = 1e30f;
				for (int I = 0; I < N; ++I)
				{
					if (!Done[static_cast<size_t>(I)] && Dist[static_cast<size_t>(I)] < Best)
					{
						Best = Dist[static_cast<size_t>(I)];
						U = I;
					}
				}
				if (U < 0 || U == To)
				{
					break;
				}
				Done[static_cast<size_t>(U)] = true;
				for (int V : Links[static_cast<size_t>(U)])
				{
					const float D = Best + Distance(U, V);
					if (D < Dist[static_cast<size_t>(V)])
					{
						Dist[static_cast<size_t>(V)] = D;
						Prev[static_cast<size_t>(V)] = U;
					}
				}
			}
			if (From != To && Prev[static_cast<size_t>(To)] < 0)
			{
				return false;
			}
			for (int At = To; At >= 0; At = Prev[static_cast<size_t>(At)])
			{
				OutPath.insert(OutPath.begin(), At);
				if (At == From)
				{
					break;
				}
			}
			return !OutPath.empty() && OutPath.front() == From;
		}
	};
}
