// Flash to Bang - game mode.

#include "Core/FTBGameMode.h"
#include "FlashtoBang.h"
#include "Audio/FTBSynth.h"
#include "Core/FTBEventHub.h"
#include "Core/FTBRoundConfig.h"
#include "FX/FTBEffectActor.h"
#include "Player/FTBAutoplayComponent.h"
#include "Player/FTBObserverPawn.h"
#include "Player/FTBPlayerController.h"
#include "World/FTBTank.h"
#include "World/FTBWorldBuilder.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	// Rounds chained with -FTBRuns survive OpenLevel through process statics.
	int32 GRunsDone = 0;
	int32 GWins = 0;
}

AFTBGameMode::AFTBGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AFTBObserverPawn::StaticClass();
	PlayerControllerClass = AFTBPlayerController::StaticClass();
	TankClass = AFTBTank::StaticClass();
	RoundConfigAsset = TSoftObjectPtr<UFTBRoundConfig>(FSoftObjectPath(TEXT("/Game/FlashtoBang/Data/DA_FTB_Round.DA_FTB_Round")));
}

void AFTBGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	Hub = NewObject<UFTBEventHub>(this, TEXT("FTBEventHub"));
	LoadConfig();
	BuildRoad();
}

void AFTBGameMode::LoadConfig()
{
	Config = RoundConfigAsset.LoadSynchronous();
	if (!Config)
	{
		UE_LOG(LogFTB, Warning, TEXT("[FTB] Round data asset missing (%s) - using code defaults"), *RoundConfigAsset.ToString());
		Config = NewObject<UFTBRoundConfig>(this);
	}
}

void AFTBGameMode::BuildRoad()
{
	Road = FFTBRoadPath();
	const float DeckZ = Config->BridgeDeckHeightM * 100.0f;
	for (const FVector2D& P : Config->RoadPointsM)
	{
		Road.Points.Add(FVector(P.X * 100.0f, P.Y * 100.0f, 0.0f));
	}
	Road.Rebuild();
	const int32 B = FMath::Clamp(Config->BridgeStartIndex, 0, FMath::Max(0, Road.Points.Num() - 2));
	Road.BridgeS = Road.CumS.IsValidIndex(B) ? Road.CumS[B] : Road.Length;
	Road.BridgeEndS = Road.CumS.IsValidIndex(B + 1) ? Road.CumS[B + 1] : Road.Length;

	// Densify (every 10 m) so the road follows the terrain; ramp up onto the bridge deck.
	FFTBRoadPath Dense;
	const float Step = 1000.0f;
	const float Ramp = 3500.0f;
	for (float S = 0.0f; S <= Road.Length + 1.0f; S += Step)
	{
		FVector P = Road.At(FMath::Min(S, Road.Length));
		float Z = FTBTerrain::HeightUU(Config->Terrain, P);
		const float ToDeck = FMath::Max(Road.BridgeS - S, S - Road.BridgeEndS);
		if (ToDeck < Ramp)
		{
			Z = FMath::Lerp(DeckZ, FMath::Max(Z, 0.0f), FMath::Clamp(ToDeck / Ramp, 0.0f, 1.0f));
		}
		P.Z = Z;
		Dense.Points.Add(P);
	}
	Dense.Rebuild();
	Dense.BridgeS = Road.BridgeS;
	Dense.BridgeEndS = Road.BridgeEndS;
	Road = Dense;
	UE_LOG(LogFTB, Log, TEXT("[FTB] Road %.0f m, bridge at %.0f m"), Road.Length * 0.01f, Road.BridgeS * 0.01f);
}

float AFTBGameMode::FTB_TerrainHeightUU(const FVector& Location) const
{
	return FTBTerrain::HeightUU(Config->Terrain, Location);
}

FVector AFTBGameMode::FTB_GetPostEye(int32 PostIndex) const
{
	if (!Config || !Config->Posts.IsValidIndex(PostIndex))
	{
		return FVector::ZeroVector;
	}
	const FFTBPostRow& Row = Config->Posts[PostIndex];
	FVector P(Row.LocationM.X * 100.0f, Row.LocationM.Y * 100.0f, 0.0f);
	P.Z = FTBTerrain::HeightUU(Config->Terrain, P) + Row.EyeHeightM * 100.0f;
	return P;
}

