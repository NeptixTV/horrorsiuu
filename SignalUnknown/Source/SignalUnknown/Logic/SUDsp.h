// SIGNAL UNKNOWN – procedural audio generators (engine independent, real-time safe).
//
// The project ships without recorded audio. Every sound in the prototype – wind, birds, rain,
// radio static, the (garbled) radio voice, engine, horn, music on the car radio, creature
// sounds, footsteps, knocks – is synthesised here. USUProceduralAudioComponent runs one
// generator per component on the audio render thread.
//
// Threading: SetParam/Trigger are called from the game thread, Render from the audio thread.
// Parameters are atomics, triggers go through a single-producer/single-consumer ring buffer.
// Render never allocates.
#pragma once

#include "SUMathLite.h"

#include <array>
#include <atomic>
#include <memory>

namespace SULogic
{
namespace Dsp
{
	constexpr float TwoPi = 6.28318530718f;

	inline float SoftClip(float X)
	{
		// smooth saturation, |result| < 1
		if (X > 3.0f) { return 1.0f; }
		if (X < -3.0f) { return -1.0f; }
		const float X2 = X * X;
		return X * (27.0f + X2) / (27.0f + 9.0f * X2);
	}

	inline float Flush(float X)
	{
		return std::fabs(X) < 1e-20f ? 0.0f : X;
	}

	class Noise
	{
	public:
		explicit Noise(uint32_t Seed = 1u) : State(Seed ? Seed : 1u) {}
		void Reseed(uint32_t Seed) { State = Seed ? Seed : 1u; }

		float White()
		{
			State ^= State << 13;
			State ^= State >> 17;
			State ^= State << 5;
			return static_cast<float>(static_cast<int32_t>(State)) * (1.0f / 2147483648.0f);
		}
		float Uniform01() { return White() * 0.5f + 0.5f; }
		float Pink()
		{
			const float W = White();
			P0 = 0.99765f * P0 + W * 0.0990460f;
			P1 = 0.96300f * P1 + W * 0.2965164f;
			P2 = 0.57000f * P2 + W * 1.0526913f;
			return (P0 + P1 + P2 + W * 0.1848f) * 0.18f;
		}
		float Brown()
		{
			B = (B + 0.02f * White()) / 1.02f;
			return B * 3.5f;
		}

	private:
		uint32_t State;
		float P0 = 0.0f, P1 = 0.0f, P2 = 0.0f, B = 0.0f;
	};

	struct OnePole
	{
		float A = 1.0f;
		float Z = 0.0f;
		void SetCutoff(float Hz, float SampleRate)
		{
			const float X = std::exp(-TwoPi * Clampf(Hz, 1.0f, SampleRate * 0.49f) / SampleRate);
			A = 1.0f - X;
		}
		float Lowpass(float In) { Z = Flush(Z + A * (In - Z)); return Z; }
		float Highpass(float In) { return In - Lowpass(In); }
		static float Clampf(float V, float Lo, float Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	};

	/** RBJ biquad, transposed direct form II. */
	struct Biquad
	{
		float B0 = 1.0f, B1 = 0.0f, B2 = 0.0f, A1 = 0.0f, A2 = 0.0f;
		float Z1 = 0.0f, Z2 = 0.0f;

		void Set(int Type, float Hz, float Q, float SampleRate)
		{
			Hz = OnePole::Clampf(Hz, 10.0f, SampleRate * 0.45f);
			Q = OnePole::Clampf(Q, 0.1f, 30.0f);
			const float W0 = TwoPi * Hz / SampleRate;
			const float CosW = std::cos(W0);
			const float Alpha = std::sin(W0) / (2.0f * Q);
			float NB0, NB1, NB2;
			const float NA0 = 1.0f + Alpha;
			if (Type == 0) // lowpass
			{
				NB0 = (1.0f - CosW) * 0.5f; NB1 = 1.0f - CosW; NB2 = NB0;
			}
			else if (Type == 1) // highpass
			{
				NB0 = (1.0f + CosW) * 0.5f; NB1 = -(1.0f + CosW); NB2 = NB0;
			}
			else // bandpass, 0 dB peak
			{
				NB0 = Alpha; NB1 = 0.0f; NB2 = -Alpha;
			}
			B0 = NB0 / NA0; B1 = NB1 / NA0; B2 = NB2 / NA0;
			A1 = (-2.0f * CosW) / NA0; A2 = (1.0f - Alpha) / NA0;
		}
		void Lowpass(float Hz, float Q, float SR) { Set(0, Hz, Q, SR); }
		void Highpass(float Hz, float Q, float SR) { Set(1, Hz, Q, SR); }
		void Bandpass(float Hz, float Q, float SR) { Set(2, Hz, Q, SR); }

		float Process(float In)
		{
			const float Out = B0 * In + Z1;
			Z1 = Flush(B1 * In - A1 * Out + Z2);
			Z2 = Flush(B2 * In - A2 * Out);
			return Out;
		}
		void Reset() { Z1 = Z2 = 0.0f; }
	};

	/** Exponential parameter smoothing to avoid zipper noise. */
	struct Smoother
	{
		float Value = 0.0f;
		float Coeff = 0.001f;
		void Setup(float TimeSeconds, float SampleRate)
		{
			Coeff = 1.0f - std::exp(-1.0f / std::max(1.0f, TimeSeconds * SampleRate));
		}
		float Next(float Target) { Value += Coeff * (Target - Value); return Value; }
	};

	enum class SoundKind : int
	{
		Wind = 0,
		Rain,
		Birds,
		Crickets,
		RadioStatic,
		RadioVoice,
		Engine,
		Horn,
		Click,
		Music,
		Creature,
		Impact,
		Footstep,
		Stinger,
		Hum,
		Pump,
		Thunder,
		DoorCreak,
		Count
	};

	/** Named parameter slots. Each generator documents which ones it reads. */
	namespace Param
	{
		constexpr int Gain = 0;       // all: output gain (0..2)
		constexpr int A = 1;          // generator specific
		constexpr int B = 2;
		constexpr int C = 3;
		constexpr int D = 4;
		constexpr int Interior = 5;   // 0..1, muffles the sound (inside a car/building)
		constexpr int Num = 8;
	}

	class Generator
	{
	public:
		virtual ~Generator() = default;

		void Prepare(float InSampleRate, int InChannels, uint32_t Seed)
		{
			SampleRate = InSampleRate > 1000.0f ? InSampleRate : 48000.0f;
			Channels = InChannels == 2 ? 2 : 1;
			Rng.Reseed(Seed * 2654435761u + 17u);
			GainSmooth.Setup(0.03f, SampleRate);
			InteriorLp.SetCutoff(20000.0f, SampleRate);
			InteriorLpR.SetCutoff(20000.0f, SampleRate);
			OnPrepare();
		}

		void SetParam(int Index, float Value)
		{
			if (Index >= 0 && Index < Param::Num)
			{
				Params[static_cast<size_t>(Index)].store(Value, std::memory_order_relaxed);
			}
		}

		float GetParam(int Index) const
		{
			return (Index >= 0 && Index < Param::Num) ? Params[static_cast<size_t>(Index)].load(std::memory_order_relaxed) : 0.0f;
		}

		/** Game thread. Drops the trigger if the queue is full. */
		void Trigger(int Kind, float Strength)
		{
			const uint32_t H = Head.load(std::memory_order_relaxed);
			const uint32_t Next = (H + 1u) & (QueueSize - 1u);
			if (Next == Tail.load(std::memory_order_acquire))
			{
				return;
			}
			Queue[H].Kind = Kind;
			Queue[H].Strength = Strength;
			Head.store(Next, std::memory_order_release);
		}

