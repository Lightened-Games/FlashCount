// Flash to Bang - project material paths (built by Tools/Editor/build_content.py). Soft paths, loaded at runtime.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMesh;

enum class EFTBMat : uint8
{
	Stone, StonePale, Plaster, Planks, PlanksDark, Roof, RoofWarm, Thatch, Steel, SteelDark, Wreck, Mud, Sandbag, Rock,
	Terrain, Water, Conifer, Bush, Flare, Fire, Smoke, Dirt, Marker, Spark, Silhouette, Broadleaf2, Reeds,
	Count
};

namespace FTBArt
{
	FLASHTOBANG_API UMaterialInterface* Mat(EFTBMat Which);
	/** /Engine/BasicShapes/Plane (100 x 100, facing +Z), used under our materials for billboards and markers. */
	FLASHTOBANG_API UStaticMesh* PlaneMesh();
	FLASHTOBANG_API const TCHAR* MatPath(EFTBMat Which);
}
