// Flash to Bang - radio trace.

#include "Components/FTBRadioTraceComponent.h"
#include "FlashtoBang.h"
#include "Core/FTBWorld.h"

UFTBRadioTraceComponent::UFTBRadioTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

float UFTBRadioTraceComponent::GetTrace(int32 PostIndex) const
{
	return Trace.IsValidIndex(PostIndex) ? Trace[PostIndex] : 0.0f;
}

int32 UFTBRadioTraceComponent::GetCallsLeft(int32 PostIndex) const
{
	return FMath::Max(0, FMath::CeilToInt((TraceThreshold - GetTrace(PostIndex) - KINDA_SMALL_NUMBER) / FMath::Max(TracePerCall, 0.01f)));
}

void UFTBRadioTraceComponent::Broadcast(int32 PostIndex, bool bTracedNow)
{
	if (UFTBEventHub* Hub = FTB::Hub(this))
	{
		Hub->OnTraceChanged.Broadcast(PostIndex, GetTrace(PostIndex), IsTraced(PostIndex));
		if (bTracedNow)
		{
			Hub->OnPostTraced.Broadcast(PostIndex, GetTrace(PostIndex), true);
		}
	}
}

void UFTBRadioTraceComponent::AddCall(int32 PostIndex)
{
	if (PostIndex < 0)
	{
		return;
	}
	if (Trace.Num() <= PostIndex)
	{
		Trace.SetNumZeroed(PostIndex + 1);
	}
	Trace[PostIndex] += TracePerCall;
	bool bTracedNow = false;
	if (bActive && Phase == EFTBTracePhase::Quiet && Trace[PostIndex] >= TraceThreshold - KINDA_SMALL_NUMBER)
	{
		Phase = EFTBTracePhase::Slewing;
		TracedPost = PostIndex;
		PhaseTime = 0.0f;
		bTracedNow = true;
		UE_LOG(LogFTB, Log, TEXT("[FTB] Post %d traced (%.1f)"), PostIndex, Trace[PostIndex]);
	}
	Broadcast(PostIndex, bTracedNow);
}

void UFTBRadioTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bActive)
	{
		return;
	}
	for (int32 i = 0; i < Trace.Num(); ++i)
	{
		if (i == TracedPost && Phase != EFTBTracePhase::Quiet)
		{
			continue;
		}
		const float Before = Trace[i];
		Trace[i] = FMath::Max(0.0f, Trace[i] - DecayPerSecond * DeltaTime);
		if (FMath::FloorToInt(Before * 4.0f) != FMath::FloorToInt(Trace[i] * 4.0f))
		{
			Broadcast(i, false);
		}
	}

	if (Phase == EFTBTracePhase::Quiet)
	{
		return;
	}
	PhaseTime += DeltaTime;
	UFTBEventHub* Hub = FTB::Hub(this);
	if (Phase == EFTBTracePhase::Slewing && PhaseTime >= RangingDelay)
	{
		Phase = EFTBTracePhase::Ranging;
		PhaseTime = 0.0f;
		if (Hub) { Hub->OnCounterBatteryRequested.Broadcast(TracedPost, false); }
	}
	else if (Phase == EFTBTracePhase::Ranging && PhaseTime >= LethalDelay)
	{
		Phase = EFTBTracePhase::Lethal;
		PhaseTime = 0.0f;
		if (Hub) { Hub->OnCounterBatteryRequested.Broadcast(TracedPost, true); }
	}
	else if (Phase == EFTBTracePhase::Lethal && PhaseTime >= 4.0f)
	{
		// The strike is done (the game mode resolved it); the post stays half-known.
		const int32 Post = TracedPost;
		Phase = EFTBTracePhase::Quiet;
		TracedPost = INDEX_NONE;
		if (Trace.IsValidIndex(Post))
		{
			Trace[Post] = FMath::Min(Trace[Post], TraceAfterStrike);
		}
		Broadcast(Post, false);
	}
}