		/** Audio thread. Interleaved output, Frames * Channels samples. */
		void Render(float* Out, int Frames)
		{
			for (;;)
			{
				const uint32_t T = Tail.load(std::memory_order_relaxed);
				if (T == Head.load(std::memory_order_acquire))
				{
					break;
				}
				const TriggerMsg Msg = Queue[T];
				Tail.store((T + 1u) & (QueueSize - 1u), std::memory_order_release);
				OnTrigger(Msg.Kind, Msg.Strength);
			}

			const float Interior = Clamp01f(GetParam(Param::Interior));
			const float Cut = 18000.0f * (1.0f - Interior) + 650.0f * Interior;
			InteriorLp.SetCutoff(Cut, SampleRate);
			InteriorLpR.SetCutoff(Cut, SampleRate);
			const float TargetGain = Clampf(GetParam(Param::Gain), 0.0f, 2.0f) * (1.0f - 0.35f * Interior);

			RenderBlock(Out, Frames);

			for (int I = 0; I < Frames; ++I)
			{
				const float G = GainSmooth.Next(TargetGain);
				for (int Ch = 0; Ch < Channels; ++Ch)
				{
					float& S = Out[I * Channels + Ch];
					float V = S;
					if (!std::isfinite(V))
					{
						V = 0.0f;
					}
					if (Interior > 0.01f)
					{
						V = Ch == 0 ? InteriorLp.Lowpass(V) : InteriorLpR.Lowpass(V);
					}
					S = SoftClip(V * G);
				}
			}
		}

		int GetChannels() const { return Channels; }

	protected:
		virtual void OnPrepare() {}
		virtual void OnTrigger(int /*Kind*/, float /*Strength*/) {}
		/** Fill Out (Frames * Channels, interleaved) with the raw signal. */
		virtual void RenderBlock(float* Out, int Frames) = 0;

		static float Clamp01f(float V) { return V < 0.0f ? 0.0f : (V > 1.0f ? 1.0f : V); }
		static float Clampf(float V, float Lo, float Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

		void WriteFrame(float* Out, int Frame, float L, float R) const
		{
			if (Channels == 2)
			{
				Out[Frame * 2] = L;
				Out[Frame * 2 + 1] = R;
			}
			else
			{
				Out[Frame] = (L + R) * 0.5f;
			}
		}

		float SampleRate = 48000.0f;
		int Channels = 1;
		Noise Rng;

	private:
		struct TriggerMsg
		{
			int Kind = 0;
			float Strength = 0.0f;
		};
		static constexpr uint32_t QueueSize = 32u;
		std::array<std::atomic<float>, Param::Num> Params{};
		std::array<TriggerMsg, QueueSize> Queue{};
		std::atomic<uint32_t> Head{0u};
		std::atomic<uint32_t> Tail{0u};
		Smoother GainSmooth;
		OnePole InteriorLp;
		OnePole InteriorLpR;
	};

	// ------------------------------------------------------------------ Wind
	// A = intensity (0..1), B = gustiness (0..1)
	class WindGen final : public Generator
	{
		Biquad BandL, BandR;
		OnePole HissL, HissR;
		Noise N2{991u};
		Smoother Intensity;
		float GustPhase = 0.0f, GustPhase2 = 0.0f, Wander = 0.0f;
		int Counter = 0;

		void OnPrepare() override
		{
			Intensity.Setup(1.5f, SampleRate);
			BandL.Bandpass(400.0f, 0.7f, SampleRate);
			BandR.Bandpass(430.0f, 0.7f, SampleRate);
			HissL.SetCutoff(3000.0f, SampleRate);
			HissR.SetCutoff(3200.0f, SampleRate);
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Target = Clamp01f(GetParam(Param::A));
			const float Gusty = Clamp01f(GetParam(Param::B));
			for (int I = 0; I < Frames; ++I)
			{
				if ((Counter++ & 63) == 0)
				{
					GustPhase += 64.0f / SampleRate * 0.11f;
					GustPhase2 += 64.0f / SampleRate * 0.037f;
					Wander = Clampf(Wander + Rng.White() * 0.02f, -1.0f, 1.0f);
					const float Gust = 0.55f + 0.25f * std::sin(GustPhase * TwoPi) * Gusty + 0.2f * std::sin(GustPhase2 * TwoPi) + 0.15f * Wander * Gusty;
					const float Center = 260.0f + 700.0f * Clamp01f(Gust);
					BandL.Bandpass(Center, 0.8f, SampleRate);
					BandR.Bandpass(Center * 1.07f, 0.8f, SampleRate);
				}
				const float Amp = Intensity.Next(Target);
				const float L = BandL.Process(Rng.Brown() * 2.5f) * Amp + HissL.Highpass(Rng.White()) * 0.04f * Amp * Amp;
				const float R = BandR.Process(N2.Brown() * 2.5f) * Amp + HissR.Highpass(N2.White()) * 0.04f * Amp * Amp;
				WriteFrame(Out, I, L, R);
			}
		}
	};

	// ------------------------------------------------------------------ Rain
	// A = intensity (0..1)
	class RainGen final : public Generator
	{
		Biquad HpL, HpR, LpL, LpR, Drop;
		Smoother Intensity;
		float DropEnv = 0.0f;
		float DropPan = 0.5f;

		void OnPrepare() override
		{
			Intensity.Setup(2.0f, SampleRate);
			HpL.Highpass(700.0f, 0.7f, SampleRate);
			HpR.Highpass(750.0f, 0.7f, SampleRate);
			LpL.Lowpass(7000.0f, 0.7f, SampleRate);
			LpR.Lowpass(6800.0f, 0.7f, SampleRate);
			Drop.Bandpass(3000.0f, 6.0f, SampleRate);
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Target = Clamp01f(GetParam(Param::A));
			for (int I = 0; I < Frames; ++I)
			{
				const float Amp = Intensity.Next(Target);
				if (Rng.Uniform01() < Amp * 0.0016f)
				{
					DropEnv = 0.6f + 0.4f * Rng.Uniform01();
					DropPan = Rng.Uniform01();
					Drop.Bandpass(1800.0f + 3500.0f * Rng.Uniform01(), 8.0f, SampleRate);
				}
				DropEnv *= 0.9965f;
				const float D = Drop.Process(Rng.White()) * DropEnv * 1.6f;
				const float L = LpL.Process(HpL.Process(Rng.White())) * 0.32f * Amp + D * (1.0f - DropPan) * Amp;
				const float R = LpR.Process(HpR.Process(Rng.White())) * 0.32f * Amp + D * DropPan * Amp;
				WriteFrame(Out, I, L, R);
			}
		}
	};

	// ------------------------------------------------------------------ Birds
	// A = density (0..1)
	class BirdGen final : public Generator
	{
		struct Chirp
		{
			bool bActive = false;
			float Phase = 0.0f, Time = 0.0f, Duration = 0.1f, F0 = 3000.0f, F1 = 4000.0f, Pan = 0.5f, Amp = 0.3f;
			int NotesLeft = 0;
			float Gap = 0.0f;
		};
		std::array<Chirp, 3> Chirps{};
		float Countdown = 1.0f;

