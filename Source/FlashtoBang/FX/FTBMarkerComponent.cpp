// Flash to Bang - 3D world markers.

#include "FX/FTBMarkerComponent.h"
#include "World/FTBArt.h"
#include "Core/FTBWorld.h"
#include "Core/FTBRoundConfig.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

UFTBMarkerComponent::UFTBMarkerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UFTBMarkerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UFTBEventHub* Hub = FTB::Hub(this))
	{
		Hub->OnFixPlotted.AddDynamic(this, &UFTBMarkerComponent::HandleFix);
		Hub->OnShellCalled.AddDynamic(this, &UFTBMarkerComponent::HandleShellCalled);
		Hub->OnShellImpact.AddDynamic(this, &UFTBMarkerComponent::HandleImpact);
		Hub->OnStateChanged.AddDynamic(this, &UFTBMarkerComponent::HandleState);
	}
}

UFTBMarkerComponent::FMarker& UFTBMarkerComponent::Acquire()
{
	for (FMarker& M : Markers)
	{
		if (!M.bActive) { return M; }
	}
	FMarker& M = Markers.AddDefaulted_GetRef();
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(GetOwner());
	Mesh->SetStaticMesh(FTBArt::PlaneMesh());
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetUsingAbsoluteLocation(true);
	Mesh->SetUsingAbsoluteRotation(true);
	Mesh->SetUsingAbsoluteScale(true);
	Mesh->SetupAttachment(this);
	Mesh->RegisterComponent();
	M.MID = UMaterialInstanceDynamic::Create(FTBArt::Mat(EFTBMat::Marker), Mesh);
	Mesh->SetMaterial(0, M.MID);
	M.Mesh = Mesh;
	MeshRefs.Add(Mesh);
	return M;
}

void UFTBMarkerComponent::Place(FMarker& M, const FVector& Location, float RadiusUU, float Opacity, float Life, int32 ShellId)
{
	const float Now = FTB::Now(this);
	M.Location = Location + FVector(0, 0, 150);
	M.RadiusUU = RadiusUU;
	M.Opacity = Opacity;
	M.Born = Now;
	M.Expires = Now + Life;
	M.ShellId = ShellId;
	M.bShell = ShellId != INDEX_NONE;
	M.bLead = false;
	M.bActive = true;
	M.Mesh->SetVisibility(true);
	M.Mesh->SetWorldRotation(FRotator::ZeroRotator);
	M.MID->SetVectorParameterValue(TEXT("Color"), Accent);
	M.MID->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Min(MarkerEmissive, 3.5f));
}

void UFTBMarkerComponent::HandleFix(const FFTBFix& Fix)
{
	if (!Fix.bValid) { return; }
	// Drop older fix markers: one fix (and its lead) on screen at a time keeps the valley readable.
	for (FMarker& M : Markers)
	{
		if (M.bActive && !M.bShell) { M.bActive = false; M.Mesh->SetVisibility(false); }
	}
	Place(Acquire(), Fix.Location, FixRingRadiusM * 100.0f, 1.0f, FixLifetime, INDEX_NONE);
	if (Fix.bOnRoad)
	{
		FMarker& Lead = Acquire();
		Place(Lead, Fix.LeadLocation, LeadRingRadiusM * 100.0f, 0.6f, FixLifetime, INDEX_NONE);
		Lead.bLead = true;
		Lead.Fix = Fix;
	}
}

void UFTBMarkerComponent::HandleShellCalled(const FFTBShell& Shell)
{
	Place(Acquire(), Shell.Target, ShellRingRadiusM * 100.0f, 0.9f, Shell.ImpactTime - Shell.CallTime + 0.2f, Shell.Id);
	ShellImpactTimes.Add(Shell.Id, Shell.ImpactTime);
}

void UFTBMarkerComponent::HandleImpact(FVector Location, bool bKill)
{
	for (FMarker& M : Markers)
	{
		if (M.bActive && M.bShell && FVector::Dist2D(M.Location, Location) < 200.0f)
		{
			M.bActive = false;
			M.Mesh->SetVisibility(false);
		}
	}
}

void UFTBMarkerComponent::HandleState(EFTBState NewState, EFTBState OldState)
{
	if (NewState == EFTBState::Victory || NewState == EFTBState::Defeat)
	{
		for (FMarker& M : Markers)
		{
			M.bActive = false;
			if (M.Mesh) { M.Mesh->SetVisibility(false); }
		}
	}
}

void UFTBMarkerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float Now = FTB::Now(this);
	const FVector Eye = GetOwner()->GetActorLocation();
	for (FMarker& M : Markers)
	{
		if (!M.bActive) { continue; }
		if (Now >= M.Expires)
		{
			M.bActive = false;
			M.Mesh->SetVisibility(false);
			continue;
		}
		const float Age = Now - M.Born;
		const float Life = FMath::Max(M.Expires - M.Born, 0.1f);
		if (M.bLead)
		{
			const IFTBWorldQuery* Q = FTB::Query(this);
			const UFTBRoundConfig* Config = Q ? Q->FTB_GetConfig() : nullptr;
			if (Q && Config)
			{
				M.Location = Q->FTB_GetRoad().At(FTB::LeadS(M.Fix, Now, Config->TankSpeedMps * 100.0f)) + FVector(0, 0, 150);
			}
		}
		const float Distance = FVector::Dist(Eye, M.Location);
		const float MinRadius = Distance * FMath::Tan(FMath::DegreesToRadians(MinAngularSizeDeg * 0.5f));
		float Radius = FMath::Max(M.RadiusUU, MinRadius);
		float Opacity = M.Opacity * FMath::Clamp(Age / 0.18f, 0.0f, 1.0f);
		if (M.bShell)
		{
			// Ring tightens from 1.6x to the kill radius as the shell comes in, pulsing faster near impact.
			const float T = FMath::Clamp(Age / Life, 0.0f, 1.0f);
			Radius *= FMath::Lerp(1.6f, 1.0f, T);
			Opacity *= 0.65f + 0.35f * FMath::Abs(FMath::Sin(Age * (3.0f + 9.0f * T)));
		}
		else
		{
			Opacity *= 1.0f - FMath::Square(FMath::Clamp((Age - Life * 0.6f) / (Life * 0.4f), 0.0f, 1.0f));
		}
		M.Mesh->SetWorldLocation(M.Location);
		M.Mesh->SetWorldScale3D(FVector(Radius * 2.0f / 100.0f));
		M.MID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	}
}
