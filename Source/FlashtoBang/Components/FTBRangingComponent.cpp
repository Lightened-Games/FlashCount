// Flash to Bang - sound ranging.

#include "Components/FTBRangingComponent.h"
#include "FlashtoBang.h"
#include "Core/FTBRoundConfig.h"
#include "Core/FTBTerrain.h"
#include "Core/FTBWorld.h"

UFTBRangingComponent::UFTBRangingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UFTBRangingComponent::GetElapsed() const
{
	return bRunning ? FTB::Now(this) - StartTime : 0.0f;
}

void UFTBRangingComponent::Cancel()
{
	if (bRunning)
	{
		bRunning = false;
		if (UFTBEventHub* Hub = FTB::Hub(this))
		{
			Hub->OnWatch.Broadcast(false, 0.0f, 0.0f);
		}
	}
}

FVector UFTBRangingComponent::SolveFix(const FVector& Eye, const FVector& ViewDir, float SlantRangeUU, const FFTBTerrainParams& Terrain)
{
	const FVector Flat = ViewDir.GetSafeNormal2D();
	// The ground point P along the bearing such that |P - Eye| = slant range (iterate on the terrain height).
	float Horizontal = SlantRangeUU;
	FVector P = Eye + Flat * Horizontal;
	for (int32 Iter = 0; Iter < 4; ++Iter)
	{
		P.Z = FTBTerrain::HeightUU(Terrain, P);
		const float DZ = Eye.Z - P.Z;
		Horizontal = FMath::Sqrt(FMath::Max(SlantRangeUU * SlantRangeUU - DZ * DZ, 1.0f));
		P = Eye + Flat * Horizontal;
	}
	P.Z = FTBTerrain::HeightUU(Terrain, P);
	return P;
}

bool UFTBRangingComponent::Toggle(const FVector& Eye, const FVector& ViewDir)
{
	IFTBWorldQuery* Q = FTB::Query(this);
	UFTBEventHub* Hub = Q ? Q->FTB_GetHub() : nullptr;
	const UFTBRoundConfig* Config = Q ? Q->FTB_GetConfig() : nullptr;
	if (!Q || !Hub || !Config)
	{
		return false;
	}
	const float Now = FTB::Now(this);
	if (!bRunning)
	{
		bRunning = true;
		StartTime = Now;
		StartEye = Eye;
		StartDir = ViewDir.GetSafeNormal();
		Hub->OnWatch.Broadcast(true, 0.0f, 0.0f);
		return true;
	}

	bRunning = false;
	const float Delay = Now - StartTime;
	if (Delay < MinDelaySeconds)
	{
		Hub->OnWatch.Broadcast(false, Delay, 0.0f);
		Hub->OnPrompt.Broadcast(FTB::PromptTooShort);
		return true;
	}
	const float RangeM = FMath::Min(Delay * Config->SpeedOfSoundMps, MaxRangeM);
	FFTBFix Fix;
	Fix.bValid = true;
	Fix.DelaySeconds = Delay;
	Fix.RangeM = RangeM;
	Fix.Time = Now;
	Fix.FlashTime = StartTime;
	Fix.BearingDeg = FRotator::ClampAxis(90.0f - StartDir.Rotation().Yaw);
	Fix.Location = SolveFix(StartEye, StartDir, RangeM * 100.0f, Config->Terrain);
	Fix.LeadSeconds = LeadSeconds;

	const FFTBRoadPath& Road = Q->FTB_GetRoad();
	float RoadDist = 0.0f;
	const float S = Road.Project(Fix.Location, RoadDist);
	if (Road.IsValid() && RoadDist <= RoadSnapDistanceM * 100.0f)
	{
		Fix.bOnRoad = true;
		Fix.RoadS = S;
		Fix.Location = Road.At(S);
		Fix.LeadLocation = Road.At(FTB::LeadS(Fix, Now, Config->TankSpeedMps * 100.0f));
	}
	else
	{
		Fix.LeadLocation = Fix.Location;
	}
	LastFix = Fix;
	UE_LOG(LogFTB, Log, TEXT("[FTB] Fix delay=%.2fs range=%.0fm bearing=%.0f onRoad=%d"), Delay, RangeM, Fix.BearingDeg, Fix.bOnRoad);
	Hub->OnWatch.Broadcast(false, Delay, RangeM);
	Hub->OnFixPlotted.Broadcast(Fix);
	return true;
}