		void Start(Chirp& C)
		{
			C.bActive = true;
			C.Time = 0.0f;
			C.Duration = 0.04f + 0.09f * Rng.Uniform01();
			C.F0 = 2400.0f + 2800.0f * Rng.Uniform01();
			C.F1 = C.F0 + (Rng.White() * 1600.0f);
			C.Gap = 0.03f + 0.08f * Rng.Uniform01();
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Density = Clamp01f(GetParam(Param::A));
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				Countdown -= Dt;
				if (Countdown <= 0.0f)
				{
					Countdown = (0.6f + 5.0f * Rng.Uniform01()) / std::max(0.05f, Density);
					if (Density > 0.02f)
					{
						for (Chirp& C : Chirps)
						{
							if (!C.bActive && C.NotesLeft == 0)
							{
								C.NotesLeft = 2 + static_cast<int>(Rng.Uniform01() * 5.0f);
								C.Pan = Rng.Uniform01();
								C.Amp = 0.12f + 0.2f * Rng.Uniform01();
								Start(C);
								break;
							}
						}
					}
				}
				float L = 0.0f, R = 0.0f;
				for (Chirp& C : Chirps)
				{
					if (C.bActive)
					{
						const float T = C.Time / C.Duration;
						const float F = C.F0 + (C.F1 - C.F0) * T;
						C.Phase += F * Dt;
						C.Phase -= std::floor(C.Phase);
						const float Env = std::sin(T * 3.14159f);
						const float S = std::sin(C.Phase * TwoPi) * Env * Env * C.Amp;
						L += S * (1.0f - C.Pan);
						R += S * C.Pan;
						C.Time += Dt;
						if (C.Time >= C.Duration)
						{
							C.bActive = false;
							--C.NotesLeft;
						}
					}
					else if (C.NotesLeft > 0)
					{
						C.Gap -= Dt;
						if (C.Gap <= 0.0f)
						{
							Start(C);
						}
					}
				}
				WriteFrame(Out, I, L, R);
			}
		}
	};

	// ------------------------------------------------------------------ Crickets
	// A = density (0..1)
	class CricketGen final : public Generator
	{
		struct Cricket
		{
			float Freq = 4500.0f, Phase = 0.0f, Pan = 0.5f, ChirpTimer = 0.0f, ChirpPeriod = 0.7f, PulseRate = 32.0f, Amp = 0.08f;
		};
		std::array<Cricket, 4> Crickets{};

		void OnPrepare() override
		{
			for (Cricket& C : Crickets)
			{
				C.Freq = 4100.0f + 900.0f * Rng.Uniform01();
				C.Pan = Rng.Uniform01();
				C.ChirpPeriod = 0.5f + 0.6f * Rng.Uniform01();
				C.ChirpTimer = Rng.Uniform01() * C.ChirpPeriod;
				C.PulseRate = 26.0f + 10.0f * Rng.Uniform01();
				C.Amp = 0.04f + 0.05f * Rng.Uniform01();
			}
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Density = Clamp01f(GetParam(Param::A));
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				float L = 0.0f, R = 0.0f;
				for (size_t K = 0; K < Crickets.size(); ++K)
				{
					Cricket& C = Crickets[K];
					if (static_cast<float>(K) / static_cast<float>(Crickets.size()) >= Density)
					{
						continue;
					}
					C.ChirpTimer += Dt;
					if (C.ChirpTimer > C.ChirpPeriod)
					{
						C.ChirpTimer -= C.ChirpPeriod;
					}
					const float Pulses = C.ChirpTimer * C.PulseRate;
					float Gate = 0.0f;
					if (Pulses < 3.0f)
					{
						const float Frac = Pulses - std::floor(Pulses);
						Gate = Frac < 0.5f ? std::sin(Frac * 2.0f * 3.14159f) : 0.0f;
					}
					C.Phase += C.Freq * Dt;
					C.Phase -= std::floor(C.Phase);
					const float S = std::sin(C.Phase * TwoPi) * Gate * C.Amp;
					L += S * (1.0f - C.Pan);
					R += S * C.Pan;
				}
				WriteFrame(Out, I, L, R);
			}
		}
	};

	// ------------------------------------------------------------------ Radio static
	// A = level (0..1), B = crackle amount (0..1)
	class RadioStaticGen final : public Generator
	{
		Biquad Band, Crack;
		float CrackEnv = 0.0f;
		Smoother Level;

		void OnPrepare() override
		{
			Band.Bandpass(1600.0f, 0.45f, SampleRate);
			Crack.Bandpass(2500.0f, 2.0f, SampleRate);
			Level.Setup(0.05f, SampleRate);
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Target = Clamp01f(GetParam(Param::A));
			const float Crackle = Clamp01f(GetParam(Param::B));
			for (int I = 0; I < Frames; ++I)
			{
				const float Lv = Level.Next(Target);
				if (Rng.Uniform01() < 0.0006f * (0.2f + Crackle))
				{
					CrackEnv = 0.5f + 0.5f * Rng.Uniform01();
				}
				CrackEnv *= 0.995f;
				const float S = Band.Process(Rng.White()) * 0.55f * Lv + Crack.Process(Rng.White()) * CrackEnv * 1.2f * Crackle;
				WriteFrame(Out, I, S, S);
			}
		}
	};

	/** Shared garbled-speech model: formant-filtered pulse train with syllable rhythm. */
	class Babble
	{
	public:
		void Prepare(float InSampleRate, uint32_t Seed)
		{
			SR = InSampleRate;
			N.Reseed(Seed + 77u);
			F1.Bandpass(600.0f, 5.0f, SR);
			F2.Bandpass(1500.0f, 7.0f, SR);
			Fric.Highpass(2500.0f, 0.7f, SR);
			Env.Setup(0.012f, SR);
		}
		/** Pitch in Hz, returns raw (not band limited) speech-like signal. */
		float Next(bool bSpeaking, float PitchHz)
		{
			const float Dt = 1.0f / SR;
			StateTimer -= Dt;
			if (StateTimer <= 0.0f)
			{
				NextSyllable(bSpeaking);
			}
			Vibrato += Dt * 5.3f;
			const float F0 = PitchHz * (1.0f + 0.02f * std::sin(Vibrato * TwoPi) + Contour);
			Phase += F0 * Dt;
			if (Phase >= 1.0f)
			{
				Phase -= 1.0f;
			}
			const float Glottal = (Phase < 0.4f ? Phase / 0.4f : 1.0f - (Phase - 0.4f) / 0.6f) * 2.0f - 1.0f;
			const float E = Env.Next(Voiced ? 1.0f : 0.0f);
			const float FE = FricEnv;
			FricEnv *= 0.9992f;
			const float Voice = (F1.Process(Glottal) * 1.4f + F2.Process(Glottal) * 0.9f) * E;
			const float Hiss = Fric.Process(N.White()) * FE * 0.35f;
			return Voice + Hiss;
		}

	private:
		void NextSyllable(bool bSpeaking)
		{
			static const float Formants[5][2] = {{730.0f, 1090.0f}, {530.0f, 1840.0f}, {270.0f, 2290.0f}, {570.0f, 840.0f}, {440.0f, 1020.0f}};
			if (!bSpeaking)
			{
				Voiced = false;
				StateTimer = 0.05f;
				return;
			}
			if (SyllablesLeft <= 0)
			{
				// word gap
				Voiced = false;
				StateTimer = 0.05f + 0.16f * N.Uniform01();
				if (N.Uniform01() < 0.12f)
				{
					StateTimer += 0.35f; // sentence pause
				}
				SyllablesLeft = 1 + static_cast<int>(N.Uniform01() * 3.5f);
				return;
			}
			--SyllablesLeft;
			Voiced = true;
			StateTimer = 0.09f + 0.14f * N.Uniform01();
			const int V = static_cast<int>(N.Uniform01() * 4.99f);
			F1.Bandpass(Formants[V][0], 5.0f, SR);
			F2.Bandpass(Formants[V][1], 7.0f, SR);
			Contour = N.White() * 0.08f;
			if (N.Uniform01() < 0.4f)
			{
				FricEnv = 0.6f + 0.4f * N.Uniform01(); // consonant
			}
		}

		float SR = 48000.0f;
		Noise N{5u};
		Biquad F1, F2, Fric;
		Smoother Env;
		float Phase = 0.0f, Vibrato = 0.0f, StateTimer = 0.0f, Contour = 0.0f, FricEnv = 0.0f;
		bool Voiced = false;
		int SyllablesLeft = 0;
	};

