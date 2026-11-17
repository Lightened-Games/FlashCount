// Flash to Bang - column tank.

#include "World/FTBTank.h"
#include "World/FTBArt.h"
#include "World/FTBMeshLib.h"
#include "Core/FTBTerrain.h"
#include "FX/FTBBillboardEmitter.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"

namespace
{
	// Local frames: hull origin on the ground at the tank centre, +X forward.
	const FVector TurretPivotLocal(-10.0f, 0.0f, 182.0f);
	const FVector BarrelPivotLocal(150.0f, 0.0f, 42.0f);
	const FVector MuzzleLocal(450.0f, 0.0f, 0.0f);
	const FRotator AlongX(-90.0f, 0.0f, 0.0f);   // rotates local Z onto +X
	const FRotator AlongY(0.0f, 0.0f, 90.0f);    // rotates local Z onto -Y/+Y (wheels)
}

AFTBTank::AFTBTank()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	HullMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Hull"));
	HullMesh->SetupAttachment(Root);
	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(Root);
	TurretPivot->SetRelativeLocation(TurretPivotLocal);
	TurretMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Turret"));
	TurretMesh->SetupAttachment(TurretPivot);
	BarrelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelPivot"));
	BarrelPivot->SetupAttachment(TurretPivot);
	BarrelPivot->SetRelativeLocation(BarrelPivotLocal);
	BarrelMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Barrel"));
	BarrelMesh->SetupAttachment(BarrelPivot);
	for (UProceduralMeshComponent* M : { HullMesh.Get(), TurretMesh.Get(), BarrelMesh.Get() })
	{
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->bUseAsyncCooking = true;
	}
	Tags.Add(TEXT("Tank"));
}

