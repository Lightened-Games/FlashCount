// Flash to Bang - interfaces. Systems talk through these, never through concrete game classes.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/FTBTypes.h"
#include "FTBInterfaces.generated.h"

class UFTBEventHub;
class UFTBRoundConfig;
struct FFTBRoadPath;

/** Anything that can make a flash and a boom: live tanks and burning wrecks. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFTBFireSource : public UInterface { GENERATED_BODY() };

class FLASHTOBANG_API IFTBFireSource
{
	GENERATED_BODY()
public:
	virtual bool FTB_IsAlive() const = 0;
	virtual bool FTB_CanCookOff(float Now) const = 0;
	virtual FVector FTB_GetFlashOrigin(EFTBFlashKind Kind) const = 0;
	/** Visual side of firing: recoil, cook-off sparks. The flash light itself is spawned by the director. */
	virtual void FTB_OnFired(EFTBFlashKind Kind, float Now) = 0;
	/** Turn the turret toward a point (the traced post) or back to its idle sweep (bTraced = false). */
	virtual void FTB_AimAt(const FVector& Target, bool bTraced) = 0;
	virtual float FTB_GetRoadS() const = 0;
	virtual float FTB_GetSpeedUU() const = 0;
	/** Smoke column (centre at ground, horizontal radius). False when there is no smoke. */
	virtual bool FTB_GetSmoke(FVector& OutCentre, float& OutRadiusUU) const = 0;
};

/** Anything a shell can hit. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFTBShellTarget : public UInterface { GENERATED_BODY() };

class FLASHTOBANG_API IFTBShellTarget
{
	GENERATED_BODY()
public:
	/** Returns true when this impact destroyed the target. */
	virtual bool FTB_ApplyShellImpact(const FVector& Impact, float KillRadiusUU, float Now) = 0;
};

/** The game mode's read-only world: terrain, road, rules, match state. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFTBWorldQuery : public UInterface { GENERATED_BODY() };

class FLASHTOBANG_API IFTBWorldQuery
{
	GENERATED_BODY()
public:
	virtual UFTBEventHub* FTB_GetHub() const = 0;
	virtual const UFTBRoundConfig* FTB_GetConfig() const = 0;
	virtual const FFTBRoadPath& FTB_GetRoad() const = 0;
	virtual float FTB_TerrainHeightUU(const FVector& Location) const = 0;
	virtual EFTBState FTB_GetState() const = 0;
	virtual float FTB_GetStateTime() const = 0;
	virtual float FTB_GetCountdownRemaining() const = 0;
	virtual float FTB_GetBridgeEta() const = 0;
	virtual int32 FTB_GetKills() const = 0;
	virtual int32 FTB_GetKillsToWin() const = 0;
	virtual int32 FTB_GetCalls() const = 0;
	virtual float FTB_GetBestFixErrorM() const = 0;
	virtual float FTB_GetActiveTime() const = 0;
	virtual EFTBDefeatReason FTB_GetDefeatReason() const = 0;
	virtual FVector FTB_GetPostEye(int32 PostIndex) const = 0;
	virtual FVector FTB_GetBridgeLocation() const = 0;
	virtual bool FTB_IsAutoplay() const = 0;
	/** Resolve a friendly shell landing at Impact. Returns true on a kill. */
	virtual bool FTB_ResolveShell(const FVector& Impact, float KillRadiusUU) = 0;
	/** Distance from the observer to the nearest live tank (metres), for the engine rumble bed. */
	virtual float FTB_GetColumnDistanceM() const = 0;
	virtual int32 FTB_GetPostCount() const = 0;
	virtual FText FTB_GetPostName(int32 PostIndex) const = 0;
	/** Live tank positions (for the bot's target choice and HUD); wrecks via FTB_GetWrecks. */
	virtual void FTB_GetLiveTanks(TArray<FVector>& OutLocations, TArray<float>& OutRoadS) const = 0;
	/** Wreck locations the observer has seen die (for markers and the bot). */
	virtual void FTB_GetWrecks(TArray<FVector>& Out) const = 0;
	virtual bool FTB_IsCountdownLocked() const = 0;
};

/** The spotter on the hill. The controller, HUD and autoplay bot drive it through this. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFTBObserver : public UInterface { GENERATED_BODY() };

class FLASHTOBANG_API IFTBObserver
{
	GENERATED_BODY()
public:
	virtual void Obs_Look(float DeltaYaw, float DeltaPitch) = 0;
	/** Turn the view toward a world point at up to MaxDegPerSec (bot). Returns remaining angle in degrees. */
	virtual float Obs_TurnToward(const FVector& Target, float MaxDegPerSec, float DeltaTime) = 0;
	virtual FRotator Obs_GetView() const = 0;
	virtual FVector Obs_GetEye() const = 0;
	/** Space: start or stop the stopwatch. Returns false when blocked (running, countdown). */
	virtual bool Obs_ToggleWatch() = 0;
	virtual bool Obs_IsWatchRunning() const = 0;
	/** Left click: call a shell onto the crosshair point. */
	virtual bool Obs_CallFire() = 0;
	virtual void Obs_SetBinoculars(bool bOn) = 0;
	virtual bool Obs_IsBinoculars() const = 0;
	virtual bool Obs_RunToPost(int32 PostIndex) = 0;
	virtual int32 Obs_GetPost() const = 0;
	virtual bool Obs_IsRunning() const = 0;
	/** 0..1 progress of the current run (1 when not running). */
	virtual float Obs_GetRunAlpha() const = 0;
	virtual bool Obs_GetAimPoint(FVector& OutPoint) const = 0;
	virtual float Obs_GetZoomAlpha() const = 0;
	/** Screen fade (0 clear .. 1 black) while changing posts; the HUD draws it. */
	virtual float Obs_GetFadeAlpha() const = 0;
};
