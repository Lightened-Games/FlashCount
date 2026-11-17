// Flash to Bang - builds the valley from code (P35/P40 pattern): terrain, road, river, bridge, the three posts,
// farms, town, hedgerows and trees, all skinned with the CC0 surface instances. Deterministic per seed.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/FTBTypes.h"
#include "World/FTBArt.h"
#include "World/FTBMeshLib.h"
#include "FTBWorldBuilder.generated.h"

class UProceduralMeshComponent;
class UFTBRoundConfig;
struct FFTBRoadPath;

/** One mesh bucket per project material (fixed array: references stay valid while other buckets grow). */
struct FFTBMeshSet
{
	FFTBMeshData Data[static_cast<int32>(EFTBMat::Count)];
	FFTBMeshData& operator[](EFTBMat Mat) { return Data[static_cast<int32>(Mat)]; }
	int32 NumUsed() const { int32 N = 0; for (const FFTBMeshData& D : Data) { N += D.IsEmpty() ? 0 : 1; } return N; }
};

UCLASS()
class FLASHTOBANG_API AFTBWorldBuilder : public AActor
{
	GENERATED_BODY()

public:
	AFTBWorldBuilder();

	void Build(const UFTBRoundConfig& Config, const FFTBRoadPath& Road, int32 Seed);

	/** Writes the generated meshes to asset-library/Models/Generated as glTF (original work). */
	static void ExportModels(const FString& Directory, const UFTBRoundConfig& Config);

	// Structure builders (local origin = ground point of the structure).
	static void BuildChurch(FFTBMeshSet& Set, const FVector& Ground);
	/** Terrain (optional) seats the loose bales and the cart on the ground at their own spots. */
	static void BuildHaystack(FFTBMeshSet& Set, const FVector& Ground, FRandomStream& Rng, const FFTBTerrainParams* Terrain = nullptr);
	static void BuildRidgePost(FFTBMeshSet& Set, const FVector& Ground, FRandomStream& Rng);
	static void BuildHouse(FFTBMeshSet& Set, const FVector& Ground, float Yaw, const FVector& SizeM, EFTBMat Wall, EFTBMat Roof);
	static void BuildBridge(FFTBMeshSet& Set, const FVector& A, const FVector& B, float DeckZ, float WaterZ, float BedZ);
	static void AddTree(FFTBMeshSet& Set, const FVector& Ground, float HeightM, bool bConifer, bool bTrunk, FRandomStream& Rng);
	static void AddBush(FFTBMeshSet& Set, const FVector& Ground, float SizeM, FRandomStream& Rng);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FineCellM = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CoarseCellM = 32.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D TerrainMinM = FVector2D(-2600.0f, -700.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D TerrainMaxM = FVector2D(2600.0f, 3300.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D FineMinM = FVector2D(-420.0f, -320.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D FineMaxM = FVector2D(560.0f, 780.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RoadHalfWidthM = 3.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 FarmCount = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 TownHouses = 16;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 HedgeLines = 26;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 Groves = 16;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 BackdropTrees = 520;
	/** Keep tall things (trees, farms) this far off the road on the posts' side, so the column stays visible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SightlineBandTallM = 420.0f;
	/** Same for hedgerow bushes (2-5 m tall). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SightlineBandLowM = 180.0f;
	/** Width of the muddy margin on each river bank (metres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BankWidthM = 7.0f;

private:
	void BuildTerrain(const FFTBTerrainParams& T);
	void BuildRoad(const FFTBRoadPath& Road, const FFTBTerrainParams& T, FFTBMeshSet& Set) const;
	void BuildRiver(const FFTBTerrainParams& T, FFTBMeshSet& Set) const;
	void BuildRiverBanks(const FFTBTerrainParams& T, const FFTBRoadPath& Road, FFTBMeshSet& Set, FFTBMeshSet& Foliage, FRandomStream& Rng) const;
	bool IsClear(const FVector2D& PM, float Margin, const FFTBRoadPath& Road, const FFTBTerrainParams& T) const;
	/** True when something tall at PM would hide the road from the posts (between the hill and the road, within BandM). */
	bool BlocksSightline(const FVector2D& PM, float BandM, const FFTBRoadPath& Road) const;
	FVector2D PostCentreM = FVector2D(0.0f, 70.0f);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> TerrainMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> EnvMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> FoliageMesh;
	TArray<FVector2D> Occupied;
};
