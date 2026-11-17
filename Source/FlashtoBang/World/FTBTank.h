// Flash to Bang - a column tank: drives the road, fires, slews its turret at a traced post, burns and cooks off when hit.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/FTBInterfaces.h"
#include "Core/FTBTypes.h"
#include "FTBTank.generated.h"

class UProceduralMeshComponent;
class UPointLightComponent;
class UFTBBillboardEmitter;
struct FFTBRoadPath;
struct FFTBMeshData;

UCLASS()
class FLASHTOBANG_API AFTBTank : public AActor, public IFTBFireSource, public IFTBShellTarget
{
	GENERATED_BODY()

public:
	AFTBTank();

	/** Road and terrain are owned by the game mode and outlive every tank. */
	void InitTank(const FFTBRoadPath* InRoad, const FFTBTerrainParams& InTerrain, float StartS, float SpeedUU, const FVector& IdleAimPoint, int32 Seed);
	void SetDriving(bool bOn) { bDriving = bOn; }

	virtual void Tick(float DeltaSeconds) override;

	// IFTBFireSource
	virtual bool FTB_IsAlive() const override { return !bWrecked; }
	virtual bool FTB_CanCookOff(float Now) const override;
	virtual FVector FTB_GetFlashOrigin(EFTBFlashKind Kind) const override;
	virtual void FTB_OnFired(EFTBFlashKind Kind, float Now) override;
	virtual void FTB_AimAt(const FVector& Target, bool bTraced) override;
	virtual float FTB_GetRoadS() const override { return RoadS; }
	virtual float FTB_GetSpeedUU() const override { return bWrecked ? 0.0f : SpeedUU; }
	virtual bool FTB_GetSmoke(FVector& OutCentre, float& OutRadiusUU) const override;

	// IFTBShellTarget
	virtual bool FTB_ApplyShellImpact(const FVector& Impact, float KillRadiusUU, float Now) override;

	/** Builds the tank parts in local space (also used by the asset-library glTF export). */
	static void BuildParts(FFTBMeshData& Hull, FFTBMeshData& Tracks, FFTBMeshData& Turret, FFTBMeshData& TurretDark, FFTBMeshData& Barrel);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TurretIdleSlewDegPerSec = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TurretTracedSlewDegPerSec = 32.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RecoilDistance = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RecoilRecoverSeconds = 0.6f;
	/** Smoke column: grows to this radius (metres) over SmokeGrowSeconds and drifts with the wind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SmokeMaxRadiusM = 78.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SmokeGrowSeconds = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector WindUU = FVector(260.0f, -180.0f, 0.0f);
	/** How far the smoke mass leans downwind (seconds of wind travel, capped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SmokeDriftSeconds = 24.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CookOffDelayMin = 7.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CookOffDelayMax = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 MaxCookOffs = 2;
	/** Extra kill reach for the tank's own size (metres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HullHalfLengthM = 3.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FireLightCandela = 2200.0f;
	/** Fog read-through shell (M_FTB_Silhouette): how far it sits outside the hull, in cm. 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SilhouetteInflateCm = 4.0f;

private:
	void UpdatePlacement();
	void BecomeWreck(float Now);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> HullMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> TurretPivot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> TurretMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> BarrelPivot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> BarrelMesh;
	UPROPERTY(Transient) TObjectPtr<UFTBBillboardEmitter> DustTrail;
	UPROPERTY(Transient) TObjectPtr<UFTBBillboardEmitter> SmokeColumn;
	UPROPERTY(Transient) TObjectPtr<UFTBBillboardEmitter> WreckFire;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FireLight;

	const FFTBRoadPath* Road = nullptr;
	FFTBTerrainParams Terrain;
	FVector IdleAim = FVector::ZeroVector;
	FVector AimTarget = FVector::ZeroVector;
	bool bTracedAim = false;
	float RoadS = 0.0f;
	float SpeedUU = 1100.0f;
	float TurretYaw = 0.0f;
	float RecoilT = 1.0f;
	float WreckTime = -1.0f;
	float NextCookOffTime = 0.0f;
	int32 CookOffs = 0;
	bool bWrecked = false;
	bool bDriving = false;
	FRandomStream Rng;
};
