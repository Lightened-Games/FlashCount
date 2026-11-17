// Flash to Bang - the spotter.

#include "Player/FTBObserverPawn.h"
#include "FlashtoBang.h"
#include "Audio/FTBSoundscapeComponent.h"
#include "Components/FTBArtilleryComponent.h"
#include "Components/FTBRadioTraceComponent.h"
#include "Components/FTBRangingComponent.h"
#include "Core/FTBRoundConfig.h"
#include "Core/FTBTerrain.h"
#include "Core/FTBWorld.h"
#include "FX/FTBMarkerComponent.h"
#include "Feedback/FTBGameFeelComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AFTBObserverPawn::AFTBObserverPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(NormalFOV);
	Camera->bConstrainAspectRatio = false;
	Ranging = CreateDefaultSubobject<UFTBRangingComponent>(TEXT("Ranging"));
	Artillery = CreateDefaultSubobject<UFTBArtilleryComponent>(TEXT("Artillery"));
	RadioTrace = CreateDefaultSubobject<UFTBRadioTraceComponent>(TEXT("RadioTrace"));
	GameFeel = CreateDefaultSubobject<UFTBGameFeelComponent>(TEXT("GameFeel"));
	Soundscape = CreateDefaultSubobject<UFTBSoundscapeComponent>(TEXT("Soundscape"));
	Markers = CreateDefaultSubobject<UFTBMarkerComponent>(TEXT("Markers"));
	Markers->SetupAttachment(Root);
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	SetCanBeDamaged(false);
	Tags.Add(TEXT("Observer"));
}

void AFTBObserverPawn::BeginPlay()
{
	Super::BeginPlay();
	Ranging->LeadSeconds = Artillery->FlightSeconds;
	Artillery->OnCalledNative.AddUObject(this, &AFTBObserverPawn::HandleShellCalled);
	const IFTBWorldQuery* Q = FTB::Query(this);
	const UFTBRoundConfig* Config = Q ? Q->FTB_GetConfig() : nullptr;
	SnapToPost(Config ? Config->StartPostIndex : 0);
	if (FParse::Param(FCommandLine::Get(), TEXT("FTBRouteCheck")))
	{
		LogRouteCheck();
	}
}

void AFTBObserverPawn::SnapToPost(int32 PostIndex)
{
	const IFTBWorldQuery* Q = FTB::Query(this);
	const UFTBRoundConfig* Config = Q ? Q->FTB_GetConfig() : nullptr;
	if (!Q || !Config || !Config->Posts.IsValidIndex(PostIndex))
	{
		return;
	}
	CurrentPost = PostIndex;
	TargetPost = PostIndex;
	const FFTBPostRow& Row = Config->Posts[PostIndex];
	SetActorLocation(Q->FTB_GetPostEye(PostIndex));
	if (!bPlaced)
	{
		ViewYaw = Row.ViewYaw;
		ViewPitch = Row.ViewPitch;
		bPlaced = true;
	}
	BaseYaw = Row.ViewYaw;
	ApplyView();
}

FVector AFTBObserverPawn::Obs_GetEye() const
{
	return Camera->GetComponentLocation();
}

bool AFTBObserverPawn::CanAct() const
{
	const IFTBWorldQuery* Q = FTB::Query(this);
	return Q && Q->FTB_GetState() == EFTBState::Active;
}

void AFTBObserverPawn::Broadcast(FName KeyId) const
{
	if (UFTBEventHub* Hub = FTB::Hub(this))
	{
		Hub->OnInput.Broadcast(KeyId, false);
	}
}

void AFTBObserverPawn::ApplyView()
{
	ViewPitch = FMath::Clamp(ViewPitch, PitchMin, PitchMax);
	const float Rel = FMath::Clamp(FRotator::NormalizeAxis(ViewYaw - BaseYaw), -YawArc, YawArc);
	ViewYaw = BaseYaw + Rel;
	Camera->SetWorldRotation(FRotator(ViewPitch, ViewYaw, 0.0f));
}