FVector AFTBGameMode::FTB_GetBridgeLocation() const
{
	return Road.At((Road.BridgeS + Road.BridgeEndS) * 0.5f);
}

int32 AFTBGameMode::FTB_GetPostCount() const { return Config ? Config->Posts.Num() : 0; }
FText AFTBGameMode::FTB_GetPostName(int32 PostIndex) const { return (Config && Config->Posts.IsValidIndex(PostIndex)) ? Config->Posts[PostIndex].DisplayName : FText::GetEmpty(); }
int32 AFTBGameMode::FTB_GetKillsToWin() const { return Config ? Config->KillsToWin : 4; }

void AFTBGameMode::StartPlay()
{
	BuildWorld();
	Super::StartPlay();
}

void AFTBGameMode::BuildWorld()
{
	UWorld* World = GetWorld();
	if (AWorldSettings* WS = World->GetWorldSettings())
	{
		WS->KillZ = WorldKillZ;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	Builder = World->SpawnActor<AFTBWorldBuilder>(AFTBWorldBuilder::StaticClass(), FTransform::Identity, Params);
	if (Builder)
	{
		Builder->Build(*Config, Road, WorldSeed);
	}
	SpawnColumn();
}

void AFTBGameMode::SpawnColumn()
{
	const float SpeedUU = Config->TankSpeedMps * 100.0f;
	const float LeadStart = Road.BridgeS - SpeedUU * Config->LeadEtaSeconds;
	const FVector Town = FTB_GetBridgeLocation() + FVector(10000.0f, -15000.0f, 0.0f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 i = 0; i < Config->Tanks.Num(); ++i)
	{
		const FFTBTankRow& Row = Config->Tanks[i];
		AFTBTank* Tank = GetWorld()->SpawnActor<AFTBTank>(TankClass ? TankClass.Get() : AFTBTank::StaticClass(), FTransform::Identity, Params);
		if (!Tank)
		{
			continue;
		}
		Tank->CookOffDelayMin = Config->CookOffDelayMin;
		Tank->CookOffDelayMax = Config->CookOffDelayMax;
		Tank->MaxCookOffs = Config->MaxCookOffsPerWreck;
		Tank->InitTank(&Road, Config->Terrain, LeadStart - Row.BehindLeadM * 100.0f, SpeedUU * Row.SpeedScale, Town, WorldSeed + i * 31);
		Tanks.Add(Tank);
	}
	UE_LOG(LogFTB, Log, TEXT("[FTB] Column: %d tanks, lead starts %.0f m before the bridge"), Tanks.Num(), (Road.BridgeS - LeadStart) * 0.01f);
}

AActor* AFTBGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AFTBGameMode::BeginPlay()
{
	Super::BeginPlay();
	int32 Seed = 4242;
	FParse::Value(FCommandLine::Get(), TEXT("FTBSeed="), Seed);
	// Chained runs (-FTBRuns) each get their own seed so a sweep samples different rounds.
	Rng.Initialize(Seed + GRunsDone * 101);
	FParse::Value(FCommandLine::Get(), TEXT("FTBRuns="), RunsWanted);
	bShots = FParse::Param(FCommandLine::Get(), TEXT("FTBShots"));

	Hub->OnCounterBatteryRequested.AddDynamic(this, &AFTBGameMode::HandleCounterBatteryRequest);
	Hub->OnPostTraced.AddDynamic(this, &AFTBGameMode::HandlePostTraced);
	Hub->OnShellCalled.AddDynamic(this, &AFTBGameMode::HandleShellCalled);
	Hub->OnFixPlotted.AddDynamic(this, &AFTBGameMode::HandleFix);
	Hub->OnPostChanged.AddDynamic(this, &AFTBGameMode::HandlePost);

	const FString Lib = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("asset-library"));
	if (FParse::Param(FCommandLine::Get(), TEXT("FTBExportAudio")))
	{
		FTBSynth::ExportAll(Lib / TEXT("Audio/Generated/FlashtoBang"));
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("FTBExportMeshes")))
	{
		AFTBWorldBuilder::ExportModels(Lib / TEXT("Models/Generated/FlashtoBang"), *Config);
	}

	// No engine debug text over the game (shader/VSM notices would end up in the recording).
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
		GEngine->Exec(GetWorld(), TEXT("DisableAllScreenMessages"));
	}
	CheckCommandLineAutoplay();
	SetState(EFTBState::Ready);
}

