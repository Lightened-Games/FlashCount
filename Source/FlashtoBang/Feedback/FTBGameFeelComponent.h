// Flash to Bang - game feel (P20): camera shakes scaled by distance, hit-stop, vignette requests, arrival dust.
// Listens to the event hub only.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Ticker.h"
#include "Core/FTBTypes.h"
#include "FTBGameFeelComponent.generated.h"

class UCameraShakeBase;

UCLASS(ClassGroup = (FTB), meta = (BlueprintSpawnableComponent))
class FLASHTOBANG_API UFTBGameFeelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFTBGameFeelComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** 50 ms freeze (global dilation), restored on real time. */
	void HitStop(float RealSeconds);
	void Shake(TSubclassOf<UCameraShakeBase> Shake, float Scale);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitStopSeconds = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitStopDilation = 0.05f;
	/** Boom shake falls off with distance: full at this range, nothing past 2.5x. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BoomShakeFullRangeM = 700.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BoomShakeScale = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CounterBatteryVignette = 1.0f;

private:
	UFUNCTION() void HandleBoom(const FFTBFlashEvent& Flash);
	UFUNCTION() void HandleImpact(FVector Location, bool bKill);
	UFUNCTION() void HandleCounterBattery(FVector Location, bool bLethal, float DistanceM);
	UFUNCTION() void HandlePost(int32 PostIndex, bool bRunning);
	UFUNCTION() void HandleKill(FVector Location, int32 Kills);

	FTSTicker::FDelegateHandle HitStopHandle;
	bool bBound = false;
};
