// Flash to Bang - autoplay bot.

#include "Player/FTBAutoplayComponent.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "FlashtoBang.h"
#include "Components/FTBArtilleryComponent.h"
#include "Components/FTBRadioTraceComponent.h"
#include "Core/FTBRoundConfig.h"
#include "Core/FTBWorld.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

UFTBAutoplayComponent::UFTBAutoplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFTBAutoplayComponent::BeginPlay()
{
	Super::BeginPlay();
	int32 Seed = 4242;
	FParse::Value(FCommandLine::Get(), TEXT("FTBSeed="), Seed);
	static int32 GBotRuns = 0;
	Rng.Initialize(Seed * 7 + 3 + 977 * GBotRuns++);
	if (FParse::Param(FCommandLine::Get(), TEXT("FTBShowcase")))
	{
		Skill = ShowcaseSkill;
	}
	FParse::Value(FCommandLine::Get(), TEXT("FTBSkill="), Skill);
	FString DefeatMode;
	if (FParse::Value(FCommandLine::Get(), TEXT("FTBDefeat="), DefeatMode))
	{
		bPassive = DefeatMode.Equals(TEXT("bridge"), ESearchCase::IgnoreCase);
		bNeverRelocate = DefeatMode.Equals(TEXT("post"), ESearchCase::IgnoreCase);
	}
}

void UFTBAutoplayComponent::SetBotEnabled(bool bOn)
{
	bEnabled = bOn;
	if (bEnabled && !bBound)
	{
		if (UFTBEventHub* Hub = FTB::Hub(this))
		{
			Hub->OnFlash.AddDynamic(this, &UFTBAutoplayComponent::HandleFlash);
			Hub->OnBoom.AddDynamic(this, &UFTBAutoplayComponent::HandleBoom);
			Hub->OnFixPlotted.AddDynamic(this, &UFTBAutoplayComponent::HandleFix);
			Hub->OnTankKilled.AddDynamic(this, &UFTBAutoplayComponent::HandleKill);
			Hub->OnStateChanged.AddDynamic(this, &UFTBAutoplayComponent::HandleState);
			bBound = true;
		}
	}
	UE_LOG(LogFTB, Log, TEXT("[FTB] Autoplay bot %s (skill %.2f, passive=%d, neverRelocate=%d)"), bOn ? TEXT("ON") : TEXT("OFF"), Skill, bPassive, bNeverRelocate);
}

IFTBObserver* UFTBAutoplayComponent::Observer() const
{
	const AController* C = Cast<AController>(GetOwner());
	return (C && C->GetPawn()) ? Cast<IFTBObserver>(C->GetPawn()) : nullptr;
}

float UFTBAutoplayComponent::Gauss(float Sigma)
{
	// Box-Muller, clamped to 2.5 sigma.
	const float U1 = FMath::Max(Rng.FRand(), 1e-4f);
	const float U2 = Rng.FRand();
	const float Z = FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Cos(UE_TWO_PI * U2);
	return FMath::Clamp(Z, -2.5f, 2.5f) * Sigma;
}

void UFTBAutoplayComponent::SetPhase(EFTBBotPhase NewPhase)
{
	Phase = NewPhase;
	PhaseTime = 0.0f;
}

void UFTBAutoplayComponent::HandleState(EFTBState NewState, EFTBState OldState)
{
	SetPhase(EFTBBotPhase::Idle);
	if (NewState != EFTBState::Active)
	{
		if (IFTBObserver* O = Observer()) { O->Obs_SetBinoculars(false); }
	}
}

void UFTBAutoplayComponent::HandleKill(FVector Location, int32 Kills)
{
	KnownWrecks.Add(Location);
}