void AFTBTank::BuildParts(FFTBMeshData& Hull, FFTBMeshData& Tracks, FFTBMeshData& Turret, FFTBMeshData& TurretDark, FFTBMeshData& Barrel)
{
	using namespace FTBMesh;
	const float UV = 1.6f;
	// --- Hull (painted steel) ---
	AddBox(Hull, FTransform(FVector(-10, 0, 98)), FVector(292, 104, 44), 10.0f, UV);
	AddTapered(Hull, FTransform(FVector(-28, 0, 158)), FVector(262, 146, 18), FVector2D(0.97f, 0.94f), FVector2D::ZeroVector, UV);
	{
		// Sloped glacis from the deck down to the nose.
		const FVector G[8] = {
			FVector(234, -142, -50), FVector(338, -100, -50), FVector(338, 100, -50), FVector(234, 142, -50),
			FVector(234, -142, 18), FVector(250, -136, 18), FVector(250, 136, 18), FVector(234, 142, 18) };
		AddBlock(Hull, FTransform(FVector(0, 0, 158)), G, UV);
	}
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddBox(Hull, FTransform(FVector(0, S * 138, 141)), FVector(328, 30, 4), 1.5f, UV);           // fender
		AddBox(Hull, FTransform(FVector(70, S * 140, 156)), FVector(42, 18, 12), 3.0f, UV);           // tool box
		AddBox(Hull, FTransform(FVector(-120, S * 141, 152)), FVector(60, 14, 8), 2.0f, UV);          // pioneer tools
		AddBox(Hull, FTransform(FVector(-302, S * 70, 140)), FVector(10, 22, 30), 3.0f, UV);          // jerry cans
		AddCylinder(Hull, FTransform(FRotator(-90, 0, 0), FVector(330, S * 92, 150)), 9.0f, 5.0f, 10, true, -1.0f, UV); // headlights
	}
	// --- Tracks, wheels, exhausts, grills (dark steel) ---
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddBox(Tracks, FTransform(FVector(0, S * 124, 56)), FVector(334, 32, 52), 26.0f, UV);
		for (int32 W = 0; W < 5; ++W)
		{
			const float X = -216.0f + W * 108.0f;
			AddCylinder(Tracks, FTransform(AlongY, FVector(X, S * 158, 46)), 38.0f, 5.0f, 16, true, -1.0f, UV);
			AddCylinder(Tracks, FTransform(AlongY, FVector(X, S * 164, 46)), 12.0f, 3.0f, 8, true, -1.0f, UV);
		}
		AddCylinder(Tracks, FTransform(AlongY, FVector(300, S * 156, 74)), 32.0f, 6.0f, 14, true, -1.0f, UV);  // sprocket
		AddCylinder(Tracks, FTransform(AlongY, FVector(-300, S * 156, 70)), 30.0f, 6.0f, 14, true, -1.0f, UV); // idler
		AddCylinder(Tracks, FTransform(AlongX, FVector(-318, S * 46, 128)), 11.0f, 18.0f, 10, true, -1.0f, UV); // exhaust
		for (int32 L = 0; L < 3; ++L)
		{
			AddBox(Tracks, FTransform(FVector(292 - L * 20, S * 70, 150 + L * 22)), FVector(8, 30, 3), 1.0f, UV);   // spare links on glacis
		}
	}
	AddBox(Tracks, FTransform(FVector(-230, 0, 177)), FVector(52, 96, 3), 1.0f, UV);  // engine grill

	// --- Turret (local to the turret pivot) ---
	AddTapered(Turret, FTransform(FVector(0, 0, 44)), FVector(150, 118, 44), FVector2D(0.8f, 0.74f), FVector2D(-12, 0), UV);
	AddBox(Turret, FTransform(FVector(-158, 0, 42)), FVector(36, 92, 30), 6.0f, UV);           // bustle
	AddBox(Turret, FTransform(FVector(140, 0, 42)), FVector(22, 58, 34), 6.0f, UV);            // mantlet
	AddCylinder(Turret, FTransform(FVector(-40, 52, 98)), 30.0f, 12.0f, 18, true, 27.0f, UV);  // cupola
	AddCylinder(Turret, FTransform(FVector(-40, 52, 112)), 25.0f, 2.5f, 18, true, -1.0f, UV);  // hatch
	AddBox(Turret, FTransform(FVector(-60, -56, 92)), FVector(20, 18, 8), 3.0f, UV);           // loader hatch
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddBox(Turret, FTransform(FVector(-20, S * 112, 46)), FVector(70, 6, 18), 2.0f, UV);  // stowage
	}
	AddCylinder(TurretDark, FTransform(FVector(-118, -66, 210)), 1.5f, 120.0f, 4, false, -1.0f, UV);  // antenna
	AddCylinder(TurretDark, FTransform(FVector(-118, -66, 88)), 5.0f, 4.0f, 8, true, -1.0f, UV);
	AddBox(TurretDark, FTransform(FVector(60, 46, 92)), FVector(8, 6, 6), 1.0f, UV);              // periscope

	// --- Barrel (local to the barrel pivot, along +X) ---
	AddCylinder(Barrel, FTransform(AlongX, FVector(210, 0, 0)), 10.0f, 210.0f, 14, true, 8.5f, UV);
	AddCylinder(Barrel, FTransform(AlongX, FVector(170, 0, 0)), 14.0f, 26.0f, 14, true, -1.0f, UV);   // fume extractor
	AddCylinder(Barrel, FTransform(AlongX, FVector(428, 0, 0)), 15.0f, 16.0f, 14, true, -1.0f, UV);   // muzzle brake
	AddCylinder(Barrel, FTransform(AlongX, FVector(20, 0, 0)), 18.0f, 24.0f, 14, true, -1.0f, UV);    // collar
}

