// Flash to Bang - event hub. Gameplay broadcasts here; HUD, audio, markers, juice and the bot listen.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/FTBTypes.h"
#include "FTBEventHub.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFTBStateChanged, EFTBState, NewState, EFTBState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFTBCountdown, int32, SecondsLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFTBFlashSignal, const FFTBFlashEvent&, Flash);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFTBWatchSignal, bool, bRunning, float, DelaySeconds, float, RangeM);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFTBFixSignal, const FFTBFix&, Fix);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFTBShellSignal, const FFTBShell&, Shell);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFTBImpactSignal, FVector, Location, bool, bKill);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFTBKillSignal, FVector, Location, int32, Kills);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFTBTraceSignal, int32, PostIndex, float, Trace, bool, bTraced);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFTBCounterBatteryRequest, int32, PostIndex, bool, bLethal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFTBCounterBatteryImpact, FVector, Location, bool, bLethal, float, DistanceToObserverM);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFTBPostSignal, int32, PostIndex, bool, bRunning);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFTBInputSignal, FName, KeyId, bool, bSimulated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFTBPromptSignal, FName, PromptId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFTBFeelSignal, float, Strength, FLinearColor, Colour);

UCLASS()
class FLASHTOBANG_API UFTBEventHub : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable) FFTBStateChanged OnStateChanged;
	UPROPERTY(BlueprintAssignable) FFTBCountdown OnCountdown;
	/** Light reaches the observer. */
	UPROPERTY(BlueprintAssignable) FFTBFlashSignal OnFlash;
	/** Sound reaches the observer. */
	UPROPERTY(BlueprintAssignable) FFTBFlashSignal OnBoom;
	UPROPERTY(BlueprintAssignable) FFTBWatchSignal OnWatch;
	UPROPERTY(BlueprintAssignable) FFTBFixSignal OnFixPlotted;
	UPROPERTY(BlueprintAssignable) FFTBShellSignal OnShellCalled;
	/** Two seconds before impact: the whistle. */
	UPROPERTY(BlueprintAssignable) FFTBShellSignal OnShellIncoming;
	UPROPERTY(BlueprintAssignable) FFTBImpactSignal OnShellImpact;
	UPROPERTY(BlueprintAssignable) FFTBKillSignal OnTankKilled;
	UPROPERTY(BlueprintAssignable) FFTBTraceSignal OnTraceChanged;
	UPROPERTY(BlueprintAssignable) FFTBTraceSignal OnPostTraced;
	UPROPERTY(BlueprintAssignable) FFTBCounterBatteryRequest OnCounterBatteryRequested;
	UPROPERTY(BlueprintAssignable) FFTBCounterBatteryImpact OnCounterBatteryImpact;
	UPROPERTY(BlueprintAssignable) FFTBPostSignal OnPostChanged;
	/** Every player (or bot) input, so the legend row can light up. */
	UPROPERTY(BlueprintAssignable) FFTBInputSignal OnInput;
	/** Gameplay asks for a contextual hint (e.g. "Can't time while running"). */
	UPROPERTY(BlueprintAssignable) FFTBPromptSignal OnPrompt;
	/** Red edge vignette pulse request (strength 0..1). */
	UPROPERTY(BlueprintAssignable) FFTBFeelSignal OnVignette;
};