void UFTBAutoplayComponent::HandleFlash(const FFTBFlashEvent& Flash)
{
	IFTBObserver* O = Observer();
	const IFTBWorldQuery* Q = FTB::Query(this);
	if (!bEnabled || bPassive || !O || !Q || Q->FTB_GetState() != EFTBState::Active || !Flash.bVisible)
	{
		return;
	}
	++FlashesSeen;
	if (Phase != EFTBBotPhase::Idle || O->Obs_IsRunning())
	{
		return;
	}
	for (const FVector& W : KnownWrecks)
	{
		if (FVector::Dist2D(W, Flash.Location) < WreckIgnoreRadiusM * 100.0f)
		{
			return;
		}
	}
	const APawn* Pawn = Cast<AController>(GetOwner())->GetPawn();
	const UFTBArtilleryComponent* Art = Pawn ? Pawn->FindComponentByClass<UFTBArtilleryComponent>() : nullptr;
	if (Art)
	{
		// With the battery busy, keep working: time the flash now and fire as soon as a gun is free.
		if (Art->NumInFlight() >= Art->MaxInFlight && !bTimeWhileBatteryBusy)
		{
			return;
		}
		for (const FFTBShell& S : Art->GetInFlight())
		{
			if (FVector::Dist2D(S.Target, Flash.Location) < ShellOverlapRadiusM * 100.0f)
			{
				return;
			}
		}
	}
	TargetFlash = Flash;
	const float Sigma = FMath::Lerp(TimingSigmaLow, TimingSigmaHigh, Skill);
	PressTime = Flash.FlashTime + FMath::Max(0.08f, ReactionMean + Gauss(Sigma));
	StopTime = -1.0f;
	AimOffsetDeg = FVector2D(Gauss(FMath::Lerp(AimSigmaLowDeg, AimSigmaHighDeg, Skill)), Gauss(FMath::Lerp(AimSigmaLowDeg, AimSigmaHighDeg, Skill)) * 0.3f);
	SetPhase(EFTBBotPhase::TurnToFlash);
}

void UFTBAutoplayComponent::HandleBoom(const FFTBFlashEvent& Flash)
{
	if (!bEnabled || Phase != EFTBBotPhase::Timing || Flash.Id != TargetFlash.Id)
	{
		return;
	}
	const float Sigma = FMath::Lerp(TimingSigmaLow, TimingSigmaHigh, Skill);
	StopTime = FTB::Now(this) + FMath::Max(0.08f, ReactionMean + Gauss(Sigma));
}

void UFTBAutoplayComponent::HandleFix(const FFTBFix& Fix)
{
	if (!bEnabled || Phase != EFTBBotPhase::Timing)
	{
		return;
	}
	if (!Fix.bOnRoad)
	{
		UE_LOG(LogFTB, Log, TEXT("[FTB] Bot: fix off the road (%.0f m), skipping"), Fix.RangeM);
		SetPhase(EFTBBotPhase::Idle);
		return;
	}
	CurrentFix = Fix;
	AimHold = 0.0f;
	++FlashesTimed;
	SetPhase(EFTBBotPhase::Aiming);
}

int32 UFTBAutoplayComponent::PickPost() const
{
	const IFTBWorldQuery* Q = FTB::Query(this);
	IFTBObserver* O = Observer();
	const APawn* Pawn = Cast<AController>(GetOwner())->GetPawn();
	const UFTBRadioTraceComponent* Trace = Pawn ? Pawn->FindComponentByClass<UFTBRadioTraceComponent>() : nullptr;
	if (!Q || !O || !Trace)
	{
		return INDEX_NONE;
	}
	int32 Best = INDEX_NONE;
	float BestScore = BIG_NUMBER;
	const FVector Here = O->Obs_GetEye();
	for (int32 i = 0; i < Q->FTB_GetPostCount(); ++i)
	{
		if (i == O->Obs_GetPost() || Trace->IsTraced(i))
		{
			continue;
		}
		const float Score = Trace->GetTrace(i) * 100000.0f + FVector::Dist(Here, Q->FTB_GetPostEye(i));
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = i;
		}
	}
	return Best;
}

void UFTBAutoplayComponent::DecideRelocation()
{
	IFTBObserver* O = Observer();
	const APawn* Pawn = Cast<AController>(GetOwner())->GetPawn();
	const UFTBRadioTraceComponent* Trace = Pawn ? Pawn->FindComponentByClass<UFTBRadioTraceComponent>() : nullptr;
	if (!O || !Trace || bNeverRelocate || O->Obs_IsRunning())
	{
		return;
	}
	const int32 Post = O->Obs_GetPost();
	if (Trace->IsTraced(Post) || Trace->GetCallsLeft(Post) <= 0)
	{
		const int32 Next = PickPost();
		if (Next != INDEX_NONE && O->Obs_RunToPost(Next))
		{
			SetPhase(EFTBBotPhase::Running);
		}
	}
}

void UFTBAutoplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	IFTBObserver* O = Observer();
	const IFTBWorldQuery* Q = FTB::Query(this);
	if (!bEnabled || !O || !Q || Q->FTB_GetState() != EFTBState::Active)
	{
		return;
	}
	// FactoryKit take-over: real input hands the round to the player; when the bot resumes it starts fresh.
	if (const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		if (!Flow->IsBotDriving())
		{
			bPausedByPlayer = true;
			return;
		}
		if (bPausedByPlayer)
		{
			bPausedByPlayer = false;
			SetPhase(EFTBBotPhase::Idle);
		}
	}
	const float Now = FTB::Now(this);
	PhaseTime += DeltaTime;
	if (Phase != EFTBBotPhase::Idle && Phase != EFTBBotPhase::Running && PhaseTime > StuckSeconds)
	{
		UE_LOG(LogFTB, Warning, TEXT("[FTB] Bot stuck in phase %d, resetting"), static_cast<int32>(Phase));
		if (O->Obs_IsWatchRunning()) { O->Obs_ToggleWatch(); }
		SetPhase(EFTBBotPhase::Idle);
	}

	switch (Phase)
	{
	case EFTBBotPhase::Idle:
	{
		O->Obs_SetBinoculars(false);
		// Calm scan toward the middle of the column.
		TArray<FVector> Live;
		TArray<float> LiveS;
		Q->FTB_GetLiveTanks(Live, LiveS);
		if (Live.Num() > 0)
		{
			FVector Mid = FVector::ZeroVector;
			for (const FVector& L : Live) { Mid += L / Live.Num(); }
			O->Obs_TurnToward(Mid, 25.0f, DeltaTime);
		}
		DecideRelocation();
		break;
	}
	case EFTBBotPhase::TurnToFlash:
	{
		const float Remaining = O->Obs_TurnToward(TargetFlash.Location, TurnDegPerSec, DeltaTime);
		if (Now >= PressTime && Remaining < 0.6f)
		{
			if (O->Obs_ToggleWatch())
			{
				SetPhase(EFTBBotPhase::Timing);
			}
			else
			{
				SetPhase(EFTBBotPhase::Idle);
			}
		}
		break;
	}
	case EFTBBotPhase::Timing:
		if (bBinocularsWhileTiming && PhaseTime > 0.25f)
		{
			O->Obs_SetBinoculars(true);
		}
		if (StopTime > 0.0f && Now >= StopTime)
		{
			StopTime = -1.0f;
			O->Obs_ToggleWatch();
			if (Phase == EFTBBotPhase::Timing)
			{
				SetPhase(EFTBBotPhase::Idle);
			}
		}
		break;
	case EFTBBotPhase::Aiming:
	{
		const UFTBRoundConfig* Config = Q->FTB_GetConfig();
		const float SpeedUU = Config ? Config->TankSpeedMps * 100.0f : 1100.0f;
		// Aim where the lead ring will be once the turn settles.
		const FVector Target = Q->FTB_GetRoad().At(FTB::LeadS(CurrentFix, Now + 0.1f, SpeedUU));
		const FVector Eye = O->Obs_GetEye();
		const FRotator ToTarget = (Target - Eye).Rotation() + FRotator(AimOffsetDeg.Y, AimOffsetDeg.X, 0.0f);
		const FVector Aim = Eye + ToTarget.Vector() * FVector::Dist(Eye, Target);
		O->Obs_SetBinoculars(true);
		const float Remaining = O->Obs_TurnToward(Aim, TurnDegPerSec * 0.6f, DeltaTime);
		AimHold = Remaining < 0.15f ? AimHold + DeltaTime : 0.0f;
		const APawn* AimPawn = Cast<AController>(GetOwner())->GetPawn();
		const UFTBArtilleryComponent* AimArt = AimPawn ? AimPawn->FindComponentByClass<UFTBArtilleryComponent>() : nullptr;
		const bool bGunReady = !AimArt || AimArt->CanCall();
		if (AimHold >= AimSettleSeconds && bGunReady)
		{
			const bool bCalled = O->Obs_CallFire();
			if (bCalled || PhaseTime > AimTimeoutSeconds)
			{
				O->Obs_SetBinoculars(false);
				SetPhase(EFTBBotPhase::Idle);
				DecideRelocation();
			}
		}
		else if (PhaseTime > AimTimeoutSeconds)
		{
			// The battery stayed busy too long: this fix has gone stale.
			O->Obs_SetBinoculars(false);
			SetPhase(EFTBBotPhase::Idle);
		}
		break;
	}
	case EFTBBotPhase::Running:
		if (!O->Obs_IsRunning())
		{
			SetPhase(EFTBBotPhase::Idle);
		}
		break;
	}
}
