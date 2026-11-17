// Flash to Bang - valley builder.

#include "World/FTBWorldBuilder.h"
#include "FlashtoBang.h"
#include "Core/FTBRoundConfig.h"
#include "Core/FTBTerrain.h"
#include "World/FTBTank.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"

using namespace FTBMesh;

namespace
{
	FVector M(float X, float Y, float Z) { return FVector(X, Y, Z) * 100.0f; }

	FFTBMeshData& Get(FFTBMeshSet& Set, EFTBMat Mat) { return Set[Mat]; }

	/** Gable roof prism along local X, sitting on Z = 0 of Xf. */
	void AddGable(FFTBMeshData& D, const FTransform& Xf, float HalfL, float HalfW, float Height, float Overhang, float UV)
	{
		AddTapered(D, FTransform(FVector(0, 0, Height * 0.5f)) * Xf, FVector(HalfL + Overhang, HalfW + Overhang, Height * 0.5f), FVector2D(1.0f, 0.02f), FVector2D::ZeroVector, UV);
	}
}

AFTBWorldBuilder::AFTBWorldBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	TerrainMesh->SetupAttachment(RootComponent);
	TerrainMesh->bUseComplexAsSimpleCollision = true;
	TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
	EnvMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Environment"));
	EnvMesh->SetupAttachment(RootComponent);
	EnvMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FoliageMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Foliage"));
	FoliageMesh->SetupAttachment(RootComponent);
	FoliageMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// ---------------------------------------------------------------------------------------------
// Terrain
// ---------------------------------------------------------------------------------------------

void AFTBWorldBuilder::BuildTerrain(const FFTBTerrainParams& T)
{
	auto Axis = [this](float Min, float Max, float FMin, float FMax)
	{
		TArray<float> Out;
		float V = Min;
		while (V < Max)
		{
			Out.Add(V);
			const bool bFine = V >= FMin - 1.0f && V < FMax;
			V += bFine ? FineCellM : CoarseCellM;
			if (V > FMin && Out.Last() < FMin && !bFine) { V = FMin; }
		}
		Out.Add(Max);
		return Out;
	};
	const TArray<float> Xs = Axis(TerrainMinM.X, TerrainMaxM.X, FineMinM.X, FineMaxM.X);
	const TArray<float> Ys = Axis(TerrainMinM.Y, TerrainMaxM.Y, FineMinM.Y, FineMaxM.Y);
	const int32 NX = Xs.Num(), NY = Ys.Num();

	FFTBMeshData D;
	D.Verts.Reserve(NX * NY);
	for (int32 j = 0; j < NY; ++j)
	{
		for (int32 i = 0; i < NX; ++i)
		{
			const float X = Xs[i], Y = Ys[j];
			const float H = FTBTerrain::HeightM(T, X, Y);
			const float E = 0.5f;
			const float DX = FTBTerrain::HeightM(T, X + E, Y) - FTBTerrain::HeightM(T, X - E, Y);
			const float DY = FTBTerrain::HeightM(T, X, Y + E) - FTBTerrain::HeightM(T, X, Y - E);
			D.Verts.Add(M(X, Y, H));
			D.Normals.Add(FVector(-DX, -DY, 2.0f * E).GetSafeNormal());
			D.UVs.Add(FVector2D(X, Y) * 0.25f);
			D.Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));
		}
	}
	for (int32 j = 0; j + 1 < NY; ++j)
	{
		for (int32 i = 0; i + 1 < NX; ++i)
		{
			const int32 A = j * NX + i, B = A + 1, C = A + NX, E = C + 1;
			// Up-facing: Cross(B-A, C-A) must point down (UE front-face rule).
			D.Tris.Append({ A, C, B, B, C, E });
		}
	}
	Commit(TerrainMesh, 0, D, FTBArt::Mat(EFTBMat::Terrain), true);
	UE_LOG(LogFTB, Log, TEXT("[FTB] Terrain %d x %d = %d verts"), NX, NY, D.Verts.Num());
}

// ---------------------------------------------------------------------------------------------
// Road and river
// ---------------------------------------------------------------------------------------------

void AFTBWorldBuilder::BuildRoad(const FFTBRoadPath& Road, const FFTBTerrainParams& T, FFTBMeshSet& Set) const
{
	FFTBMeshData& D = Get(Set, EFTBMat::Mud);
	const float Step = 400.0f;
	const int32 N = FMath::CeilToInt(Road.Length / Step);
	const float HW = RoadHalfWidthM * 100.0f;
	const int32 Base = D.Verts.Num();
	for (int32 k = 0; k <= N; ++k)
	{
		const float S = FMath::Min(Road.Length, k * Step);
		const FVector C = Road.At(S);
		const FVector Dir = Road.DirAt(S);
		const FVector Side(-Dir.Y, Dir.X, 0.0f);
		for (int32 e = 0; e < 2; ++e)
		{
			FVector P = C + Side * (e == 0 ? -HW : HW);
			const bool bOnBridge = S > Road.BridgeS - 100.0f && S < Road.BridgeEndS + 100.0f;
			P.Z = bOnBridge ? C.Z + 2.0f : FMath::Max(FTBTerrain::HeightUU(T, P), C.Z - 30.0f) + 14.0f;
			D.Verts.Add(P);
			D.Normals.Add(FVector::UpVector);
			D.UVs.Add(FVector2D(e == 0 ? 0.0f : RoadHalfWidthM * 2.0f, S * 0.01f) * 0.25f);
			D.Tangents.Add(FProcMeshTangent(Side, false));
		}
	}
	for (int32 k = 0; k < N; ++k)
	{
		const int32 A = Base + k * 2, B = A + 1, C = A + 2, E = A + 3;
		// Orient each strip so it faces up.
		const FVector Cr = FVector::CrossProduct(D.Verts[B] - D.Verts[A], D.Verts[C] - D.Verts[A]);
		if (Cr.Z < 0.0f) { D.Tris.Append({ A, B, C, B, E, C }); }
		else { D.Tris.Append({ A, C, B, B, C, E }); }
	}
}

