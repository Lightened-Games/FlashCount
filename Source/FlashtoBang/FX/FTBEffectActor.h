// Flash to Bang - one-shot world effects: muzzle flash, cook-off, shell impact, kill fireball, counter-battery blast, arrival dust.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FTBEffectActor.generated.h"

class UFTBBillboardEmitter;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EFTBEffect : uint8
{
	MuzzleFlash,
	CookOff,
	ShellImpact,
	TankKill,
	CounterBattery,
	ArrivalDust
};

UCLASS()
class FLASHTOBANG_API AFTBEffectActor : public AActor
{
	GENERATED_BODY()

public:
	AFTBEffectActor();

	/** Intensity 0..1 scales brightness (smoke-dimmed flashes). */
	static AFTBEffectActor* Spawn(UWorld* World, EFTBEffect Kind, const FVector& Location, float Intensity = 1.0f, int32 Seed = 0);

	virtual void Tick(float DeltaSeconds) override;

	/** Flash glow angular size (degrees) so a flash reads at any range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FlareAngleDeg = 1.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FlareMinSize = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MuzzleFlashSeconds = 0.16f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CookOffFlashSeconds = 0.6f;
	/** Candela of the flash light at peak (physical camera exposure is off: small units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MuzzleLightCandela = 9000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ExplosionLightCandela = 14000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FlareEmissive = 3.2f;

private:
	void Configure(EFTBEffect InKind, float InIntensity, int32 Seed);
	UFTBBillboardEmitter* AddEmitter(int32 MatIndex, const struct FFTBEmitterParams& Params, int32 Seed, int32 Burst);

	UPROPERTY(Transient) TArray<TObjectPtr<UFTBBillboardEmitter>> Emitters;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Flare;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> FlareMID;

	EFTBEffect Kind = EFTBEffect::MuzzleFlash;
	float Intensity = 1.0f;
	float Age = 0.0f;
	float FlashSeconds = 0.15f;
	float LightPeak = 0.0f;
	float LightSeconds = 0.15f;
	float FlareBaseSize = 1000.0f;
	float MaxLife = 6.0f;
	FLinearColor FlareTint = FLinearColor::White;
};
