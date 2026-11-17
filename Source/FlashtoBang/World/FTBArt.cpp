// Flash to Bang - project material lookup.

#include "World/FTBArt.h"
#include "FlashtoBang.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/StrongObjectPtr.h"

const TCHAR* FTBArt::MatPath(EFTBMat Which)
{
	switch (Which)
	{
	case EFTBMat::Stone:      return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Stone.MI_FTB_Stone");
	case EFTBMat::StonePale:  return TEXT("/Game/FlashtoBang/Materials/MI_FTB_StonePale.MI_FTB_StonePale");
	case EFTBMat::Plaster:    return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Plaster.MI_FTB_Plaster");
	case EFTBMat::Planks:     return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Planks.MI_FTB_Planks");
	case EFTBMat::PlanksDark: return TEXT("/Game/FlashtoBang/Materials/MI_FTB_PlanksDark.MI_FTB_PlanksDark");
	case EFTBMat::Roof:       return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Roof.MI_FTB_Roof");
	case EFTBMat::RoofWarm:   return TEXT("/Game/FlashtoBang/Materials/MI_FTB_RoofWarm.MI_FTB_RoofWarm");
	case EFTBMat::Thatch:     return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Thatch.MI_FTB_Thatch");
	case EFTBMat::Steel:      return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Steel.MI_FTB_Steel");
	case EFTBMat::SteelDark:  return TEXT("/Game/FlashtoBang/Materials/MI_FTB_SteelDark.MI_FTB_SteelDark");
	case EFTBMat::Wreck:      return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Wreck.MI_FTB_Wreck");
	case EFTBMat::Mud:        return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Mud.MI_FTB_Mud");
	case EFTBMat::Sandbag:    return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Sandbag.MI_FTB_Sandbag");
	case EFTBMat::Rock:       return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Rock.MI_FTB_Rock");
	case EFTBMat::Terrain:    return TEXT("/Game/FlashtoBang/Materials/M_FTB_Terrain.M_FTB_Terrain");
	case EFTBMat::Water:      return TEXT("/Game/FlashtoBang/Materials/M_FTB_Water.M_FTB_Water");
	case EFTBMat::Conifer:    return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Conifer.MI_FTB_Conifer");
	case EFTBMat::Bush:       return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Bush.MI_FTB_Bush");
	case EFTBMat::Flare:      return TEXT("/Game/FlashtoBang/Materials/M_FTB_Flare.M_FTB_Flare");
	case EFTBMat::Fire:       return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Fire.MI_FTB_Fire");
	case EFTBMat::Smoke:      return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Smoke.MI_FTB_Smoke");
	case EFTBMat::Dirt:       return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Dirt.MI_FTB_Dirt");
	case EFTBMat::Marker:     return TEXT("/Game/FlashtoBang/Materials/M_FTB_Marker.M_FTB_Marker");
	case EFTBMat::Spark:      return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Spark.MI_FTB_Spark");
	case EFTBMat::Silhouette: return TEXT("/Game/FlashtoBang/Materials/M_FTB_Silhouette.M_FTB_Silhouette");
	case EFTBMat::Broadleaf2: return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Broadleaf2.MI_FTB_Broadleaf2");
	case EFTBMat::Reeds:      return TEXT("/Game/FlashtoBang/Materials/MI_FTB_Reeds.MI_FTB_Reeds");
	default:                  return TEXT("");
	}
}

UMaterialInterface* FTBArt::Mat(EFTBMat Which)
{
	static TStrongObjectPtr<UMaterialInterface> Cache[static_cast<int32>(EFTBMat::Count)];
	const int32 Index = static_cast<int32>(Which);
	if (Index < 0 || Index >= static_cast<int32>(EFTBMat::Count))
	{
		return nullptr;
	}
	if (!Cache[Index].IsValid())
	{
		UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(nullptr, MatPath(Which));
		if (!Loaded)
		{
			UE_LOG(LogFTB, Error, TEXT("[FTB] Missing project material %s - run Tools/build_content.ps1"), MatPath(Which));
			return nullptr;
		}
		Cache[Index].Reset(Loaded);
	}
	return Cache[Index].Get();
}

UStaticMesh* FTBArt::PlaneMesh()
{
	static TStrongObjectPtr<UStaticMesh> Plane;
	if (!Plane.IsValid())
	{
		Plane.Reset(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	}
	return Plane.Get();
}
