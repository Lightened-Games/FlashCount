// Flash to Bang - round data defaults.

#include "Core/FTBRoundConfig.h"

#define LOCTEXT_NAMESPACE "FlashtoBang"

UFTBRoundConfig::UFTBRoundConfig()
{
	auto Post = [](FName Id, const FText& Name, EFTBPostKind Kind, FVector2D Loc, float Eye, float Yaw, FVector2D Approach, float Radius)
	{
		FFTBPostRow Row;
		Row.Id = Id;
		Row.DisplayName = Name;
		Row.Kind = Kind;
		Row.LocationM = Loc;
		Row.EyeHeightM = Eye;
		Row.ViewYaw = Yaw;
		Row.ViewPitch = -3.0f;
		Row.ApproachM = Approach;
		Row.ObstacleRadiusM = Radius;
		return Row;
	};
	Posts = {
		Post(TEXT("Church"), LOCTEXT("PostChurch", "CHURCH"), EFTBPostKind::ChurchTower, FVector2D(-65.0f, 110.0f), 19.7f, 92.0f, FVector2D(5.9f, 0.5f), 5.5f),
		Post(TEXT("Haystack"), LOCTEXT("PostHay", "HAYSTACK"), EFTBPostKind::Haystack, FVector2D(0.0f, 72.0f), 5.55f, 92.0f, FVector2D(-6.8f, 2.6f), 6.2f),
		Post(TEXT("Ridge"), LOCTEXT("PostRidge", "RIDGE"), EFTBPostKind::Ridge, FVector2D(78.0f, 22.0f), 1.9f, 100.0f, FVector2D(-6.8f, -2.0f), 6.0f),
	};

	RoadPointsM = {
		FVector2D(-1950.0f, 1900.0f), FVector2D(-1500.0f, 1800.0f), FVector2D(-1100.0f, 1640.0f), FVector2D(-760.0f, 1480.0f),
		FVector2D(-420.0f, 1360.0f), FVector2D(-160.0f, 1220.0f), FVector2D(40.0f, 1080.0f), FVector2D(330.0f, 1040.0f),
		FVector2D(470.0f, 860.0f), FVector2D(320.0f, 708.0f), FVector2D(320.0f, 656.0f), FVector2D(362.0f, 560.0f),
		FVector2D(432.0f, 470.0f),
	};
	BridgeStartIndex = 9;

	for (int32 i = 0; i < 5; ++i)
	{
		FFTBTankRow Row;
		Row.BehindLeadM = 110.0f * i;
		Tanks.Add(Row);
	}
}

#undef LOCTEXT_NAMESPACE
