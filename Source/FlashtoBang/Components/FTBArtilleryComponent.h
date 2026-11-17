// Flash to Bang - the observer's artillery battery: a called shell lands FlightSeconds later at the called point.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/FTBTypes.h"
#include "FTBArtilleryComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FFTBOnShellCalledNative, const FFTBShell&);

UCLASS(ClassGroup = (FTB), meta = (BlueprintSpawnableComponent))
class FLASHTOBANG_API UFTBArtilleryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFTBArtilleryComponent();

	/** Returns false when the battery is reloading or already has MaxInFlight shells up. */
	bool Call(const FVector& Target);
	bool CanCall() const;
	float GetReloadRemaining() const;
	int32 NumInFlight() const { return InFlight.Num(); }
	const TArray<FFTBShell>& GetInFlight() const { return InFlight; }

	/** Owner-side hook (the pawn adds radio trace for every call). */
	FFTBOnShellCalledNative OnCalledNative;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (Units = "s")) float FlightSeconds = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 MaxInFlight = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (Units = "s")) float ReloadSeconds = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (Units = "m")) float KillRadiusM = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (Units = "s")) float WhistleLeadSeconds = 2.0f;

private:
	TArray<FFTBShell> InFlight;
	float LastCallTime = -100.0f;
	int32 NextId = 0;
};
