// Flash to Bang - procedural audio.

#include "Audio/FTBSynth.h"
#include "FlashtoBang.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"

namespace
{
	using FTBSynth::FBuffer;
	using FTBSynth::SampleRate;

	struct FOnePole
	{
		float Y = 0.0f;
		float Process(float X, float CutoffHz)
		{
			const float A = 1.0f - FMath::Exp(-UE_TWO_PI * FMath::Max(CutoffHz, 1.0f) / SampleRate);
			Y += A * (X - Y);
			return Y;
		}
	};

	FBuffer Make(float Seconds) { FBuffer B; B.SetNumZeroed(FMath::Max(1, FMath::RoundToInt(Seconds * SampleRate))); return B; }
	float TimeOf(int32 i) { return static_cast<float>(i) / SampleRate; }

	void Normalize(FBuffer& B, float Peak)
	{
		float Max = 0.0001f;
		for (float V : B) { Max = FMath::Max(Max, FMath::Abs(V)); }
		const float G = Peak / Max;
		for (float& V : B) { V *= G; }
	}

	/** Soft-knee saturation keeps big booms from clipping harshly. */
	float Sat(float X) { X = FMath::Clamp(X, -3.0f, 3.0f); return X * (27.0f + X * X) / (27.0f + 9.0f * X * X); }

	/** Hillside echo: a couple of delayed, darker copies. */
	void AddEcho(FBuffer& B, float Delay, float Gain, float Cutoff)
	{
		const int32 D = FMath::RoundToInt(Delay * SampleRate);
		FOnePole LP;
		FBuffer Src = B;
		for (int32 i = D; i < B.Num(); ++i)
		{
			B[i] += LP.Process(Src[i - D], Cutoff) * Gain;
		}
	}

	FBuffer Rumble(float Seconds, float DistanceM, float Muffle, int32 Seed, float Weight)
	{
		FRandomStream R(Seed);
		FBuffer B = Make(Seconds);
		const float Far = FMath::Clamp(DistanceM / 2000.0f, 0.0f, 1.0f);
		const float Cut = FMath::Lerp(900.0f, 160.0f, Far) * FMath::Lerp(1.0f, 0.45f, Muffle);
		FOnePole LP1, LP2, HP, Crack;
		float Phase = 0.0f;
		for (int32 i = 0; i < B.Num(); ++i)
		{
			const float T = TimeOf(i);
			const float N = R.FRandRange(-1.0f, 1.0f);
			// Body: noise through two poles, decaying.
			const float Body = LP2.Process(LP1.Process(N, Cut), Cut * 0.8f) * 3.2f;
			const float EnvBody = FMath::Min(1.0f, T / 0.012f) * FMath::Exp(-T / (0.35f + 0.55f * Far));
			// Thump: falling sine.
			const float F = FMath::Lerp(62.0f, 34.0f, FMath::Min(1.0f, T / 0.5f));
			Phase += F / SampleRate;
			const float Thump = FMath::Sin(UE_TWO_PI * Phase) * FMath::Exp(-T / 0.22f) * Weight;
			// Crack: only close-ish, high passed noise.
			const float Hi = N - HP.Process(N, 1800.0f);
			const float CrackEnv = FMath::Exp(-T / 0.035f) * (1.0f - Far) * (1.0f - Muffle);
			B[i] = Body * EnvBody + Thump * 0.9f + Crack.Process(Hi, 6000.0f) * CrackEnv * 0.6f;
		}
		return B;
	}
}

FBuffer FTBSynth::Boom(float DistanceM, float Muffle, int32 Seed)
{
	const float Far = FMath::Clamp(DistanceM / 2000.0f, 0.0f, 1.0f);
	FBuffer B = Rumble(2.6f + 1.4f * Far, DistanceM, Muffle, Seed, 1.0f);
	AddEcho(B, 0.42f + 0.2f * Far, 0.35f, 300.0f);
	AddEcho(B, 1.05f + 0.3f * Far, 0.22f, 200.0f);
	for (float& V : B) { V = Sat(V * 1.4f); }
	Normalize(B, 0.92f * FMath::Lerp(1.0f, 0.55f, Far) * FMath::Lerp(1.0f, 0.75f, Muffle));
	return B;
}

