// Flash to Bang - one-shot world effects.

#include "FX/FTBEffectActor.h"
#include "FX/FTBBillboardEmitter.h"
#include "World/FTBArt.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

AFTBEffectActor::AFTBEffectActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
	SetCanBeDamaged(false);
}

AFTBEffectActor* AFTBEffectActor::Spawn(UWorld* World, EFTBEffect Kind, const FVector& Location, float InIntensity, int32 Seed)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFTBEffectActor* FX = World->SpawnActor<AFTBEffectActor>(AFTBEffectActor::StaticClass(), FTransform(Location), Params);
	if (FX)
	{
		FX->Configure(Kind, InIntensity, Seed);
	}
	return FX;
}

UFTBBillboardEmitter* AFTBEffectActor::AddEmitter(int32 MatIndex, const FFTBEmitterParams& Params, int32 Seed, int32 Burst)
{
	UFTBBillboardEmitter* E = NewObject<UFTBBillboardEmitter>(this);
	E->SetupAttachment(RootComponent);
	E->RegisterComponent();
	E->Setup(FTBArt::Mat(static_cast<EFTBMat>(MatIndex)), Params, Seed);
	E->Burst(Burst);
	Emitters.Add(E);
	return E;
}

void AFTBEffectActor::Configure(EFTBEffect InKind, float InIntensity, int32 Seed)
{
	Kind = InKind;
	Intensity = FMath::Clamp(InIntensity, 0.0f, 1.0f);

	FVector Camera = GetActorLocation() + FVector(-100000, 0, 0);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager) { Camera = PC->PlayerCameraManager->GetCameraLocation(); }
	}
	const float Distance = FVector::Dist(Camera, GetActorLocation());

	const bool bFlash = Kind == EFTBEffect::MuzzleFlash || Kind == EFTBEffect::CookOff;
	FlashSeconds = Kind == EFTBEffect::CookOff ? CookOffFlashSeconds : MuzzleFlashSeconds;
	FlareTint = Kind == EFTBEffect::CookOff ? FLinearColor(1.0f, 0.42f, 0.12f) : FLinearColor(1.0f, 0.93f, 0.78f);
	float AngleScale = Kind == EFTBEffect::CookOff ? 1.7f : 1.0f;
	if (Kind == EFTBEffect::ShellImpact || Kind == EFTBEffect::TankKill || Kind == EFTBEffect::CounterBattery)
	{
		FlareTint = FLinearColor(1.0f, 0.6f, 0.3f);
		AngleScale = Kind == EFTBEffect::TankKill ? 1.4f : 0.8f;
		FlashSeconds = Kind == EFTBEffect::TankKill ? 0.45f : 0.22f;
	}

	if (Kind != EFTBEffect::ArrivalDust && Intensity > 0.02f)
	{
		Flare = NewObject<UStaticMeshComponent>(this);
		Flare->SetStaticMesh(FTBArt::PlaneMesh());
		Flare->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Flare->SetCastShadow(false);
		Flare->SetupAttachment(RootComponent);
		Flare->RegisterComponent();
		FlareMID = UMaterialInstanceDynamic::Create(FTBArt::Mat(EFTBMat::Flare), Flare);
		Flare->SetMaterial(0, FlareMID);
		FlareBaseSize = FMath::Max(FlareMinSize, 2.0f * Distance * FMath::Tan(FMath::DegreesToRadians(FlareAngleDeg * AngleScale * 0.5f)));
		FlareMID->SetVectorParameterValue(TEXT("Tint"), FlareTint);
	}

	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0, 0, 250));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetCastShadows(false);
	Light->SetLightColor(FlareTint);
	Light->SetIntensity(0.0f);
	Light->RegisterComponent();
	LightPeak = (bFlash ? MuzzleLightCandela : ExplosionLightCandela) * Intensity;
	LightSeconds = FlashSeconds * 1.4f;
	Light->SetAttenuationRadius(Kind == EFTBEffect::ArrivalDust ? 10.0f : 9000.0f);
	if (Kind == EFTBEffect::ArrivalDust)
	{
		LightPeak = 0.0f;
	}

	FFTBEmitterParams P;
	switch (Kind)
	{
	case EFTBEffect::MuzzleFlash:
	case EFTBEffect::CounterBattery:
		if (Kind == EFTBEffect::MuzzleFlash)
		{
			// Muzzle blast smoke: a grey puff that lingers by the tank.
			P.Lifetime = 3.5f; P.SizeStart = 500.0f; P.SizeEnd = 1800.0f; P.Velocity = FVector(0, 0, 120); P.VelocityJitter = FVector(220, 220, 60);
			P.Drag = 0.6f; P.OpacityPeak = 0.55f * FMath::Max(Intensity, 0.4f); P.SpawnRadius = 200.0f; P.Tint = FLinearColor(0.62f, 0.64f, 0.66f); P.SpinDegPerSec = 14.0f;
			AddEmitter(static_cast<int32>(EFTBMat::Smoke), P, Seed, 4);
			MaxLife = 4.5f;
			break;
		}
		// Counter-battery shell landing: falls through to the impact recipe, bigger.
		[[fallthrough]];
	case EFTBEffect::ShellImpact:
	case EFTBEffect::TankKill:
	{
		const float Big = Kind == EFTBEffect::CounterBattery ? 1.25f : 1.0f;
		P = FFTBEmitterParams();
		P.Lifetime = 2.6f; P.SizeStart = 700.0f * Big; P.SizeEnd = 2600.0f * Big; P.Velocity = FVector(0, 0, 1300); P.VelocityJitter = FVector(500, 500, 400);
		P.Gravity = 900.0f; P.Drag = 0.4f; P.OpacityPeak = 0.9f; P.SpawnRadius = 250.0f; P.Tint = FLinearColor(0.42f, 0.34f, 0.26f); P.FadeIn = 0.05f;
		AddEmitter(static_cast<int32>(EFTBMat::Dirt), P, Seed, 10);
		P = FFTBEmitterParams();
		P.Lifetime = 6.0f; P.SizeStart = 1200.0f * Big; P.SizeEnd = 3600.0f * Big; P.Velocity = FVector(150, 0, 250); P.VelocityJitter = FVector(200, 200, 80);
		P.Drag = 0.3f; P.OpacityPeak = 0.7f; P.SpawnRadius = 300.0f; P.Tint = FLinearColor(0.5f, 0.5f, 0.52f); P.SpinDegPerSec = 10.0f;
		AddEmitter(static_cast<int32>(EFTBMat::Smoke), P, Seed + 1, 5);
		MaxLife = 7.0f;
		if (Kind == EFTBEffect::TankKill)
		{
			P = FFTBEmitterParams();
			P.Lifetime = 1.1f; P.SizeStart = 600.0f; P.SizeEnd = 2200.0f; P.Velocity = FVector(0, 0, 700); P.VelocityJitter = FVector(300, 300, 200);
			P.Drag = 1.0f; P.OpacityPeak = 1.0f; P.SpawnRadius = 150.0f; P.Emissive = 2.6f; P.FadeIn = 0.05f;
			AddEmitter(static_cast<int32>(EFTBMat::Fire), P, Seed + 2, 8);
			P = FFTBEmitterParams();
			P.Lifetime = 1.6f; P.SizeStart = 60.0f; P.SizeEnd = 20.0f; P.Velocity = FVector(0, 0, 1800); P.VelocityJitter = FVector(1100, 1100, 700);
			P.Gravity = 1400.0f; P.OpacityPeak = 1.0f; P.Emissive = 3.0f; P.FadeIn = 0.0f; P.SpawnRadius = 80.0f;
			AddEmitter(static_cast<int32>(EFTBMat::Spark), P, Seed + 3, 24);
		}
		break;
	}
	case EFTBEffect::CookOff:
		P.Lifetime = 1.4f; P.SizeStart = 50.0f; P.SizeEnd = 15.0f; P.Velocity = FVector(0, 0, 2200); P.VelocityJitter = FVector(900, 900, 800);
		P.Gravity = 1500.0f; P.OpacityPeak = 1.0f; P.Emissive = 3.2f; P.FadeIn = 0.0f; P.SpawnRadius = 60.0f;
		AddEmitter(static_cast<int32>(EFTBMat::Spark), P, Seed, 30);
		P = FFTBEmitterParams();
		P.Lifetime = 0.9f; P.SizeStart = 500.0f; P.SizeEnd = 1600.0f; P.Velocity = FVector(0, 0, 900); P.VelocityJitter = FVector(300, 300, 300);
		P.Drag = 1.2f; P.OpacityPeak = 1.0f; P.Emissive = 2.8f; P.FadeIn = 0.05f; P.SpawnRadius = 100.0f;
		AddEmitter(static_cast<int32>(EFTBMat::Fire), P, Seed + 1, 6);
		MaxLife = 2.5f;
		break;
	case EFTBEffect::ArrivalDust:
		P.Lifetime = 1.4f; P.SizeStart = 60.0f; P.SizeEnd = 220.0f; P.Velocity = FVector(0, 0, 40); P.VelocityJitter = FVector(120, 120, 20);
		P.Drag = 1.5f; P.OpacityPeak = 0.5f; P.SpawnRadius = 60.0f; P.Tint = FLinearColor(0.5f, 0.44f, 0.36f);
		AddEmitter(static_cast<int32>(EFTBMat::Dirt), P, Seed, 6);
		MaxLife = 1.8f;
		break;
	}
	SetActorTickEnabled(true);
	Tick(0.0f);
}

void AFTBEffectActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	FVector Camera = GetActorLocation() + FVector(-100000, 0, 0);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager) { Camera = PC->PlayerCameraManager->GetCameraLocation(); }
	}

	if (Flare)
	{
		const float T = Age / FMath::Max(FlashSeconds, 0.01f);
		float Alpha = T < 1.0f ? FMath::Square(1.0f - T) : 0.0f;
		if (Kind == EFTBEffect::CookOff)
		{
			Alpha *= 0.6f + 0.4f * FMath::Abs(FMath::Sin(Age * 38.0f));
		}
		const FVector ToCamera = (Camera - GetActorLocation()).GetSafeNormal();
		// Pull the glow toward the camera so terrain never cuts it in half.
		Flare->SetWorldLocationAndRotation(GetActorLocation() + FVector(0, 0, 250) + ToCamera * FlareBaseSize * 0.6f, FRotationMatrix::MakeFromZ(ToCamera).Rotator());
		Flare->SetWorldScale3D(FVector(FlareBaseSize * (0.7f + 0.3f * Alpha) / 100.0f));
		FlareMID->SetScalarParameterValue(TEXT("Opacity"), Alpha * Intensity);
		FlareMID->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Min(FlareEmissive, 3.5f));
		Flare->SetVisibility(Alpha > 0.001f);
	}
	if (Light)
	{
		const float T = Age / FMath::Max(LightSeconds, 0.01f);
		Light->SetIntensity(T < 1.0f ? LightPeak * FMath::Square(1.0f - T) : 0.0f);
	}

	bool bLive = Age < FMath::Max(FlashSeconds, LightSeconds);
	for (UFTBBillboardEmitter* E : Emitters)
	{
		bLive |= E && E->HasLiveParticles();
	}
	if (!bLive || Age > MaxLife)
	{
		Destroy();
	}
}