	// ------------------------------------------------------------------ Radio voice
	// A = speaking (0/1), B = pitch multiplier (0.5..2), C = signal quality (0..1), D = static level
	class RadioVoiceGen final : public Generator
	{
		Babble Speech;
		Biquad Hp, Lp, StaticBand;
		float DropTimer = 0.0f;
		float DropGain = 1.0f;
		Smoother DropSmooth;

		void OnPrepare() override
		{
			Speech.Prepare(SampleRate, 31u);
			Hp.Highpass(320.0f, 0.7f, SampleRate);
			Lp.Lowpass(3100.0f, 0.8f, SampleRate);
			StaticBand.Bandpass(1500.0f, 0.5f, SampleRate);
			DropSmooth.Setup(0.01f, SampleRate);
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const bool bSpeaking = GetParam(Param::A) > 0.5f;
			const float Pitch = 118.0f * Clampf(GetParam(Param::B) <= 0.0f ? 1.0f : GetParam(Param::B), 0.4f, 2.5f);
			const float Signal = Clamp01f(GetParam(Param::C) <= 0.0f ? 1.0f : GetParam(Param::C));
			const float StaticLevel = Clamp01f(GetParam(Param::D));
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				DropTimer -= Dt;
				if (DropTimer <= 0.0f)
				{
					DropTimer = 0.045f + 0.06f * Rng.Uniform01();
					DropGain = (Rng.Uniform01() < (1.0f - Signal) * 0.65f) ? 0.04f : 1.0f;
				}
				const float G = DropSmooth.Next(DropGain);
				float V = Speech.Next(bSpeaking, Pitch);
				V = Lp.Process(Hp.Process(V));
				V = SoftClip(V * 2.2f) * 0.55f * G;
				const float Hiss = StaticBand.Process(Rng.White()) * (StaticLevel * 0.25f + (1.0f - G) * 0.45f * (bSpeaking ? 1.0f : 0.0f));
				const float S = V + Hiss;
				WriteFrame(Out, I, S, S);
			}
		}
	};

	// ------------------------------------------------------------------ Engine
	// A = rpm, B = load (0..1), C = running (0/1)
	// Triggers: 0 = crank that catches, 1 = crank that fails, 2 = stall
	class EngineGen final : public Generator
	{
		Smoother Rpm, Load, Run;
		Biquad Exhaust, Body;
		float Phase = 0.0f, SubPhase = 0.0f, StarterPhase = 0.0f;
		float CrankTime = 0.0f;
		bool bCranking = false;
		bool bCrankCatches = false;
		float StallEnv = 0.0f;

