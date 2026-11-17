// FactoryKit (design guide 2.5): start screen, cinematic shots, take-over autoplay, themed menu + legend.
// Every value lives here and is edited in DefaultGame.ini under [/Script/<Module>.FactoryKitSettings]
// (or Project Settings > Game > Factory Kit). No game names in code: each game themes the kit with config only.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FactoryKitSettings.generated.h"

class USoundBase;
class USoundMix;
class USoundClass;
class UTexture2D;

/** Where the start-screen title and menu sit. Each game picks one that differs from recent games (design guide 2.6). */
UENUM(BlueprintType)
enum class EFactoryMenuLayout : uint8
{
	LeftColumn,    // title + menu stacked, vertically centred on the left (Tether Point)
	LowerLeft,     // stacked in the lower-left corner, title large above the menu (Shadow-Bound)
	RightColumn,   // mirrored: right-aligned on the right, hero on the left
	CenterBottom,  // title centred high, menu centred along the bottom
	CenterStack    // everything centred (minimal, poster-like)
};

/** One camera move, relative to an anchor (the player, or the first actor with AnchorTag). +X is the anchor's facing. */
USTRUCT(BlueprintType)
struct FFactoryShot
{
	GENERATED_BODY()

	/** Actor tag to anchor on; None = the player's pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") FName AnchorTag;
	/** Offsets turn with the anchor's yaw (true) or stay in world axes (false). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") bool bUseAnchorYaw = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") FVector StartOffset = FVector(-300.0f, 150.0f, 120.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") FVector EndOffset = FVector(-260.0f, -150.0f, 110.0f);
	/** Point the camera looks at, relative to the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") FVector LookAtOffset = FVector(0.0f, 0.0f, 90.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") float StartFOV = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") float EndFOV = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") float Duration = 7.0f;
	/** Ease-in-out exponent (1 = linear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") float EaseExponent = 2.0f;
};

/** One row of the bottom-left key legend. */
USTRUCT(BlueprintType)
struct FFactoryLegendRow
{
	GENERATED_BODY()

	/** What the row shows for the key ("WASD", "SPACE", "LMB"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legend") FText KeyText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legend") FText Label;
	/** Game action id; PulseLegend(ActionId) lights the row (also call it from the bot's action path). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legend") FName ActionId;
	/** Real keys that also light the row when pressed (names as in EKeys: W, A, SpaceBar, LeftMouseButton...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legend") TArray<FName> Keys;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Factory Kit"))
class UFactoryKitSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFactoryKitSettings();

	static const UFactoryKitSettings* Get() { return GetDefault<UFactoryKitSettings>(); }

	// ---------------- Title and theme ----------------
	UPROPERTY(Config, EditAnywhere, Category = "Title") FText Title;
	UPROPERTY(Config, EditAnywhere, Category = "Title") FText Tagline;
	/** Optional motif image drawn under the title (orbit ring, brush stroke...). Empty = a thin accent line. */
	UPROPERTY(Config, EditAnywhere, Category = "Title") TSoftObjectPtr<UTexture2D> MotifTexture;
	UPROPERTY(Config, EditAnywhere, Category = "Title") FVector2D MotifSize = FVector2D(320.0f, 3.0f);
	UPROPERTY(Config, EditAnywhere, Category = "Title") EFactoryMenuLayout MenuLayout = EFactoryMenuLayout::LeftColumn;
	/** Distance of the column from its screen edge (px at 1080p). */
	UPROPERTY(Config, EditAnywhere, Category = "Title") float MenuEdgeMargin = 110.0f;
	/** Menu shots are shifted so the hero sits opposite the text (cm, in the anchor frame); 0 = use the shots as written. */
	UPROPERTY(Config, EditAnywhere, Category = "Title") float MenuFrameShift = 70.0f;