void AFTBTank::InitTank(const FFTBRoadPath* InRoad, const FFTBTerrainParams& InTerrain, float StartS, float InSpeedUU, const FVector& IdleAimPoint, int32 Seed)
{
	Road = InRoad;
	Terrain = InTerrain;
	RoadS = StartS;
	SpeedUU = InSpeedUU;
	IdleAim = IdleAimPoint;
	AimTarget = IdleAimPoint;
	Rng.Initialize(Seed);
	CookOffDelayMin = FMath::Max(1.0f, CookOffDelayMin);

	FFTBMeshData Hull, Tracks, Turret, TurretDark, Barrel;
	BuildParts(Hull, Tracks, Turret, TurretDark, Barrel);
	FTBMesh::Commit(HullMesh, 0, Hull, FTBArt::Mat(EFTBMat::Steel), false);
	FTBMesh::Commit(HullMesh, 1, Tracks, FTBArt::Mat(EFTBMat::SteelDark), false);
	FTBMesh::Commit(TurretMesh, 0, Turret, FTBArt::Mat(EFTBMat::Steel), false);
	FTBMesh::Commit(TurretMesh, 1, TurretDark, FTBArt::Mat(EFTBMat::SteelDark), false);
	FTBMesh::Commit(BarrelMesh, 0, Barrel, FTBArt::Mat(EFTBMat::Steel), false);
	if (SilhouetteInflateCm > 0.0f)
	{
		// Unfogged translucent shell: invisible up close, a dark rim-lit silhouette through the valley fog.
		FFTBMeshData HullAll = Hull;
		HullAll.Append(Tracks);
		FFTBMeshData TurretAll = Turret;
		TurretAll.Append(TurretDark);
		UMaterialInterface* Sil = FTBArt::Mat(EFTBMat::Silhouette);
		FTBMesh::Commit(HullMesh, 2, FTBMesh::Inflated(HullAll, SilhouetteInflateCm), Sil, false);
		FTBMesh::Commit(TurretMesh, 2, FTBMesh::Inflated(TurretAll, SilhouetteInflateCm), Sil, false);
		FTBMesh::Commit(BarrelMesh, 1, FTBMesh::Inflated(Barrel, SilhouetteInflateCm * 0.5f), Sil, false);
		for (UProceduralMeshComponent* M : { HullMesh.Get(), TurretMesh.Get(), BarrelMesh.Get() })
		{
			M->SetCastShadow(true);
			M->bCastHiddenShadow = false;
		}
	}

	// Dust kicked up by the tracks: brown, low, readable in binoculars.
	DustTrail = NewObject<UFTBBillboardEmitter>(this);
	DustTrail->SetupAttachment(Root);
	DustTrail->SetRelativeLocation(FVector(-340, 0, 40));
	DustTrail->RegisterComponent();
	FFTBEmitterParams Dust;
	Dust.Rate = 2.5f; Dust.MaxParticles = 14; Dust.Lifetime = 4.5f; Dust.SizeStart = 350.0f; Dust.SizeEnd = 1300.0f;
	Dust.Velocity = FVector(0, 0, 60); Dust.VelocityJitter = FVector(80, 80, 30); Dust.Drag = 0.5f; Dust.OpacityPeak = 0.42f;
	Dust.SpawnRadius = 120.0f; Dust.Tint = FLinearColor(0.62f, 0.56f, 0.46f); Dust.SpinDegPerSec = 12.0f;
	DustTrail->Setup(FTBArt::Mat(EFTBMat::Dirt), Dust, Seed + 11);
	DustTrail->Wind = WindUU * 0.5f;

	TurretYaw = 0.0f;
	UpdatePlacement();
}

void AFTBTank::UpdatePlacement()
{
	if (!Road || !Road->IsValid())
	{
		return;
	}
	FVector P = Road->At(RoadS);
	const FVector Dir = Road->DirAt(RoadS);
	const FVector Front = P + Dir * 300.0f;
	const FVector Back = P - Dir * 300.0f;
	const float ZF = FMath::Max(FTBTerrain::HeightUU(Terrain, Front), Road->At(RoadS + 300.0f).Z);
	const float ZB = FMath::Max(FTBTerrain::HeightUU(Terrain, Back), Road->At(RoadS - 300.0f).Z);
	P.Z = FMath::Max(P.Z, (ZF + ZB) * 0.5f) + 4.0f;
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(ZF - ZB, 600.0f));
	SetActorLocationAndRotation(P, FRotator(Pitch, Dir.Rotation().Yaw, 0.0f));
}

