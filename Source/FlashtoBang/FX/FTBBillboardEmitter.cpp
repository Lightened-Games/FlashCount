// Flash to Bang - camera-facing flipbook particles.

#include "FX/FTBBillboardEmitter.h"
#include "World/FTBArt.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FName PhaseName(TEXT("Phase"));
	const FName OpacityName(TEXT("Opacity"));
	const FName TintName(TEXT("Tint"));
	const FName EmissiveName(TEXT("EmissiveStrength"));
	constexpr float MaxEmissive = 3.5f;
}

UFTBBillboardEmitter::UFTBBillboardEmitter()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UFTBBillboardEmitter::Setup(UMaterialInterface* InMaterial, const FFTBEmitterParams& InParams, int32 InSeed)
{
	Material = InMaterial;
	Params = InParams;
	Rng.Initialize(InSeed);
	Particles.SetNum(FMath::Max(1, Params.MaxParticles));
}

UFTBBillboardEmitter::FParticle* UFTBBillboardEmitter::FreeSlot()
{
	for (FParticle& P : Particles)
	{
		if (!P.bAlive)
		{
			return &P;
		}
	}
	return nullptr;
}

void UFTBBillboardEmitter::SpawnOne()
{
	FParticle* P = FreeSlot();
	if (!P || !Material || !GetOwner())
	{
		return;
	}
	if (!P->Mesh)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(GetOwner());
		Mesh->SetStaticMesh(FTBArt::PlaneMesh());
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Mesh->SetupAttachment(this);
		Mesh->RegisterComponent();
		P->MID = UMaterialInstanceDynamic::Create(Material, Mesh);
		Mesh->SetMaterial(0, P->MID);
		P->Mesh = Mesh;
		MeshRefs.Add(Mesh);
	}
	const FVector Origin = GetComponentLocation();
	const FVector Offset = Rng.GetUnitVector() * Rng.FRandRange(0.0f, Params.SpawnRadius);
	P->Position = Origin + FVector(Offset.X, Offset.Y, FMath::Abs(Offset.Z) * 0.5f);
	P->Velocity = Params.Velocity + FVector(Rng.FRandRange(-1, 1) * Params.VelocityJitter.X, Rng.FRandRange(-1, 1) * Params.VelocityJitter.Y, Rng.FRandRange(-1, 1) * Params.VelocityJitter.Z);
	P->Age = 0.0f;
	P->Life = FMath::Max(0.05f, Params.Lifetime * (1.0f + Rng.FRandRange(-Params.LifetimeJitter, Params.LifetimeJitter)));
	P->Roll = Params.bRandomRoll ? Rng.FRandRange(0.0f, 360.0f) : 0.0f;
	P->Spin = Params.SpinDegPerSec * Rng.FRandRange(0.6f, 1.0f) * (Rng.FRand() < 0.5f ? -1.0f : 1.0f);
	P->PhaseOffset = Rng.FRandRange(0.0f, FMath::Max(0.0f, 1.0f - Params.PhaseSpan));
	P->bAlive = true;
	P->Mesh->SetVisibility(true);
	P->MID->SetVectorParameterValue(TintName, Params.Tint);
	P->MID->SetScalarParameterValue(EmissiveName, FMath::Clamp(Params.Emissive, 0.0f, MaxEmissive));
}

void UFTBBillboardEmitter::Burst(int32 Count)
{
	for (int32 i = 0; i < Count; ++i)
	{
		SpawnOne();
	}
}

bool UFTBBillboardEmitter::HasLiveParticles() const
{
	for (const FParticle& P : Particles)
	{
		if (P.bAlive)
		{
			return true;
		}
	}
	return false;
}

void UFTBBillboardEmitter::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bEmitting && Params.Rate > 0.0f)
	{
		SpawnAccumulator += DeltaTime * Params.Rate * RateScale;
		while (SpawnAccumulator >= 1.0f)
		{
			SpawnAccumulator -= 1.0f;
			SpawnOne();
		}
	}

	FVector CameraLocation = GetComponentLocation() + FVector(-1000, 0, 0);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
		}
	}

	for (FParticle& P : Particles)
	{
		if (!P.bAlive)
		{
			continue;
		}
		P.Age += DeltaTime;
		const float T = P.Age / P.Life;
		if (T >= 1.0f)
		{
			P.bAlive = false;
			P.Mesh->SetVisibility(false);
			continue;
		}
		P.Velocity *= FMath::Max(0.0f, 1.0f - Params.Drag * DeltaTime);
		P.Velocity.Z -= Params.Gravity * DeltaTime;
		P.Position += (P.Velocity + Wind) * DeltaTime;
		P.Roll += P.Spin * DeltaTime;

		const float Size = FMath::Lerp(Params.SizeStart, Params.SizeEnd, 1.0f - FMath::Square(1.0f - T));
		const FVector ToCamera = (CameraLocation - P.Position).GetSafeNormal();
		FRotator Facing = FRotationMatrix::MakeFromZ(ToCamera).Rotator();
		const FQuat Rot = FQuat(ToCamera, FMath::DegreesToRadians(P.Roll)) * Facing.Quaternion();
		P.Mesh->SetWorldLocationAndRotation(P.Position, Rot);
		P.Mesh->SetWorldScale3D(FVector(Size / 100.0f));

		const float FadeIn = Params.FadeIn > 0.0f ? FMath::Clamp(T / Params.FadeIn, 0.0f, 1.0f) : 1.0f;
		const float FadeOut = 1.0f - FMath::Square(FMath::Clamp((T - 0.35f) / 0.65f, 0.0f, 1.0f));
		P.MID->SetScalarParameterValue(OpacityName, Params.OpacityPeak * FadeIn * FadeOut);
		P.MID->SetScalarParameterValue(PhaseName, FMath::Clamp(P.PhaseOffset + T * Params.PhaseSpan, 0.0f, 0.999f));
	}
}
