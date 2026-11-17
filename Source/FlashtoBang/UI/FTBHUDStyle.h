// Flash to Bang - HUD style data asset (design guide 8.7.4): palette, type, sizes, margins and timings live here.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FTBHUDStyle.generated.h"

class UFontFace;
class UTexture2D;

UCLASS(BlueprintType)
class FLASHTOBANG_API UFTBHUDStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFTBHUDStyle();

	// Palette: off-white text, one accent (your marks), one danger (their threat).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette") FLinearColor Text;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette") FLinearColor TextDim;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette") FLinearColor Accent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette") FLinearColor Danger;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette") FLinearColor Track;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Palette") FLinearColor Scrim;

	// Type: Barlow Condensed (OFL), two weights (Medium for numbers and body, SemiBold for labels), three sizes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") TSoftObjectPtr<UFontFace> RegularFace;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") TSoftObjectPtr<UFontFace> BoldFace;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 LabelSize = 28;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 BodySize = 28;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 HeadlineSize = 51;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 LabelTracking = 110;
	/** Crisp edge on every glyph: a thin dark outline plus a soft drop shadow, readable over sky, brick or fog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 OutlineSize = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") FLinearColor OutlineColour = FLinearColor(0.0f, 0.0f, 0.0f, 0.8f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") FVector2D ShadowOffset = FVector2D(2.0f, 2.5f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") FLinearColor ShadowColour = FLinearColor(0.0f, 0.0f, 0.0f, 0.95f);

	// Layout.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float Margin = 36.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float TopScrim = 210.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float BottomScrim = 260.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float ProgressWidth = 176.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float ChipShear = -14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TSoftObjectPtr<UTexture2D> BinocularMask;
	/** Soft-edged dark backing behind each corner group, so text never sits on bare brick, sky or fog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TSoftObjectPtr<UTexture2D> PanelTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FLinearColor PanelColour = FLinearColor(0.0f, 0.004f, 0.008f, 0.58f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FMargin PanelPadding = FMargin(18.0f, 11.0f);
	/** Bottom-right group keeps this width so changing lines never shift it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float BottomRightMinWidth = 390.0f;
	/** Screen dim under the end panel (the ending camera shot stays visible). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float EndDimAlpha = 0.38f;

	// Sizes cut ~20 % (user, 2026-11-17: "make them a smaller by 20%"); text stays >= 28 px (design guide 2.10).
	// ---- Themed surfaces (design guide 2.5.16, user 2026-11-16: "theme based but it shouldn't take a lot of area") ----
	// A forward observer's kit: small olive-drab steel plates behind the HUD corners and the prompt, the end report on
	// field-message paper with a rubber-stamp headline, shell icons as the rating, an olive canvas tag for the button.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") TSoftObjectPtr<UTexture2D> PlateTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/FlashtoBang/Textures/T_FTB_HudPlate.T_FTB_HudPlate")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") float PlateMargin = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") TSoftObjectPtr<UTexture2D> PaperTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/FlashtoBang/Textures/T_FTB_Paper.T_FTB_Paper")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") float PaperMargin = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") FMargin PaperPadding = FMargin(58.0f, 46.0f, 58.0f, 37.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") TSoftObjectPtr<UTexture2D> TagTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/FlashtoBang/Textures/T_FTB_Tag.T_FTB_Tag")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") float TagMargin = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") FMargin TagPadding = FMargin(56.0f, 6.0f, 35.0f, 8.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") TSoftObjectPtr<UTexture2D> ShellIconTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/FlashtoBang/Textures/T_FTB_ShellIcon.T_FTB_ShellIcon")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") FVector2D ShellIconSize = FVector2D(18.0f, 46.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") FLinearColor Ink = FLinearColor(0.035f, 0.03f, 0.02f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") FLinearColor InkDim = FLinearColor(0.16f, 0.13f, 0.085f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") FLinearColor Stamp = FLinearColor(0.42f, 0.05f, 0.03f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") FLinearColor StampWin = FLinearColor(0.12f, 0.17f, 0.06f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") FLinearColor Brass = FLinearColor(0.62f, 0.38f, 0.1f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") FLinearColor ShellEmpty = FLinearColor(0.12f, 0.1f, 0.07f, 0.35f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme") float StampAngle = -4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float LegendKeyWidth = 130.0f;
	/** World tags stay this far (px) from the screen sides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float TagEdgeMargin = 110.0f;
	/** Centre prompt band: height and centre (px below screen centre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float PromptBandHeight = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float PromptBandCentre = 112.0f;
	/** World tags: vertical gap kept between labels, and how far (px) a crowded label may be pushed up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float TagGap = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float TagMaxPush = 130.0f;
	/** World tags shrink to this scale and fade to this opacity at TagFarDistanceM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float TagFarScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float TagFarOpacity = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float TagFarDistanceM = 1800.0f;

	// Motion (seconds).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float FadeSeconds = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float ObjectiveBannerSeconds = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float EventBannerSeconds = 1.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float PromptSeconds = 2.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float LegendDimAfter = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float KeyHighlightSeconds = 0.3f;
};