		void OnPrepare() override
		{
			Rpm.Setup(0.25f, SampleRate);
			Rpm.Value = 800.0f;
			Load.Setup(0.15f, SampleRate);
			Run.Setup(0.35f, SampleRate);
			Exhaust.Lowpass(1200.0f, 0.9f, SampleRate);
			Body.Lowpass(2400.0f, 0.7f, SampleRate);
		}
		void OnTrigger(int Kind, float) override
		{
			if (Kind == 0 || Kind == 1)
			{
				bCranking = true;
				bCrankCatches = Kind == 0;
				CrankTime = 0.0f;
			}
			else if (Kind == 2)
			{
				StallEnv = 1.0f;
			}
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float TargetRpm = Clampf(GetParam(Param::A), 0.0f, 7000.0f);
			const float TargetLoad = Clamp01f(GetParam(Param::B));
			const float TargetRun = GetParam(Param::C) > 0.5f ? 1.0f : 0.0f;
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				const float R = Rpm.Next(std::max(TargetRpm, 650.0f));
				const float Ld = Load.Next(TargetLoad);
				const float Rn = Run.Next(TargetRun);
				const float Fire = R / 60.0f * 4.0f;
				Phase += Fire * Dt;
				Phase -= std::floor(Phase);
				SubPhase += Fire * 0.5f * Dt;
				SubPhase -= std::floor(SubPhase);
				const float P = Phase * TwoPi;
				float Tone = std::sin(P) + 0.55f * std::sin(2.0f * P + 0.4f) + 0.3f * std::sin(3.0f * P + 1.1f) + 0.18f * std::sin(5.0f * P);
				Tone += 0.6f * std::sin(SubPhase * TwoPi);
				const float Pulse = std::pow(0.5f + 0.5f * std::sin(P), 4.0f);
				const float Exh = Exhaust.Process(Rng.White() * Pulse) * (0.6f + 1.2f * Ld);
				float S = Body.Process(Tone * 0.22f + Exh) * Rn * (0.45f + 0.55f * Ld);

				if (bCranking)
				{
					CrankTime += Dt;
					StarterPhase += (170.0f + 25.0f * std::sin(CrankTime * TwoPi * 11.0f)) * Dt;
					StarterPhase -= std::floor(StarterPhase);
					const float Starter = (StarterPhase * 2.0f - 1.0f) * 0.12f * (0.6f + 0.4f * std::sin(CrankTime * TwoPi * 11.0f));
					if (!bCrankCatches && Rng.Uniform01() < 0.0009f)
					{
						StallEnv = 0.7f; // sputter of a cylinder that almost fires
					}
					S += Starter;
					const float Limit = bCrankCatches ? 0.7f : 1.4f;
					if (CrankTime > Limit)
					{
						bCranking = false;
					}
				}
				if (StallEnv > 0.001f)
				{
					S += Exhaust.Process(Rng.White()) * StallEnv * 0.8f;
					StallEnv *= 0.9993f;
				}
				WriteFrame(Out, I, S, S);
			}
		}
	};

	// ------------------------------------------------------------------ Horn
	// A = on (0/1)
	class HornGen final : public Generator
	{
		Smoother Env;
		float P1 = 0.0f, P2 = 0.0f;
		Biquad Lp;
		void OnPrepare() override
		{
			Env.Setup(0.015f, SampleRate);
			Lp.Lowpass(2800.0f, 0.8f, SampleRate);
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Target = GetParam(Param::A) > 0.5f ? 1.0f : 0.0f;
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				const float E = Env.Next(Target);
				P1 += 405.0f * Dt; P1 -= std::floor(P1);
				P2 += 508.0f * Dt; P2 -= std::floor(P2);
				const float Sq = (P1 < 0.5f ? 1.0f : -1.0f) + (P2 < 0.5f ? 1.0f : -1.0f);
				const float S = Lp.Process(Sq * 0.25f) * E;
				WriteFrame(Out, I, S, S);
			}
		}
	};

	// ------------------------------------------------------------------ Clicks
	// Triggers: 0 = relay tick, 1 = relay tock, 2 = switch, 3 = camera shutter, 4 = radio squelch on, 5 = squelch off
	class ClickGen final : public Generator
	{
		struct Voice
		{
			float Time = -1.0f, Freq = 2500.0f, Decay = 0.01f, Amp = 0.5f, NoiseAmp = 0.3f, Delay = 0.0f, Phase = 0.0f;
		};
		std::array<Voice, 6> Voices{};
		Biquad Hp;
		void OnPrepare() override { Hp.Highpass(1500.0f, 0.7f, SampleRate); }
		void Add(float Freq, float Decay, float Amp, float NoiseAmp, float Delay)
		{
			for (Voice& V : Voices)
			{
				if (V.Time < 0.0f)
				{
					V.Time = 0.0f; V.Freq = Freq; V.Decay = Decay; V.Amp = Amp; V.NoiseAmp = NoiseAmp; V.Delay = Delay; V.Phase = 0.0f;
					return;
				}
			}
		}
		void OnTrigger(int Kind, float Strength) override
		{
			const float S = Strength <= 0.0f ? 1.0f : Strength;
			switch (Kind)
			{
			case 0: Add(2900.0f, 0.006f, 0.35f * S, 0.25f * S, 0.0f); break;
			case 1: Add(2100.0f, 0.007f, 0.3f * S, 0.22f * S, 0.0f); break;
			case 2: Add(1800.0f, 0.01f, 0.4f * S, 0.5f * S, 0.0f); break;
			case 3: Add(3500.0f, 0.012f, 0.3f * S, 0.8f * S, 0.0f); Add(2600.0f, 0.02f, 0.25f * S, 0.7f * S, 0.045f); break;
			case 4: Add(1200.0f, 0.03f, 0.2f * S, 0.9f * S, 0.0f); break;
			case 5: Add(900.0f, 0.06f, 0.15f * S, 1.0f * S, 0.0f); break;
			default: break;
			}
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				float S = 0.0f;
				for (Voice& V : Voices)
				{
					if (V.Time < 0.0f)
					{
						continue;
					}
					V.Time += Dt;
					const float T = V.Time - V.Delay;
					if (T < 0.0f)
					{
						continue;
					}
					const float Env = std::exp(-T / V.Decay);
					V.Phase += V.Freq * Dt;
					V.Phase -= std::floor(V.Phase);
					S += (std::sin(V.Phase * TwoPi) * V.Amp + Hp.Process(Rng.White()) * V.NoiseAmp) * Env;
					if (Env < 0.001f)
					{
						V.Time = -1.0f;
					}
				}
				WriteFrame(Out, I, S, S);
			}
		}
	};

	/** Karplus-Strong plucked string with a fixed maximum delay line. */
	class Pluck
	{
	public:
		void Excite(float Freq, float SR, float Amp, Noise& N)
		{
			Length = static_cast<int>(SR / std::max(40.0f, Freq));
			Length = Length < 2 ? 2 : (Length >= MaxLen ? MaxLen - 1 : Length);
			for (int I = 0; I < Length; ++I)
			{
				Line[static_cast<size_t>(I)] = N.White() * Amp;
			}
			Pos = 0;
			Active = true;
			Life = 0;
		}
		float Next()
		{
			if (!Active)
			{
				return 0.0f;
			}
			const int Next1 = (Pos + 1) % Length;
			const float Out = Line[static_cast<size_t>(Pos)];
			Line[static_cast<size_t>(Pos)] = Flush((Out + Line[static_cast<size_t>(Next1)]) * 0.4985f);
			Pos = Next1;
			if (++Life > 48000 * 4)
			{
				Active = false;
			}
			return Out;
		}

	private:
		static constexpr int MaxLen = 2048;
		std::array<float, MaxLen> Line{};
		int Length = 100;
		int Pos = 0;
		int Life = 0;
		bool Active = false;
	};

	// ------------------------------------------------------------------ Music (car radio)
	// A = station (0 folk, 1 ambient, 2 news), B = on (0/1), C = signal (0..1), D = intrusion (0..1)
	class MusicGen final : public Generator
	{
		std::array<Pluck, 6> Strings{};
		size_t NextString = 0;
		Babble Speech;
		Babble IntruderSpeech;
		Biquad SpeakerHp, SpeakerLp, VoiceHp, VoiceLp, PadLp, StaticBand, IntruderLp;
		Smoother OnEnv, IntrusionEnv;
		float StepTimer = 0.0f;
		int Step = 0;
		int Chord = 0;
		float PadPhase[3] = {0.0f, 0.0f, 0.0f};
		float BellPhase = 0.0f, BellEnv = 0.0f, BellFreq = 880.0f, KickEnv = 0.0f, KickPhase = 0.0f;
		float DropTimer = 0.0f, DropGain = 1.0f;
		Smoother DropSmooth;

		static float Midi(float Note) { return 440.0f * std::pow(2.0f, (Note - 69.0f) / 12.0f); }

		void OnPrepare() override
		{
			Speech.Prepare(SampleRate, 101u);
			IntruderSpeech.Prepare(SampleRate, 666u);
			SpeakerHp.Highpass(110.0f, 0.7f, SampleRate);
			SpeakerLp.Lowpass(6500.0f, 0.7f, SampleRate);
			VoiceHp.Highpass(250.0f, 0.7f, SampleRate);
			VoiceLp.Lowpass(3600.0f, 0.7f, SampleRate);
			PadLp.Lowpass(900.0f, 0.6f, SampleRate);
			StaticBand.Bandpass(1700.0f, 0.5f, SampleRate);
			IntruderLp.Lowpass(1800.0f, 1.2f, SampleRate);
			OnEnv.Setup(0.08f, SampleRate);
			IntrusionEnv.Setup(0.2f, SampleRate);
			DropSmooth.Setup(0.01f, SampleRate);
		}

		void RenderBlock(float* Out, int Frames) override
		{
			static const int Progression[4][3] = {{57, 60, 64}, {53, 57, 60}, {48, 52, 55}, {55, 59, 62}}; // Am F C G
			const int Station = static_cast<int>(GetParam(Param::A) + 0.5f);
			const float OnTarget = GetParam(Param::B) > 0.5f ? 1.0f : 0.0f;
			const float Signal = Clamp01f(GetParam(Param::C) <= 0.0f ? 1.0f : GetParam(Param::C));
			const float Intrusion = Clamp01f(GetParam(Param::D));
			const float Dt = 1.0f / SampleRate;
			const float StepLen = Station == 1 ? 0.75f : 0.357f; // 84 bpm eighths / slow ambient

			for (int I = 0; I < Frames; ++I)
			{
				const float On = OnEnv.Next(OnTarget);
				const float Intr = IntrusionEnv.Next(Intrusion);

				StepTimer -= Dt;
				if (StepTimer <= 0.0f)
				{
					StepTimer += StepLen;
					++Step;
					if (Step % 8 == 0)
					{
						Chord = (Chord + 1) % 4;
					}
					const int* Tones = Progression[Chord];
					if (Station == 0)
					{
						const int Tone = Tones[static_cast<size_t>(Rng.Uniform01() * 2.99f)];
						const float Octave = (Step % 4 == 0) ? -12.0f : (Rng.Uniform01() < 0.3f ? 12.0f : 0.0f);
						Strings[NextString].Excite(Midi(static_cast<float>(Tone) + Octave), SampleRate, 0.5f, Rng);
						NextString = (NextString + 1) % Strings.size();
						if (Step % 4 == 0)
						{
							KickEnv = 1.0f;
						}
					}
					else if (Station == 1 && Rng.Uniform01() < 0.18f)
					{
						BellEnv = 0.5f;
						BellFreq = Midi(static_cast<float>(Tones[static_cast<size_t>(Rng.Uniform01() * 2.99f)]) + 24.0f);
					}
				}

				float Music = 0.0f;
				if (Station == 0 || Station == 1)
				{
					for (Pluck& P : Strings)
					{
						Music += P.Next();
					}
					// soft pad
					const int* Tones = Progression[Chord];
					float Pad = 0.0f;
					for (int K = 0; K < 3; ++K)
					{
						PadPhase[K] += Midi(static_cast<float>(Tones[K]) - (Station == 1 ? 12.0f : 0.0f)) * Dt;
						PadPhase[K] -= std::floor(PadPhase[K]);
						Pad += (PadPhase[K] * 2.0f - 1.0f);
					}
					Music += PadLp.Process(Pad) * (Station == 1 ? 0.22f : 0.08f);
					KickPhase += (55.0f + 60.0f * KickEnv) * Dt;
					KickPhase -= std::floor(KickPhase);
					Music += std::sin(KickPhase * TwoPi) * KickEnv * 0.35f;
					KickEnv *= 0.9993f;
					BellPhase += BellFreq * Dt;
					BellPhase -= std::floor(BellPhase);
					Music += std::sin(BellPhase * TwoPi + 2.0f * std::sin(BellPhase * TwoPi * 3.5f)) * BellEnv * 0.25f;
					BellEnv *= 0.99985f;
				}
				else
				{
					// news: announcer voice over a quiet bed
					const float V = Speech.Next(true, 128.0f);
					Music = VoiceLp.Process(VoiceHp.Process(V)) * 0.8f;
				}

				// reception
				DropTimer -= Dt;
				if (DropTimer <= 0.0f)
				{
					DropTimer = 0.05f + 0.08f * Rng.Uniform01();
					DropGain = Rng.Uniform01() < (1.0f - Signal) * 0.5f ? 0.1f : 1.0f;
				}
				const float G = DropSmooth.Next(DropGain);
				const float Static = StaticBand.Process(Rng.White());

				// intrusion: a slow low voice through heavy static replaces the program
				float Intruder = 0.0f;
				if (Intr > 0.001f)
				{
					Intruder = IntruderLp.Process(IntruderSpeech.Next(Intrusion > 0.5f, 72.0f)) * 1.3f;
				}
				float Mix = Music * G * (1.0f - Intr) + Static * (0.04f + (1.0f - G) * 0.25f + Intr * 0.22f) + Intruder * Intr;
				Mix = SpeakerLp.Process(SpeakerHp.Process(Mix)) * On;
				WriteFrame(Out, I, Mix, Mix);
			}
		}
	};

	// ------------------------------------------------------------------ Creature
	// A = breath (0..1), B = growl (0..1). Triggers: 0 = scream, 1 = thud/hit, 2 = clicking chatter
	class CreatureGen final : public Generator
	{
		Biquad BreathBand, GrowlLp, ScreamBand, ScreamBand2, ThudLp;
		float BreathPhase = 0.0f, GrowlPhase = 0.0f, Rough = 0.0f, ScreamTime = -1.0f, ScreamPhase = 0.0f, ScreamPhase2 = 0.0f;
		float ThudEnv = 0.0f, ThudPhase = 0.0f, ChatterTime = -1.0f, ChatterEnv = 0.0f;
		Smoother BreathS, GrowlS;

		void OnPrepare() override
		{
			BreathBand.Bandpass(520.0f, 1.2f, SampleRate);
			GrowlLp.Lowpass(650.0f, 1.0f, SampleRate);
			ScreamBand.Bandpass(1250.0f, 2.5f, SampleRate);
			ScreamBand2.Bandpass(2600.0f, 3.0f, SampleRate);
			ThudLp.Lowpass(300.0f, 0.8f, SampleRate);
			BreathS.Setup(0.3f, SampleRate);
			GrowlS.Setup(0.2f, SampleRate);
		}
		void OnTrigger(int Kind, float) override
		{
			if (Kind == 0) { ScreamTime = 0.0f; }
			else if (Kind == 1) { ThudEnv = 1.0f; }
			else if (Kind == 2) { ChatterTime = 0.0f; }
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float BreathT = Clamp01f(GetParam(Param::A));
			const float GrowlT = Clamp01f(GetParam(Param::B));
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				const float Br = BreathS.Next(BreathT);
				const float Gr = GrowlS.Next(GrowlT);
				BreathPhase += Dt / 2.6f;
				BreathPhase -= std::floor(BreathPhase);
				const float BreathEnv = std::pow(std::max(0.0f, std::sin(BreathPhase * TwoPi)), 2.0f) + 0.6f * std::pow(std::max(0.0f, -std::sin(BreathPhase * TwoPi)), 3.0f);
				float S = BreathBand.Process(Rng.White()) * BreathEnv * Br * 0.9f;

				GrowlPhase += (58.0f + 9.0f * std::sin(BreathPhase * TwoPi)) * Dt;
				GrowlPhase -= std::floor(GrowlPhase);
				Rough = Rough * 0.97f + Rng.White() * 0.03f;
				const float RoughAm = 0.6f + 0.4f * std::sin(GrowlPhase * TwoPi * 0.43f) + Rough * 4.0f;
				S += GrowlLp.Process((GrowlPhase * 2.0f - 1.0f) * RoughAm) * Gr * 0.5f;

				if (ScreamTime >= 0.0f)
				{
					ScreamTime += Dt;
					const float T = ScreamTime;
					const float F = 980.0f - 420.0f * std::min(1.0f, T / 1.4f) + 60.0f * std::sin(T * TwoPi * 7.0f);
					ScreamPhase += F * Dt; ScreamPhase -= std::floor(ScreamPhase);
					ScreamPhase2 += F * 1.37f * Dt; ScreamPhase2 -= std::floor(ScreamPhase2);
					const float Env = std::min(1.0f, T / 0.06f) * std::exp(-std::max(0.0f, T - 0.4f) * 2.2f);
					const float Raw = (ScreamPhase * 2.0f - 1.0f) + (ScreamPhase2 * 2.0f - 1.0f) * 0.7f + Rng.White() * 0.5f;
					S += (ScreamBand.Process(Raw) * 1.4f + ScreamBand2.Process(Raw) * 0.8f) * Env;
					if (T > 2.4f) { ScreamTime = -1.0f; }
				}
				if (ThudEnv > 0.001f)
				{
					ThudPhase += (48.0f + 40.0f * ThudEnv) * Dt; ThudPhase -= std::floor(ThudPhase);
					S += (std::sin(ThudPhase * TwoPi) * 0.9f + ThudLp.Process(Rng.White()) * 1.2f) * ThudEnv;
					ThudEnv *= 0.9985f;
				}
				if (ChatterTime >= 0.0f)
				{
					ChatterTime += Dt;
					if (Rng.Uniform01() < 15.0f * Dt)
					{
						ChatterEnv = 1.0f;
					}
					S += ScreamBand2.Process(Rng.White()) * ChatterEnv * 0.9f;
					ChatterEnv *= 0.992f;
					if (ChatterTime > 0.8f) { ChatterTime = -1.0f; }
				}
				WriteFrame(Out, I, S, S);
			}
		}
	};

	/** One-shot voice used by impact, footstep and door sounds. */
	struct ShotVoice
	{
		bool bActive = false;
		float Time = 0.0f;
		int Kind = 0;
		float Strength = 1.0f;
		float Phase = 0.0f;
		float Phase2 = 0.0f;
		float Aux = 0.0f;
		Biquad F1, F2;
	};

	// ------------------------------------------------------------------ Impacts
	// Triggers: 0 = wood knock, 1 = metal bang, 2 = scratch, 3 = body fall, 4 = glass
	class ImpactGen final : public Generator
	{
		std::array<ShotVoice, 6> Voices{};
		void OnTrigger(int Kind, float Strength) override
		{
			for (ShotVoice& V : Voices)
			{
				if (!V.bActive)
				{
					V.bActive = true; V.Time = 0.0f; V.Kind = Kind; V.Strength = Strength <= 0.0f ? 1.0f : Strength; V.Phase = 0.0f; V.Phase2 = 0.0f;
					V.F1.Reset(); V.F2.Reset();
					V.F1.Bandpass(Kind == 2 ? 3000.0f : (Kind == 4 ? 4200.0f : 650.0f), Kind == 2 ? 3.0f : 1.2f, SampleRate);
					V.F2.Lowpass(Kind == 3 ? 260.0f : 2000.0f, 0.8f, SampleRate);
					return;
				}
			}
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				float S = 0.0f;
				for (ShotVoice& V : Voices)
				{
					if (!V.bActive)
					{
						continue;
					}
					V.Time += Dt;
					const float T = V.Time;
					float X = 0.0f;
					float Life = 0.5f;
					switch (V.Kind)
					{
					case 0: // wood knock
						V.Phase += 150.0f * Dt; V.Phase -= std::floor(V.Phase);
						X = std::sin(V.Phase * TwoPi) * std::exp(-T / 0.05f) * 0.9f + V.F1.Process(Rng.White()) * std::exp(-T / 0.02f) * 1.4f;
						Life = 0.4f;
						break;
					case 1: // metal
						V.Phase += 423.0f * Dt; V.Phase -= std::floor(V.Phase);
						V.Phase2 += 1131.0f * Dt; V.Phase2 -= std::floor(V.Phase2);
						X = (std::sin(V.Phase * TwoPi) + 0.6f * std::sin(V.Phase2 * TwoPi)) * std::exp(-T / 0.35f) * 0.5f + V.F2.Process(Rng.White()) * std::exp(-T / 0.03f);
						Life = 2.0f;
						break;
					case 2: // scratch (claws on wood)
						V.Aux = V.Aux * 0.9f + Rng.White() * 0.1f;
						X = V.F1.Process(Rng.White()) * (0.5f + 4.0f * std::fabs(V.Aux)) * std::sin(std::min(1.0f, T / 0.8f) * 3.14159f) * 1.3f;
						Life = 0.8f;
						break;
					case 3: // heavy body / thud
						V.Phase += (45.0f + 30.0f * std::exp(-T / 0.1f)) * Dt; V.Phase -= std::floor(V.Phase);
						X = std::sin(V.Phase * TwoPi) * std::exp(-T / 0.18f) + V.F2.Process(Rng.White()) * std::exp(-T / 0.07f) * 1.5f;
						Life = 0.9f;
						break;
					default: // glass
						V.Phase += 3100.0f * Dt; V.Phase -= std::floor(V.Phase);
						X = std::sin(V.Phase * TwoPi) * std::exp(-T / 0.25f) * 0.3f + V.F1.Process(Rng.White()) * std::exp(-T / 0.12f);
						Life = 1.2f;
						break;
					}
					S += X * V.Strength;
					if (T > Life)
					{
						V.bActive = false;
					}
				}
				WriteFrame(Out, I, S, S);
			}
		}
	};

	// ------------------------------------------------------------------ Footsteps
	// Triggers: kind = surface (0 dirt/grass, 1 gravel, 2 wood, 3 asphalt/concrete, 4 metal)
	class FootstepGen final : public Generator
	{
		std::array<ShotVoice, 4> Voices{};
		void OnTrigger(int Kind, float Strength) override
		{
			for (ShotVoice& V : Voices)
			{
				if (!V.bActive)
				{
					V.bActive = true; V.Time = 0.0f; V.Kind = Kind; V.Strength = Strength <= 0.0f ? 1.0f : Strength; V.Phase = 0.0f; V.Aux = 0.0f;
					V.F1.Reset(); V.F2.Reset();
					const float Jit = 0.85f + 0.3f * Rng.Uniform01();
					switch (Kind)
					{
					case 1: V.F1.Bandpass(2600.0f * Jit, 1.5f, SampleRate); V.F2.Lowpass(900.0f, 0.7f, SampleRate); break;
					case 2: V.F1.Bandpass(380.0f * Jit, 2.0f, SampleRate); V.F2.Lowpass(1500.0f, 0.7f, SampleRate); break;
					case 3: V.F1.Bandpass(1400.0f * Jit, 0.8f, SampleRate); V.F2.Highpass(400.0f, 0.7f, SampleRate); break;
					case 4: V.F1.Bandpass(920.0f * Jit, 12.0f, SampleRate); V.F2.Bandpass(1460.0f * Jit, 10.0f, SampleRate); break;
					default: V.F1.Lowpass(800.0f * Jit, 0.7f, SampleRate); V.F2.Bandpass(2000.0f, 0.8f, SampleRate); break;
					}
					return;
				}
			}
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				float S = 0.0f;
				for (ShotVoice& V : Voices)
				{
					if (!V.bActive)
					{
						continue;
					}
					V.Time += Dt;
					const float T = V.Time;
					float X = 0.0f;
					switch (V.Kind)
					{
					case 1: // gravel: granular crunch
						if (T < 0.09f && Rng.Uniform01() < 0.02f) { V.Aux = 1.0f; }
						V.Aux *= 0.985f;
						X = V.F1.Process(Rng.White()) * V.Aux * 1.4f + V.F2.Process(Rng.White()) * std::exp(-T / 0.05f) * 0.5f;
						break;
					case 2: // wood planks
						V.Phase += 110.0f * Dt; V.Phase -= std::floor(V.Phase);
						X = std::sin(V.Phase * TwoPi) * std::exp(-T / 0.06f) * 0.7f + V.F1.Process(Rng.White()) * std::exp(-T / 0.05f) * 1.2f;
						break;
					case 3: // asphalt
						X = V.F1.Process(V.F2.Process(Rng.White())) * std::exp(-T / 0.03f) * 1.6f;
						break;
					case 4: // metal stairs
						X = (V.F1.Process(Rng.White()) + V.F2.Process(Rng.White())) * std::exp(-T / 0.12f) * 2.5f;
						break;
					default: // soft forest floor
						X = V.F1.Process(Rng.White()) * std::exp(-T / 0.07f) * 1.1f + V.F2.Process(Rng.White()) * std::exp(-T / 0.03f) * 0.3f;
						break;
					}
					S += X * V.Strength * 0.6f;
					if (T > 0.3f)
					{
						V.bActive = false;
					}
				}
				WriteFrame(Out, I, S, S);
			}
		}
	};

	// ------------------------------------------------------------------ Stingers
	// Triggers: 0 = slow dissonant swell, 1 = low boom
	class StingerGen final : public Generator
	{
		std::array<float, 5> Phases{};
		float SwellTime = -1.0f, BoomEnv = 0.0f, BoomPhase = 0.0f;
		Biquad Sweep, BoomLp;
		void OnPrepare() override
		{
			Sweep.Lowpass(200.0f, 2.0f, SampleRate);
			BoomLp.Lowpass(180.0f, 0.8f, SampleRate);
		}
		void OnTrigger(int Kind, float) override
		{
			if (Kind == 0) { SwellTime = 0.0f; }
			else { BoomEnv = 1.0f; }
		}
		void RenderBlock(float* Out, int Frames) override
		{
			static const float Freqs[5] = {110.0f, 116.54f, 164.81f, 174.61f, 246.94f};
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				float S = 0.0f;
				if (SwellTime >= 0.0f)
				{
					SwellTime += Dt;
					const float T = SwellTime;
					if ((I & 31) == 0)
					{
						Sweep.Lowpass(200.0f + 2800.0f * std::min(1.0f, T / 2.6f), 2.0f, SampleRate);
					}
					float Raw = 0.0f;
					for (size_t K = 0; K < Phases.size(); ++K)
					{
						Phases[K] += Freqs[K] * (1.0f + 0.003f * static_cast<float>(K)) * Dt;
						Phases[K] -= std::floor(Phases[K]);
						Raw += Phases[K] * 2.0f - 1.0f;
					}
					const float Env = T < 2.6f ? (T / 2.6f) * (T / 2.6f) : std::exp(-(T - 2.6f) * 6.0f);
					S += Sweep.Process(Raw * 0.25f) * Env;
					if (T > 3.6f) { SwellTime = -1.0f; }
				}
				if (BoomEnv > 0.0005f)
				{
					BoomPhase += 42.0f * Dt; BoomPhase -= std::floor(BoomPhase);
					S += (std::sin(BoomPhase * TwoPi) + BoomLp.Process(Rng.White()) * 2.0f) * BoomEnv * 0.9f;
					BoomEnv *= 0.99985f;
				}
				WriteFrame(Out, I, S, S);
			}
		}
	};

	// ------------------------------------------------------------------ Hum (generator, fridge, lamps)
	// A = level (0..1), B = base frequency (Hz, default 50)
	class HumGen final : public Generator
	{
		float Phase = 0.0f;
		Smoother Level;
		Biquad Lp;
		void OnPrepare() override
		{
			Level.Setup(0.3f, SampleRate);
			Lp.Lowpass(1500.0f, 0.7f, SampleRate);
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Target = Clamp01f(GetParam(Param::A));
			const float Base = GetParam(Param::B) > 10.0f ? GetParam(Param::B) : 50.0f;
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				const float L = Level.Next(Target);
				Phase += Base * Dt; Phase -= std::floor(Phase);
				const float P = Phase * TwoPi;
				const float S = (std::sin(P) * 0.5f + std::sin(2.0f * P) * 0.35f + std::sin(3.0f * P) * 0.2f + std::sin(6.0f * P) * 0.1f + Lp.Process(Rng.White()) * 0.15f) * L * 0.6f;
				WriteFrame(Out, I, S, S);
			}
		}
	};

	// ------------------------------------------------------------------ Fuel pump
	// A = flowing (0/1)
	class PumpGen final : public Generator
	{
		Smoother Flow;
		float Phase = 0.0f, Wobble = 0.0f;
		Biquad FlowLp;
		void OnPrepare() override
		{
			Flow.Setup(0.25f, SampleRate);
			FlowLp.Lowpass(1300.0f, 0.7f, SampleRate);
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Target = GetParam(Param::A) > 0.5f ? 1.0f : 0.0f;
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				const float F = Flow.Next(Target);
				Wobble += 3.1f * Dt; Wobble -= std::floor(Wobble);
				Phase += (215.0f + 6.0f * std::sin(Wobble * TwoPi)) * Dt; Phase -= std::floor(Phase);
				const float Motor = (std::sin(Phase * TwoPi) * 0.4f + std::sin(2.0f * Phase * TwoPi) * 0.2f) * F;
				const float Liquid = FlowLp.Process(Rng.White()) * 0.5f * F;
				WriteFrame(Out, I, Motor * 0.5f + Liquid, Motor * 0.5f + Liquid);
			}
		}
	};

	// ------------------------------------------------------------------ Thunder
	// Trigger: strength (0..1)
	class ThunderGen final : public Generator
	{
		float Time = -1.0f, Strength = 1.0f, Attack = 0.1f, Decay = 4.0f, CrackEnv = 0.0f;
		Biquad Lp, CrackBand;
		Noise N2{4242u};
		void OnPrepare() override
		{
			Lp.Lowpass(260.0f, 0.7f, SampleRate);
			CrackBand.Bandpass(900.0f, 0.8f, SampleRate);
		}
		void OnTrigger(int, float InStrength) override
		{
			Time = 0.0f;
			Strength = InStrength <= 0.0f ? 1.0f : InStrength;
			Attack = 0.05f + 0.3f * Rng.Uniform01();
			Decay = 3.0f + 3.0f * Rng.Uniform01();
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				float S = 0.0f;
				if (Time >= 0.0f)
				{
					Time += Dt;
					const float Env = Time < Attack ? Time / Attack : std::exp(-(Time - Attack) / Decay * 2.0f);
					if (Time < 1.2f && Rng.Uniform01() < 0.0008f)
					{
						CrackEnv = 1.0f;
					}
					CrackEnv *= 0.9985f;
					S = Lp.Process(N2.Brown() * 3.0f) * Env * Strength * 1.4f + CrackBand.Process(Rng.White()) * CrackEnv * 0.6f * Strength;
					if (Time > Attack + Decay * 2.5f)
					{
						Time = -1.0f;
					}
				}
				WriteFrame(Out, I, S, S);
			}
		}
	};

	// ------------------------------------------------------------------ Door creak
	// Triggers: 0 = opening creak, 1 = closing thump
	class DoorCreakGen final : public Generator
	{
		std::array<ShotVoice, 3> Voices{};
		void OnTrigger(int Kind, float Strength) override
		{
			for (ShotVoice& V : Voices)
			{
				if (!V.bActive)
				{
					V.bActive = true; V.Time = 0.0f; V.Kind = Kind; V.Strength = Strength <= 0.0f ? 1.0f : Strength;
					V.Aux = 0.6f + 0.6f * Rng.Uniform01(); // duration
					V.F1.Reset(); V.F2.Reset();
					V.F2.Lowpass(220.0f, 0.8f, SampleRate);
					return;
				}
			}
		}
		void RenderBlock(float* Out, int Frames) override
		{
			const float Dt = 1.0f / SampleRate;
			for (int I = 0; I < Frames; ++I)
			{
				float S = 0.0f;
				for (ShotVoice& V : Voices)
				{
					if (!V.bActive)
					{
						continue;
					}
					V.Time += Dt;
					const float T = V.Time;
					if (V.Kind == 0)
					{
						const float Progress = std::min(1.0f, T / V.Aux);
						if ((I & 15) == 0)
						{
							V.F1.Bandpass(420.0f + 600.0f * Progress + 80.0f * std::sin(T * 31.0f), 9.0f, SampleRate);
						}
						V.Phase += (38.0f + 20.0f * Progress) * Dt; V.Phase -= std::floor(V.Phase);
						const float StickSlip = V.Phase < 0.15f ? 1.0f : 0.15f;
						S += V.F1.Process(Rng.White()) * StickSlip * std::sin(Progress * 3.14159f) * 2.2f * V.Strength;
						if (T > V.Aux) { V.bActive = false; }
					}
					else
					{
						V.Phase += 70.0f * Dt; V.Phase -= std::floor(V.Phase);
						S += (std::sin(V.Phase * TwoPi) * 0.8f + V.F2.Process(Rng.White()) * 1.5f) * std::exp(-T / 0.08f) * V.Strength;
						if (T > 0.5f) { V.bActive = false; }
					}
				}
				WriteFrame(Out, I, S, S);
			}
		}
	};

	inline std::unique_ptr<Generator> CreateGenerator(SoundKind Kind)
	{
		switch (Kind)
		{
		case SoundKind::Wind: return std::make_unique<WindGen>();
		case SoundKind::Rain: return std::make_unique<RainGen>();
		case SoundKind::Birds: return std::make_unique<BirdGen>();
		case SoundKind::Crickets: return std::make_unique<CricketGen>();
		case SoundKind::RadioStatic: return std::make_unique<RadioStaticGen>();
		case SoundKind::RadioVoice: return std::make_unique<RadioVoiceGen>();
		case SoundKind::Engine: return std::make_unique<EngineGen>();
		case SoundKind::Horn: return std::make_unique<HornGen>();
		case SoundKind::Click: return std::make_unique<ClickGen>();
		case SoundKind::Music: return std::make_unique<MusicGen>();
		case SoundKind::Creature: return std::make_unique<CreatureGen>();
		case SoundKind::Impact: return std::make_unique<ImpactGen>();
		case SoundKind::Footstep: return std::make_unique<FootstepGen>();
		case SoundKind::Stinger: return std::make_unique<StingerGen>();
		case SoundKind::Hum: return std::make_unique<HumGen>();
		case SoundKind::Pump: return std::make_unique<PumpGen>();
		case SoundKind::Thunder: return std::make_unique<ThunderGen>();
		case SoundKind::DoorCreak: return std::make_unique<DoorCreakGen>();
		default: return nullptr;
		}
	}

	/** Ambience generators are rendered in stereo, everything else mono (spatialised by the engine). */
	inline bool IsStereoKind(SoundKind Kind)
	{
		return Kind == SoundKind::Wind || Kind == SoundKind::Rain || Kind == SoundKind::Birds || Kind == SoundKind::Crickets;
	}
}
}