void AFTBObserverPawn::Obs_Look(float DeltaYaw, float DeltaPitch)
{
	const IFTBWorldQuery* Q = FTB::Query(this);
	if (Q && Q->FTB_IsCountdownLocked())
	{
		return;
	}
	const float FovScale = FMath::Lerp(1.0f, BinocularFOV / NormalFOV, ZoomAlpha);
	ViewYaw += DeltaYaw * LookSensitivity * FovScale;
	ViewPitch += DeltaPitch * LookSensitivity * FovScale;
	ApplyView();
}

float AFTBObserverPawn::Obs_TurnToward(const FVector& Target, float MaxDegPerSec, float DeltaTime)
{
	const FRotator Want = (Target - Obs_GetEye()).Rotation();
	const float DYaw = FRotator::NormalizeAxis(Want.Yaw - ViewYaw);
	const float DPitch = FRotator::NormalizeAxis(Want.Pitch - ViewPitch);
	const float Step = MaxDegPerSec * DeltaTime;
	const float Len = FMath::Sqrt(DYaw * DYaw + DPitch * DPitch);
	if (Len <= Step || Len < KINDA_SMALL_NUMBER)
	{
		ViewYaw += DYaw;
		ViewPitch += DPitch;
	}
	else
	{
		// Ease out: slow down over the last few degrees.
		const float Scale = FMath::Min(1.0f, Step / Len) * FMath::Clamp(Len / 6.0f, 0.35f, 1.0f);
		ViewYaw += DYaw * Scale;
		ViewPitch += DPitch * Scale;
	}
	ApplyView();
	const FRotator Now(ViewPitch, ViewYaw, 0.0f);
	return FMath::Sqrt(FMath::Square(FRotator::NormalizeAxis(Want.Yaw - Now.Yaw)) + FMath::Square(FRotator::NormalizeAxis(Want.Pitch - Now.Pitch)));
}

bool AFTBObserverPawn::Obs_ToggleWatch()
{
	Broadcast(FTB::KeyWatch);
	if (!CanAct())
	{
		return false;
	}
	if (bRunning)
	{
		if (UFTBEventHub* Hub = FTB::Hub(this)) { Hub->OnPrompt.Broadcast(FTB::PromptCantTimeRunning); }
		return false;
	}
	return Ranging->Toggle(Obs_GetEye(), Camera->GetForwardVector());
}

bool AFTBObserverPawn::Obs_IsWatchRunning() const
{
	return Ranging->IsRunning();
}

bool AFTBObserverPawn::Obs_GetAimPoint(FVector& OutPoint) const
{
	const FVector Start = Obs_GetEye();
	const FVector End = Start + Camera->GetForwardVector() * AimTraceRangeM * 100.0f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FTBAim), true, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		OutPoint = Hit.ImpactPoint;
		return true;
	}
	return false;
}

bool AFTBObserverPawn::Obs_CallFire()
{
	Broadcast(FTB::KeyFire);
	if (!CanAct())
	{
		return false;
	}
	UFTBEventHub* Hub = FTB::Hub(this);
	if (bRunning)
	{
		if (Hub) { Hub->OnPrompt.Broadcast(FTB::PromptCantTimeRunning); }
		return false;
	}
	FVector Point;
	if (!Obs_GetAimPoint(Point))
	{
		if (Hub) { Hub->OnPrompt.Broadcast(FTB::PromptNoTarget); }
		return false;
	}
	if (!Artillery->Call(Point))
	{
		if (Hub) { Hub->OnPrompt.Broadcast(FTB::PromptReloading); }
		return false;
	}
	return true;
}

void AFTBObserverPawn::HandleShellCalled(const FFTBShell& Shell)
{
	RadioTrace->AddCall(CurrentPost);
}

void AFTBObserverPawn::Obs_SetBinoculars(bool bOn)
{
	if (bRunning)
	{
		bOn = false;
	}
	if (bOn != bBinoculars)
	{
		bBinoculars = bOn;
		Broadcast(FTB::KeyBinoculars);
	}
}

float AFTBObserverPawn::Obs_GetRunAlpha() const
{
	return bRunning ? FMath::Clamp(RunElapsed / FMath::Max(RunTotal, 0.01f), 0.0f, 1.0f) : 1.0f;
}

