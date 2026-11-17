// Flash to Bang - editor-only helpers for the content script (P24): Python can't give a BlockingVolume a brush.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FTBEditorTools.generated.h"

UCLASS()
class FLASHTOBANG_API UFTBEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Spawn a BlockingVolume with a real cube brush. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "FTB|Editor", meta = (WorldContext = "WorldContextObject"))
	static AActor* SpawnBlockingVolume(UObject* WorldContextObject, FVector Location, FVector HalfExtent, FString Label);

	/** Terrain height (cm) at a world XY using the round config defaults, so the map script places actors on the ground. */
	UFUNCTION(BlueprintCallable, Category = "FTB|Editor")
	static float TerrainHeight(float X, float Y);

	/** Eye position (cm) of a post from the round config defaults. */
	UFUNCTION(BlueprintCallable, Category = "FTB|Editor")
	static FVector PostEye(int32 PostIndex);
};