FBuffer FTBSynth::Explosion(float DistanceM, int32 Seed)
{
	FRandomStream R(Seed + 99);
	const float Far = FMath::Clamp(DistanceM / 2000.0f, 0.0f, 1.0f);
	FBuffer B = Rumble(3.2f, DistanceM * 0.6f, 0.0f, Seed, 1.4f);
	// Debris patter on top, if close.
	FOnePole LP;
	for (int32 k = 0; k < 40; ++k)
	{
		const int32 Start = FMath::RoundToInt(R.FRandRange(0.15f, 1.6f) * SampleRate);
		const float Amp = R.FRandRange(0.05f, 0.25f) * (1.0f - Far);
		for (int32 i = 0; i < 300 && Start + i < B.Num(); ++i)
		{
			B[Start + i] += LP.Process(R.FRandRange(-1, 1), 3000.0f) * Amp * FMath::Exp(-i / 60.0f);
		}
	}
	AddEcho(B, 0.5f, 0.3f, 280.0f);
	for (float& V : B) { V = Sat(V * 1.8f); }
	Normalize(B, FMath::Lerp(0.98f, 0.6f, Far));
	return B;
}

FBuffer FTBSynth::CookOff(float DistanceM, int32 Seed)
{
	FRandomStream R(Seed);
	const float Far = FMath::Clamp(DistanceM / 2000.0f, 0.0f, 1.0f);
	FBuffer B = Rumble(2.4f, DistanceM, 0.2f, Seed, 0.7f);
	FOnePole LP;
	// Rapid secondary pops: ammunition going up.
	for (int32 k = 0; k < 14; ++k)
	{
		const int32 Start = FMath::RoundToInt(R.FRandRange(0.1f, 1.8f) * SampleRate);
		const float Amp = R.FRandRange(0.25f, 0.7f);
		for (int32 i = 0; i < 2600 && Start + i < B.Num(); ++i)
		{
			B[Start + i] += LP.Process(R.FRandRange(-1, 1), FMath::Lerp(1400.0f, 400.0f, Far)) * Amp * FMath::Exp(-i / 500.0f) * 2.0f;
		}
	}
	for (float& V : B) { V = Sat(V * 1.3f); }
	Normalize(B, FMath::Lerp(0.8f, 0.45f, Far));
	return B;
}

FBuffer FTBSynth::WatchClick(bool bStop)
{
	FBuffer B = Make(bStop ? 0.16f : 0.08f);
	FRandomStream R(bStop ? 3 : 2);
	auto Click = [&](int32 Offset, float Freq)
	{
		for (int32 i = 0; i < 1400 && Offset + i < B.Num(); ++i)
		{
			const float T = TimeOf(i);
			B[Offset + i] += (FMath::Sin(UE_TWO_PI * Freq * T) * 0.7f + R.FRandRange(-0.4f, 0.4f)) * FMath::Exp(-T / 0.006f);
		}
	};
	Click(0, 3200.0f);
	if (bStop) { Click(FMath::RoundToInt(0.07f * SampleRate), 2600.0f); }
	Normalize(B, 0.55f);
	return B;
}

FBuffer FTBSynth::RadioCall(int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(0.95f);
	FOnePole LP, HP;
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i);
		const float N = R.FRandRange(-1, 1);
		const float Band = LP.Process(N, 3200.0f) - HP.Process(N, 700.0f);
		float V = 0.0f;
		if (T < 0.16f) { V += Band * 1.4f * FMath::Exp(-T / 0.08f); }               // squelch
		const float B1 = (T > 0.22f && T < 0.31f) ? 1.0f : 0.0f;
		const float B2 = (T > 0.38f && T < 0.47f) ? 1.0f : 0.0f;
		V += (B1 + B2) * FMath::Sin(UE_TWO_PI * 1250.0f * T) * 0.5f;                 // two beeps: "shot out"
		if (T > 0.52f) { V += Band * 0.35f * FMath::Exp(-(T - 0.52f) / 0.25f); }    // static tail
		B[i] = V;
	}
	Normalize(B, 0.6f);
	return B;
}

