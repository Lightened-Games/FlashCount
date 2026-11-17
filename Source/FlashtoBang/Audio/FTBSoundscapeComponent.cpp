// Flash to Bang - soundscape.

#include "Audio/FTBSoundscapeComponent.h"
#include "Audio/FTBSynth.h"
#include "Core/FTBRoundConfig.h"
#include "Core/FTBWorld.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UFTBSoundscapeComponent::UFTBSoundscapeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UFTBSoundscapeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UFTBEventHub* Hub = FTB::Hub(this))
	{
		Hub->OnBoom.AddDynamic(this, &UFTBSoundscapeComponent::HandleBoom);
		Hub->OnWatch.AddDynamic(this, &UFTBSoundscapeComponent::HandleWatch);
		Hub->OnShellCalled.AddDynamic(this, &UFTBSoundscapeComponent::HandleShellCalled);
		Hub->OnShellIncoming.AddDynamic(this, &UFTBSoundscapeComponent::HandleShellIncoming);
		Hub->OnShellImpact.AddDynamic(this, &UFTBSoundscapeComponent::HandleImpact);
		Hub->OnTankKilled.AddDynamic(this, &UFTBSoundscapeComponent::HandleKill);
		Hub->OnPostTraced.AddDynamic(this, &UFTBSoundscapeComponent::HandleTraced);
		Hub->OnCounterBatteryRequested.AddDynamic(this, &UFTBSoundscapeComponent::HandleCounterBatteryRequest);
		Hub->OnCounterBatteryImpact.AddDynamic(this, &UFTBSoundscapeComponent::HandleCounterBatteryImpact);
		Hub->OnPostChanged.AddDynamic(this, &UFTBSoundscapeComponent::HandlePost);
		Hub->OnStateChanged.AddDynamic(this, &UFTBSoundscapeComponent::HandleState);
		Hub->OnCountdown.AddDynamic(this, &UFTBSoundscapeComponent::HandleCountdown);
		Hub->OnInput.AddDynamic(this, &UFTBSoundscapeComponent::HandleInput);
	}
	WindLoop = FTBSynth::PlayLoop(this, FTBSynth::WindBed(12.0f, 11), WindVolume * MasterVolume);
	EngineLoop = FTBSynth::PlayLoop(this, FTBSynth::EngineBed(6.0f, 12), 0.0f);
}

void UFTBSoundscapeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	for (UAudioComponent* C : { WindLoop.Get(), EngineLoop.Get() })
	{
		if (C) { C->Stop(); }
	}
	Super::EndPlay(Reason);
}

void UFTBSoundscapeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (EngineLoop)
	{
		const IFTBWorldQuery* Q = FTB::Query(this);
		const float D = Q ? Q->FTB_GetColumnDistanceM() : 99999.0f;
		const bool bActive = Q && Q->FTB_GetState() == EFTBState::Active;
		const float Level = bActive ? FMath::Clamp(1.0f - D / EngineAudibleM, 0.0f, 1.0f) : 0.0f;
		EngineLoop->SetVolumeMultiplier(FMath::Max(0.001f, EngineVolumeNear * Level * Level * MasterVolume));
	}
}

float UFTBSoundscapeComponent::Dist(const FVector& Location) const
{
	return FVector::Dist(Location, GetOwner()->GetActorLocation()) * 0.01f;
}

void UFTBSoundscapeComponent::PlayDelayed(float Delay, TFunction<void()> Fn)
{
	if (Delay <= 0.01f)
	{
		Fn();
		return;
	}
	FTimerHandle Handle;
	TWeakObjectPtr<UFTBSoundscapeComponent> Weak(this);
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Fn]() { if (Weak.IsValid()) { Fn(); } }), Delay, false);
}

void UFTBSoundscapeComponent::HandleBoom(const FFTBFlashEvent& Flash)
{
	const int32 Seed = ++SoundSeed;
	if (Flash.Kind == EFTBFlashKind::CookOff)
	{
		FTBSynth::Play(this, FTBSynth::CookOff(Flash.DistanceM, Seed), MasterVolume);
		return;
	}
	FTBSynth::Play(this, FTBSynth::Boom(Flash.DistanceM, 1.0f - Flash.Visibility, Seed), MasterVolume);
}

void UFTBSoundscapeComponent::HandleWatch(bool bRunning, float Delay, float Range)
{
	FTBSynth::Play(this, FTBSynth::WatchClick(!bRunning), MasterVolume);
}