void AFTBTank::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();

	if (!bWrecked)
	{
		if (bDriving)
		{
			RoadS += SpeedUU * DeltaSeconds;
			UpdatePlacement();
		}
		if (DustTrail)
		{
			DustTrail->SetEmitting(bDriving);
		}

		// Turret: idle sweep around the town, or a hard slew toward a traced post.
		FVector Target = AimTarget;
		if (!bTracedAim)
		{
			Target = IdleAim + FVector(FMath::Sin(Now * 0.21f + RoadS * 0.0001f) * 25000.0f, 0.0f, 0.0f);
		}
		const FVector Pivot = TurretPivot->GetComponentLocation();
		const float WantWorldYaw = (Target - Pivot).Rotation().Yaw;
		const float WantLocal = FRotator::NormalizeAxis(WantWorldYaw - GetActorRotation().Yaw);
		const float MaxStep = (bTracedAim ? TurretTracedSlewDegPerSec : TurretIdleSlewDegPerSec) * DeltaSeconds;
		TurretYaw += FMath::Clamp(FRotator::NormalizeAxis(WantLocal - TurretYaw), -MaxStep, MaxStep);
		TurretPivot->SetRelativeRotation(FRotator(0.0f, TurretYaw, 0.0f));

		RecoilT = FMath::Min(1.0f, RecoilT + DeltaSeconds / FMath::Max(RecoilRecoverSeconds, 0.05f));
		const float Kick = (1.0f - RecoilT) * (1.0f - RecoilT);
		BarrelPivot->SetRelativeLocation(BarrelPivotLocal - FVector(RecoilDistance * Kick, 0, 0));
	}
	else
	{
		const float Age = Now - WreckTime;
		if (FireLight)
		{
			const float Flicker = 0.7f + 0.3f * FMath::Sin(Now * 17.0f) * FMath::Sin(Now * 6.3f + 1.0f);
			FireLight->SetIntensity(FireLightCandela * Flicker * FMath::Clamp(Age / 1.5f, 0.0f, 1.0f));
		}
		if (SmokeColumn)
		{
			SmokeColumn->SetRateScale(FMath::Clamp(0.4f + Age / SmokeGrowSeconds, 0.4f, 1.4f));
		}
	}
}

bool AFTBTank::FTB_CanCookOff(float Now) const
{
	return bWrecked && CookOffs < MaxCookOffs && Now >= NextCookOffTime;
}

FVector AFTBTank::FTB_GetFlashOrigin(EFTBFlashKind Kind) const
{
	if (Kind == EFTBFlashKind::CookOff || bWrecked)
	{
		return TurretPivot->GetComponentLocation() + FVector(0, 0, 80);
	}
	return BarrelMesh->GetComponentTransform().TransformPosition(MuzzleLocal);
}

void AFTBTank::FTB_OnFired(EFTBFlashKind Kind, float Now)
{
	if (Kind == EFTBFlashKind::CookOff)
	{
		++CookOffs;
		NextCookOffTime = Now + Rng.FRandRange(CookOffDelayMin, CookOffDelayMax);
		return;
	}
	RecoilT = 0.0f;
}

void AFTBTank::FTB_AimAt(const FVector& Target, bool bTraced)
{
	bTracedAim = bTraced;
	AimTarget = bTraced ? Target : IdleAim;
}

bool AFTBTank::FTB_GetSmoke(FVector& OutCentre, float& OutRadiusUU) const
{
	if (!bWrecked)
	{
		return false;
	}
	const float Age = GetWorld()->GetTimeSeconds() - WreckTime;
	const float Grow = FTBTerrain::Smooth(0.0f, SmokeGrowSeconds, Age);
	OutRadiusUU = SmokeMaxRadiusM * 100.0f * Grow;
	// The smoke mass leans downwind of the wreck.
	OutCentre = GetActorLocation() + WindUU * FMath::Min(Age, SmokeDriftSeconds) * 0.5f;
	return OutRadiusUU > 100.0f;
}

bool AFTBTank::FTB_ApplyShellImpact(const FVector& Impact, float KillRadiusUU, float Now)
{
	if (bWrecked)
	{
		return false;
	}
	const float D = FVector::Dist2D(Impact, GetActorLocation());
	if (D > KillRadiusUU + HullHalfLengthM * 100.0f)
	{
		return false;
	}
	BecomeWreck(Now);
	return true;
}