void AFTBGameMode::CheckCommandLineAutoplay()
{
	bAutoplayExit = FParse::Param(FCommandLine::Get(), TEXT("autoplayexit")) || FParse::Param(FCommandLine::Get(), TEXT("FTBExit"));
	// Showcase take (record_showcase.ps1): same game and rules, only a shorter end-screen hold before quitting.
	if (FParse::Param(FCommandLine::Get(), TEXT("FTBShowcase")))
	{
		EndScreenAutoRestartSeconds = ShowcaseEndHoldSeconds;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("autoplay")) || bAutoplayExit)
	{
		UE_LOG(LogFTB, Warning, TEXT("Autoplay mode requested via command line."));
		bAutoplayActive = true;
		SetupAutoplayController();
	}
}

void AFTBGameMode::SetupAutoplayController()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (UFTBAutoplayComponent* Bot = PC->FindComponentByClass<UFTBAutoplayComponent>())
			{
				Bot->SetBotEnabled(true);
			}
		}
	}
}

FVector AFTBGameMode::ObserverEye() const
{
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (const IFTBObserver* O = PC->GetPawn() ? Cast<IFTBObserver>(PC->GetPawn()) : nullptr)
		{
			return O->Obs_GetEye();
		}
	}
	return FTB_GetPostEye(0);
}

void AFTBGameMode::SetState(EFTBState NewState)
{
	const EFTBState Old = State;
	State = NewState;
	StateTime = 0.0f;
	const bool bDrive = NewState == EFTBState::Active;
	for (AActor* A : Tanks)
	{
		if (AFTBTank* T = Cast<AFTBTank>(A)) { T->SetDriving(bDrive); }
	}
	if (NewState == EFTBState::Ready)
	{
		LastCountdown = -1;
	}
	if (NewState == EFTBState::Active)
	{
		NextShotTime = GetWorld()->GetTimeSeconds() + Config->FirstShotDelay;
	}
	UE_LOG(LogFTB, Log, TEXT("[FTB] State -> %s"), *UEnum::GetValueAsString(NewState));
	Hub->OnStateChanged.Broadcast(NewState, Old);
	if (NewState == EFTBState::Victory || NewState == EFTBState::Defeat)
	{
		LogResult();
		// Ending shot (FactoryKit config): victory and a bridge loss look out over the valley from behind the post;
		// a shelled post pulls back to show the post itself.
		if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
		{
			const APlayerController* PC = GetWorld()->GetFirstPlayerController();
			Flow->PlayEnding(NewState == EFTBState::Victory, PC ? PC->GetPawn() : nullptr, DefeatReason == EFTBDefeatReason::Bridge);
		}
	}
}

float AFTBGameMode::FTB_GetCountdownRemaining() const
{
	return State == EFTBState::Ready ? FMath::Max(0.0f, IntroSeconds + CountdownSeconds - StateTime) : 0.0f;
}

void AFTBGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StateTime += DeltaSeconds;
	if (!bFlowBegun)
	{
		BeginFlow();
	}
	switch (State)
	{
	case EFTBState::Ready:   TickReady(); break;
	case EFTBState::Active:  TickActive(DeltaSeconds); break;
	default:                 TickEnd(); break;
	}
	TickShots();
}

void AFTBGameMode::BeginFlow()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return; // the pawn is not there yet
	}
	bFlowBegun = true;
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		Flow->OnStartGame.AddUniqueDynamic(this, &AFTBGameMode::HandleFlowStart);
		Flow->Begin(bAutoplayActive);
		return;
	}
	HandleFlowStart();
}

void AFTBGameMode::HandleFlowStart()
{
	bWaitingForFlow = false;
	StateTime = 0.0f;
	UE_LOG(LogFTB, Log, TEXT("[FTB] Flow done - countdown starts"));
}

