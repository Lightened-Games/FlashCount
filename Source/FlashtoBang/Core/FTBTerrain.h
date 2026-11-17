// Flash to Bang - analytic terrain and road path shared by the world builder, tanks, ranging and the bot.

#pragma once

#include "CoreMinimal.h"
#include "Core/FTBTypes.h"
#include "FTBTerrain.generated.h"

namespace FTBTerrain
{
	/** River centre line Y (metres) at X (metres). */
	inline float RiverY(const FFTBTerrainParams& P, float XM)
	{
		return P.RiverBaseY + P.RiverWaveM * FMath::Sin(XM / P.RiverWaveLengthM);
	}

	inline float Smooth(float Edge0, float Edge1, float X)
	{
		const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Terrain height in metres at (X, Y) metres. */
	inline float HeightM(const FFTBTerrainParams& P, float X, float Y)
	{
		float H = 1.6f + P.ValleyUndulationM * (FMath::Sin(X / 170.0f) * FMath::Cos(Y / 230.0f) + 0.55f * FMath::Sin((X + Y) / 95.0f));

		// The observers' hill: low on the left (church), high on the right (ridge).
		const float HillHeight = FMath::Lerp(P.HillLowM, P.HillHighM, FMath::Clamp((X + 450.0f) / 800.0f, 0.0f, 1.0f));
		const float HillRise = Smooth(P.HillFootY, P.HillCrestY, Y);
		const float BehindCrest = 1.0f - 0.55f * Smooth(P.HillCrestY - 60.0f, P.HillCrestY - 420.0f, Y);
		H += HillHeight * HillRise * BehindCrest + 3.0f * FMath::Sin(X / 60.0f) * HillRise;

		// Far hills behind the valley and on its flanks give a silhouette through the fog.
		const float Backdrop = Smooth(P.BackdropStartY, P.BackdropStartY + 700.0f, Y);
		H += Backdrop * (P.BackdropHeightM + 35.0f * FMath::Sin(X / 210.0f) + 20.0f * FMath::Sin(X / 77.0f + 1.3f));
		H += 70.0f * Smooth(1500.0f, 2200.0f, FMath::Abs(X - 150.0f)) * (1.0f - HillRise);

		// River channel.
		const float D = FMath::Abs(Y - RiverY(P, X));
		H -= P.RiverDepthM * (1.0f - Smooth(P.RiverHalfWidthM * 0.6f, P.RiverHalfWidthM * 1.9f, D));
		return H;
	}

	inline float HeightUU(const FFTBTerrainParams& P, const FVector& LocationUU)
	{
		return HeightM(P, LocationUU.X * 0.01f, LocationUU.Y * 0.01f) * 100.0f;
	}
}

/** Polyline road with arc-length lookup (world units). */
USTRUCT(BlueprintType)
struct FFTBRoadPath
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Road") TArray<FVector> Points;
	UPROPERTY(BlueprintReadOnly, Category = "Road") TArray<float> CumS;
	UPROPERTY(BlueprintReadOnly, Category = "Road") float Length = 0.0f;
	/** Arc length where the bridge begins (a tank past this point has crossed). */
	UPROPERTY(BlueprintReadOnly, Category = "Road") float BridgeS = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Road") float BridgeEndS = 0.0f;

	bool IsValid() const { return Points.Num() >= 2; }

	void Rebuild()
	{
		CumS.SetNum(Points.Num());
		Length = 0.0f;
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			if (i > 0)
			{
				Length += FVector::Dist(Points[i - 1], Points[i]);
			}
			CumS[i] = Length;
		}
	}

	int32 SegmentAt(float S) const
	{
		const int32 N = Points.Num();
		int32 Lo = 0, Hi = N - 1;
		while (Hi - Lo > 1)
		{
			const int32 Mid = (Lo + Hi) / 2;
			if (CumS[Mid] <= S) { Lo = Mid; } else { Hi = Mid; }
		}
		return Lo;
	}

	FVector At(float S) const
	{
		if (!IsValid()) { return FVector::ZeroVector; }
		S = FMath::Clamp(S, 0.0f, Length);
		const int32 I = SegmentAt(S);
		const float SegLen = FMath::Max(CumS[I + 1] - CumS[I], 1.0f);
		return FMath::Lerp(Points[I], Points[I + 1], (S - CumS[I]) / SegLen);
	}

	FVector DirAt(float S) const
	{
		if (!IsValid()) { return FVector::ForwardVector; }
		const FVector A = At(S - 600.0f);
		const FVector B = At(S + 600.0f);
		return (B - A).GetSafeNormal2D();
	}

	/** Closest arc length to P (2D). OutDist2D is the horizontal distance to the road. */
	float Project(const FVector& P, float& OutDist2D) const
	{
		float BestS = 0.0f;
		OutDist2D = BIG_NUMBER;
		for (int32 i = 0; i + 1 < Points.Num(); ++i)
		{
			const FVector2D A(Points[i]), B(Points[i + 1]), Q(P);
			const FVector2D AB = B - A;
			const float L2 = FMath::Max(AB.SizeSquared(), 1.0f);
			const float T = FMath::Clamp(FVector2D::DotProduct(Q - A, AB) / L2, 0.0f, 1.0f);
			const float D = FVector2D::Distance(Q, A + AB * T);
			if (D < OutDist2D)
			{
				OutDist2D = D;
				BestS = CumS[i] + T * FMath::Sqrt(L2);
			}
		}
		return BestS;
	}
};
