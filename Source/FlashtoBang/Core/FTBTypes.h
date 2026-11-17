// Flash to Bang - shared enums and structs.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FTBTypes.generated.h"

UENUM(BlueprintType)
enum class EFTBState : uint8
{
	Ready,
	Active,
	Victory,
	Defeat
};

UENUM(BlueprintType)
enum class EFTBDefeatReason : uint8
{
	None,
	Bridge,
	PostHit
};

UENUM(BlueprintType)
enum class EFTBFlashKind : uint8
{
	/** A live tank firing its gun. */
	Muzzle,
	/** A burning wreck's ammunition going up (decoy). */
	CookOff,
	/** A tank firing at the observer's traced post. */
	CounterBattery
};

UENUM(BlueprintType)
enum class EFTBPostKind : uint8
{
	ChurchTower,
	Haystack,
	Ridge
};

/** One observation post on the hillside. Locations are metres on the valley map. */
USTRUCT(BlueprintType)
struct FFTBPostRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") EFTBPostKind Kind = EFTBPostKind::ChurchTower;
	/** Ground position (metres). Height comes from the terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") FVector2D LocationM = FVector2D::ZeroVector;
	/** Eye height above the terrain at this post (metres): tower top, top of the stack, lying on the ridge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float EyeHeightM = 2.0f;
	/** Default view yaw (degrees, 90 = north into the valley). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float ViewYaw = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float ViewPitch = -4.0f;
	/** Where a run arrives, relative to LocationM (metres): a ground point just outside the structure (ladder foot, tower door). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") FVector2D ApproachM = FVector2D(0.0f, -6.0f);
	/** Runs between other posts steer around this radius (metres) so the camera never passes through the structure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float ObstacleRadiusM = 6.0f;
};

/** A muzzle flash, cook-off or counter-battery shot as the world produced it. */
USTRUCT(BlueprintType)
struct FFTBFlashEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Flash") int32 Id = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Flash") EFTBFlashKind Kind = EFTBFlashKind::Muzzle;
	UPROPERTY(BlueprintReadOnly, Category = "Flash") FVector Location = FVector::ZeroVector;
	/** World time the light left the source (light arrives instantly). */
	UPROPERTY(BlueprintReadOnly, Category = "Flash") float FlashTime = 0.0f;
	/** World time the sound reaches the observer. */
	UPROPERTY(BlueprintReadOnly, Category = "Flash") float BoomTime = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Flash") float DistanceM = 0.0f;
	/** Brightness reaching the observer after smoke (1 = clear air). */
	UPROPERTY(BlueprintReadOnly, Category = "Flash") float Visibility = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Flash") bool bVisible = true;
	UPROPERTY(BlueprintReadOnly, Category = "Flash") TWeakObjectPtr<AActor> Source;
};

/** A plotted fix: where the stopwatch + bearing says the flash came from. */
USTRUCT(BlueprintType)
struct FFTBFix
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Fix") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Fix") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Fix") float DelaySeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Fix") float RangeM = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Fix") float BearingDeg = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Fix") float Time = 0.0f;
	/** When the timed flash happened (Time - DelaySeconds): the tank has been moving on since then. */
	UPROPERTY(BlueprintReadOnly, Category = "Fix") float FlashTime = 0.0f;
	/** True when the fix snapped onto the road (the column's only route). */
	UPROPERTY(BlueprintReadOnly, Category = "Fix") bool bOnRoad = false;
	UPROPERTY(BlueprintReadOnly, Category = "Fix") float RoadS = 0.0f;
	/** Where a tank seen at the fix will be when a shell called at plot time lands (it marches on: see LeadAt). */
	UPROPERTY(BlueprintReadOnly, Category = "Fix") FVector LeadLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Fix") float LeadSeconds = 10.0f;
};

/** A shell called onto a point. */
USTRUCT(BlueprintType)
struct FFTBShell
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shell") int32 Id = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Shell") FVector Target = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Shell") float CallTime = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Shell") float ImpactTime = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Shell") bool bWhistled = false;
};

/** Valley terrain shape (metres). One function feeds the mesh, the posts, the road and every fix. */
USTRUCT(BlueprintType)
struct FFTBTerrainParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float ValleyUndulationM = 2.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float HillLowM = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float HillHighM = 92.0f;
	/** Hill rises from HillFootY (0 m) to HillCrestY (full height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float HillFootY = 330.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float HillCrestY = -70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float BackdropStartY = 2050.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float BackdropHeightM = 140.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float RiverBaseY = 640.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float RiverWaveM = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float RiverWaveLengthM = 260.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float RiverHalfWidthM = 16.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float RiverDepthM = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain") float WaterLevelM = -1.6f;
};