FBuffer FTBSynth::Whistle(float Seconds)
{
	FBuffer B = Make(Seconds);
	float Phase = 0.0f;
	FRandomStream R(5);
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i) / Seconds;
		const float F = FMath::Lerp(1900.0f, 650.0f, T * T);
		Phase += F / SampleRate;
		const float Env = FMath::Min(1.0f, T * 4.0f) * (0.25f + 0.75f * T);
		B[i] = (FMath::Sin(UE_TWO_PI * Phase) + R.FRandRange(-0.08f, 0.08f)) * Env;
	}
	Normalize(B, 0.42f);
	return B;
}

FBuffer FTBSynth::Incoming(float Seconds)
{
	FBuffer B = Make(Seconds);
	FRandomStream R(9);
	FOnePole LP, HP;
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i) / Seconds;
		const float N = R.FRandRange(-1, 1);
		const float Band = LP.Process(N, FMath::Lerp(2500.0f, 6000.0f, T)) - HP.Process(N, FMath::Lerp(1200.0f, 900.0f, T));
		B[i] = Band * FMath::Pow(T, 1.6f);
	}
	Normalize(B, 0.7f);
	return B;
}

FBuffer FTBSynth::Interference(int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(1.3f);
	FOnePole LP, HP;
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i);
		const float N = R.FRandRange(-1, 1);
		const float Band = LP.Process(N, 2600.0f) - HP.Process(N, 500.0f);
		const float AM = 0.5f + 0.5f * FMath::Sin(UE_TWO_PI * 9.0f * T);
		const float Tone = FMath::Sin(UE_TWO_PI * (880.0f + 300.0f * FMath::Sin(UE_TWO_PI * 3.0f * T)) * T) * 0.2f;
		B[i] = (Band * AM + Tone) * FMath::Min(1.0f, T / 0.1f) * FMath::Min(1.0f, (1.3f - T) / 0.3f);
	}
	Normalize(B, 0.5f);
	return B;
}

FBuffer FTBSynth::TurretServo()
{
	FBuffer B = Make(1.8f);
	FOnePole LP;
	float Phase = 0.0f;
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i);
		const float F = 95.0f + 20.0f * FMath::Sin(UE_TWO_PI * 5.0f * T);
		Phase += F / SampleRate;
		const float Saw = 2.0f * FMath::Frac(Phase) - 1.0f;
		B[i] = LP.Process(Saw, 700.0f) * FMath::Min(1.0f, T / 0.2f) * FMath::Min(1.0f, (1.8f - T) / 0.4f);
	}
	Normalize(B, 0.35f);
	return B;
}

FBuffer FTBSynth::Footsteps(float Seconds, int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(Seconds);
	FOnePole LP;
	const float Step = 0.29f;
	for (float T0 = 0.05f; T0 < Seconds - 0.1f; T0 += Step * R.FRandRange(0.92f, 1.08f))
	{
		const int32 Start = FMath::RoundToInt(T0 * SampleRate);
		const float Amp = R.FRandRange(0.6f, 1.0f);
		for (int32 i = 0; i < 4000 && Start + i < B.Num(); ++i)
		{
			const float T = TimeOf(i);
			const float Thud = FMath::Sin(UE_TWO_PI * 70.0f * T) * FMath::Exp(-T / 0.03f);
			const float Grass = LP.Process(R.FRandRange(-1, 1), 2400.0f) * FMath::Exp(-T / 0.05f) * 0.6f;
			B[Start + i] += (Thud + Grass) * Amp;
		}
	}
	// Breathing under the steps.
	FOnePole BL;
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i);
		const float Breath = 0.5f + 0.5f * FMath::Sin(UE_TWO_PI * 1.6f * T);
		B[i] += BL.Process(R.FRandRange(-1, 1), 1200.0f) * Breath * Breath * 0.12f;
	}
	Normalize(B, 0.45f);
	return B;
}

FBuffer FTBSynth::Arrival(int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(0.5f);
	FOnePole LP;
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i);
		B[i] = (FMath::Sin(UE_TWO_PI * 55.0f * T) * FMath::Exp(-T / 0.06f) + LP.Process(R.FRandRange(-1, 1), 1800.0f) * FMath::Exp(-T / 0.12f) * 0.8f);
	}
	Normalize(B, 0.55f);
	return B;
}