TArray<FVector> AFTBObserverPawn::BuildRunPath(int32 FromPost, int32 ToPost) const
{
	const IFTBWorldQuery* Q = FTB::Query(this);
	const UFTBRoundConfig* Config = Q ? Q->FTB_GetConfig() : nullptr;
	if (!Config || !Config->Posts.IsValidIndex(FromPost) || !Config->Posts.IsValidIndex(ToPost))
	{
		return { GetActorLocation(), GetActorLocation() };
	}
	// From the foot of this post, across the hillside, to the foot of the next one (outside every structure).
	auto Foot = [&](const FFTBPostRow& Row)
	{
		FVector G((Row.LocationM.X + Row.ApproachM.X) * 100.0f, (Row.LocationM.Y + Row.ApproachM.Y) * 100.0f, 0.0f);
		G.Z = FTBTerrain::HeightUU(Config->Terrain, G) + 170.0f;
		return G;
	};
	const FVector A1 = Foot(Config->Posts[FromPost]);
	const FVector B1 = Foot(Config->Posts[ToPost]);

	// Steer around any post whose structure lies on the straight line (haystack between church and ridge, etc).
	TArray<FVector> Way = { A1 };
	FVector Cursor = A1;
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		bool bDetoured = false;
		for (const FFTBPostRow& Row : Config->Posts)
		{
			const FVector2D C(Row.LocationM * 100.0f);
			const FVector2D S0(Cursor), S1(B1);
			const FVector2D Seg = S1 - S0;
			const float L2 = FMath::Max(Seg.SizeSquared(), 1.0f);
			const float T = FVector2D::DotProduct(C - S0, Seg) / L2;
			if (T <= 0.02f || T >= 0.98f)
			{
				continue;
			}
			const FVector2D Closest = S0 + Seg * T;
			const float R = Row.ObstacleRadiusM * 100.0f;
			if (FVector2D::Distance(Closest, C) >= R)
			{
				continue;
			}
			FVector2D Side = Closest - C;
			if (Side.IsNearlyZero()) { Side = FVector2D(-Seg.Y, Seg.X); }
			const FVector2D D = C + Side.GetSafeNormal() * (R + DetourClearanceM * 100.0f);
			Cursor = FVector(D.X, D.Y, 0.0f);
			Way.Add(Cursor);
			bDetoured = true;
			break;
		}
		if (!bDetoured) { break; }
	}
	Way.Add(B1);

	// Follow the hillside contour between waypoints so the dash never cuts through the hill.
	TArray<FVector> Path = { A1 };
	for (int32 w = 1; w < Way.Num(); ++w)
	{
		const int32 Steps = FMath::Max(1, FMath::RoundToInt(FVector::Dist2D(Way[w - 1], Way[w]) / 800.0f));
		for (int32 k = 1; k <= Steps; ++k)
		{
			FVector P = FMath::Lerp(Way[w - 1], Way[w], static_cast<float>(k) / Steps);
			P.Z = FTBTerrain::HeightUU(Config->Terrain, P) + 170.0f;
			Path.Add(P);
		}
	}
	return Path;
}

void AFTBObserverPawn::LogRouteCheck() const
{
	const IFTBWorldQuery* Q = FTB::Query(this);
	const UFTBRoundConfig* Config = Q ? Q->FTB_GetConfig() : nullptr;
	if (!Config) { return; }
	for (int32 A = 0; A < Config->Posts.Num(); ++A)
	{
		for (int32 B = 0; B < Config->Posts.Num(); ++B)
		{
			if (A == B) { continue; }
			const TArray<FVector> Path = BuildRunPath(A, B);
			FString Report;
			bool bOk = true;
			for (int32 K = 0; K < Config->Posts.Num(); ++K)
			{
				const FVector2D C(Config->Posts[K].LocationM * 100.0f);
				float Min = BIG_NUMBER;
				for (int32 i = 0; i + 1 < Path.Num(); ++i)
				{
					const FVector2D S0(Path[i]), S1(Path[i + 1]);
					const FVector2D Seg = S1 - S0;
					const float T = FMath::Clamp(FVector2D::DotProduct(C - S0, Seg) / FMath::Max(Seg.SizeSquared(), 1.0f), 0.0f, 1.0f);
					Min = FMath::Min(Min, FVector2D::Distance(C, S0 + Seg * T));
				}
				const bool bClear = Min * 0.01f >= Config->Posts[K].ObstacleRadiusM - 0.01f;
				bOk &= bClear;
				Report += FString::Printf(TEXT(" %s=%.1fm%s"), *Config->Posts[K].Id.ToString(), Min * 0.01f, bClear ? TEXT("") : TEXT("(INSIDE)"));
			}
			UE_LOG(LogFTB, Warning, TEXT("[FTB] Route %d->%d %s points=%d clearance:%s"), A, B, bOk ? TEXT("OK") : TEXT("BLOCKED"), Path.Num(), *Report);
		}
	}
}