void AFTBGameMode::TickReady()
{
	if (bWaitingForFlow)
	{
		StateTime = 0.0f; // the start screen and fly-in own the screen; the countdown waits
		return;
	}
	const float Remaining = FTB_GetCountdownRemaining();
	if (Remaining <= CountdownSeconds)
	{
		const int32 Whole = FMath::CeilToInt(Remaining);
		if (Whole != LastCountdown)
		{
			LastCountdown = Whole;
			Hub->OnCountdown.Broadcast(Whole);
		}
	}
	if (Remaining <= 0.0f)
	{
		SetState(EFTBState::Active);
	}
}

void AFTBGameMode::TickActive(float DeltaSeconds)
{
	ActiveTime += DeltaSeconds;
	const float Now = GetWorld()->GetTimeSeconds();

	// Booms reaching the observer.
	for (int32 i = PendingBooms.Num() - 1; i >= 0; --i)
	{
		if (Now >= PendingBooms[i].BoomTime)
		{
			const FFTBFlashEvent Flash = PendingBooms[i];
			PendingBooms.RemoveAt(i);
			Hub->OnBoom.Broadcast(Flash);
		}
	}

	// Counter-battery strikes landing.
	for (int32 i = Strikes.Num() - 1; i >= 0; --i)
	{
		FPendingStrike& S = Strikes[i];
		if (S.bFired && Now >= S.LandTime)
		{
			const FPendingStrike Strike = S;
			Strikes.RemoveAt(i);
			AFTBEffectActor::Spawn(GetWorld(), EFTBEffect::CounterBattery, Strike.Impact, 1.0f, NextFlashId);
			const FVector Eye = ObserverEye();
			const float DistM = FVector::Dist(Eye, Strike.Impact) * 0.01f;
			const float HorizontalM = FVector::Dist2D(Eye, Strike.Impact) * 0.01f;
			Hub->OnCounterBatteryImpact.Broadcast(Strike.Impact, Strike.bLethal, DistM);
			UE_LOG(LogFTB, Log, TEXT("[FTB] Counter-battery %s landed %.0f m from the observer"), Strike.bLethal ? TEXT("LETHAL") : TEXT("ranging"), HorizontalM);
			if (Strike.bLethal)
			{
				for (AActor* A : Tanks)
				{
					if (IFTBFireSource* F = Cast<IFTBFireSource>(A)) { F->FTB_AimAt(FVector::ZeroVector, false); }
				}
				if (HorizontalM <= Config->LethalRadiusM && State == EFTBState::Active)
				{
					Defeat(EFTBDefeatReason::PostHit);
					return;
				}
			}
		}
	}

	TickFireDirector(Now);

	// A tank on the bridge loses the round.
	for (AActor* A : Tanks)
	{
		const IFTBFireSource* F = Cast<IFTBFireSource>(A);
		if (F && F->FTB_IsAlive() && F->FTB_GetRoadS() >= Road.BridgeS)
		{
			Defeat(EFTBDefeatReason::Bridge);
			return;
		}
	}
}

void AFTBGameMode::TickFireDirector(float Now)
{
	// Counter-battery shots fire as soon as they are requested.
	for (FPendingStrike& S : Strikes)
	{
		if (!S.bFired)
		{
			AActor* Best = nullptr;
			float BestD = BIG_NUMBER;
			for (AActor* A : Tanks)
			{
				const IFTBFireSource* F = Cast<IFTBFireSource>(A);
				if (F && F->FTB_IsAlive())
				{
					const float D = FVector::Dist(A->GetActorLocation(), S.Impact);
					if (D < BestD) { BestD = D; Best = A; }
				}
			}
			S.bFired = true;
			S.LandTime = Now + Config->CounterBatteryFlightSeconds;
			if (Best)
			{
				Fire(Best, EFTBFlashKind::CounterBattery, Now);
			}
		}
	}

	if (Now < NextShotTime || PendingBooms.Num() > 0)
	{
		return;
	}
	// Weighted pick: live tanks, plus wrecks ready to cook off.
	TArray<TPair<AActor*, EFTBFlashKind>> Candidates;
	TArray<float> Weights;
	float Total = 0.0f;
	for (AActor* A : Tanks)
	{
		const IFTBFireSource* F = Cast<IFTBFireSource>(A);
		if (!F) { continue; }
		if (F->FTB_IsAlive())
		{
			Candidates.Add({ A, EFTBFlashKind::Muzzle });
			Weights.Add(1.0f);
			Total += 1.0f;
		}
		else if (F->FTB_CanCookOff(Now))
		{
			Candidates.Add({ A, EFTBFlashKind::CookOff });
			Weights.Add(Config->CookOffWeight);
			Total += Config->CookOffWeight;
		}
	}
	if (Candidates.Num() == 0)
	{
		return;
	}
	float Pick = Rng.FRandRange(0.0f, Total);
	int32 Chosen = 0;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		Pick -= Weights[i];
		if (Pick <= 0.0f) { Chosen = i; break; }
	}
	Fire(Candidates[Chosen].Key, Candidates[Chosen].Value, Now);
}