FBuffer FTBSynth::CountdownTick(bool bGo)
{
	FBuffer B = Make(bGo ? 0.9f : 0.18f);
	FOnePole LP;
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i);
		if (bGo)
		{
			// Field whistle blast: two detuned tones.
			const float V = FMath::Sin(UE_TWO_PI * 1480.0f * T) + 0.6f * FMath::Sin(UE_TWO_PI * 1497.0f * T) + 0.3f * FMath::Sin(UE_TWO_PI * 2960.0f * T);
			B[i] = LP.Process(V, 5000.0f) * FMath::Min(1.0f, T / 0.03f) * FMath::Min(1.0f, (0.9f - T) / 0.25f);
		}
		else
		{
			B[i] = FMath::Sin(UE_TWO_PI * 880.0f * T) * FMath::Exp(-T / 0.05f);
		}
	}
	Normalize(B, bGo ? 0.45f : 0.4f);
	return B;
}

namespace
{
	/** Soft brass-like chord: stacked saws through a low-pass with a swell. */
	void AddChord(FBuffer& B, float Start, float Len, const TArray<float>& Freqs, float Amp)
	{
		const int32 S = FMath::RoundToInt(Start * SampleRate);
		const int32 N = FMath::RoundToInt(Len * SampleRate);
		TArray<float> Phases;
		Phases.SetNumZeroed(Freqs.Num());
		FOnePole LP;
		for (int32 i = 0; i < N && S + i < B.Num(); ++i)
		{
			const float T = TimeOf(i);
			float V = 0.0f;
			for (int32 k = 0; k < Freqs.Num(); ++k)
			{
				Phases[k] += Freqs[k] * (1.0f + 0.003f * FMath::Sin(UE_TWO_PI * 5.0f * T + k)) / SampleRate;
				V += 2.0f * FMath::Frac(Phases[k]) - 1.0f;
			}
			const float Env = FMath::Min(1.0f, T / 0.08f) * FMath::Min(1.0f, (Len - T) / 0.35f);
			B[S + i] += LP.Process(V, 900.0f + 1400.0f * Env) * Env * Amp;
		}
	}
}

FBuffer FTBSynth::Victory()
{
	FBuffer B = Make(3.2f);
	AddChord(B, 0.0f, 0.6f, { 196.0f, 246.9f, 293.7f }, 0.3f);
	AddChord(B, 0.55f, 0.6f, { 220.0f, 277.2f, 329.6f }, 0.3f);
	AddChord(B, 1.1f, 2.1f, { 261.6f, 329.6f, 392.0f, 523.3f }, 0.32f);
	Normalize(B, 0.6f);
	return B;
}

FBuffer FTBSynth::Defeat()
{
	FBuffer B = Make(3.0f);
	AddChord(B, 0.0f, 1.0f, { 146.8f, 174.6f, 220.0f }, 0.3f);
	AddChord(B, 0.9f, 2.1f, { 110.0f, 130.8f, 164.8f }, 0.32f);
	Normalize(B, 0.55f);
	return B;
}

FBuffer FTBSynth::BinocularsUp(bool bUp)
{
	FBuffer B = Make(0.12f);
	FRandomStream R(bUp ? 21 : 22);
	FOnePole LP;
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i);
		B[i] = LP.Process(R.FRandRange(-1, 1), bUp ? 2400.0f : 1600.0f) * FMath::Exp(-T / 0.03f);
	}
	Normalize(B, 0.25f);
	return B;
}

FBuffer FTBSynth::KillConfirm()
{
	FBuffer B = Make(0.5f);
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i);
		const float F = T < 0.12f ? 660.0f : 990.0f;
		B[i] = FMath::Sin(UE_TWO_PI * F * T) * FMath::Exp(-FMath::Fmod(T, 0.12f) / 0.07f) * (T < 0.36f ? 1.0f : 0.0f);
	}
	Normalize(B, 0.28f);
	return B;
}

