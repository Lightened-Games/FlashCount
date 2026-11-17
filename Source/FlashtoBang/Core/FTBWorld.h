// Flash to Bang - helpers to reach the world query and event hub from any object (interface casts only).

#pragma once

#include "CoreMinimal.h"
#include "Core/FTBInterfaces.h"
#include "Core/FTBEventHub.h"
#include "Core/FTBTerrain.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

namespace FTB
{
	inline IFTBWorldQuery* Query(const UObject* Ctx)
	{
		AGameModeBase* GM = Ctx ? UGameplayStatics::GetGameMode(Ctx) : nullptr;
		return GM ? Cast<IFTBWorldQuery>(GM) : nullptr;
	}

	inline UFTBEventHub* Hub(const UObject* Ctx)
	{
		IFTBWorldQuery* Q = Query(Ctx);
		return Q ? Q->FTB_GetHub() : nullptr;
	}

	inline float Now(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		return W ? W->GetTimeSeconds() : 0.0f;
	}

	/** Road distance where a shell called at Now meets a tank that was at the fix when it fired (the lead marches on). */
	inline float LeadS(const FFTBFix& Fix, float NowSeconds, float SpeedUU)
	{
		return Fix.RoadS + SpeedUU * ((NowSeconds - Fix.FlashTime) + Fix.LeadSeconds);
	}

	/** Key ids broadcast on UFTBEventHub::OnInput (the HUD legend rows). */
	inline const FName KeyWatch(TEXT("Watch"));
	inline const FName KeyFire(TEXT("Fire"));
	inline const FName KeyBinoculars(TEXT("Binoculars"));
	inline const FName KeyPost(TEXT("Post"));
	inline const FName KeyRestart(TEXT("Restart"));

	/** Prompt ids broadcast on UFTBEventHub::OnPrompt. */
	inline const FName PromptCantTimeRunning(TEXT("CantTimeRunning"));
	inline const FName PromptReloading(TEXT("Reloading"));
	inline const FName PromptNoTarget(TEXT("NoTarget"));
	inline const FName PromptTooShort(TEXT("TooShort"));
}
