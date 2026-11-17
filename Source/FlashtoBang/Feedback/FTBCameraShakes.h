// Flash to Bang - camera shake presets (Perlin noise, EngineCameras).

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "FTBCameraShakes.generated.h"

/** Distant boom arriving / arrival at a post. */
UCLASS()
class FLASHTOBANG_API UFTBShakeMicro : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UFTBShakeMicro(const FObjectInitializer& ObjectInitializer);
};

/** Shell impact nearby. */
UCLASS()
class FLASHTOBANG_API UFTBShakeImpact : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UFTBShakeImpact(const FObjectInitializer& ObjectInitializer);
};

/** Counter-battery round landing on (or next to) the post. */
UCLASS()
class FLASHTOBANG_API UFTBShakeHeavy : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UFTBShakeHeavy(const FObjectInitializer& ObjectInitializer);
};
