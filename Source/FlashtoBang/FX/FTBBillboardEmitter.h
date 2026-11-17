// Flash to Bang - camera-facing flipbook particles on plane meshes (P37 pattern: our generated sheets, no VFX pack).
// Continuous (smoke columns, wreck fire, dust trails) or burst (impacts, flashes).

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FTBBillboardEmitter.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FFTBEmitterParams
{
	GENERATED_BODY()

	/** Particles per second while emitting (0 = burst only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Rate = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 MaxParticles = 24;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Lifetime = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LifetimeJitter = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SizeStart = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SizeEnd = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector Velocity = FVector(0, 0, 200);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector VelocityJitter = FVector(60, 60, 60);
	/** Velocity multiplier per second (drag). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Drag = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Gravity = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SpawnRadius = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float OpacityPeak = 0.8f;
	/** Fraction of life spent fading in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FadeIn = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FLinearColor Tint = FLinearColor::White;
	/** For additive materials; clamped to 3.5 by the material and here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Emissive = 1.0f;
	/** Fraction of the flipbook played over one life. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PhaseSpan = 1.0f;
	/** Random roll of each quad around the view axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bRandomRoll = true;
	/** Slow random spin of each quad (degrees per second, sign random): smoke turns over as it rises. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SpinDegPerSec = 0.0f;
};

UCLASS(ClassGroup = (FTB), meta = (BlueprintSpawnableComponent))
class FLASHTOBANG_API UFTBBillboardEmitter : public USceneComponent
{
	GENERATED_BODY()

public:
	UFTBBillboardEmitter();

	void Setup(UMaterialInterface* InMaterial, const FFTBEmitterParams& InParams, int32 InSeed);
	void SetEmitting(bool bOn) { bEmitting = bOn; }
	void SetRateScale(float Scale) { RateScale = Scale; }
	void Burst(int32 Count);
	bool HasLiveParticles() const;
	/** Extra wind applied to every particle (uu/s). */
	FVector Wind = FVector::ZeroVector;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FFTBEmitterParams Params;

private:
	struct FParticle
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		TObjectPtr<UMaterialInstanceDynamic> MID;
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.0f;
		float Life = 1.0f;
		float Roll = 0.0f;
		float Spin = 0.0f;
		float PhaseOffset = 0.0f;
		bool bAlive = false;
	};

	void SpawnOne();
	FParticle* FreeSlot();

	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Material;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> MeshRefs;
	TArray<FParticle> Particles;
	FRandomStream Rng;
	float SpawnAccumulator = 0.0f;
	float RateScale = 1.0f;
	bool bEmitting = false;
};
