// Flash to Bang - camera shake presets.

#include "Feedback/FTBCameraShakes.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

namespace
{
	void Configure(UPerlinNoiseCameraShakePattern* P, float Duration, float Loc, float Rot, float Freq)
	{
		P->Duration = Duration;
		P->BlendInTime = 0.02f;
		P->BlendOutTime = Duration * 0.6f;
		P->LocationAmplitudeMultiplier = Loc;
		P->RotationAmplitudeMultiplier = Rot;
		P->X.Amplitude = 0.4f; P->X.Frequency = Freq;
		P->Y.Amplitude = 1.0f; P->Y.Frequency = Freq * 1.1f;
		P->Z.Amplitude = 1.0f; P->Z.Frequency = Freq * 0.9f;
		P->Pitch.Amplitude = 1.0f; P->Pitch.Frequency = Freq;
		P->Yaw.Amplitude = 0.5f; P->Yaw.Frequency = Freq * 1.2f;
		P->Roll.Amplitude = 0.7f; P->Roll.Frequency = Freq * 0.8f;
		P->FOV.Amplitude = 0.0f;
	}
}

UFTBShakeMicro::UFTBShakeMicro(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	UPerlinNoiseCameraShakePattern* P = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("Pattern"));
	Configure(P, 0.35f, 0.6f, 0.12f, 18.0f);
	SetRootShakePattern(P);
}

UFTBShakeImpact::UFTBShakeImpact(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	UPerlinNoiseCameraShakePattern* P = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("Pattern"));
	Configure(P, 0.5f, 2.0f, 0.35f, 16.0f);
	SetRootShakePattern(P);
}

UFTBShakeHeavy::UFTBShakeHeavy(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = true;
	UPerlinNoiseCameraShakePattern* P = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("Pattern"));
	Configure(P, 0.9f, 9.0f, 1.6f, 14.0f);
	SetRootShakePattern(P);
}
