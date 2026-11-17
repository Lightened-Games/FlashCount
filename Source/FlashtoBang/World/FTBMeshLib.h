// Flash to Bang - procedural geometry (bevelled boxes, tapered blocks, cylinders, lathes, cards) for code-built art.
// Every piece gets planar UVs in metres so the textured surface master tiles at a constant texel density.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

struct FLASHTOBANG_API FFTBMeshData
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;

	int32 Num() const { return Verts.Num(); }
	bool IsEmpty() const { return Tris.Num() == 0; }
	void Append(const FFTBMeshData& Other);
};

namespace FTBMesh
{
	/** UV scale: UV units per metre (1 = texture repeats every metre before the material's own tiling). */
	FLASHTOBANG_API void AddTri(FFTBMeshData& D, const FVector& A, const FVector& B, const FVector& C, const FVector& Normal, float UVPerM = 1.0f);
	FLASHTOBANG_API void AddQuad(FFTBMeshData& D, const FVector& A, const FVector& B, const FVector& C, const FVector& E, const FVector& Normal, float UVPerM = 1.0f);

	/** Chamfered box: Half extents in local space, Bevel in world units. */
	FLASHTOBANG_API void AddBox(FFTBMeshData& D, const FTransform& Xf, const FVector& Half, float Bevel = 0.0f, float UVPerM = 1.0f);

	/** Six-sided block from 8 corners: bottom (-x-y, +x-y, +x+y, -x+y) then top in the same order (local space). */
	FLASHTOBANG_API void AddBlock(FFTBMeshData& D, const FTransform& Xf, const FVector Corners[8], float UVPerM = 1.0f);

	/** Box whose top is scaled by TopScale (x, y) and shifted by TopOffset: turrets, roofs (TopScale.Y = 0 makes a gable prism). */
	FLASHTOBANG_API void AddTapered(FFTBMeshData& D, const FTransform& Xf, const FVector& Half, const FVector2D& TopScale, const FVector2D& TopOffset = FVector2D::ZeroVector, float UVPerM = 1.0f);

	/** Cylinder along local Z, centred. Smooth sides. */
	FLASHTOBANG_API void AddCylinder(FFTBMeshData& D, const FTransform& Xf, float Radius, float HalfHeight, int32 Segments, bool bCaps = true, float TopRadius = -1.0f, float UVPerM = 1.0f);

	/** Surface of revolution around local Z from (radius, z) profile points, bottom to top. */
	FLASHTOBANG_API void AddLathe(FFTBMeshData& D, const FTransform& Xf, const TArray<FVector2D>& Profile, int32 Segments, float UVPerM = 1.0f);

	/** Upright card (quad) with explicit UV rectangle, for foliage. Width/height in world units, base at Centre. */
	FLASHTOBANG_API void AddCard(FFTBMeshData& D, const FVector& Base, float Width, float Height, float YawDeg, const FVector2D& UVMin, const FVector2D& UVMax);

	/** Copy pushed out along its normals (silhouette shells). */
	FLASHTOBANG_API FFTBMeshData Inflated(const FFTBMeshData& D, float Distance);

	/** Writes the data into a mesh section. */
	FLASHTOBANG_API void Commit(UProceduralMeshComponent* Mesh, int32 Section, const FFTBMeshData& D, UMaterialInterface* Material, bool bCollision);

	/** Minimal glTF 2.0 writer (embedded buffer) for asset-library exports. Meshes are named; positions go UE -> glTF axes. */
	FLASHTOBANG_API bool ExportGLTF(const FString& FilePath, const TArray<TPair<FString, const FFTBMeshData*>>& Meshes);
}