float AFTBGameMode::SmokeVisibility(const FVector& From, const FVector& To, const AActor* IgnoreSource)
{
	float Vis = 1.0f;
	int32 Burning = 0;
	const FVector2D A(From), B(To);
	const FVector2D AB = B - A;
	const float L2 = FMath::Max(AB.SizeSquared(), 1.0f);
	for (AActor* Actor : Tanks)
	{
		const IFTBFireSource* F = Cast<IFTBFireSource>(Actor);
		FVector Centre;
		float Radius = 0.0f;
		if (!F || !F->FTB_GetSmoke(Centre, Radius))
		{
			continue;
		}
		++Burning;
		if (Actor == IgnoreSource)
		{
			continue;
		}
		const FVector2D C(Centre);
		const float T = FVector2D::DotProduct(C - A, AB) / L2;
		if (T < 0.02f || T > 1.02f)
		{
			continue;
		}
		const float D = FVector2D::Distance(C, A + AB * FMath::Clamp(T, 0.0f, 1.0f));
		if (D < Radius)
		{
			// Thickest at the core of the column.
			const float Core = 1.0f - FMath::Square(D / Radius);
			Vis *= FMath::Lerp(1.0f, FlashOcclusionPerColumn, Core);
		}
	}
	// Smoke spreads across the whole valley as the wrecks burn: haze, plus drifts that cross the sight line.
	Vis *= FMath::Pow(FMath::Max(0.0f, 1.0f - HazePerWreck), static_cast<float>(Burning));
	if (Burning > 0 && Rng.FRand() < SmokeDriftChancePerWreck * Burning)
	{
		Vis *= SmokeDriftDim;
	}
	return Vis;
}

void AFTBGameMode::Fire(AActor* Source, EFTBFlashKind Kind, float Now)
{
	IFTBFireSource* F = Cast<IFTBFireSource>(Source);
	if (!F)
	{
		return;
	}
	const FVector Eye = ObserverEye();
	FFTBFlashEvent Flash;
	Flash.Id = NextFlashId++;
	Flash.Kind = Kind;
	Flash.Location = F->FTB_GetFlashOrigin(Kind);
	Flash.FlashTime = Now;
	Flash.DistanceM = FVector::Dist(Eye, Flash.Location) * 0.01f;
	Flash.BoomTime = Now + Flash.DistanceM / Config->SpeedOfSoundMps;
	Flash.Visibility = SmokeVisibility(Eye, Flash.Location, Kind == EFTBFlashKind::CookOff ? Source : nullptr);
	Flash.bVisible = Flash.Visibility >= FlashHiddenBelow;
	Flash.Source = Source;
	F->FTB_OnFired(Kind, Now);
	AFTBEffectActor::Spawn(GetWorld(), Kind == EFTBFlashKind::CookOff ? EFTBEffect::CookOff : EFTBEffect::MuzzleFlash, Flash.Location,
		Flash.bVisible ? Flash.Visibility : Flash.Visibility * 0.35f, Flash.Id);
	PendingBooms.Add(Flash);
	RecentFlashes.Add(Flash);
	if (RecentFlashes.Num() > 12) { RecentFlashes.RemoveAt(0); }
	HiddenFlashes += Flash.bVisible ? 0 : 1;
	CookOffFlashes += Kind == EFTBFlashKind::CookOff ? 1 : 0;
	NextShotTime = Flash.BoomTime + Rng.FRandRange(Config->GapAfterBoomMin, Config->GapAfterBoomMax);
	UE_LOG(LogFTB, Verbose, TEXT("[FTB] Flash %d kind=%d dist=%.0fm vis=%.2f"), Flash.Id, static_cast<int32>(Kind), Flash.DistanceM, Flash.Visibility);
	Hub->OnFlash.Broadcast(Flash);
}

