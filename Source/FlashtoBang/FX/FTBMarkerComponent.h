// Flash to Bang - 3D world markers (Pillar 3): amber rings in the valley for the fix, the lead point and every
// shell in flight (ring = kill radius, shrinking toward impact). Listens to the hub only.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Core/FTBTypes.h"
#include "FTBMarkerComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup = (FTB), meta = (BlueprintSpawnableComponent))
class FLASHTOBANG_API UFTBMarkerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFTBMarkerComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FLinearColor Accent = FLinearColor(0.75f, 0.42f, 0.09f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FixRingRadiusM = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LeadRingRadiusM = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ShellRingRadiusM = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FixLifetime = 9.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MarkerEmissive = 2.2f;
	/** Rings are drawn at least this many screen-degrees wide so far fixes stay visible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MinAngularSizeDeg = 0.9f;

private:
	struct FMarker
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		TObjectPtr<UMaterialInstanceDynamic> MID;
		FVector Location = FVector::ZeroVector;
		float RadiusUU = 1000.0f;
		float Born = 0.0f;
		float Expires = 0.0f;
		float Opacity = 1.0f;
		int32 ShellId = INDEX_NONE;
		bool bShell = false;
		bool bActive = false;
		/** Lead ring: slides along the road at column speed (where a shell called now would meet the tank). */
		bool bLead = false;
		FFTBFix Fix;
	};

	UFUNCTION() void HandleFix(const FFTBFix& Fix);
	UFUNCTION() void HandleShellCalled(const FFTBShell& Shell);
	UFUNCTION() void HandleImpact(FVector Location, bool bKill);
	UFUNCTION() void HandleState(EFTBState NewState, EFTBState OldState);

	FMarker& Acquire();
	void Place(FMarker& M, const FVector& Location, float RadiusUU, float Opacity, float Life, int32 ShellId);

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> MeshRefs;
	TArray<FMarker> Markers;
	TMap<int32, float> ShellImpactTimes;
};