void AFTBWorldBuilder::BuildRiver(const FFTBTerrainParams& T, FFTBMeshSet& Set) const
{
	FFTBMeshData& D = Get(Set, EFTBMat::Water);
	const float HW = T.RiverHalfWidthM * 1.55f;
	const int32 Base = D.Verts.Num();
	int32 Count = 0;
	for (float X = TerrainMinM.X; X <= TerrainMaxM.X; X += 15.0f, ++Count)
	{
		const float Y = FTBTerrain::RiverY(T, X);
		const float DYDX = T.RiverWaveM / T.RiverWaveLengthM * FMath::Cos(X / T.RiverWaveLengthM);
		const FVector2D Side = FVector2D(-DYDX, 1.0f).GetSafeNormal();
		for (int32 e = 0; e < 2; ++e)
		{
			const float Sgn = e == 0 ? -1.0f : 1.0f;
			D.Verts.Add(M(X + Side.X * HW * Sgn, Y + Side.Y * HW * Sgn, T.WaterLevelM));
			D.Normals.Add(FVector::UpVector);
			D.UVs.Add(FVector2D(X, Y + Sgn * HW) * 0.05f);
			D.Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));
		}
	}
	for (int32 k = 0; k + 1 < Count; ++k)
	{
		const int32 A = Base + k * 2, B = A + 1, C = A + 2, E = A + 3;
		const FVector Cr = FVector::CrossProduct(D.Verts[B] - D.Verts[A], D.Verts[C] - D.Verts[A]);
		if (Cr.Z < 0.0f) { D.Tris.Append({ A, B, C, B, E, C }); }
		else { D.Tris.Append({ A, C, B, B, C, E }); }
	}
}

bool AFTBWorldBuilder::IsClear(const FVector2D& PM, float Margin, const FFTBRoadPath& Road, const FFTBTerrainParams& T) const
{
	float RoadDist = 0.0f;
	Road.Project(FVector(PM.X * 100.0f, PM.Y * 100.0f, 0.0f), RoadDist);
	if (RoadDist < Margin * 100.0f) { return false; }
	if (FMath::Abs(PM.Y - FTBTerrain::RiverY(T, PM.X)) < T.RiverHalfWidthM * 2.0f + Margin * 0.5f) { return false; }
	for (const FVector2D& O : Occupied)
	{
		if (FVector2D::Distance(O, PM) < Margin) { return false; }
	}
	return true;
}

bool AFTBWorldBuilder::BlocksSightline(const FVector2D& PM, float BandM, const FFTBRoadPath& Road) const
{
	if (!Road.IsValid())
	{
		return false;
	}
	const FVector P(PM.X * 100.0f, PM.Y * 100.0f, 0.0f);
	float Dist = 0.0f;
	const float S = Road.Project(P, Dist);
	if (S > Road.BridgeEndS || Dist > BandM * 100.0f)
	{
		return false;
	}
	const FVector Q = Road.At(S);
	const FVector Dir = Road.DirAt(S);
	const FVector C(PostCentreM.X * 100.0f, PostCentreM.Y * 100.0f, 0.0f);
	const float SideP = Dir.X * (P.Y - Q.Y) - Dir.Y * (P.X - Q.X);
	const float SideC = Dir.X * (C.Y - Q.Y) - Dir.Y * (C.X - Q.X);
	return SideP * SideC > 0.0f;
}

// ---------------------------------------------------------------------------------------------
// Structures
// ---------------------------------------------------------------------------------------------

void AFTBWorldBuilder::BuildRiverBanks(const FFTBTerrainParams& T, const FFTBRoadPath& Road, FFTBMeshSet& Set, FFTBMeshSet& Foliage, FRandomStream& Rng) const
{
	// A muddy margin where water meets grass (sloping strip on each bank) and reed clumps along it.
	FFTBMeshData& D = Get(Set, EFTBMat::Mud);
	const float Inner = T.RiverHalfWidthM * 1.45f;
	const float Outer = T.RiverHalfWidthM * 1.45f + BankWidthM;
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const int32 Base = D.Verts.Num();
		int32 Count = 0;
		for (float X = TerrainMinM.X; X <= TerrainMaxM.X; X += 15.0f, ++Count)
		{
			const float Y = FTBTerrain::RiverY(T, X);
			const float DYDX = T.RiverWaveM / T.RiverWaveLengthM * FMath::Cos(X / T.RiverWaveLengthM);
			const FVector2D N = FVector2D(-DYDX, 1.0f).GetSafeNormal() * Side;
			for (int32 e = 0; e < 2; ++e)
			{
				const float Off = e == 0 ? Inner : Outer;
				const FVector2D P = FVector2D(X, Y) + N * Off;
				const float Z = e == 0 ? T.WaterLevelM + 0.05f : FTBTerrain::HeightM(T, P.X, P.Y) + 0.08f;
				D.Verts.Add(M(P.X, P.Y, Z));
				D.Normals.Add(FVector::UpVector);
				D.UVs.Add(FVector2D(X, Off) * 0.2f);
				D.Tangents.Add(FProcMeshTangent(FVector(1, 0, 0), false));
			}
		}
		for (int32 k = 0; k + 1 < Count; ++k)
		{
			const int32 A = Base + k * 2, B = A + 1, C = A + 2, E = A + 3;
			const FVector Cr = FVector::CrossProduct(D.Verts[B] - D.Verts[A], D.Verts[C] - D.Verts[A]);
			if (Cr.Z < 0.0f) { D.Tris.Append({ A, B, C, B, E, C }); }
			else { D.Tris.Append({ A, C, B, B, C, E }); }
		}
	}
	FFTBMeshData& Reeds = Get(Foliage, EFTBMat::Reeds);
	for (float X = TerrainMinM.X; X < TerrainMaxM.X; X += Rng.FRandRange(6.0f, 14.0f))
	{
		const float Side = Rng.FRand() < 0.5f ? -1.0f : 1.0f;
		const float Y = FTBTerrain::RiverY(T, X) + Side * (Inner + Rng.FRandRange(0.5f, BankWidthM * 0.7f));
		float RoadDist = 0.0f;
		Road.Project(M(X, Y, 0), RoadDist);
		if (RoadDist < 1200.0f || BlocksSightline(FVector2D(X, Y), SightlineBandLowM, Road))
		{
			continue;
		}
		const FVector G = M(X, Y, FTBTerrain::HeightM(T, X, Y));
		const float Sz = Rng.FRandRange(1.2f, 2.0f) * 100.0f;
		const float Yaw = Rng.FRandRange(0.0f, 180.0f);
		for (int32 K = 0; K < 2; ++K)
		{
			AddCard(Reeds, G - FVector(0, 0, 20), Sz * 1.5f, Sz, Yaw + K * 90.0f, FVector2D(0.5f, 0.7f), FVector2D(1.0f, 1.0f)); // lower shrub cell
		}
	}
}

