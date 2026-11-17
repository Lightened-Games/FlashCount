// Flash to Bang - soundscape: every action and event gets a synthesised sound; impacts are heard after the
// real sound-travel delay, like the booms. Wind and column engine beds underneath. Listens to the hub only.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/FTBTypes.h"
#include "FTBSoundscapeComponent.generated.h"

class UAudioComponent;

UCLASS(ClassGroup = (FTB), meta = (BlueprintSpawnableComponent))
class FLASHTOBANG_API UFTBSoundscapeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFTBSoundscapeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MasterVolume = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WindVolume = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float EngineVolumeNear = 0.7f;
	/** Engine bed is silent beyond this column distance (metres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float EngineAudibleM = 1800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WhistleVolume = 0.5f;

private:
	UFUNCTION() void HandleBoom(const FFTBFlashEvent& Flash);
	UFUNCTION() void HandleWatch(bool bRunning, float Delay, float Range);
	UFUNCTION() void HandleShellCalled(const FFTBShell& Shell);
	UFUNCTION() void HandleShellIncoming(const FFTBShell& Shell);
	UFUNCTION() void HandleImpact(FVector Location, bool bKill);
	UFUNCTION() void HandleKill(FVector Location, int32 Kills);
	UFUNCTION() void HandleTraced(int32 PostIndex, float Trace, bool bTraced);
	UFUNCTION() void HandleCounterBatteryRequest(int32 PostIndex, bool bLethal);
	UFUNCTION() void HandleCounterBatteryImpact(FVector Location, bool bLethal, float DistanceM);
	UFUNCTION() void HandlePost(int32 PostIndex, bool bRunning);
	UFUNCTION() void HandleState(EFTBState NewState, EFTBState OldState);
	UFUNCTION() void HandleCountdown(int32 SecondsLeft);
	UFUNCTION() void HandleInput(FName KeyId, bool bSimulated);

	void PlayDelayed(float Delay, TFunction<void()> Fn);
	float Dist(const FVector& Location) const;

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> WindLoop;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> EngineLoop;
	int32 SoundSeed = 1;
	bool bBinoUp = false;
};