FBuffer FTBSynth::WindBed(float Seconds, int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(Seconds);
	FOnePole LP1, LP2;
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i);
		// Gusts: cutoff and level breathe slowly; period divides the loop length so it wraps cleanly.
		const float Gust = 0.5f + 0.5f * FMath::Sin(UE_TWO_PI * T * 2.0f / Seconds) * FMath::Sin(UE_TWO_PI * T * 3.0f / Seconds + 0.7f);
		B[i] = LP2.Process(LP1.Process(R.FRandRange(-1, 1), 300.0f + 500.0f * Gust), 600.0f) * (0.4f + 0.6f * Gust);
	}
	// Cross-fade the ends for a seamless loop.
	const int32 F = SampleRate / 2;
	for (int32 i = 0; i < F; ++i)
	{
		const float A = static_cast<float>(i) / F;
		B[i] = B[i] * A + B[B.Num() - F + i] * (1.0f - A);
	}
	B.SetNum(B.Num() - F);
	Normalize(B, 0.3f);
	return B;
}

FBuffer FTBSynth::EngineBed(float Seconds, int32 Seed)
{
	FRandomStream R(Seed);
	FBuffer B = Make(Seconds);
	FOnePole LP;
	float Phase = 0.0f;
	for (int32 i = 0; i < B.Num(); ++i)
	{
		const float T = TimeOf(i);
		Phase += (38.0f + 2.0f * FMath::Sin(UE_TWO_PI * T * 2.0f / Seconds)) / SampleRate;
		const float Firing = FMath::Pow(0.5f + 0.5f * FMath::Sin(UE_TWO_PI * Phase), 6.0f);
		B[i] = LP.Process(R.FRandRange(-1, 1) * Firing + 0.4f * FMath::Sin(UE_TWO_PI * Phase * 0.5f), 220.0f);
	}
	const int32 F = SampleRate / 2;
	for (int32 i = 0; i < F; ++i)
	{
		const float A = static_cast<float>(i) / F;
		B[i] = B[i] * A + B[B.Num() - F + i] * (1.0f - A);
	}
	B.SetNum(B.Num() - F);
	Normalize(B, 0.35f);
	return B;
}

// ---------------------------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------------------------

namespace
{
	TArray<int16> ToPCM(const FBuffer& Samples, float Volume)
	{
		TArray<int16> PCM;
		PCM.SetNumUninitialized(Samples.Num());
		for (int32 i = 0; i < Samples.Num(); ++i)
		{
			PCM[i] = static_cast<int16>(FMath::Clamp(Samples[i] * Volume, -1.0f, 1.0f) * 32000.0f);
		}
		return PCM;
	}

	UWorld* WorldOf(const UObject* Ctx)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
		return (World && World->GetAudioDeviceRaw()) ? World : nullptr;
	}
}

UAudioComponent* FTBSynth::Play(const UObject* WorldContext, const FBuffer& Samples, float Volume)
{
	UWorld* World = WorldOf(WorldContext);
	if (!World || Samples.Num() == 0)
	{
		return nullptr;
	}
	const TArray<int16> PCM = ToPCM(Samples, 1.0f);
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = static_cast<float>(Samples.Num()) / SampleRate;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()), PCM.Num() * sizeof(int16));
	UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(World, Wave, Volume, 1.0f, 0.0f, nullptr, false, true);
	if (!Comp)
	{
		return nullptr;
	}
	TWeakObjectPtr<UAudioComponent> Weak(Comp);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]() { if (Weak.IsValid()) { Weak->Stop(); } }), Wave->Duration + 0.1f, false);
	return Comp;
}

UAudioComponent* FTBSynth::PlayLoop(const UObject* WorldContext, const FBuffer& Samples, float Volume)
{
	UWorld* World = WorldOf(WorldContext);
	if (!World || Samples.Num() == 0)
	{
		return nullptr;
	}
	TSharedRef<TArray<int16>> PCM = MakeShared<TArray<int16>>(ToPCM(Samples, 1.0f));
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = INDEFINITELY_LOOPING_DURATION;
	Wave->SoundGroup = SOUNDGROUP_Default;
	Wave->bLooping = false;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM->GetData()), PCM->Num() * sizeof(int16));
	Wave->OnSoundWaveProceduralUnderflow.BindLambda([PCM](USoundWaveProcedural* InWave, int32 SamplesRequired)
	{
		InWave->QueueAudio(reinterpret_cast<const uint8*>(PCM->GetData()), PCM->Num() * sizeof(int16));
	});
	return UGameplayStatics::SpawnSound2D(World, Wave, Volume, 1.0f, 0.0f, nullptr, false, true);
}