void AFTBWorldBuilder::BuildChurch(FFTBMeshSet& Set, const FVector& G)
{
	FFTBMeshData& Stone = Get(Set, EFTBMat::Stone);
	FFTBMeshData& Roof = Get(Set, EFTBMat::Roof);
	FFTBMeshData& Wood = Get(Set, EFTBMat::PlanksDark);
	const float UV = 0.8f;
	const FTransform O(G);
	// Tower: 6 m square shaft to the belfry floor at 17.8 m, plinth and string courses.
	AddBox(Stone, FTransform(M(0, 0, 0.6f)) * O, M(3.6f, 3.6f, 0.6f), 8.0f, UV);
	AddBox(Stone, FTransform(M(0, 0, 9.0f)) * O, M(3.0f, 3.0f, 8.8f), 6.0f, UV);
	AddBox(Stone, FTransform(M(0, 0, 9.0f)) * O, M(3.15f, 3.15f, 0.18f), 4.0f, UV);
	AddBox(Stone, FTransform(M(0, 0, 17.6f)) * O, M(3.3f, 3.3f, 0.25f), 5.0f, UV);
	// Corner buttresses.
	for (int32 Sx = -1; Sx <= 1; Sx += 2)
	{
		for (int32 Sy = -1; Sy <= 1; Sy += 2)
		{
			// Stepped (not tapered) so the brick never stretches into a smeared wedge.
			AddBox(Stone, FTransform(M(Sx * 3.05f, Sy * 3.05f, 2.5f)) * O, M(0.55f, 0.55f, 2.5f), 3.0f, UV);
			AddBox(Stone, FTransform(M(Sx * 3.0f, Sy * 3.0f, 6.8f)) * O, M(0.4f, 0.4f, 1.8f), 3.0f, UV);
		}
	}
	// Belfry: four corner piers, low parapet, lintel ring; open on all sides for the view.
	for (int32 Sx = -1; Sx <= 1; Sx += 2)
	{
		for (int32 Sy = -1; Sy <= 1; Sy += 2)
		{
			AddBox(Stone, FTransform(M(Sx * 2.75f, Sy * 2.75f, 20.0f)) * O, M(0.3f, 0.3f, 2.2f), 4.0f, UV);
		}
	}
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const float Yaw = Side * 90.0f;
		const FTransform R(FRotator(0, Yaw, 0));
		AddBox(Stone, FTransform(M(2.75f, 0, 18.18f)) * R * O, M(0.2f, 2.3f, 0.26f), 3.0f, UV);   // parapet (low: the view is over it)
		AddBox(Stone, FTransform(M(2.75f, 0, 22.3f)) * R * O, M(0.3f, 3.0f, 0.22f), 3.0f, UV);    // lintel
	}
	AddBox(Wood, FTransform(M(0, 0, 17.92f)) * O, M(2.4f, 2.4f, 0.06f), 1.0f, UV);                // belfry floor boards
	AddBox(Wood, FTransform(M(-1.6f, -1.6f, 20.6f)) * O, M(0.12f, 0.12f, 1.2f), 1.0f, UV);        // bell frame post
	// Spire.
	AddTapered(Roof, FTransform(M(0, 0, 26.0f)) * O, M(3.4f, 3.4f, 3.5f), FVector2D(0.04f, 0.04f), FVector2D::ZeroVector, 0.6f);
	// Nave running south behind the tower, with a gable roof, and a small apse.
	AddBox(Stone, FTransform(M(0, -14.0f, 4.5f)) * O, M(5.2f, 11.0f, 4.5f), 6.0f, UV);
	AddGable(Roof, FTransform(FRotator(0, 90, 0), M(0, -14.0f, 9.0f)) * O, 1100.0f, 520.0f, 450.0f, 50.0f, 0.6f);
	AddCylinder(Stone, FTransform(M(0, -26.0f, 3.5f)) * O, 380.0f, 350.0f, 10, true, -1.0f, UV);
	AddTapered(Roof, FTransform(M(0, -26.0f, 8.0f)) * O, M(4.0f, 4.0f, 1.2f), FVector2D(0.1f, 0.1f), FVector2D::ZeroVector, 0.6f);
	// Windows (dark recesses) along the nave.
	for (int32 W = 0; W < 4; ++W)
	{
		for (int32 S = -1; S <= 1; S += 2)
		{
			AddBox(Wood, FTransform(M(S * 5.22f, -7.0f - W * 4.4f, 5.0f)) * O, M(0.05f, 0.55f, 1.4f), 0.0f, UV);
		}
	}
	AddBox(Wood, FTransform(M(0, 3.02f, 1.6f)) * O, M(0.8f, 0.06f, 1.5f), 0.0f, UV);             // tower door
	// A few headstones (the long churchyard wall was dropped 2026-11-16: half-buried in the slope it read as a ramp).
	for (int32 K = 0; K < 8; ++K)
	{
		AddBox(Stone, FTransform(FRotator(0, (K % 3) * 4.0f - 4.0f, (K % 2) * 3.0f), M(-8.5f + (K % 4) * 1.4f, -4.0f - (K / 4) * 3.0f, 0.45f)) * O, M(0.1f, 0.35f, 0.5f), 2.0f, UV);
	}
}