void AFTBObserverPawn::FinishEntering()
{
	bRunning = false;
	SnapToPost(TargetPost);
	const IFTBWorldQuery* Q = FTB::Query(this);
	const UFTBRoundConfig* Config = Q ? Q->FTB_GetConfig() : nullptr;
	if (Config && Config->Posts.IsValidIndex(TargetPost))
	{
		ViewYaw = Config->Posts[TargetPost].ViewYaw;
		ViewPitch = Config->Posts[TargetPost].ViewPitch;
		ApplyView();
	}
	Move = EFTBMove::Settling;
	MoveTime = 0.0f;
	if (UFTBEventHub* Hub = FTB::Hub(this))
	{
		Hub->OnPostChanged.Broadcast(CurrentPost, false);
	}
}

bool AFTBObserverPawn::Obs_RunToPost(int32 PostIndex)
{
	Broadcast(FTB::KeyPost);
	const IFTBWorldQuery* Q = FTB::Query(this);
	const UFTBRoundConfig* Config = Q ? Q->FTB_GetConfig() : nullptr;
	if (!CanAct() || !Config || !Config->Posts.IsValidIndex(PostIndex) || PostIndex == CurrentPost || bRunning)
	{
		return false;
	}
	Ranging->Cancel();
	Obs_SetBinoculars(false);

	RunPath = BuildRunPath(CurrentPost, PostIndex);
	const FVector From = GetActorLocation();
	const FVector To = Q->FTB_GetPostEye(PostIndex);
	const FVector A1 = RunPath[0];
	RunTimes.Reset();
	RunTotal = 0.0f;
	for (int32 i = 1; i < RunPath.Num(); ++i)
	{
		const FVector D = RunPath[i] - RunPath[i - 1];
		const bool bClimb = FMath::Abs(D.Z) > D.Size2D();
		const float Seconds = FMath::Max(0.15f, D.Size() * 0.01f / (bClimb ? ClimbSpeedMps : RunSpeedMps));
		RunTotal += Seconds;
		RunTimes.Add(RunTotal);
	}
	RunElapsed = 0.0f;
	TargetPost = PostIndex;
	bRunning = true;
	// Fade out where you stand, then the dash starts at the foot of the post (see Tick).
	Move = EFTBMove::Leaving;
	MoveTime = 0.0f;
	(void)A1;
	BaseYaw = (To - From).GetSafeNormal2D().Rotation().Yaw;
	UE_LOG(LogFTB, Log, TEXT("[FTB] Run %d -> %d (%.1fs)"), CurrentPost, PostIndex, RunTotal);
	if (UFTBEventHub* Hub = FTB::Hub(this))
	{
		Hub->OnPostChanged.Broadcast(PostIndex, true);
	}
	return true;
}

void AFTBObserverPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float RealDelta = FMath::Min(DeltaSeconds / FMath::Max(GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation(), 0.01f), 0.1f);
	ZoomAlpha = FMath::Clamp(ZoomAlpha + (bBinoculars ? 1.0f : -1.0f) * RealDelta / FMath::Max(ZoomSeconds, 0.01f), 0.0f, 1.0f);
	const float Ease = 1.0f - FMath::Square(1.0f - ZoomAlpha);
	Camera->SetFieldOfView(FMath::Lerp(NormalFOV, BinocularFOV, Ease));

	// One fade value, on real time (hit-stop never stretches it), eased both ways: no stacked camera fades, no pops.
	MoveTime += RealDelta;
	const auto Smooth = [](float X) { X = FMath::Clamp(X, 0.0f, 1.0f); return X * X * (3.0f - 2.0f * X); };
	bool bHoldStill = false;
	switch (Move)
	{
	case EFTBMove::Leaving:
		FadeAlpha = Smooth(MoveTime / FMath::Max(FadeOutSeconds, 0.01f));
		if (MoveTime >= FadeOutSeconds + FadeHoldSeconds)
		{
			SetActorLocation(RunPath[0]);
			Move = EFTBMove::Running;
			MoveTime = 0.0f;
		}
		bHoldStill = true;
		break;
	case EFTBMove::Running:
		FadeAlpha = 1.0f - Smooth(MoveTime / FMath::Max(FadeInSeconds, 0.01f));
		break;
	case EFTBMove::Entering:
		FadeAlpha = Smooth(MoveTime / FMath::Max(FadeOutSeconds, 0.01f));
		if (MoveTime >= FadeOutSeconds + FadeHoldSeconds)
		{
			FinishEntering();
		}
		bHoldStill = true;
		break;
	case EFTBMove::Settling:
		FadeAlpha = 1.0f - Smooth(MoveTime / FMath::Max(FadeInSeconds, 0.01f));
		if (MoveTime >= FadeInSeconds)
		{
			Move = EFTBMove::None;
			FadeAlpha = 0.0f;
		}
		break;
	default:
		FadeAlpha = 0.0f;
		break;
	}
	if (Move != EFTBMove::None || FadeAlpha > 0.0f)
	{
		// QA: per-frame fade trace (-FTBFadeLog) to prove each transition is a clean ramp with no pops.
		static const bool bFadeLog = FParse::Param(FCommandLine::Get(), TEXT("FTBFadeLog"));
		if (bFadeLog)
		{
			UE_LOG(LogFTB, Log, TEXT("[FTB] Fade move=%d t=%.3f alpha=%.3f"), static_cast<int32>(Move), MoveTime, FadeAlpha);
		}
	}
	if (bHoldStill)
	{
		return;
	}

	if (bRunning && Move == EFTBMove::Running)
	{
		RunElapsed += DeltaSeconds;
		int32 Seg = 0;
		while (Seg < RunTimes.Num() - 1 && RunElapsed > RunTimes[Seg]) { ++Seg; }
		const float SegStart = Seg > 0 ? RunTimes[Seg - 1] : 0.0f;
		const float T = FMath::Clamp((RunElapsed - SegStart) / FMath::Max(RunTimes[Seg] - SegStart, 0.01f), 0.0f, 1.0f);
		FVector P = FMath::Lerp(RunPath[Seg], RunPath[Seg + 1], T);
		P.Z += FMath::Abs(FMath::Sin(RunElapsed * HeadBobHz * UE_PI)) * HeadBobCm;
		SetActorLocation(P);
		// Look where you run, then settle back on the valley.
		const float Settle = FMath::Clamp((RunTotal - RunElapsed) / 0.8f, 0.0f, 1.0f);
		const IFTBWorldQuery* Q = FTB::Query(this);
		const UFTBRoundConfig* Config = Q ? Q->FTB_GetConfig() : nullptr;
		const float ValleyYaw = (Config && Config->Posts.IsValidIndex(TargetPost)) ? Config->Posts[TargetPost].ViewYaw : 90.0f;
		const float WantYaw = FMath::Lerp(ValleyYaw, BaseYaw, Settle);
		ViewYaw += FRotator::NormalizeAxis(WantYaw - ViewYaw) * FMath::Min(1.0f, DeltaSeconds * 5.0f);
		ViewPitch += (FMath::Lerp(RunPitchArrive, RunPitchDash, Settle) - ViewPitch) * FMath::Min(1.0f, DeltaSeconds * 4.0f);
		Camera->SetWorldRotation(FRotator(ViewPitch, ViewYaw, 0.0f));
		if (RunElapsed >= RunTotal)
		{
			// At the foot of the post: ease to black, take position on top, ease back in.
			Move = EFTBMove::Entering;
			MoveTime = 0.0f;
		}
		return;
	}

	// Slight hand sway in the binoculars sells the magnification.
	if (ZoomAlpha > 0.0f)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		const FRotator Sway(FMath::Sin(Now * 1.3f) * BinocularSway * ZoomAlpha, FMath::Sin(Now * 0.9f + 1.0f) * BinocularSway * ZoomAlpha, 0.0f);
		Camera->SetWorldRotation(FRotator(ViewPitch, ViewYaw, 0.0f) + Sway);
	}
	else
	{
		ApplyView();
	}
}