void UFTBSoundscapeComponent::HandleShellCalled(const FFTBShell& Shell)
{
	FTBSynth::Play(this, FTBSynth::RadioCall(++SoundSeed), MasterVolume * 0.9f);
}

void UFTBSoundscapeComponent::HandleShellIncoming(const FFTBShell& Shell)
{
	const float D = Dist(Shell.Target);
	FTBSynth::Play(this, FTBSynth::Whistle(2.0f), WhistleVolume * MasterVolume * FMath::Clamp(900.0f / FMath::Max(D, 300.0f), 0.25f, 1.0f));
}

void UFTBSoundscapeComponent::HandleImpact(FVector Location, bool bKill)
{
	const IFTBWorldQuery* Q = FTB::Query(this);
	const float Speed = Q && Q->FTB_GetConfig() ? Q->FTB_GetConfig()->SpeedOfSoundMps : 343.0f;
	const float D = Dist(Location);
	const int32 Seed = ++SoundSeed;
	TWeakObjectPtr<UFTBSoundscapeComponent> Weak(this);
	const float Volume = MasterVolume;
	PlayDelayed(D / Speed, [Weak, D, Seed, Volume, bKill]()
	{
		FTBSynth::Play(Weak.Get(), FTBSynth::Explosion(D, Seed), Volume);
		if (bKill)
		{
			FTBSynth::Play(Weak.Get(), FTBSynth::CookOff(D, Seed + 7), Volume * 0.7f);
		}
	});
}

void UFTBSoundscapeComponent::HandleKill(FVector Location, int32 Kills)
{
	FTBSynth::Play(this, FTBSynth::KillConfirm(), MasterVolume);
}

void UFTBSoundscapeComponent::HandleTraced(int32 PostIndex, float Trace, bool bTraced)
{
	FTBSynth::Play(this, FTBSynth::Interference(++SoundSeed), MasterVolume);
	TWeakObjectPtr<UFTBSoundscapeComponent> Weak(this);
	const float Volume = MasterVolume;
	PlayDelayed(0.6f, [Weak, Volume]() { FTBSynth::Play(Weak.Get(), FTBSynth::TurretServo(), Volume * 0.6f); });
}

void UFTBSoundscapeComponent::HandleCounterBatteryRequest(int32 PostIndex, bool bLethal)
{
	const IFTBWorldQuery* Q = FTB::Query(this);
	const float Flight = Q && Q->FTB_GetConfig() ? Q->FTB_GetConfig()->CounterBatteryFlightSeconds : 3.2f;
	TWeakObjectPtr<UFTBSoundscapeComponent> Weak(this);
	const float Volume = MasterVolume;
	PlayDelayed(FMath::Max(0.0f, Flight - 1.6f), [Weak, Volume]() { FTBSynth::Play(Weak.Get(), FTBSynth::Incoming(1.6f), Volume * 0.8f); });
}

void UFTBSoundscapeComponent::HandleCounterBatteryImpact(FVector Location, bool bLethal, float DistanceM)
{
	FTBSynth::Play(this, FTBSynth::Explosion(FMath::Max(DistanceM, 20.0f), ++SoundSeed), MasterVolume);
}

void UFTBSoundscapeComponent::HandlePost(int32 PostIndex, bool bRunning)
{
	if (bRunning)
	{
		FTBSynth::Play(this, FTBSynth::Footsteps(6.0f, ++SoundSeed), MasterVolume * 0.8f);
	}
	else
	{
		FTBSynth::Play(this, FTBSynth::Arrival(++SoundSeed), MasterVolume);
	}
}

void UFTBSoundscapeComponent::HandleState(EFTBState NewState, EFTBState OldState)
{
	if (NewState == EFTBState::Victory)
	{
		FTBSynth::Play(this, FTBSynth::Victory(), MasterVolume);
	}
	else if (NewState == EFTBState::Defeat)
	{
		FTBSynth::Play(this, FTBSynth::Defeat(), MasterVolume);
	}
}

void UFTBSoundscapeComponent::HandleCountdown(int32 SecondsLeft)
{
	FTBSynth::Play(this, FTBSynth::CountdownTick(SecondsLeft <= 0), MasterVolume);
}

void UFTBSoundscapeComponent::HandleInput(FName KeyId, bool bSimulated)
{
	if (KeyId == FTB::KeyBinoculars)
	{
		bBinoUp = !bBinoUp;
		FTBSynth::Play(this, FTBSynth::BinocularsUp(bBinoUp), MasterVolume);
	}
}