void AFTBWorldBuilder::BuildHaystack(FFTBMeshSet& Set, const FVector& G, FRandomStream& Rng, const FFTBTerrainParams* Terrain)
{
	// Local ground offset (cm) at a point relative to the stack centre.
	auto GroundAt = [&](const FVector& Local) { return Terrain ? FTBTerrain::HeightUU(*Terrain, G + Local) - G.Z : 0.0f; };
	FFTBMeshData& Straw = Get(Set, EFTBMat::Thatch);
	FFTBMeshData& Wood = Get(Set, EFTBMat::Planks);
	const FTransform O(G);
	// Solid rick: base sunk 2.5 m so it sits flush on the slope, bulged sides, a trodden flat top to lie on (4.75 m).
	TArray<FVector2D> Profile = { FVector2D(470, -250), FVector2D(462, 40), FVector2D(472, 170), FVector2D(458, 290), FVector2D(412, 385),
		FVector2D(330, 445), FVector2D(250, 468), FVector2D(120, 474), FVector2D(0, 475) };
	AddLathe(Straw, O, Profile, 48, 0.9f);
	// Binding ropes around the rick (following the profile radius) and two props leaning on it.
	FFTBMeshData& Rope = Get(Set, EFTBMat::PlanksDark);
	for (const float RopeZ : { 120.0f, 250.0f, 360.0f })
	{
		float R = 470.0f;
		for (int32 k = 0; k + 1 < Profile.Num(); ++k)
		{
			if (RopeZ >= Profile[k].Y && RopeZ <= Profile[k + 1].Y)
			{
				R = FMath::Lerp(Profile[k].X, Profile[k + 1].X, (RopeZ - Profile[k].Y) / FMath::Max(Profile[k + 1].Y - Profile[k].Y, 1.0f));
			}
		}
		AddCylinder(Rope, FTransform(FVector(0, 0, RopeZ)) * O, R + 3.0f, 3.0f, 48, false, -1.0f, 1.0f);
	}
	for (int32 K = 0; K < 2; ++K)
	{
		const float A = 150.0f + K * 70.0f;
		const FVector P = FRotator(0, A, 0).RotateVector(FVector(560, 0, 0));
		AddCylinder(Get(Set, EFTBMat::Planks), FTransform(FRotator(-16.0f, A + 180.0f, 0), P + FVector(0, 0, GroundAt(P) + 200)) * O, 7.0f, 240.0f, 8, true, 6.0f, 1.0f);
	}
	// Loose bales around it.
	for (int32 K = 0; K < 4; ++K)
	{
		const float A = 40.0f + K * 70.0f + Rng.FRandRange(-10.0f, 10.0f);
		const FVector P = FRotator(0, A, 0).RotateVector(FVector(720 + Rng.FRandRange(-60, 60), 0, 0));
		AddCylinder(Straw, FTransform(FRotator(0, A + 90.0f, 90.0f), P + FVector(0, 0, GroundAt(P) + 58.0f)) * O, 70.0f, 70.0f, 14, true, -1.0f, 0.5f);
	}
	// Ladder up the west side (where runs arrive), leaning on the stack, and a hay cart.
	const FTransform L(FRotator(-14.0f, 0, 0), FVector(-530, -80, 230));
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddBox(Wood, FTransform(FVector(0, S * 28.0f, 0)) * L * O, FVector(4, 4, 290), 1.0f, 1.0f);
	}
	for (int32 R = 0; R < 9; ++R)
	{
		AddBox(Wood, FTransform(FVector(0, 0, -260.0f + R * 62.0f)) * L * O, FVector(3, 28, 3), 0.5f, 1.0f);
	}
	const FTransform Cart(FRotator(0, 25.0f, 0), FVector(-850, -300, GroundAt(FVector(-850, -300, 0)) - 6.0f));
	AddBox(Wood, FTransform(FVector(0, 0, 110)) * Cart * O, FVector(180, 90, 8), 2.0f, 1.0f);
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddBox(Wood, FTransform(FVector(0, S * 90.0f, 145)) * Cart * O, FVector(180, 4, 35), 1.0f, 1.0f);
		AddCylinder(Wood, FTransform(FRotator(0, 0, 90), FVector(0, S * 100.0f, 70)) * Cart * O, 70.0f, 5.0f, 14, true, -1.0f, 1.0f);
	}
	AddBox(Wood, FTransform(FVector(260, 0, 100)) * Cart * O, FVector(120, 4, 4), 1.0f, 1.0f);
}

void AFTBWorldBuilder::BuildRidgePost(FFTBMeshSet& Set, const FVector& G, FRandomStream& Rng)
{
	FFTBMeshData& Rock = Get(Set, EFTBMat::Rock);
	FFTBMeshData& Bags = Get(Set, EFTBMat::Sandbag);
	FFTBMeshData& Wood = Get(Set, EFTBMat::PlanksDark);
	const FTransform O(G);
	// Outcrop of tilted boulders around the observer.
	for (int32 K = 0; K < 9; ++K)
	{
		const float A = K * 40.0f + Rng.FRandRange(-12, 12);
		const float R = (K % 3 == 0) ? 520.0f : 380.0f + Rng.FRandRange(0, 200);
		const FVector P = FRotator(0, A, 0).RotateVector(FVector(R, 0, 0));
		const FVector Size(Rng.FRandRange(120, 260), Rng.FRandRange(100, 220), Rng.FRandRange(70, 160));
		// Keep the view north open: no tall boulders in the front arc.
		const bool bFront = FMath::Abs(FRotator::NormalizeAxis(A - 90.0f)) < 55.0f;
		const float ZScale = bFront ? 0.45f : 1.0f;
		AddBox(Rock, FTransform(FRotator(Rng.FRandRange(-14, 14), A, Rng.FRandRange(-14, 14)), P + FVector(0, 0, Size.Z * 0.3f * ZScale)) * O, FVector(Size.X, Size.Y, Size.Z * ZScale), 40.0f, 0.6f);
	}
	// Sandbag crescent in front (north), low enough to look over lying down.
	for (int32 Row = 0; Row < 2; ++Row)
	{
		for (int32 K = 0; K < 9; ++K)
		{
			const float A = 50.0f + K * 10.0f + (Row ? 5.0f : 0.0f);
			const FVector P = FRotator(0, A, 0).RotateVector(FVector(230, 0, 0));
			AddBox(Bags, FTransform(FRotator(0, A + 90.0f, Rng.FRandRange(-3, 3)), P + FVector(0, 0, 18 + Row * 30)) * O, FVector(30, 20, 15), 9.0f, 1.2f);
		}
	}
	// A broken fence post and a field telephone crate.
	AddBox(Wood, FTransform(FRotator(6, 0, -4), FVector(-160, -120, 70)) * O, FVector(8, 8, 70), 2.0f, 1.0f);
	AddBox(Wood, FTransform(FVector(-60, -80, 15)) * O, FVector(22, 16, 15), 3.0f, 1.0f);
}