bool AFTBGameMode::FTB_ResolveShell(const FVector& Impact, float KillRadiusUU)
{
	if (State != EFTBState::Active)
	{
		return false;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	bool bKill = false;
	FVector KillLocation = Impact;
	for (AActor* A : Tanks)
	{
		if (IFTBShellTarget* T = Cast<IFTBShellTarget>(A))
		{
			if (T->FTB_ApplyShellImpact(Impact, KillRadiusUU, Now))
			{
				bKill = true;
				KillLocation = A->GetActorLocation();
				Wrecks.Add(KillLocation);
				++Kills;
			}
		}
	}
	FVector Ground = Impact;
	Ground.Z = FMath::Max(Impact.Z, FTBTerrain::HeightUU(Config->Terrain, Impact));
	AFTBEffectActor::Spawn(GetWorld(), bKill ? EFTBEffect::TankKill : EFTBEffect::ShellImpact, bKill ? KillLocation : Ground, 1.0f, Kills * 13 + Calls);
	UE_LOG(LogFTB, Log, TEXT("[FTB] Shell landed %s kill=%d kills=%d"), *Impact.ToCompactString(), bKill, Kills);
	Hub->OnShellImpact.Broadcast(bKill ? KillLocation : Ground, bKill);
	if (bKill)
	{
		Hub->OnTankKilled.Broadcast(KillLocation, Kills);
		if (Kills >= FTB_GetKillsToWin())
		{
			Victory();
		}
	}
	return bKill;
}

float AFTBGameMode::FTB_GetBridgeEta() const
{
	float Best = BIG_NUMBER;
	for (AActor* A : Tanks)
	{
		const IFTBFireSource* F = Cast<IFTBFireSource>(A);
		if (F && F->FTB_IsAlive() && F->FTB_GetSpeedUU() > 1.0f)
		{
			Best = FMath::Min(Best, (Road.BridgeS - F->FTB_GetRoadS()) / F->FTB_GetSpeedUU());
		}
	}
	if (Best == BIG_NUMBER && Config)
	{
		const float SpeedUU = Config->TankSpeedMps * 100.0f;
		const float LeadStart = Road.BridgeS - SpeedUU * Config->LeadEtaSeconds;
		Best = (Road.BridgeS - LeadStart) / SpeedUU;
	}
	return FMath::Max(0.0f, Best);
}

float AFTBGameMode::FTB_GetColumnDistanceM() const
{
	const FVector Eye = ObserverEye();
	float Best = BIG_NUMBER;
	for (AActor* A : Tanks)
	{
		const IFTBFireSource* F = Cast<IFTBFireSource>(A);
		if (F && F->FTB_IsAlive())
		{
			Best = FMath::Min(Best, FVector::Dist(Eye, A->GetActorLocation()) * 0.01f);
		}
	}
	return Best;
}

void AFTBGameMode::FTB_GetLiveTanks(TArray<FVector>& OutLocations, TArray<float>& OutRoadS) const
{
	OutLocations.Reset();
	OutRoadS.Reset();
	for (AActor* A : Tanks)
	{
		const IFTBFireSource* F = Cast<IFTBFireSource>(A);
		if (F && F->FTB_IsAlive())
		{
			OutLocations.Add(A->GetActorLocation());
			OutRoadS.Add(F->FTB_GetRoadS());
		}
	}
}

void AFTBGameMode::HandleCounterBatteryRequest(int32 PostIndex, bool bLethal)
{
	if (State != EFTBState::Active)
	{
		return;
	}
	FPendingStrike S;
	S.Post = PostIndex;
	S.bLethal = bLethal;
	FVector Post = FTB_GetPostEye(PostIndex);
	FVector Ground = Post;
	Ground.Z = FTBTerrain::HeightUU(Config->Terrain, Post);
	if (!bLethal)
	{
		// The ranging round falls short, between the post and the valley, with a little scatter.
		Ground += FVector(Rng.FRandRange(-1500.0f, 1500.0f), Config->RangingShortM * 100.0f, 0.0f);
		Ground.Z = FTBTerrain::HeightUU(Config->Terrain, Ground);
	}
	else
	{
		Ground.Z = Post.Z - 150.0f;
	}
	S.Impact = Ground;
	Strikes.Add(S);
}

void AFTBGameMode::HandlePostTraced(int32 PostIndex, float Trace, bool bTraced)
{
	++TracedCount;
	const FVector Target = FTB_GetPostEye(PostIndex);
	for (AActor* A : Tanks)
	{
		if (IFTBFireSource* F = Cast<IFTBFireSource>(A))
		{
			if (F->FTB_IsAlive()) { F->FTB_AimAt(Target, true); }
		}
	}
	if (bShots) { Shot(TEXT("traced")); }
}

void AFTBGameMode::HandleShellCalled(const FFTBShell& Shell)
{
	++Calls;
	if (bShots && Calls == 1) { Shot(TEXT("firstcall")); }
}

void AFTBGameMode::HandlePost(int32 PostIndex, bool bRunning)
{
	if (bRunning)
	{
		++Relocations;
		if (bShots && Relocations == 1) { Shot(TEXT("run")); }
	}
	else
	{
		ArrivedPost = PostIndex;
		ArriveTime = GetWorld()->GetTimeSeconds();
	}
}

void AFTBGameMode::HandleFix(const FFTBFix& Fix)
{
	// Score the fix against the flash the player was most likely timing (closest flash time to the watch start).
	const float Start = Fix.FlashTime;
	const FFTBFlashEvent* Best = nullptr;
	for (const FFTBFlashEvent& F : RecentFlashes)
	{
		if (FMath::Abs(F.FlashTime - Start) < 1.5f && (!Best || FMath::Abs(F.FlashTime - Start) < FMath::Abs(Best->FlashTime - Start)))
		{
			Best = &F;
		}
	}
	if (!Best)
	{
		return;
	}
	const float Err = FVector::Dist2D(Fix.Location, Best->Location) * 0.01f;
	++Fixes;
	SumFixErrorM += Err;
	BestFixErrorM = BestFixErrorM < 0.0f ? Err : FMath::Min(BestFixErrorM, Err);
	UE_LOG(LogFTB, Log, TEXT("[FTB] Fix error %.0f m (delay %.2fs, true %.2fs, kind %d)"), Err, Fix.DelaySeconds, Best->BoomTime - Best->FlashTime, static_cast<int32>(Best->Kind));
	if (bShots && Fixes == 1) { Shot(TEXT("firstfix")); }
}

void AFTBGameMode::Defeat(EFTBDefeatReason Reason)
{
	if (State != EFTBState::Active) { return; }
	DefeatReason = Reason;
	SetState(EFTBState::Defeat);
}

void AFTBGameMode::Victory()
{
	if (State != EFTBState::Active) { return; }
	SetState(EFTBState::Victory);
}

void AFTBGameMode::LogResult() const
{
	UE_LOG(LogFTB, Warning, TEXT("[FTB] RESULT run=%d result=%s reason=%d kills=%d calls=%d time=%.1f fixes=%d meanFixErr=%.0f bestFixErr=%.0f hidden=%d cookoffs=%d traced=%d moves=%d"),
		GRunsDone + 1, State == EFTBState::Victory ? TEXT("VICTORY") : TEXT("DEFEAT"), static_cast<int32>(DefeatReason), Kills, Calls, ActiveTime,
		Fixes, Fixes ? SumFixErrorM / Fixes : -1.0f, BestFixErrorM, HiddenFlashes, CookOffFlashes, TracedCount, Relocations);
}

void AFTBGameMode::TickEnd()
{
	if (!bAutoplayActive || StateTime < EndScreenAutoRestartSeconds)
	{
		return;
	}
	++GRunsDone;
	GWins += State == EFTBState::Victory ? 1 : 0;
	if (bAutoplayExit && GRunsDone >= RunsWanted)
	{
		UE_LOG(LogFTB, Warning, TEXT("Autoplay finished: %d/%d wins - exiting."), GWins, GRunsDone);
		bAutoplayActive = false;
		FPlatformMisc::RequestExit(false, TEXT("FlashtoBangAutoplay"));
		return;
	}
	bAutoplayActive = false;
	RequestRestart();
}

void AFTBGameMode::RequestRestart()
{
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		Flow->MarkRestart(); // the next load skips the start screen and fly-in
	}
	UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void AFTBGameMode::Shot(const FString& Tag)
{
	if (ShotsTaken.Contains(Tag))
	{
		return;
	}
	ShotsTaken.Add(Tag);
	const FString Name = FString::Printf(TEXT("FTB_%02d_%s.png"), ShotIndex++, *Tag);
	FScreenshotRequest::RequestScreenshot(Name, true, false);
	UE_LOG(LogFTB, Log, TEXT("[FTB] Screenshot %s"), *Name);
}

void AFTBGameMode::TickShots()
{
	if (!bShots)
	{
		return;
	}
	if (State == EFTBState::Ready && StateTime > 0.6f) { Shot(TEXT("banner")); }
	if (State == EFTBState::Ready && FTB_GetCountdownRemaining() < 2.5f && FTB_GetCountdownRemaining() > 1.5f) { Shot(TEXT("countdown")); }
	if (State == EFTBState::Active)
	{
		if (ActiveTime >= NextTimedShot)
		{
			Shot(FString::Printf(TEXT("play%03d"), FMath::RoundToInt(ActiveTime)));
			NextTimedShot += 12.0f;
		}
		if (Kills >= 1 && !ShotsTaken.Contains(TEXT("firstkill")) && StateTime > 0.0f) { Shot(TEXT("firstkill")); }
		if (HiddenFlashes >= 1) { Shot(TEXT("smoke")); }
		if (ArriveTime > 0.0f && GetWorld()->GetTimeSeconds() - ArriveTime > 0.6f)
		{
			Shot(FString::Printf(TEXT("arrive_post%d"), ArrivedPost));
			ArriveTime = -1.0f;
		}
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			const IFTBObserver* O = PC->GetPawn() ? Cast<IFTBObserver>(PC->GetPawn()) : nullptr;
			if (O && O->Obs_GetZoomAlpha() >= 0.99f) { Shot(Kills == 0 ? TEXT("binoculars") : TEXT("binoculars_late")); }
		}
		if (CookOffFlashes >= 1) { Shot(TEXT("cookoff")); }
	}
	// QA only: -FTBTankCam=near parks the camera beside the lead tank; =far looks at it from the post through the binoculars.
	FString TankCam;
	if (State == EFTBState::Active && ActiveTime > 4.0f && !ShotsTaken.Contains(TEXT("tankcam")) && FParse::Value(FCommandLine::Get(), TEXT("FTBTankCam="), TankCam) && Tanks.Num() > 0)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		IFTBObserver* O = Pawn ? Cast<IFTBObserver>(Pawn) : nullptr;
		if (O)
		{
			const FVector T = Tanks[0]->GetActorLocation();
			if (TankCam.Equals(TEXT("haystack"), ESearchCase::IgnoreCase))
			{
				// Third-person look at the haystack post from downhill, to check it sits flush on the slope.
				const FVector Eye = FTB_GetPostEye(1);
				FVector Cam = Eye + FVector(1300.0f, 900.0f, 0.0f);
				Cam.Z = FTBTerrain::HeightUU(Config->Terrain, Cam) + 250.0f;
				Pawn->SetActorLocation(Cam);
				O->Obs_TurnToward(Eye - FVector(0, 0, 300.0f), 100000.0f, 1.0f);
				if (ActiveTime > 4.6f) { Shot(TEXT("tankcam")); }
				return;
			}
			if (TankCam.Equals(TEXT("near"), ESearchCase::IgnoreCase))
			{
				Pawn->SetActorLocation(T + FVector(-1600.0f, -1500.0f, 650.0f));
			}
			else
			{
				O->Obs_SetBinoculars(true);
			}
			O->Obs_TurnToward(T + FVector(0, 0, 150), 100000.0f, 1.0f);
			if (ActiveTime > 4.6f) { Shot(TEXT("tankcam")); }
		}
	}
	if (State == EFTBState::Victory && StateTime > 2.6f) { Shot(TEXT("victory")); }
	if (State == EFTBState::Defeat && StateTime > 2.6f) { Shot(TEXT("defeat")); }
}
