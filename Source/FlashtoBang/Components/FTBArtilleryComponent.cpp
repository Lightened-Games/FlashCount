// Flash to Bang - artillery battery.

#include "Components/FTBArtilleryComponent.h"
#include "FlashtoBang.h"
#include "Core/FTBWorld.h"

UFTBArtilleryComponent::UFTBArtilleryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool UFTBArtilleryComponent::CanCall() const
{
	return InFlight.Num() < MaxInFlight && GetReloadRemaining() <= 0.0f;
}

float UFTBArtilleryComponent::GetReloadRemaining() const
{
	return FMath::Max(0.0f, LastCallTime + ReloadSeconds - FTB::Now(this));
}

bool UFTBArtilleryComponent::Call(const FVector& Target)
{
	if (!CanCall())
	{
		return false;
	}
	const float Now = FTB::Now(this);
	FFTBShell Shell;
	Shell.Id = NextId++;
	Shell.Target = Target;
	Shell.CallTime = Now;
	Shell.ImpactTime = Now + FlightSeconds;
	InFlight.Add(Shell);
	LastCallTime = Now;
	UE_LOG(LogFTB, Log, TEXT("[FTB] Shell %d called at %s, lands %.1fs"), Shell.Id, *Target.ToCompactString(), Shell.ImpactTime);
	if (UFTBEventHub* Hub = FTB::Hub(this))
	{
		Hub->OnShellCalled.Broadcast(Shell);
	}
	OnCalledNative.Broadcast(Shell);
	return true;
}

void UFTBArtilleryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	IFTBWorldQuery* Q = FTB::Query(this);
	if (!Q)
	{
		return;
	}
	const float Now = FTB::Now(this);
	UFTBEventHub* Hub = Q->FTB_GetHub();
	for (int32 i = InFlight.Num() - 1; i >= 0; --i)
	{
		FFTBShell& Shell = InFlight[i];
		if (!Shell.bWhistled && Now >= Shell.ImpactTime - WhistleLeadSeconds)
		{
			Shell.bWhistled = true;
			if (Hub) { Hub->OnShellIncoming.Broadcast(Shell); }
		}
		if (Now >= Shell.ImpactTime)
		{
			const FFTBShell Landed = Shell;
			InFlight.RemoveAt(i);
			Q->FTB_ResolveShell(Landed.Target, KillRadiusM * 100.0f);
		}
	}
}