void AFTBWorldBuilder::BuildHouse(FFTBMeshSet& Set, const FVector& G, float Yaw, const FVector& S, EFTBMat Wall, EFTBMat RoofMat)
{
	FFTBMeshData& W = Get(Set, Wall);
	FFTBMeshData& R = Get(Set, RoofMat);
	FFTBMeshData& Dark = Get(Set, EFTBMat::PlanksDark);
	FFTBMeshData& Wood = Get(Set, EFTBMat::Planks);
	FFTBMeshData& Base = Get(Set, EFTBMat::Stone);
	const FTransform O(FRotator(0, Yaw, 0), G);
	const float UV = 0.75f;
	const float HX = S.X * 0.5f, HY = S.Y * 0.5f;
	// Walls sunk 0.6 m so slopes never show a gap, on a slightly proud rubble plinth.
	AddBox(W, FTransform(M(0, 0, S.Z * 0.5f - 0.6f)) * O, M(HX, HY, S.Z * 0.5f + 0.6f), 5.0f, UV);
	AddBox(Base, FTransform(M(0, 0, -0.2f)) * O, M(HX + 0.12f, HY + 0.12f, 0.75f), 4.0f, UV);
	// Roof with an overhang, fascia boards along the eaves and a ridge cap.
	const float RoofH = S.Y * 0.32f;
	AddGable(R, FTransform(M(0, 0, S.Z - 0.6f)) * O, HX * 100.0f, HY * 100.0f, RoofH * 100.0f, 45.0f, 0.6f);
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		AddBox(Dark, FTransform(M(0, Side * (HY + 0.45f), S.Z - 0.62f)) * O, M(HX + 0.5f, 0.04f, 0.12f), 1.0f, UV);
	}
	AddBox(Dark, FTransform(M(0, 0, S.Z - 0.6f + RoofH + 0.04f)) * O, M(HX + 0.45f, 0.14f, 0.07f), 2.0f, UV);
	// Chimney with a cap.
	const FVector Ch(S.X * 0.28f, S.Y * 0.18f, S.Z + S.Y * 0.2f);
	AddBox(W, FTransform(M(Ch.X, Ch.Y, Ch.Z)) * O, M(0.45f, 0.45f, 1.2f), 2.0f, UV);
	AddBox(Base, FTransform(M(Ch.X, Ch.Y, Ch.Z + 1.25f)) * O, M(0.55f, 0.55f, 0.08f), 2.0f, UV);
	// Framed windows with sills on both long sides; a door with frame and step on the first side.
	const int32 Windows = FMath::Max(1, FMath::FloorToInt(S.X / 3.0f));
	for (int32 K = 0; K < Windows; ++K)
	{
		const float X = -HX + (K + 0.5f) * S.X / Windows;
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const bool bDoor = Side < 0 && K == Windows / 2;
			const float Face = Side * (HY + 0.02f);
			const float Out = Side * (HY + 0.06f);
			if (bDoor)
			{
				AddBox(Dark, FTransform(M(X, Face, 0.95f)) * O, M(0.5f, 0.05f, 1.05f), 0.0f, UV);
				AddBox(Wood, FTransform(M(X - 0.58f, Out, 1.0f)) * O, M(0.07f, 0.06f, 1.12f), 1.0f, UV);
				AddBox(Wood, FTransform(M(X + 0.58f, Out, 1.0f)) * O, M(0.07f, 0.06f, 1.12f), 1.0f, UV);
				AddBox(Wood, FTransform(M(X, Out, 2.14f)) * O, M(0.66f, 0.07f, 0.07f), 1.0f, UV);
				AddBox(Base, FTransform(M(X, Side * (HY + 0.35f), -0.05f)) * O, M(0.75f, 0.32f, 0.1f), 2.0f, UV);
				continue;
			}
			const float Z = S.Z * 0.55f;
			AddBox(Dark, FTransform(M(X, Face, Z)) * O, M(0.45f, 0.05f, 0.6f), 0.0f, UV);
			AddBox(Wood, FTransform(M(X, Out, Z + 0.66f)) * O, M(0.56f, 0.06f, 0.06f), 1.0f, UV);
			AddBox(Wood, FTransform(M(X - 0.5f, Out, Z)) * O, M(0.06f, 0.06f, 0.66f), 1.0f, UV);
			AddBox(Wood, FTransform(M(X + 0.5f, Out, Z)) * O, M(0.06f, 0.06f, 0.66f), 1.0f, UV);
			AddBox(Wood, FTransform(M(X, Out, Z)) * O, M(0.45f, 0.03f, 0.03f), 0.5f, UV);
			AddBox(Base, FTransform(M(X, Side * (HY + 0.1f), Z - 0.66f)) * O, M(0.62f, 0.12f, 0.05f), 1.5f, UV);
		}
	}
}