	/** A UFont (composite, OFL) for every kit text; empty = the engine default face. */
	UPROPERTY(Config, EditAnywhere, Category = "Theme") FSoftObjectPath FontObject;
	/** Optional: when FontObject is a UFontFace, the face used for non-title text (the title uses FontObject). */
	UPROPERTY(Config, EditAnywhere, Category = "Theme") FSoftObjectPath BodyFontObject;
	UPROPERTY(Config, EditAnywhere, Category = "Theme") FName TitleTypeface = TEXT("Bold");
	UPROPERTY(Config, EditAnywhere, Category = "Theme") FName BodyTypeface = TEXT("Regular");
	UPROPERTY(Config, EditAnywhere, Category = "Theme") int32 TitleSize = 84;
	UPROPERTY(Config, EditAnywhere, Category = "Theme") int32 TitleLetterSpacing = 120;
	UPROPERTY(Config, EditAnywhere, Category = "Theme") int32 TaglineSize = 20;
	UPROPERTY(Config, EditAnywhere, Category = "Theme") int32 TaglineLetterSpacing = 320;
	UPROPERTY(Config, EditAnywhere, Category = "Theme") int32 MenuSize = 30;
	UPROPERTY(Config, EditAnywhere, Category = "Theme") int32 MenuLetterSpacing = 160;
	UPROPERTY(Config, EditAnywhere, Category = "Theme") int32 SmallSize = 15;
	UPROPERTY(Config, EditAnywhere, Category = "Theme") int32 BriefingSize = 18;
	UPROPERTY(Config, EditAnywhere, Category = "Theme") FLinearColor TextColor = FLinearColor(0.93f, 0.94f, 0.95f, 1.0f);
	UPROPERTY(Config, EditAnywhere, Category = "Theme") FLinearColor DimColor = FLinearColor(0.62f, 0.64f, 0.68f, 0.85f);
	UPROPERTY(Config, EditAnywhere, Category = "Theme") FLinearColor AccentColor = FLinearColor(1.0f, 0.62f, 0.22f, 1.0f);
	/** Soft text shadow instead of boxes (user, 2026-11-09). */
	UPROPERTY(Config, EditAnywhere, Category = "Theme") FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);
	UPROPERTY(Config, EditAnywhere, Category = "Theme") FVector2D ShadowOffset = FVector2D(1.5f, 1.5f);
	/** Left-side readability scrim strength (0 = none); a soft gradient, never a box. */
	UPROPERTY(Config, EditAnywhere, Category = "Theme") float MenuScrimAlpha = 0.45f;
	/** Selection and fade motion time (s), ease-out. */
	UPROPERTY(Config, EditAnywhere, Category = "Theme") float MotionSeconds = 0.2f;
	UPROPERTY(Config, EditAnywhere, Category = "Theme") float SelectedSlidePx = 12.0f;

