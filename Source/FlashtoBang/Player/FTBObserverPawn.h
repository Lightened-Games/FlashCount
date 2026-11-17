// Flash to Bang - the spotter: first-person camera on one of three posts, binoculars, stopwatch, radio.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/FTBInterfaces.h"
#include "FTBObserverPawn.generated.h"

class UCameraComponent;
class UFTBRangingComponent;
class UFTBArtilleryComponent;
class UFTBRadioTraceComponent;
class UFTBGameFeelComponent;
class UFTBSoundscapeComponent;
class UFTBMarkerComponent;
struct FFTBShell;

UCLASS()
class FLASHTOBANG_API AFTBObserverPawn : public APawn, public IFTBObserver
{
	GENERATED_BODY()

public:
	AFTBObserverPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// IFTBObserver
	virtual void Obs_Look(float DeltaYaw, float DeltaPitch) override;
	virtual float Obs_TurnToward(const FVector& Target, float MaxDegPerSec, float DeltaTime) override;
	virtual FRotator Obs_GetView() const override { return FRotator(ViewPitch, ViewYaw, 0.0f); }
	virtual FVector Obs_GetEye() const override;
	virtual bool Obs_ToggleWatch() override;
	virtual bool Obs_IsWatchRunning() const override;
	virtual bool Obs_CallFire() override;
	virtual void Obs_SetBinoculars(bool bOn) override;
	virtual bool Obs_IsBinoculars() const override { return bBinoculars; }
	virtual bool Obs_RunToPost(int32 PostIndex) override;
	virtual int32 Obs_GetPost() const override { return CurrentPost; }
	virtual bool Obs_IsRunning() const override { return bRunning; }
	virtual float Obs_GetRunAlpha() const override;
	virtual bool Obs_GetAimPoint(FVector& OutPoint) const override;
	virtual float Obs_GetZoomAlpha() const override { return ZoomAlpha; }
	virtual float Obs_GetFadeAlpha() const override { return FadeAlpha; }

	UFTBRangingComponent* GetRanging() const { return Ranging; }
	UFTBArtilleryComponent* GetArtillery() const { return Artillery; }
	UFTBRadioTraceComponent* GetRadioTrace() const { return RadioTrace; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float NormalFOV = 72.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BinocularFOV = 13.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ZoomSeconds = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LookSensitivity = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PitchMin = -32.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PitchMax = 22.0f;
	/** How far the view may swing either side of the post's default yaw (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float YawArc = 110.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (Units = "m/s")) float RunSpeedMps = 18.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (Units = "m/s")) float ClimbSpeedMps = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HeadBobCm = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HeadBobHz = 3.4f;
	/** Camera pitch while dashing between posts and on arrival (degrees): level enough to see where you go. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RunPitchDash = -4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RunPitchArrive = -2.0f;
	/** Changing post: ease to black, hold, move, ease back in (real time, smoothstep). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FadeOutSeconds = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FadeHoldSeconds = 0.06f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FadeInSeconds = 0.32f;
	/** Extra clearance (metres) when a run steers around another post. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DetourClearanceM = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AimTraceRangeM = 4500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BinocularSway = 0.06f;

private:
	void SnapToPost(int32 PostIndex);
	void HandleShellCalled(const FFTBShell& Shell);
	void Broadcast(FName KeyId) const;
	bool CanAct() const;
	void ApplyView();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UFTBRangingComponent> Ranging;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UFTBArtilleryComponent> Artillery;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UFTBRadioTraceComponent> RadioTrace;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UFTBGameFeelComponent> GameFeel;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UFTBSoundscapeComponent> Soundscape;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UFTBMarkerComponent> Markers;

	float ViewYaw = 90.0f;
	float ViewPitch = -3.0f;
	float BaseYaw = 90.0f;
	bool bBinoculars = false;
	float ZoomAlpha = 0.0f;
	int32 CurrentPost = 0;
	int32 TargetPost = 0;
	bool bRunning = false;
	TArray<FVector> RunPath;
	TArray<float> RunTimes;
	float RunElapsed = 0.0f;
	float RunTotal = 1.0f;
	bool bPlaced = false;
	enum class EFTBMove : uint8 { None, Leaving, Running, Entering, Settling };
	EFTBMove Move = EFTBMove::None;
	float MoveTime = 0.0f;
	float FadeAlpha = 0.0f;
	void FinishEntering();
	/** Foot of FromPost -> around other posts -> foot of ToPost, following the terrain. */
	TArray<FVector> BuildRunPath(int32 FromPost, int32 ToPost) const;
	void LogRouteCheck() const;
};
