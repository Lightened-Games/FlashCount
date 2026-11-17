// Flash to Bang - editor helpers.

#include "Editor/FTBEditorTools.h"
#include "Core/FTBRoundConfig.h"
#include "Core/FTBTerrain.h"
#include "Engine/BlockingVolume.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#endif

AActor* UFTBEditorTools::SpawnBlockingVolume(UObject* WorldContextObject, FVector Location, FVector HalfExtent, FString Label)
{
#if WITH_EDITOR
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABlockingVolume* Volume = World->SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(), FTransform(Location), Params);
	if (!Volume)
	{
		return nullptr;
	}
	UCubeBuilder* Builder = NewObject<UCubeBuilder>();
	Builder->X = HalfExtent.X * 2.0f;
	Builder->Y = HalfExtent.Y * 2.0f;
	Builder->Z = HalfExtent.Z * 2.0f;
	UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
	Volume->SetActorLabel(Label);
	return Volume;
#else
	return nullptr;
#endif
}

float UFTBEditorTools::TerrainHeight(float X, float Y)
{
	return FTBTerrain::HeightUU(GetDefault<UFTBRoundConfig>()->Terrain, FVector(X, Y, 0.0f));
}

FVector UFTBEditorTools::PostEye(int32 PostIndex)
{
	const UFTBRoundConfig* C = GetDefault<UFTBRoundConfig>();
	if (!C->Posts.IsValidIndex(PostIndex))
	{
		return FVector::ZeroVector;
	}
	const FFTBPostRow& Row = C->Posts[PostIndex];
	FVector P(Row.LocationM.X * 100.0f, Row.LocationM.Y * 100.0f, 0.0f);
	P.Z = FTBTerrain::HeightUU(C->Terrain, P) + Row.EyeHeightM * 100.0f;
	return P;
}