	// ---------------- Menu text ----------------
	UPROPERTY(Config, EditAnywhere, Category = "Menu") FText PlayText;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") FText OptionsText;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") FText QuitText;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") FText BackText;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") FText MenuHintText;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") FText AutoplayHintText;
	/** Optional short "how to play" block on the start screen, under the tagline (accent heading + a few dim lines). */
	UPROPERTY(Config, EditAnywhere, Category = "Menu") FText BriefingTitle;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") TArray<FText> BriefingLines;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") TSoftObjectPtr<USoundBase> HoverSound;
	UPROPERTY(Config, EditAnywhere, Category = "Menu") TSoftObjectPtr<USoundBase> SelectSound;

	// ---------------- Options ----------------
	/** Optional sound mix + classes for the volume sliders; without them Master uses the device volume and Music/SFX are hidden. */
	UPROPERTY(Config, EditAnywhere, Category = "Options") TSoftObjectPtr<USoundMix> VolumeMix;
	UPROPERTY(Config, EditAnywhere, Category = "Options") TSoftObjectPtr<USoundClass> MasterClass;
	UPROPERTY(Config, EditAnywhere, Category = "Options") TSoftObjectPtr<USoundClass> MusicClass;
	UPROPERTY(Config, EditAnywhere, Category = "Options") TSoftObjectPtr<USoundClass> SfxClass;
	UPROPERTY(Config, EditAnywhere, Category = "Options") float SensitivityMin = 0.25f;
	UPROPERTY(Config, EditAnywhere, Category = "Options") float SensitivityMax = 3.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Options") float SensitivityStep = 0.25f;

	// ---------------- Flow ----------------
	/** Autoplay: seconds the start screen holds before the bot presses PLAY (the video starts here). */
	UPROPERTY(Config, EditAnywhere, Category = "Flow") float MenuHoldSeconds = 2.5f;
	UPROPERTY(Config, EditAnywhere, Category = "Flow") float MenuPressSeconds = 0.6f;
	/** Blend from the last intro shot into the gameplay camera. */
	UPROPERTY(Config, EditAnywhere, Category = "Flow") float IntroBlendSeconds = 1.0f;
	/** Take-over: seconds without real input before the bot drives again. */
	UPROPERTY(Config, EditAnywhere, Category = "Flow") float AutoplayResumeIdleSeconds = 5.0f;
	/** Mouse movement (px per event) that counts as the player taking over. */
	UPROPERTY(Config, EditAnywhere, Category = "Flow") float TakeoverMouseDeadZone = 4.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Flow") float AutoplayHintFullSeconds = 3.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Flow") float AutoplayHintDimAlpha = 0.3f;

	// ---------------- Shots ----------------
	UPROPERTY(Config, EditAnywhere, Category = "Shots") TArray<FFactoryShot> MenuShots;
	UPROPERTY(Config, EditAnywhere, Category = "Shots") TArray<FFactoryShot> IntroShots;
	UPROPERTY(Config, EditAnywhere, Category = "Shots") TArray<FFactoryShot> VictoryShots;
	UPROPERTY(Config, EditAnywhere, Category = "Shots") TArray<FFactoryShot> DefeatShots;
	/** Optional second defeat framing (e.g. a different cause); PlayEnding(false, Focus, true) uses it when set. */
	UPROPERTY(Config, EditAnywhere, Category = "Shots") TArray<FFactoryShot> DefeatAltShots;
	/** Sphere radius used to keep shot cameras out of walls. */
	UPROPERTY(Config, EditAnywhere, Category = "Shots") float CameraClipRadius = 14.0f;

	// ---------------- Legend ----------------
	UPROPERTY(Config, EditAnywhere, Category = "Legend") TArray<FFactoryLegendRow> LegendRows;
	UPROPERTY(Config, EditAnywhere, Category = "Legend") float LegendPulseSeconds = 0.3f;
	UPROPERTY(Config, EditAnywhere, Category = "Legend") FVector2D LegendMargin = FVector2D(36.0f, 34.0f);
	/** Where the controls list shows: on the start screen (bottom-right, above the menu-key hint) and/or in play (bottom-left). */
	UPROPERTY(Config, EditAnywhere, Category = "Legend") bool bLegendOnMenu = false;
	UPROPERTY(Config, EditAnywhere, Category = "Legend") bool bLegendInPlay = true;
	/** With no list in play: each pressed key flashes bottom-left and fades (the visible input for the video). */
	UPROPERTY(Config, EditAnywhere, Category = "Legend") bool bKeyFlashInPlay = false;
	UPROPERTY(Config, EditAnywhere, Category = "Legend") float KeyFlashSeconds = 0.6f;
	/** Lift of the menu-placed list above the bottom edge (clears the menu-key hint). */
	UPROPERTY(Config, EditAnywhere, Category = "Legend") float MenuLegendLift = 46.0f;
	/** Optional header above the controls list (e.g. "CONTROLS", in the accent colour). */
	UPROPERTY(Config, EditAnywhere, Category = "Legend") FText LegendTitle;
	// ---------------- Themed panels (design guide 2.5.16, user 2026-11-16) ----------------
	// Every start screen is themed to its world: the menu sits on an in-world surface (parchment, steel plate, case file,
	// stone tablet), buttons are themed objects (wood planks, stamped tags), and panel text uses the surface's ink colours.
	// Colours with alpha 0 fall back to the plain theme colours above.
	/** Surface behind the title + menu column (box-drawn: corners keep their shape). Empty = no panel. */
	UPROPERTY(Config, EditAnywhere, Category = "Panels") TSoftObjectPtr<UTexture2D> MenuPanelTexture;
	/** Box margin of panel and legend textures (0..0.5 of the texture). */
	UPROPERTY(Config, EditAnywhere, Category = "Panels") float PanelBoxMargin = 0.12f;
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FMargin MenuPanelPadding = FMargin(90.0f, 64.0f, 90.0f, 64.0f);
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FLinearColor TitleColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FLinearColor TaglineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	/** Ink colours for text drawn on a panel (options page title, legend panels). */
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FLinearColor PanelTextColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FLinearColor PanelDimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FLinearColor PanelAccentColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	/** Themed button behind every menu row (box-drawn). Empty = the plain accent-bar rows. */
	UPROPERTY(Config, EditAnywhere, Category = "Panels") TSoftObjectPtr<UTexture2D> ButtonTexture;
	UPROPERTY(Config, EditAnywhere, Category = "Panels") float ButtonBoxMargin = 0.2f;
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FMargin ButtonPadding = FMargin(48.0f, 10.0f, 48.0f, 12.0f);
	UPROPERTY(Config, EditAnywhere, Category = "Panels") float ButtonMinWidth = 380.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FLinearColor ButtonTint = FLinearColor(0.78f, 0.78f, 0.78f, 1.0f);
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FLinearColor ButtonSelectedTint = FLinearColor(1.15f, 1.05f, 0.92f, 1.0f);
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FLinearColor ButtonTextColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FLinearColor ButtonSelectedTextColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	/** Surface behind the start-screen legend and guide panels (only on the start screen). */
	UPROPERTY(Config, EditAnywhere, Category = "Panels") TSoftObjectPtr<UTexture2D> LegendPanelTexture;
	UPROPERTY(Config, EditAnywhere, Category = "Panels") FMargin LegendPanelPadding = FMargin(40.0f, 30.0f, 40.0f, 30.0f);
	/** Start-screen guide panels wrap their lines at this width (px; 0 = no wrap), so they never run under the menu. */
	UPROPERTY(Config, EditAnywhere, Category = "Panels") float LegendMaxWidth = 0.0f;

	static FLinearColor Or(const FLinearColor& C, const FLinearColor& Fallback) { return C.A > 0.0f ? C : Fallback; }
};
