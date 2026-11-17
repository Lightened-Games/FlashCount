// Flash to Bang - HUD (P21, design guide 8.7.0 recipe): boxless, soft edge scrims, light numerals, tiny tracked caps,
// hairline progress, one sheared post chip, quiet vertical legend whose rows light on input, one banner at a time,
// world tags over fixes / lead / shells / bridge, binocular overlay. Listens to the event hub; never drives gameplay.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/FTBTypes.h"
#include "FTBHUDWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class UOverlay;
class UFontFace;
class UFTBHUDStyle;
struct FCompositeFont;

UENUM()
enum class EFTBHudFont : uint8 { Light, Medium };

UCLASS()
class FLASHTOBANG_API UFTBHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") TSoftObjectPtr<UFTBHUDStyle> StyleAsset = TSoftObjectPtr<UFTBHUDStyle>(FSoftObjectPath(TEXT("/Game/FlashtoBang/UI/DA_FTB_HUDStyle.DA_FTB_HUDStyle")));

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleState(EFTBState NewState, EFTBState OldState);
	UFUNCTION() void HandleCountdown(int32 SecondsLeft);
	UFUNCTION() void HandleFlash(const FFTBFlashEvent& Flash);
	UFUNCTION() void HandleBoom(const FFTBFlashEvent& Flash);
	UFUNCTION() void HandleWatch(bool bRunning, float Delay, float Range);
	UFUNCTION() void HandleFix(const FFTBFix& Fix);
	UFUNCTION() void HandleShellCalled(const FFTBShell& Shell);
	UFUNCTION() void HandleImpact(FVector Location, bool bKill);
	UFUNCTION() void HandleKill(FVector Location, int32 Kills);
	UFUNCTION() void HandleTraced(int32 PostIndex, float Trace, bool bTraced);
	UFUNCTION() void HandlePost(int32 PostIndex, bool bRunning);
	UFUNCTION() void HandleInput(FName KeyId, bool bSimulated);
	UFUNCTION() void HandlePrompt(FName PromptId);
	UFUNCTION() void HandleVignette(float Strength, FLinearColor Colour);

private:
	struct FLegendRow { FName Id; TObjectPtr<UTextBlock> Key; TObjectPtr<UTextBlock> Label; float Lit = 0.0f; };
	struct FTag { TObjectPtr<UVerticalBox> Box; TObjectPtr<UTextBlock> Text; TObjectPtr<UImage> Rule; TObjectPtr<UImage> Stem; };
	/** A world tag waiting for this frame's declutter pass. */
	struct FPendingTag { const FTag* Tag = nullptr; FVector2D Screen = FVector2D::ZeroVector; float Opacity = 0.0f; float Raise = 0.0f; int32 Priority = 0; float Scale = 1.0f; };
	struct FShellTag { int32 Id = 0; FVector Target; float ImpactTime = 0.0f; FTag Tag; bool bUsed = false; };
	struct FPopup { FVector Location; float Age = 0.0f; FTag Tag; bool bUsed = false; };

	void BuildTree();
	void BuildScrims();
	void BuildTopLeft();
	void BuildTopRight();
	void BuildBottomRight();
	void BuildLegend();
	void BuildCentre();
	void BuildBanner();
	void BuildEndScreen();
	FTag MakeTag(const FLinearColor& RuleColour);

	UTextBlock* Txt(const FText& Text, int32 Size, const FLinearColor& Colour, EFTBHudFont Font, int32 Tracking = 0);
	UImage* Blk(const FLinearColor& Colour, const FVector2D& Size);
	UCanvasPanelSlot* Place(UWidget* W, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position);
	void AddFade(int32 Edge, float Extent, const FLinearColor& Colour, int32 Slices, TArray<TObjectPtr<UImage>>* Out);
	void EnsureFonts(const FSlateFontInfo& Fallback);
	void ShowBanner(const FText& Title, const FText& Sub, const FLinearColor& Colour, float Seconds);
	void ShowPrompt(const FText& Text, float Seconds, bool bOnceKey, FName OnceId);
	/** Queue a world tag; ResolveTags() places them all, pushing crowded ones up (with a stem) by priority. */
	bool PinTag(const FTag& Tag, const FVector& World, float Opacity, float RaisePx = 0.0f, int32 InPriority = 0, float ScaleMul = 1.0f);
	void ResolveTags();
	/** Soft-edged dark backing panel (9-sliced) around Content. */
	UWidget* Panel(UWidget* Content);
	void UpdateReadouts(float Dt);
	void UpdateBanner(float Dt);
	void UpdateTags(float Dt);
	void UpdateFeedback(float Dt);
	class IFTBWorldQuery* Query() const;
	class IFTBObserver* Observer() const;

	UPROPERTY(Transient) TObjectPtr<UFTBHUDStyle> Style;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UFontFace>> Faces;
	TSharedPtr<FCompositeFont> LightFont;
	TSharedPtr<FCompositeFont> MediumFont;
	bool bFontsBuilt = false;

	// Top-left: objective + kills.
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ObjectiveLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> KillsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> KillsTotal;
	UPROPERTY(Transient) TObjectPtr<UImage> ProgressFill;
	UPROPERTY(Transient) TObjectPtr<UImage> ProgressRest;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TopLeftBox;
	// Top-right: bridge ETA.
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EtaLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EtaText;
	// Bottom-right: stopwatch, range, post chip.
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WatchText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RangeText;
	UPROPERTY(Transient) TObjectPtr<UImage> ChipPlate;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ChipText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BatteryText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> LegendBox;
	// Centre.
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> ReticleLines;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> MilTicks;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BearingText;
	UPROPERTY(Transient) TObjectPtr<UImage> BinocularImage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PromptText;
	UPROPERTY(Transient) TObjectPtr<UWidget> PromptPanel;
	UPROPERTY(Transient) TObjectPtr<UImage> FadeImage;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> CornerPanels;
	// Banner.
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> BannerBand;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> PromptBand;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> BannerBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BannerTitle;
	UPROPERTY(Transient) TObjectPtr<UImage> BannerRule;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BannerSub;
	// End screen.
	UPROPERTY(Transient) TObjectPtr<UImage> DimImage;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> EndBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndReason;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndStats;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndPrompt;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndHeader;
	UPROPERTY(Transient) TObjectPtr<UWidget> EndCard;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> RatingIcons;
	// Feedback.
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> VignetteImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> TagWidgets;

	TArray<FLegendRow> Legend;
	TArray<FPendingTag> PendingTags;
	FTag FixTag;
	FTag LeadTag;
	FTag BridgeTag;
	TArray<FTag> PostTags;
	TArray<FTag> WreckTags;
	TArray<FShellTag> ShellTags;
	TArray<FPopup> Popups;

	FFTBFix LastFix;
	float LastDelay = 0.0f;
	float LastRange = 0.0f;
	float WatchFlashAlpha = 0.0f;
	float WatchStartTime = 0.0f;
	float BannerTime = 0.0f;
	float BannerDuration = 0.0f;
	float PromptTime = 0.0f;
	float PromptDuration = 0.0f;
	float StateAge = 0.0f;
	float SinceInput = 0.0f;
	float Vignette = 0.0f;
	FLinearColor VignetteColour = FLinearColor::Red;
	float KillPop = 0.0f;
	float PostTagsUntil = 0.0f;
	float LastVisibleFlash = -100.0f;
	float LastPromptRepeat = -100.0f;
	EFTBState LastState = EFTBState::Ready;
	bool bBound = false;
	bool bWatchRunning = false;
};
