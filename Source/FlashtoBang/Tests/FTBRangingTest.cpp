// Flash to Bang - automation test for the sound-ranging maths (N1): a perfect stopwatch lands the fix on the source,
// and every 0.1 s of late press moves it about 34 m further out along the bearing.

#include "Misc/AutomationTest.h"
#include "Components/FTBRangingComponent.h"
#include "Core/FTBRoundConfig.h"
#include "Core/FTBTerrain.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFTBRangingTest, "FlashtoBang.Ranging.FixFromDelay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFTBRangingTest::RunTest(const FString& Parameters)
{
	const UFTBRoundConfig* Config = GetDefault<UFTBRoundConfig>();
	const FFTBTerrainParams& T = Config->Terrain;
	const FVector Eye(-6500.0f, 11000.0f, FTBTerrain::HeightUU(T, FVector(-6500.0f, 11000.0f, 0.0f)) + 1950.0f);
	const float C = Config->SpeedOfSoundMps;
	for (const float RangeM : { 650.0f, 1300.0f, 2000.0f })
	{
		// Put a source on the ground straight north at roughly RangeM, measure its true slant range.
		FVector Source = Eye + FVector(0.0f, RangeM * 100.0f, 0.0f);
		Source.Z = FTBTerrain::HeightUU(T, Source);
		const float Slant = FVector::Dist(Eye, Source);
		const float Delay = Slant / (C * 100.0f);
		const FVector Dir = (Source - Eye).GetSafeNormal();

		const FVector Perfect = UFTBRangingComponent::SolveFix(Eye, Dir, Delay * C * 100.0f, T);
		const float ErrM = FVector::Dist2D(Perfect, Source) * 0.01f;
		TestTrue(FString::Printf(TEXT("Perfect press at %.0f m lands within 5 m (got %.1f m)"), RangeM, ErrM), ErrM < 5.0f);

		const FVector Late = UFTBRangingComponent::SolveFix(Eye, Dir, (Delay + 0.1f) * C * 100.0f, T);
		const float LateM = FVector::Dist2D(Late, Source) * 0.01f;
		TestTrue(FString::Printf(TEXT("0.1 s late at %.0f m is 25-45 m long (got %.1f m)"), RangeM, LateM), LateM > 25.0f && LateM < 45.0f);
	}
	return true;
}

#endif