void AFTBWorldBuilder::BuildBridge(FFTBMeshSet& Set, const FVector& A, const FVector& B, float DeckZ, float WaterZ, float BedZ)
{
	FFTBMeshData& Stone = Get(Set, EFTBMat::Stone);
	const FVector Mid = (A + B) * 0.5f;
	const FVector Dir = (B - A).GetSafeNormal2D();
	const float HalfLen = FVector::Dist2D(A, B) * 0.5f + 600.0f;
	const FTransform O(FRotator(0, Dir.Rotation().Yaw, 0), FVector(Mid.X, Mid.Y, DeckZ));
	const float UV = 0.8f;
	AddBox(Stone, FTransform(FVector(0, 0, -45)) * O, FVector(HalfLen, 460, 45), 6.0f, UV);
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddBox(Stone, FTransform(FVector(0, S * 430.0f, 50)) * O, FVector(HalfLen, 30, 52), 6.0f, UV);
		AddBox(Stone, FTransform(FVector(0, S * 445.0f, -150)) * O, FVector(HalfLen - 100.0f, 18, 120), 4.0f, UV);  // spandrel
	}
	// Two piers with cutwaters, down to the river bed.
	const float PierH = (DeckZ - BedZ) * 0.5f;
	for (int32 K = -1; K <= 1; K += 2)
	{
		const FVector P(K * HalfLen * 0.32f, 0, -PierH - 90.0f);
		AddBox(Stone, FTransform(P) * O, FVector(160, 420, PierH), 10.0f, UV);
		AddTapered(Stone, FTransform(P + FVector(0, 0, 0)) * O, FVector(160, 520, PierH * 0.7f), FVector2D(1.0f, 0.85f), FVector2D::ZeroVector, UV);
	}
	// Abutments into each bank.
	for (int32 K = -1; K <= 1; K += 2)
	{
		AddBox(Stone, FTransform(FVector(K * (HalfLen - 150.0f), 0, -(DeckZ - BedZ) * 0.5f)) * O, FVector(200, 470, (DeckZ - BedZ) * 0.5f), 10.0f, UV);
	}
	(void)WaterZ;
}

void AFTBWorldBuilder::AddTree(FFTBMeshSet& Set, const FVector& G, float HeightM, bool bConifer, bool bTrunk, FRandomStream& Rng)
{
	const EFTBMat LeafMat = bConifer ? EFTBMat::Conifer : (Rng.FRand() < 0.45f ? EFTBMat::Broadleaf2 : EFTBMat::Bush);
	FFTBMeshData& Leaves = Get(Set, LeafMat);
	const float H = HeightM * 100.0f * Rng.FRandRange(0.92f, 1.08f);
	// Square atlas cells (2026-11-17, T_FTB_Foliage 4096: baked CC0 Poly Haven trees, conifer included): square cards,
	// so every baked tree keeps its real proportions.
	const float W = H * Rng.FRandRange(0.9f, 1.1f);
	const float Yaw = Rng.FRandRange(0.0f, 180.0f);
	const FVector2D UVMin = bConifer ? FVector2D(0.0f, 0.0f) : (LeafMat == EFTBMat::Bush ? FVector2D(0.5f, 0.0f) : FVector2D(0.0f, 0.5f));
	const FVector2D UVMax = UVMin + FVector2D(0.5f, 0.5f);
	// Four crossed cards on one base (the baked trees carry their own trunk, so no offset inner set any more).
	for (int32 K = 0; K < 4; ++K)
	{
		AddCard(Leaves, G - FVector(0, 0, 40), W, H, Yaw + K * 45.0f, UVMin, UVMax);
	}
	if (bTrunk && bConifer)
	{
		AddCylinder(Get(Set, EFTBMat::PlanksDark), FTransform(G + FVector(0, 0, H * 0.2f)), H * 0.022f, H * 0.2f, 10, false, H * 0.012f, 1.0f);
	}
}

void AFTBWorldBuilder::AddBush(FFTBMeshSet& Set, const FVector& G, float SizeM, FRandomStream& Rng)
{
	FFTBMeshData& Leaves = Get(Set, EFTBMat::Bush);
	const float S = SizeM * 100.0f;
	const float Yaw = Rng.FRandRange(0.0f, 180.0f);
	for (int32 K = 0; K < 2; ++K)
	{
		AddCard(Leaves, G - FVector(0, 0, 30), S * 1.4f, S * Rng.FRandRange(0.65f, 0.85f), Yaw + K * 90.0f, FVector2D(0.5f, 0.5f), FVector2D(1.0f, 1.0f)); // shrub cell
	}
}

// ---------------------------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------------------------

