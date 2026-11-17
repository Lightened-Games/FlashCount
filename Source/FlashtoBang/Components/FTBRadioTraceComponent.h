// Flash to Bang - radio trace (new system N2): every call from a post adds trace; at the threshold the column
// works out where you are, swings its turrets, fires a ranging round, then a lethal one. Leave before it lands.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FTBRadioTraceComponent.generated.h"

UENUM(BlueprintType)
enum class EFTBTracePhase : uint8
{
	Quiet,
	Slewing,
	Ranging,
	Lethal
};

UCLASS(ClassGroup = (FTB), meta = (BlueprintSpawnableComponent))
class FLASHTOBANG_API UFTBRadioTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFTBRadioTraceComponent();

	void AddCall(int32 PostIndex);
	float GetTrace(int32 PostIndex) const;
	bool IsTraced(int32 PostIndex) const { return Phase != EFTBTracePhase::Quiet && TracedPost == PostIndex; }
	int32 GetTracedPost() const { return Phase != EFTBTracePhase::Quiet ? TracedPost : INDEX_NONE; }
	EFTBTracePhase GetPhase() const { return Phase; }
	/** Calls a post can take before it is traced (for the HUD chip: "1 more call"). */
	int32 GetCallsLeft(int32 PostIndex) const;
	void SetTraceEnabled(bool bOn) { bActive = bOn; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TracePerCall = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TraceThreshold = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DecayPerSecond = 0.04f;
	/** Seconds from traced to the ranging round being fired. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RangingDelay = 2.4f;
	/** Seconds from the ranging round to the lethal round being fired. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LethalDelay = 4.2f;
	/** Trace a post keeps after the column fired on it (they remember it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TraceAfterStrike = 1.0f;
	/** Time for the turrets to swing (the slewing phase), for the HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SlewSeconds = 2.0f;

private:
	void Broadcast(int32 PostIndex, bool bTracedNow);

	TArray<float> Trace;
	EFTBTracePhase Phase = EFTBTracePhase::Quiet;
	int32 TracedPost = INDEX_NONE;
	float PhaseTime = 0.0f;
	bool bActive = true;
};