bool FTBSynth::SaveWav(const FString& Path, const FBuffer& Samples)
{
	const TArray<int16> PCM = ToPCM(Samples, 1.0f);
	TArray<uint8> Out;
	auto U32 = [&Out](uint32 V) { Out.Append(reinterpret_cast<const uint8*>(&V), 4); };
	auto U16 = [&Out](uint16 V) { Out.Append(reinterpret_cast<const uint8*>(&V), 2); };
	const uint32 DataBytes = PCM.Num() * 2;
	Out.Append(reinterpret_cast<const uint8*>("RIFF"), 4); U32(36 + DataBytes);
	Out.Append(reinterpret_cast<const uint8*>("WAVE"), 4);
	Out.Append(reinterpret_cast<const uint8*>("fmt "), 4); U32(16); U16(1); U16(1); U32(SampleRate); U32(SampleRate * 2); U16(2); U16(16);
	Out.Append(reinterpret_cast<const uint8*>("data"), 4); U32(DataBytes);
	Out.Append(reinterpret_cast<const uint8*>(PCM.GetData()), DataBytes);
	return FFileHelper::SaveArrayToFile(Out, *Path);
}

void FTBSynth::ExportAll(const FString& Directory)
{
	IFileManager::Get().MakeDirectory(*Directory, true);
	const TArray<TPair<FString, FBuffer>> All = {
		{ TEXT("FTB_Boom_Near_700m"), Boom(700.0f, 0.0f, 1) },
		{ TEXT("FTB_Boom_Far_1500m"), Boom(1500.0f, 0.0f, 2) },
		{ TEXT("FTB_Boom_Smoke_1200m"), Boom(1200.0f, 0.8f, 3) },
		{ TEXT("FTB_ShellImpact_1000m"), Explosion(1000.0f, 4) },
		{ TEXT("FTB_CounterBattery_Close"), Explosion(40.0f, 5) },
		{ TEXT("FTB_CookOff_1100m"), CookOff(1100.0f, 6) },
		{ TEXT("FTB_Stopwatch_Start"), WatchClick(false) },
		{ TEXT("FTB_Stopwatch_Stop"), WatchClick(true) },
		{ TEXT("FTB_Radio_ShotOut"), RadioCall(7) },
		{ TEXT("FTB_Shell_Whistle"), Whistle(2.0f) },
		{ TEXT("FTB_Incoming_Shriek"), Incoming(1.6f) },
		{ TEXT("FTB_Radio_Interference"), Interference(8) },
		{ TEXT("FTB_Turret_Servo"), TurretServo() },
		{ TEXT("FTB_Footsteps_Run"), Footsteps(4.0f, 9) },
		{ TEXT("FTB_Arrival_Thud"), Arrival(10) },
		{ TEXT("FTB_Countdown_Tick"), CountdownTick(false) },
		{ TEXT("FTB_Countdown_Go"), CountdownTick(true) },
		{ TEXT("FTB_Victory"), Victory() },
		{ TEXT("FTB_Defeat"), Defeat() },
		{ TEXT("FTB_Binoculars_Up"), BinocularsUp(true) },
		{ TEXT("FTB_Kill_Confirm"), KillConfirm() },
		{ TEXT("FTB_Wind_Loop"), WindBed(12.0f, 11) },
		{ TEXT("FTB_Engine_Loop"), EngineBed(6.0f, 12) },
	};
	for (const TPair<FString, FBuffer>& Pair : All)
	{
		const FString File = Directory / (Pair.Key + TEXT(".wav"));
		UE_LOG(LogFTB, Log, TEXT("[FTB] Audio export %s %s"), *File, SaveWav(File, Pair.Value) ? TEXT("ok") : TEXT("FAILED"));
	}
}