void AFTBWorldBuilder::Build(const UFTBRoundConfig& Config, const FFTBRoadPath& Road, int32 Seed)
{
	const FFTBTerrainParams& T = Config.Terrain;
	FRandomStream Rng(Seed);
	Occupied.Reset();
	if (Config.Posts.Num() > 0)
	{
		PostCentreM = FVector2D::ZeroVector;
		for (const FFTBPostRow& Post : Config.Posts) { PostCentreM += Post.LocationM / Config.Posts.Num(); }
	}
	BuildTerrain(T);

	FFTBMeshSet Env;
	FFTBMeshSet Foliage;
	BuildRoad(Road, T, Env);
	BuildRiver(T, Env);
	BuildRiverBanks(T, Road, Env, Foliage, Rng);

	auto Ground = [&T](float XM, float YM) { return M(XM, YM, FTBTerrain::HeightM(T, XM, YM)); };

	// Posts.
	for (const FFTBPostRow& Post : Config.Posts)
	{
		const FVector G = Ground(Post.LocationM.X, Post.LocationM.Y);
		Occupied.Add(Post.LocationM);
		switch (Post.Kind)
		{
		case EFTBPostKind::ChurchTower: BuildChurch(Env, G); break;
		case EFTBPostKind::Haystack:    BuildHaystack(Env, G, Rng, &T); break;
		case EFTBPostKind::Ridge:       BuildRidgePost(Env, G, Rng); break;
		}
	}
	Occupied.Add(Config.Posts.Num() ? Config.Posts[0].LocationM + FVector2D(0, -20) : FVector2D::ZeroVector);

	// Bridge.
	if (Road.IsValid())
	{
		const FVector A = Road.At(Road.BridgeS);
		const FVector B = Road.At(Road.BridgeEndS);
		const FVector Mid = (A + B) * 0.5f;
		BuildBridge(Env, A, B, Config.BridgeDeckHeightM * 100.0f, T.WaterLevelM * 100.0f, FTBTerrain::HeightUU(T, Mid) - 100.0f);
		Occupied.Add(FVector2D(Mid) * 0.01f);
	}

	// Town on the near bank, along the road beyond the bridge.
	int32 Placed = 0;
	for (int32 Try = 0; Try < 400 && Placed < TownHouses; ++Try)
	{
		const FVector2D P(Rng.FRandRange(250.0f, 700.0f), Rng.FRandRange(390.0f, 610.0f));
		if (!IsClear(P, 15.0f, Road, T)) { continue; }
		float RoadDist = 0.0f;
		const float S = Road.Project(FVector(P * 100.0f, 0.0f), RoadDist);
		const float Yaw = Road.DirAt(S).Rotation().Yaw + (Rng.FRand() < 0.5f ? 0.0f : 90.0f);
		const FVector Size(Rng.FRandRange(8.0f, 13.0f), Rng.FRandRange(6.0f, 8.0f), Rng.FRandRange(5.5f, 8.0f));
		BuildHouse(Env, Ground(P.X, P.Y), Yaw, Size, Rng.FRand() < 0.5f ? EFTBMat::Plaster : EFTBMat::StonePale, Rng.FRand() < 0.6f ? EFTBMat::RoofWarm : EFTBMat::Roof);
		Occupied.Add(P);
		++Placed;
	}

	// Farmsteads across the valley.
	Placed = 0;
	for (int32 Try = 0; Try < 600 && Placed < FarmCount; ++Try)
	{
		const FVector2D P(Rng.FRandRange(-1700.0f, 1300.0f), Rng.FRandRange(500.0f, 2000.0f));
		if (!IsClear(P, 70.0f, Road, T) || (FMath::Abs(P.X) < 120.0f && P.Y < 700.0f) || BlocksSightline(P, SightlineBandTallM, Road)) { continue; }
		const float Yaw = Rng.FRandRange(0.0f, 90.0f);
		BuildHouse(Env, Ground(P.X, P.Y), Yaw, FVector(10.0f, 7.0f, 6.5f), EFTBMat::StonePale, EFTBMat::RoofWarm);
		const FVector Off = FRotator(0, Yaw, 0).RotateVector(M(4.0f, 16.0f, 0.0f));
		BuildHouse(Env, Ground(P.X + Off.X * 0.01f, P.Y + Off.Y * 0.01f), Yaw + 90.0f, FVector(16.0f, 10.0f, 7.5f), EFTBMat::Planks, EFTBMat::Roof);
		for (int32 K = 0; K < 4; ++K)
		{
			const FVector2D TP = P + FVector2D(Rng.FRandRange(-30, 30), Rng.FRandRange(-30, 30));
			if (IsClear(TP, 12.0f, Road, T) && !BlocksSightline(TP, SightlineBandTallM, Road))
			{
				AddTree(Foliage, Ground(TP.X, TP.Y), Rng.FRandRange(10.0f, 16.0f), Rng.FRand() < 0.3f, true, Rng);
			}
		}
		Occupied.Add(P);
		++Placed;
	}

	// Hedgerows: field boundaries that break the valley into readable distance bands.
	for (int32 H = 0; H < HedgeLines; ++H)
	{
		const FVector2D Start(Rng.FRandRange(-1900.0f, 1500.0f), Rng.FRandRange(450.0f, 2050.0f));
		const float Ang = Rng.FRand() < 0.5f ? Rng.FRandRange(-15.0f, 15.0f) : Rng.FRandRange(70.0f, 110.0f);
		const FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(Ang)), FMath::Sin(FMath::DegreesToRadians(Ang)));
		const float Len = Rng.FRandRange(140.0f, 360.0f);
		for (float D = 0.0f; D < Len; D += 6.5f)
		{
			const FVector2D P = Start + Dir * D + FVector2D(Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-1.0f, 1.0f));
			float RoadDist = 0.0f;
			Road.Project(FVector(P * 100.0f, 0.0f), RoadDist);
			if (RoadDist < 1200.0f || FMath::Abs(P.Y - FTBTerrain::RiverY(T, P.X)) < T.RiverHalfWidthM * 1.6f || BlocksSightline(P, SightlineBandLowM, Road)) { continue; }
			AddBush(Foliage, Ground(P.X, P.Y), Rng.FRandRange(3.5f, 5.5f), Rng);
			if (Rng.FRand() < 0.07f && !BlocksSightline(P, SightlineBandTallM, Road))
			{
				AddTree(Foliage, Ground(P.X, P.Y), Rng.FRandRange(11.0f, 17.0f), false, false, Rng);
			}
		}
	}

	// Groves in the valley, and willows along the river.
	for (int32 G = 0; G < Groves; ++G)
	{
		const FVector2D C(Rng.FRandRange(-2000.0f, 1700.0f), Rng.FRandRange(450.0f, 2100.0f));
		const int32 Count = Rng.RandRange(8, 22);
		const bool bConifer = Rng.FRand() < 0.6f;
		for (int32 K = 0; K < Count; ++K)
		{
			const FVector2D P = C + FVector2D(Rng.FRandRange(-60, 60), Rng.FRandRange(-45, 45));
			if (IsClear(P, 14.0f, Road, T) && !BlocksSightline(P, SightlineBandTallM, Road))
			{
				AddTree(Foliage, Ground(P.X, P.Y), Rng.FRandRange(12.0f, 22.0f), bConifer, false, Rng);
			}
		}
	}
	for (float X = TerrainMinM.X; X < TerrainMaxM.X; X += Rng.FRandRange(25.0f, 70.0f))
	{
		const float Y = FTBTerrain::RiverY(T, X) + (Rng.FRand() < 0.5f ? -1.0f : 1.0f) * (T.RiverHalfWidthM * 1.9f + Rng.FRandRange(0.0f, 6.0f));
		if (FMath::Abs(X - 320.0f) > 60.0f)
		{
			float RoadDist = 0.0f;
			Road.Project(M(X, Y, 0), RoadDist);
			if (RoadDist > 1500.0f && !BlocksSightline(FVector2D(X, Y), SightlineBandTallM, Road))
			{
				AddTree(Foliage, Ground(X, Y), Rng.FRandRange(9.0f, 14.0f), false, false, Rng);
			}
		}
	}

	// Treeline on the far hills (silhouettes through the fog) and on the flanks.
	for (int32 K = 0; K < BackdropTrees; ++K)
	{
		const FVector2D P(Rng.FRandRange(TerrainMinM.X, TerrainMaxM.X), Rng.FRandRange(2050.0f, 2900.0f));
		AddTree(Foliage, Ground(P.X, P.Y), Rng.FRandRange(16.0f, 26.0f), Rng.FRand() < 0.75f, false, Rng);
	}

	// Hillside around the posts: a few full trees with trunks and some scrub, kept out of the view arc.
	for (int32 K = 0; K < 40; ++K)
	{
		const FVector2D P(Rng.FRandRange(-380.0f, 520.0f), Rng.FRandRange(-260.0f, 300.0f));
		bool bNearPost = false;
		for (const FFTBPostRow& Post : Config.Posts)
		{
			bNearPost |= FVector2D::Distance(P, Post.LocationM) < 35.0f;
		}
		// Nothing tall directly north of a post (the view into the valley).
		bool bInView = false;
		for (const FFTBPostRow& Post : Config.Posts)
		{
			const FVector2D D = P - Post.LocationM;
			bInView |= D.Y > 0.0f && FMath::Abs(D.X) < D.Y * 1.2f + 20.0f;
		}
		if (bNearPost) { continue; }
		if (bInView)
		{
			AddBush(Foliage, Ground(P.X, P.Y), Rng.FRandRange(1.2f, 2.2f), Rng);
		}
		else
		{
			AddTree(Foliage, Ground(P.X, P.Y), Rng.FRandRange(9.0f, 15.0f), Rng.FRand() < 0.4f, true, Rng);
		}
	}

	// Telegraph poles along the road: depth cues every 60 m.
	if (Road.IsValid())
	{
		FFTBMeshData& Poles = Get(Env, EFTBMat::PlanksDark);
		for (float S = 0.0f; S < Road.BridgeS - 2000.0f; S += 6000.0f)
		{
			const FVector C = Road.At(S);
			const FVector Dir = Road.DirAt(S);
			FVector P = C + FVector(-Dir.Y, Dir.X, 0.0f) * 900.0f;
			P.Z = FTBTerrain::HeightUU(T, P);
			AddCylinder(Poles, FTransform(P + FVector(0, 0, 350)), 12.0f, 380.0f, 8, true, 9.0f, 1.0f);
			AddBox(Poles, FTransform(FRotator(0, Dir.Rotation().Yaw + 90.0f, 0), P + FVector(0, 0, 680)), FVector(6, 90, 6), 1.0f, 1.0f);
		}
	}

	int32 Section = 0;
	for (int32 K = 0; K < static_cast<int32>(EFTBMat::Count); ++K)
	{
		if (!Env.Data[K].IsEmpty()) { Commit(EnvMesh, Section++, Env.Data[K], FTBArt::Mat(static_cast<EFTBMat>(K)), false); }
	}
	Section = 0;
	for (int32 K = 0; K < static_cast<int32>(EFTBMat::Count); ++K)
	{
		if (!Foliage.Data[K].IsEmpty()) { Commit(FoliageMesh, Section++, Foliage.Data[K], FTBArt::Mat(static_cast<EFTBMat>(K)), false); }
	}
	FoliageMesh->SetCastShadow(true);
	UE_LOG(LogFTB, Log, TEXT("[FTB] World built: env sections %d, foliage sections %d"), Env.NumUsed(), Foliage.NumUsed());
}

