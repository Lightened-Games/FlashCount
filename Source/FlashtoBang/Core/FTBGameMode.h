// Flash to Bang - game mode: 4-state loop (Ready 3-2-1-GO / Active / Victory / Defeat), world build, fire director
// (one report in the air at a time), smoke occlusion, shell resolution, counter-battery, -autoplay and QA flags.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/FTBInterfaces.h"
#include "Core/FTBTerrain.h"
#include "Core/FTBTypes.h"
#include "FTBGameMode.generated.h"

class UFTBEventHub;
class UFTBRoundConfig;
class AFTBWorldBuilder;
class AFTBTank;

UCLASS()
class FLASHTOBANG_API AFTBGameMode : public AGameModeBase, public IFTBWorldQuery
{
	GENERATED_BODY()

public:
	AFTBGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// IFTBWorldQuery
	virtual UFTBEventHub* FTB_GetHub() const override { return Hub; }
	virtual const UFTBRoundConfig* FTB_GetConfig() const override { return Config; }
	virtual const FFTBRoadPath& FTB_GetRoad() const override { return Road; }
	virtual float FTB_TerrainHeightUU(const FVector& Location) const override;
	virtual EFTBState FTB_GetState() const override { return State; }
	virtual float FTB_GetStateTime() const override { return StateTime; }
	virtual float FTB_GetCountdownRemaining() const override;
	virtual float FTB_GetBridgeEta() const override;
	virtual int32 FTB_GetKills() const override { return Kills; }
	virtual int32 FTB_GetKillsToWin() const override;
	virtual int32 FTB_GetCalls() const override { return Calls; }
	virtual float FTB_GetBestFixErrorM() const override { return BestFixErrorM; }
	virtual float FTB_GetActiveTime() const override { return ActiveTime; }
	virtual EFTBDefeatReason FTB_GetDefeatReason() const override { return DefeatReason; }
	virtual FVector FTB_GetPostEye(int32 PostIndex) const override;
	virtual FVector FTB_GetBridgeLocation() const override;
	virtual bool FTB_IsAutoplay() const override { return bAutoplayActive; }
	virtual bool FTB_ResolveShell(const FVector& Impact, float KillRadiusUU) override;
	virtual void FTB_GetWrecks(TArray<FVector>& Out) const override { Out = Wrecks; }
	virtual bool FTB_IsCountdownLocked() const override { return State != EFTBState::Active; }
	virtual float FTB_GetColumnDistanceM() const override;
	virtual int32 FTB_GetPostCount() const override;
	virtual FText FTB_GetPostName(int32 PostIndex) const override;
	virtual void FTB_GetLiveTanks(TArray<FVector>& OutLocations, TArray<float>& OutRoadS) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") TSoftObjectPtr<UFTBRoundConfig> RoundConfigAsset;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CountdownSeconds = 3.0f;
	/** Ready state starts with this much calm before 3-2-1 (objective banner time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IntroSeconds = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WorldKillZ = -2000.0f;
	/** Flash brightness lost per smoke column crossed (multiplier) and the visibility below which it is not seen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FlashOcclusionPerColumn = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FlashHiddenBelow = 0.3f;
	/** Each burning wreck thickens the valley haze: every flash loses this fraction of its brightness per wreck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HazePerWreck = 0.16f;
	/** Chance per burning wreck that a drift of its smoke crosses the line of sight (flash dimmed by SmokeDriftDim). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SmokeDriftChancePerWreck = 0.14f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SmokeDriftDim = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float EndScreenAutoRestartSeconds = 6.0f;
	/** -FTBShowcase: seconds the victory screen stays up before the recording quits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ShowcaseEndHoldSeconds = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 WorldSeed = 1337;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") TSubclassOf<AFTBTank> TankClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") bool bAutoplayActive = false;

private:
	void LoadConfig();
	void BuildRoad();
	void BuildWorld();
	void SpawnColumn();
	void CheckCommandLineAutoplay();
	void SetupAutoplayController();
	void SetState(EFTBState NewState);
	void TickReady();
	void TickActive(float DeltaSeconds);
	void TickEnd();
	void TickFireDirector(float Now);
	void Fire(AActor* Source, EFTBFlashKind Kind, float Now);
	float SmokeVisibility(const FVector& From, const FVector& To, const AActor* IgnoreSource);
	FVector ObserverEye() const;
	void Defeat(EFTBDefeatReason Reason);
	void Victory();
	void RequestRestart();
	void TickShots();
	void Shot(const FString& Tag);
	void LogResult() const;

	UFUNCTION() void HandleCounterBatteryRequest(int32 PostIndex, bool bLethal);
	UFUNCTION() void HandlePostTraced(int32 PostIndex, float Trace, bool bTraced);
	UFUNCTION() void HandleShellCalled(const FFTBShell& Shell);
	UFUNCTION() void HandleFix(const FFTBFix& Fix);
	UFUNCTION() void HandlePost(int32 PostIndex, bool bRunning);
	/** FactoryKit: the start screen and intro are done; run the countdown. */
	UFUNCTION() void HandleFlowStart();
	void BeginFlow();

	struct FPendingStrike
	{
		int32 Post = 0;
		bool bLethal = false;
		bool bFired = false;
		float LandTime = 0.0f;
		FVector Impact = FVector::ZeroVector;
	};

	UPROPERTY(Transient) TObjectPtr<UFTBEventHub> Hub;
	UPROPERTY(Transient) TObjectPtr<UFTBRoundConfig> Config;
	UPROPERTY(Transient) TObjectPtr<AFTBWorldBuilder> Builder;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> Tanks;

	FFTBRoadPath Road;
	EFTBState State = EFTBState::Ready;
	EFTBDefeatReason DefeatReason = EFTBDefeatReason::None;
	float StateTime = 0.0f;
	float ActiveTime = 0.0f;
	int32 LastCountdown = -1;
	int32 Kills = 0;
	int32 Calls = 0;
	float BestFixErrorM = -1.0f;
	float SumFixErrorM = 0.0f;
	int32 Fixes = 0;
	TArray<FVector> Wrecks;
	TArray<FFTBFlashEvent> PendingBooms;
	TArray<FFTBFlashEvent> RecentFlashes;
	TArray<FPendingStrike> Strikes;
	float NextShotTime = 0.0f;
	int32 NextFlashId = 0;
	FRandomStream Rng;
	bool bAutoplayExit = false;
	/** The countdown waits here until the start screen + intro finish (FactoryKit). */
	bool bFlowBegun = false;
	bool bWaitingForFlow = true;
	bool bShots = false;
	int32 RunsWanted = 1;
	TSet<FString> ShotsTaken;
	int32 ShotIndex = 0;
	float NextTimedShot = 0.0f;
	int32 HiddenFlashes = 0;
	int32 CookOffFlashes = 0;
	int32 TracedCount = 0;
	int32 Relocations = 0;
	int32 ArrivedPost = 0;
	float ArriveTime = -1.0f;
};
