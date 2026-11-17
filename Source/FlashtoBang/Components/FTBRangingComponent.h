// Flash to Bang - sound ranging (new system N1): stopwatch between flash and boom, bearing from the view,
// fix projected onto the valley and snapped to the road, with a lead point one shell-flight further on.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/FTBTypes.h"
#include "FTBRangingComponent.generated.h"

UCLASS(ClassGroup = (FTB), meta = (BlueprintSpawnableComponent))
class FLASHTOBANG_API UFTBRangingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFTBRangingComponent();

	/** Space: start (records time + bearing) or stop (computes and broadcasts the fix). */
	bool Toggle(const FVector& Eye, const FVector& ViewDir);
	void Cancel();
	bool IsRunning() const { return bRunning; }
	float GetElapsed() const;
	const FFTBFix& GetLastFix() const { return LastFix; }

	/** Pure range maths, also used by the automation test: horizontal fix point from an eye, bearing and slant range. */
	static FVector SolveFix(const FVector& Eye, const FVector& ViewDir, float SlantRangeUU, const FFTBTerrainParams& Terrain);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RoadSnapDistanceM = 70.0f;
	/** Must match the artillery flight time: the lead point is where a tank at the fix will be when the shell lands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LeadSeconds = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MinDelaySeconds = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MaxRangeM = 3500.0f;

private:
	bool bRunning = false;
	float StartTime = 0.0f;
	FVector StartEye = FVector::ZeroVector;
	FVector StartDir = FVector::ForwardVector;
	FFTBFix LastFix;
};