void AFTBWorldBuilder::ExportModels(const FString& Directory, const UFTBRoundConfig& Config)
{
	IFileManager::Get().MakeDirectory(*Directory, true);
	FRandomStream Rng(7);
	auto ExportSet = [&Directory](const FString& Name, const FFTBMeshSet& Set)
	{
		TArray<TPair<FString, const FFTBMeshData*>> Parts;
		for (int32 K = 0; K < static_cast<int32>(EFTBMat::Count); ++K)
		{
			if (Set.Data[K].IsEmpty()) { continue; }
			FString MatName = FPaths::GetBaseFilename(FString(FTBArt::MatPath(static_cast<EFTBMat>(K))));
			MatName.Split(TEXT("."), &MatName, nullptr);
			Parts.Add(TPair<FString, const FFTBMeshData*>(Name + TEXT("_") + MatName, &Set.Data[K]));
		}
		const FString File = Directory / (Name + TEXT(".gltf"));
		const bool bOk = FTBMesh::ExportGLTF(File, Parts);
		UE_LOG(LogFTB, Log, TEXT("[FTB] Export %s -> %s"), *File, bOk ? TEXT("ok") : TEXT("FAILED"));
	};
	{
		FFTBMeshSet S; BuildChurch(S, FVector::ZeroVector); ExportSet(TEXT("FTB_ChurchTower"), S);
	}
	{
		FFTBMeshSet S; BuildHaystack(S, FVector::ZeroVector, Rng); ExportSet(TEXT("FTB_Haystack"), S);
	}
	{
		FFTBMeshSet S; BuildRidgePost(S, FVector::ZeroVector, Rng); ExportSet(TEXT("FTB_RidgePost"), S);
	}
	{
		FFTBMeshSet S; BuildHouse(S, FVector::ZeroVector, 0.0f, FVector(10, 7, 6.5f), EFTBMat::StonePale, EFTBMat::RoofWarm); ExportSet(TEXT("FTB_Farmhouse"), S);
	}
	{
		FFTBMeshSet S; BuildBridge(S, FVector(0, -2600, 0), FVector(0, 2600, 0), 450.0f, -160.0f, -600.0f); ExportSet(TEXT("FTB_StoneBridge"), S);
	}
	{
		FFTBMeshSet S; AddTree(S, FVector::ZeroVector, 14.0f, true, true, Rng); AddTree(S, FVector(800, 0, 0), 12.0f, false, true, Rng); ExportSet(TEXT("FTB_TreeCards"), S);
	}
	{
		FFTBMeshData Hull, Tracks, Turret, TurretDetail, Barrel;
		AFTBTank::BuildParts(Hull, Tracks, Turret, TurretDetail, Barrel);
		TArray<TPair<FString, const FFTBMeshData*>> Parts = {
			{ TEXT("Tank_Hull"), &Hull }, { TEXT("Tank_Tracks"), &Tracks }, { TEXT("Tank_Turret"), &Turret },
			{ TEXT("Tank_TurretDetail"), &TurretDetail }, { TEXT("Tank_Barrel"), &Barrel } };
		FTBMesh::ExportGLTF(Directory / TEXT("FTB_Tank.gltf"), Parts);
	}
	(void)Config;
}
