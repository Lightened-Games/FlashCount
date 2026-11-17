// Flash to Bang - game feel.

#include "Feedback/FTBGameFeelComponent.h"
#include "Feedback/FTBCameraShakes.h"
#include "FX/FTBEffectActor.h"
#include "Core/FTBWorld.h"
#include "Containers/Ticker.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UFTBGameFeelComponent::UFTBGameFeelComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFTBGameFeelComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UFTBEventHub* Hub = FTB::Hub(this))
	{
		Hub->OnBoom.AddDynamic(this, &UFTBGameFeelComponent::HandleBoom);
		Hub->OnShellImpact.AddDynamic(this, &UFTBGameFeelComponent::HandleImpact);
		Hub->OnCounterBatteryImpact.AddDynamic(this, &UFTBGameFeelComponent::HandleCounterBattery);
		Hub->OnPostChanged.AddDynamic(this, &UFTBGameFeelComponent::HandlePost);
		Hub->OnTankKilled.AddDynamic(this, &UFTBGameFeelComponent::HandleKill);
		bBound = true;
	}
}

void UFTBGameFeelComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HitStopHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HitStopHandle);
		HitStopHandle.Reset();
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	}
	Super::EndPlay(Reason);
}

void UFTBGameFeelComponent::Shake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && Scale > 0.01f)
	{
		PC->ClientStartCameraShake(ShakeClass, Scale);
	}
}

void UFTBGameFeelComponent::HitStop(float RealSeconds)
{
	if (HitStopHandle.IsValid())
	{
		return;
	}
	UGameplayStatics::SetGlobalTimeDilation(this, HitStopDilation);
	TWeakObjectPtr<UFTBGameFeelComponent> Weak(this);
	HitStopHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
	{
		if (Weak.IsValid())
		{
			UGameplayStatics::SetGlobalTimeDilation(Weak.Get(), 1.0f);
			Weak->HitStopHandle.Reset();
		}
		return false;
	}), RealSeconds);
}

void UFTBGameFeelComponent::HandleBoom(const FFTBFlashEvent& Flash)
{
	const float Falloff = 1.0f - FMath::Clamp((Flash.DistanceM - BoomShakeFullRangeM) / (BoomShakeFullRangeM * 1.5f), 0.0f, 1.0f);
	Shake(UFTBShakeMicro::StaticClass(), BoomShakeScale * Falloff);
}

void UFTBGameFeelComponent::HandleImpact(FVector Location, bool bKill)
{
	const float D = FVector::Dist(Location, GetOwner()->GetActorLocation()) * 0.01f;
	Shake(UFTBShakeMicro::StaticClass(), FMath::Clamp(900.0f / FMath::Max(D, 1.0f), 0.0f, 1.0f) * (bKill ? 1.0f : 0.6f));
}

void UFTBGameFeelComponent::HandleKill(FVector Location, int32 Kills)
{
	// A small freeze on the kill frame sells the hit even at a kilometre.
	HitStop(HitStopSeconds);
}

void UFTBGameFeelComponent::HandleCounterBattery(FVector Location, bool bLethal, float DistanceM)
{
	const float Near = FMath::Clamp(1.0f - DistanceM / 150.0f, 0.0f, 1.0f);
	if (Near <= 0.0f)
	{
		Shake(UFTBShakeImpact::StaticClass(), 0.4f);
		return;
	}
	Shake(UFTBShakeHeavy::StaticClass(), 0.4f + 0.6f * Near);
	HitStop(HitStopSeconds);
	if (UFTBEventHub* Hub = FTB::Hub(this))
	{
		Hub->OnVignette.Broadcast(CounterBatteryVignette * (0.5f + 0.5f * Near), FLinearColor(0.85f, 0.1f, 0.08f));
	}
}

void UFTBGameFeelComponent::HandlePost(int32 PostIndex, bool bRunning)
{
	if (bRunning)
	{
		return;
	}
	Shake(UFTBShakeMicro::StaticClass(), 0.7f);
	AActor* Owner = GetOwner();
	AFTBEffectActor::Spawn(GetWorld(), EFTBEffect::ArrivalDust, Owner->GetActorLocation() - FVector(0, 0, 150) + Owner->GetActorForwardVector() * 120.0f, 1.0f, PostIndex);
}
