// Flash to Bang - round data: posts, road, column, fire director. Edit DA_FTB_Round (or these defaults) to rebalance.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/FTBTypes.h"
#include "FTBRoundConfig.generated.h"

USTRUCT(BlueprintType)
struct FFTBTankRow
{
	GENERATED_BODY()

	/** Distance behind the lead tank along the road at GO (metres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float BehindLeadM = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank") float SpeedScale = 1.0f;
};

UCLASS(BlueprintType)
class FLASHTOBANG_API UFTBRoundConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFTBRoundConfig();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Round") TArray<FFTBPostRow> Posts;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Round") int32 StartPostIndex = 0;

	/** Road centre line, metres. The column drives it; the bridge starts at BridgeStartIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") TArray<FVector2D> RoadPointsM;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") int32 BridgeStartIndex = 9;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") float BridgeDeckHeightM = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column") TArray<FFTBTankRow> Tanks;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column") int32 KillsToWin = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column", meta = (Units = "m/s")) float TankSpeedMps = 11.0f;
	/** Seconds from GO until the lead tank reaches the bridge (sets where the column starts). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column", meta = (Units = "s")) float LeadEtaSeconds = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (Units = "m/s")) float SpeedOfSoundMps = 343.0f;

	/** Fire director: one shot at a time, the next one a short gap after the previous boom reaches the observer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire") float FirstShotDelay = 1.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire") float GapAfterBoomMin = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire") float GapAfterBoomMax = 2.0f;
	/** Wrecks start cooking off this long after they die, at most MaxCookOffsPerWreck times. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire") float CookOffDelayMin = 7.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire") float CookOffDelayMax = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire") int32 MaxCookOffsPerWreck = 2;
	/** Relative pick weight of a ready wreck versus a live tank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire") float CookOffWeight = 0.8f;

	/** Counter-battery: shell flight from the tank's flash to the post, and how short the ranging round falls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CounterBattery") float CounterBatteryFlightSeconds = 3.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CounterBattery") float RangingShortM = 48.0f;
	/** Lethal round kills the observer within this radius of the post (metres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CounterBattery") float LethalRadiusM = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") FFTBTerrainParams Terrain;
};