void AFTBTank::BecomeWreck(float Now)
{
	bWrecked = true;
	bDriving = false;
	WreckTime = Now;
	NextCookOffTime = Now + Rng.FRandRange(CookOffDelayMin, CookOffDelayMax);
	if (DustTrail)
	{
		DustTrail->SetEmitting(false);
	}
	UMaterialInterface* Burnt = FTBArt::Mat(EFTBMat::Wreck);
	UMaterialInterface* Sil = FTBArt::Mat(EFTBMat::Silhouette);
	for (UProceduralMeshComponent* M : { HullMesh.Get(), TurretMesh.Get(), BarrelMesh.Get() })
	{
		for (int32 S = 0; S < M->GetNumSections(); ++S)
		{
			if (Sil && M->GetMaterial(S) == Sil)
			{
				// The wreck burns and smokes: no fog read-through needed any more.
				M->SetMeshSectionVisible(S, false);
				continue;
			}
			M->SetMaterial(S, Burnt);
		}
	}
	// Turret blown off its ring: lifted, shoved aside and tilted, barrel drooping.
	TurretPivot->SetRelativeLocation(TurretPivotLocal + FVector(-30.0f, 70.0f, 10.0f));
	TurretPivot->SetRelativeRotation(FRotator(-4.0f, TurretYaw + 28.0f, 14.0f));
	BarrelPivot->SetRelativeRotation(FRotator(-9.0f, 0.0f, 0.0f));

	WreckFire = NewObject<UFTBBillboardEmitter>(this);
	WreckFire->SetupAttachment(Root);
	WreckFire->SetRelativeLocation(FVector(-120, 0, 190));
	WreckFire->RegisterComponent();
	FFTBEmitterParams Fire;
	Fire.Rate = 9.0f; Fire.MaxParticles = 16; Fire.Lifetime = 1.0f; Fire.SizeStart = 280.0f; Fire.SizeEnd = 520.0f;
	Fire.Velocity = FVector(0, 0, 260); Fire.VelocityJitter = FVector(60, 60, 60); Fire.OpacityPeak = 1.0f; Fire.Emissive = 2.4f;
	Fire.SpawnRadius = 110.0f; Fire.FadeIn = 0.1f;
	WreckFire->Setup(FTBArt::Mat(EFTBMat::Fire), Fire, Rng.RandHelper(9999));
	WreckFire->SetEmitting(true);
	WreckFire->Wind = WindUU * 0.3f;

	SmokeColumn = NewObject<UFTBBillboardEmitter>(this);
	SmokeColumn->SetupAttachment(Root);
	SmokeColumn->SetRelativeLocation(FVector(-60, 0, 300));
	SmokeColumn->RegisterComponent();
	FFTBEmitterParams Smoke;
	Smoke.Rate = 4.2f; Smoke.MaxParticles = 52; Smoke.Lifetime = 12.0f; Smoke.LifetimeJitter = 0.2f; Smoke.SizeStart = 800.0f; Smoke.SizeEnd = SmokeMaxRadiusM * 210.0f;
	Smoke.Velocity = FVector(0, 0, 520); Smoke.VelocityJitter = FVector(90, 90, 90); Smoke.Drag = 0.08f; Smoke.OpacityPeak = 0.88f;
	Smoke.SpawnRadius = 150.0f; Smoke.Tint = FLinearColor(0.16f, 0.16f, 0.165f); Smoke.FadeIn = 0.06f; Smoke.SpinDegPerSec = 9.0f; Smoke.PhaseSpan = 0.9f;
	SmokeColumn->Setup(FTBArt::Mat(EFTBMat::Smoke), Smoke, Rng.RandHelper(9999));
	SmokeColumn->SetEmitting(true);
	SmokeColumn->Wind = WindUU;

	FireLight = NewObject<UPointLightComponent>(this);
	FireLight->SetupAttachment(Root);
	FireLight->SetRelativeLocation(FVector(0, 0, 420));
	FireLight->SetIntensityUnits(ELightUnits::Candelas);
	FireLight->SetLightColor(FLinearColor(1.0f, 0.45f, 0.15f));
	FireLight->SetAttenuationRadius(3500.0f);
	FireLight->SetCastShadows(false);
	FireLight->SetIntensity(0.0f);
	FireLight->RegisterComponent();
	Tags.AddUnique(TEXT("Wreck"));
}
