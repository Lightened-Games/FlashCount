// Flash to Bang - autoplay bot (P22). Plays like a person: reacts to the flashes it can see, times them with
// reaction noise, aims at the marching lead ring, calls fire, and runs to a quieter post when its radio is traced.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/FTBTypes.h"
#include "FTBAutoplayComponent.generated.h"

class IFTBObserver;

UENUM()
enum class EFTBBotPhase : uint8
{
	Idle,
	TurnToFlash,
	Timing,
	Aiming,
	Running
};

UCLASS(ClassGroup = (FTB), meta = (BlueprintSpawnableComponent))
class FLASHTOBANG_API UFTBAutoplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFTBAutoplayComponent();

	void SetBotEnabled(bool bOn);
	bool IsBotEnabled() const { return bEnabled; }

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 0..1. Higher = tighter reaction timing and aim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Skill = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ReactionMean = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TimingSigmaLow = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TimingSigmaHigh = 0.035f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AimSigmaLowDeg = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AimSigmaHighDeg = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TurnDegPerSec = 170.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AimSettleSeconds = 0.35f;
	/** Ignore flashes this close to a wreck it watched die (probable cook-off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WreckIgnoreRadiusM = 50.0f;
	/** Skip flashes near a shell already in flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ShellOverlapRadiusM = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StuckSeconds = 10.0f;
	/** Raise the binoculars on the timed flash while waiting for the boom (a person studies the spot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bBinocularsWhileTiming = true;
	/** Time flashes even while both shells are in the air, then wait (tracking the lead) for a free gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bTimeWhileBatteryBusy = true;
	/** Give up a fix if no gun is free within this many seconds of aiming (it has gone stale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AimTimeoutSeconds = 7.0f;
	/** -FTBShowcase skill (steady hands, no fumbles) for the recorded take. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ShowcaseSkill = 0.95f;

private:
	UFUNCTION() void HandleFlash(const FFTBFlashEvent& Flash);
	UFUNCTION() void HandleBoom(const FFTBFlashEvent& Flash);
	UFUNCTION() void HandleFix(const FFTBFix& Fix);
	UFUNCTION() void HandleKill(FVector Location, int32 Kills);
	UFUNCTION() void HandleState(EFTBState NewState, EFTBState OldState);

	IFTBObserver* Observer() const;
	float Gauss(float Sigma);
	void SetPhase(EFTBBotPhase NewPhase);
	void DecideRelocation();
	int32 PickPost() const;

	bool bEnabled = false;
	bool bBound = false;
	bool bNeverRelocate = false;
	bool bPassive = false;
	bool bPausedByPlayer = false;
	EFTBBotPhase Phase = EFTBBotPhase::Idle;
	float PhaseTime = 0.0f;
	FFTBFlashEvent TargetFlash;
	float PressTime = 0.0f;
	float StopTime = -1.0f;
	FFTBFix CurrentFix;
	FVector2D AimOffsetDeg = FVector2D::ZeroVector;
	float AimHold = 0.0f;
	TArray<FVector> KnownWrecks;
	FRandomStream Rng;
	int32 FlashesSeen = 0;
	int32 FlashesTimed = 0;
};
