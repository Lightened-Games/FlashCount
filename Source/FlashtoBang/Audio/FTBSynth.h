// Flash to Bang - procedural audio. Every sound in the game is synthesised here at runtime; -FTBExportAudio writes
// the same buffers as WAVs into asset-library/Audio/Generated (original work, identical to what plays).

#pragma once

#include "CoreMinimal.h"

class UAudioComponent;

namespace FTBSynth
{
	constexpr int32 SampleRate = 44100;
	using FBuffer = TArray<float>;

	/** Distant gun report: low thump + rumble, high end and loudness fall with distance; Muffle (0..1) adds smoke damping. */
	FLASHTOBANG_API FBuffer Boom(float DistanceM, float Muffle, int32 Seed);
	/** Shell or counter-battery impact heard from DistanceM. */
	FLASHTOBANG_API FBuffer Explosion(float DistanceM, int32 Seed);
	FLASHTOBANG_API FBuffer CookOff(float DistanceM, int32 Seed);
	FLASHTOBANG_API FBuffer WatchClick(bool bStop);
	FLASHTOBANG_API FBuffer RadioCall(int32 Seed);
	FLASHTOBANG_API FBuffer Whistle(float Seconds);
	FLASHTOBANG_API FBuffer Incoming(float Seconds);
	FLASHTOBANG_API FBuffer Interference(int32 Seed);
	FLASHTOBANG_API FBuffer TurretServo();
	FLASHTOBANG_API FBuffer Footsteps(float Seconds, int32 Seed);
	FLASHTOBANG_API FBuffer Arrival(int32 Seed);
	FLASHTOBANG_API FBuffer CountdownTick(bool bGo);
	FLASHTOBANG_API FBuffer Victory();
	FLASHTOBANG_API FBuffer Defeat();
	FLASHTOBANG_API FBuffer BinocularsUp(bool bUp);
	FLASHTOBANG_API FBuffer KillConfirm();
	/** Loopable beds. */
	FLASHTOBANG_API FBuffer WindBed(float Seconds, int32 Seed);
	FLASHTOBANG_API FBuffer EngineBed(float Seconds, int32 Seed);

	/** One-shot 2D playback. Returns the component (auto-stops). */
	FLASHTOBANG_API UAudioComponent* Play(const UObject* WorldContext, const FBuffer& Samples, float Volume = 1.0f);
	/** Endless loop of Samples (re-queued on underflow). Volume can be changed on the returned component. */
	FLASHTOBANG_API UAudioComponent* PlayLoop(const UObject* WorldContext, const FBuffer& Samples, float Volume = 1.0f);

	FLASHTOBANG_API bool SaveWav(const FString& Path, const FBuffer& Samples);
	/** Writes every preset to Directory. */
	FLASHTOBANG_API void ExportAll(const FString& Directory);
}
