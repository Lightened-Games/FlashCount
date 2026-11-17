// Flash to Bang - HUD built in C++.

#include "UI/FTBHUDWidget.h"
#include "UI/FTBHUDStyle.h"
#include "Components/FTBArtilleryComponent.h"
#include "Components/FTBRadioTraceComponent.h"
#include "Core/FTBInterfaces.h"
#include "Core/FTBRoundConfig.h"
#include "Core/FTBWorld.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "FactoryKit/FactoryKitStyle.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Camera/PlayerCameraManager.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "FlashtoBang"

namespace
{
	TSet<FName> GPromptsShown;

	FLinearColor A(const FLinearColor& C, float Alpha) { return FLinearColor(C.R, C.G, C.B, C.A * Alpha); }

	void SetSize(UImage* Img, const FVector2D& Size)
	{
		if (!Img) { return; }
		FSlateBrush B = Img->GetBrush();
		B.ImageSize = Size;
		Img->SetBrush(B);
	}

	template <typename SlotT>
	SlotT* Pad(SlotT* Slot, const FMargin& M) { if (Slot) { Slot->SetPadding(M); } return Slot; }

	FText Clock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%02d:%02d"), S / 60, S % 60));
	}

	FText Metres(float M)
	{
		FNumberFormattingOptions O;
		O.MaximumFractionalDigits = 0;
		return FText::Format(LOCTEXT("MetresFmt", "{0} m"), FText::AsNumber(FMath::RoundToInt(M), &O));
	}
}

IFTBWorldQuery* UFTBHUDWidget::Query() const { return FTB::Query(this); }

IFTBObserver* UFTBHUDWidget::Observer() const
{
	const APlayerController* PC = GetOwningPlayer();
	return (PC && PC->GetPawn()) ? Cast<IFTBObserver>(PC->GetPawn()) : nullptr;
}

// ---------------------------------------------------------------------------------------------
// Factories
// ---------------------------------------------------------------------------------------------

void UFTBHUDWidget::EnsureFonts(const FSlateFontInfo& Fallback)
{
	if (bFontsBuilt) { return; }
	bFontsBuilt = true;
	const UFont* Roboto = Cast<UFont>(Fallback.FontObject);
	const FCompositeFont* RobotoComposite = Roboto ? Roboto->GetCompositeFont() : nullptr;
	auto Build = [this, RobotoComposite](const TSoftObjectPtr<UFontFace>& Ref) -> TSharedPtr<FCompositeFont>
	{
		UFontFace* Face = Ref.LoadSynchronous();
		if (!Face) { return nullptr; }
		Faces.Add(Face);
		TSharedPtr<FCompositeFont> C = MakeShared<FCompositeFont>();
		FTypefaceEntry& E = C->DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular")));
		E.Font = FFontData(Face);
		if (RobotoComposite) { C->FallbackTypeface.Typeface = RobotoComposite->DefaultTypeface; }
		return C;
	};
	LightFont = Build(Style->RegularFace);
	MediumFont = Build(Style->BoldFace);
}

UTextBlock* UFTBHUDWidget::Txt(const FText& Text, int32 Size, const FLinearColor& Colour, EFTBHudFont Font, int32 Tracking)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(Text);
	FSlateFontInfo F = T->GetFont();
	EnsureFonts(F);
	const TSharedPtr<FCompositeFont>& C = Font == EFTBHudFont::Light ? LightFont : MediumFont;
	if (C.IsValid()) { F = FSlateFontInfo(C, Size, TEXT("Regular")); }
	F.Size = Size;
	F.LetterSpacing = Tracking;
	F.OutlineSettings.OutlineSize = Style->OutlineSize;
	F.OutlineSettings.OutlineColor = Style->OutlineColour;
	T->SetFont(F);
	T->SetColorAndOpacity(FSlateColor(Colour));
	T->SetShadowOffset(Style->ShadowOffset);
	T->SetShadowColorAndOpacity(Style->ShadowColour);
	return T;
}

UImage* UFTBHUDWidget::Blk(const FLinearColor& Colour, const FVector2D& Size)
{
	UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush B;
	B.DrawAs = ESlateBrushDrawType::Box;
	B.ImageSize = Size;
	I->SetBrush(B);
	I->SetColorAndOpacity(Colour);
	I->SetVisibility(ESlateVisibility::HitTestInvisible);
	return I;
}

UCanvasPanelSlot* UFTBHUDWidget::Place(UWidget* W, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
{
	UCanvasPanelSlot* S = Root->AddChildToCanvas(W);
	if (S)
	{
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetAutoSize(true);
	}
	return S;
}

void UFTBHUDWidget::AddFade(int32 Edge, float Extent, const FLinearColor& Colour, int32 Slices, TArray<TObjectPtr<UImage>>* Out)
{
	// Nested slices (each from the edge to depth (i+1)/N): smooth falloff, no seams (Shift Supervisor AddFade).
	const int32 N = FMath::Max(1, Slices);
	const float SliceAlpha = 1.0f - FMath::Pow(1.0f - FMath::Clamp(Colour.A, 0.0f, 0.99f), 1.0f / N);
	for (int32 i = 0; i < N; ++i)
	{
		UImage* Slice = Blk(FLinearColor(Colour.R, Colour.G, Colour.B, SliceAlpha), FVector2D(4.0f));
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Slice);
		const float D = Extent * (i + 1) / N;
		switch (Edge)
		{
		case 0: S->SetAnchors(FAnchors(0, 0, 1, 0)); S->SetOffsets(FMargin(0, 0, 0, D)); break;
		case 1: S->SetAnchors(FAnchors(0, 1, 1, 1)); S->SetOffsets(FMargin(0, -D, 0, D)); break;
		case 2: S->SetAnchors(FAnchors(0, 0, 0, 1)); S->SetOffsets(FMargin(0, 0, D, 0)); break;
		case 3: S->SetAnchors(FAnchors(1, 0, 1, 1)); S->SetOffsets(FMargin(-D, 0, D, 0)); break;
		case 5: S->SetAnchors(FAnchors(0, 0.5f, 1, 0.5f)); S->SetOffsets(FMargin(0, Style->PromptBandCentre - D * 0.5f, 0, D)); break;
		default: S->SetAnchors(FAnchors(0, 0.3f, 1, 0.3f)); S->SetOffsets(FMargin(0, -D * 0.5f, 0, D)); break;
		}
		if (Out) { Out->Add(Slice); }
	}
}

UWidget* UFTBHUDWidget::Panel(UWidget* Content)
{
	// Themed (user, 2026-11-16: "theme based but it shouldn't take a lot of area"): a small olive-drab steel plate
	// hugging its content. Without the texture, only the padding is kept (the 2026-11-09 boxless look).
	if (UTexture2D* Plate = Style->PlateTexture.LoadSynchronous())
	{
		UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrush(FactoryKitStyle::BoxBrush(Plate, Style->PlateMargin));
		B->SetPadding(Style->PanelPadding);
		B->SetContent(Content);
		B->SetVisibility(ESlateVisibility::HitTestInvisible);
		return B;
	}
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (UOverlaySlot* S = O->AddChildToOverlay(Content)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(Style->PanelPadding); }
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	return O;
}

UFTBHUDWidget::FTag UFTBHUDWidget::MakeTag(const FLinearColor& RuleColour)
{
	FTag T;
	T.Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	T.Text = Txt(FText::GetEmpty(), Style->LabelSize, Style->Text, EFTBHudFont::Medium, 220);
	if (UVerticalBoxSlot* S = T.Box->AddChildToVerticalBox(T.Text)) { S->SetHorizontalAlignment(HAlign_Center); }
	T.Rule = Blk(RuleColour, FVector2D(30.0f, 3.0f));
	if (UVerticalBoxSlot* S = Pad(T.Box->AddChildToVerticalBox(T.Rule), FMargin(0, 3, 0, 0))) { S->SetHorizontalAlignment(HAlign_Center); }
	// Leader stem: grows when the declutter pass pushes this tag above a neighbour, so it still points at its spot.
	T.Stem = Blk(A(RuleColour, 0.75f), FVector2D(2.0f, 0.0f));
	if (UVerticalBoxSlot* S = T.Box->AddChildToVerticalBox(T.Stem)) { S->SetHorizontalAlignment(HAlign_Center); }
	T.Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	T.Box->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(T.Box))
	{
		S->SetAlignment(FVector2D(0.5f, 1.0f));
		S->SetAutoSize(true);
	}
	TagWidgets.Add(T.Box);
	return T;
}

// ---------------------------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------------------------

TSharedRef<SWidget> UFTBHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UFTBHUDWidget::BuildTree()
{
	Style = StyleAsset.LoadSynchronous();
	if (!Style)
	{
		Style = NewObject<UFTBHUDStyle>(this);
	}
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	BuildScrims();
	BuildCentre();
	FixTag = MakeTag(Style->Accent);
	LeadTag = MakeTag(Style->Accent);
	BridgeTag = MakeTag(Style->Danger);
	for (int32 i = 0; i < 3; ++i) { PostTags.Add(MakeTag(Style->TextDim)); }
	for (int32 i = 0; i < 5; ++i) { WreckTags.Add(MakeTag(A(Style->TextDim, 0.6f))); }
	BuildTopLeft();
	BuildTopRight();
	BuildBottomRight();
	BuildLegend();
	BuildBanner();
	BuildEndScreen();
}

void UFTBHUDWidget::BuildScrims()
{
	UTexture2D* Mask = Style->BinocularMask.LoadSynchronous();
	BinocularImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (Mask) { BinocularImage->SetBrushFromTexture(Mask, false); }
	BinocularImage->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f));
	BinocularImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	BinocularImage->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(BinocularImage)) { S->SetAnchors(FAnchors(0, 0, 1, 1)); S->SetOffsets(FMargin(0)); }

	FadeImage = Blk(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f), FVector2D(8.0f));
	FadeImage->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(FadeImage)) { S->SetAnchors(FAnchors(0, 0, 1, 1)); S->SetOffsets(FMargin(0)); }

	AddFade(0, Style->TopScrim, Style->Scrim, 28, nullptr);
	AddFade(1, Style->BottomScrim, Style->Scrim, 28, nullptr);

	const FLinearColor V(0.85f, 0.08f, 0.06f, 0.75f);
	AddFade(2, 280.0f, V, 18, &VignetteImages);
	AddFade(3, 280.0f, V, 18, &VignetteImages);
	AddFade(0, 200.0f, V, 14, &VignetteImages);
	AddFade(1, 200.0f, V, 14, &VignetteImages);
	for (UImage* I : VignetteImages) { I->SetRenderOpacity(0.0f); }

	DimImage = Blk(FLinearColor(0.0f, 0.0f, 0.0f, Style->EndDimAlpha), FVector2D(8.0f));
	DimImage->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(DimImage)) { S->SetAnchors(FAnchors(0, 0, 1, 1)); S->SetOffsets(FMargin(0)); }
}

void UFTBHUDWidget::BuildCentre()
{
	// Reticle: four 2 px strokes around an open centre.
	const float Gap = 7.0f, Len = 11.0f;
	const FVector2D Offs[4] = { FVector2D(-(Gap + Len * 0.5f), 0), FVector2D(Gap + Len * 0.5f, 0), FVector2D(0, -(Gap + Len * 0.5f)), FVector2D(0, Gap + Len * 0.5f) };
	for (int32 i = 0; i < 4; ++i)
	{
		UImage* L = Blk(Style->Text, i < 2 ? FVector2D(Len, 2.0f) : FVector2D(2.0f, Len));
		Place(L, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), Offs[i]);
		ReticleLines.Add(L);
	}
	// Mil ticks (binoculars only): a hairline scale across the centre.
	for (int32 i = -6; i <= 6; ++i)
	{
		if (i == 0) { continue; }
		UImage* T = Blk(A(Style->Text, 0.7f), FVector2D(2.0f, (i % 3 == 0) ? 14.0f : 7.0f));
		Place(T, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.0f), FVector2D(i * 46.0f, 22.0f));
		T->SetRenderOpacity(0.0f);
		MilTicks.Add(T);
	}
	BearingText = Txt(FText::GetEmpty(), Style->LabelSize, Style->TextDim, EFTBHudFont::Medium, 200);
	Place(BearingText, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 44.0f));
	// Soft dark band (gradient, no box) behind the centre prompt so it reads over bright fog.
	if (Style->PlateTexture.IsNull()) // the prompt's own plate replaces the band when themed
	{
		AddFade(5, Style->PromptBandHeight, FLinearColor(0.0f, 0.004f, 0.01f, 0.6f), 22, &PromptBand);
	}
	for (UImage* I : PromptBand) { I->SetRenderOpacity(0.0f); }
	PromptText = Txt(FText::GetEmpty(), Style->BodySize, Style->Text, EFTBHudFont::Light, 40);
	PromptText->SetJustification(ETextJustify::Center);
	PromptPanel = Panel(PromptText);
	PromptPanel->SetRenderOpacity(0.0f);
	Place(PromptPanel, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 92.0f));
}

void UFTBHUDWidget::BuildTopLeft()
{
	TopLeftBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ObjectiveLabel = Txt(LOCTEXT("ObjectiveLabel", "STOP THE COLUMN"), Style->LabelSize, Style->TextDim, EFTBHudFont::Medium, Style->LabelTracking);
	TopLeftBox->AddChildToVerticalBox(ObjectiveLabel);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	KillsText = Txt(FText::AsNumber(0), Style->HeadlineSize, Style->Text, EFTBHudFont::Light);
	Row->AddChildToHorizontalBox(KillsText);
	KillsTotal = Txt(FText::FromString(TEXT(" / 4")), Style->BodySize, Style->TextDim, EFTBHudFont::Light);
	if (UHorizontalBoxSlot* S = Pad(Row->AddChildToHorizontalBox(KillsTotal), FMargin(4, 0, 0, 10))) { S->SetVerticalAlignment(VAlign_Bottom); }
	Pad(TopLeftBox->AddChildToVerticalBox(Row), FMargin(0, -4, 0, 2));
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ProgressFill = Blk(Style->Accent, FVector2D(0.0f, 2.0f));
	ProgressRest = Blk(Style->Track, FVector2D(Style->ProgressWidth, 2.0f));
	Line->AddChildToHorizontalBox(ProgressFill);
	Line->AddChildToHorizontalBox(ProgressRest);
	TopLeftBox->AddChildToVerticalBox(Line);
	UWidget* TL = Panel(TopLeftBox);
	CornerPanels.Add(TL);
	Place(TL, FAnchors(0, 0), FVector2D(0, 0), FVector2D(Style->Margin - Style->PanelPadding.Left, 22.0f));
}

void UFTBHUDWidget::BuildTopRight()
{
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	EtaLabel = Txt(LOCTEXT("EtaLabel", "BRIDGE"), Style->LabelSize, Style->TextDim, EFTBHudFont::Medium, Style->LabelTracking);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(EtaLabel)) { S->SetHorizontalAlignment(HAlign_Right); }
	EtaText = Txt(FText::FromString(TEXT("01:30")), Style->HeadlineSize, Style->Text, EFTBHudFont::Light);
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(EtaText), FMargin(0, -4, 0, 0))) { S->SetHorizontalAlignment(HAlign_Right); }
	UWidget* TR = Panel(Col);
	CornerPanels.Add(TR);
	Place(TR, FAnchors(1, 0), FVector2D(1, 0), FVector2D(-(Style->Margin - Style->PanelPadding.Right), 22.0f));
}

void UFTBHUDWidget::BuildBottomRight()
{
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WatchText = Txt(FText::FromString(TEXT("0.00 s")), Style->HeadlineSize, Style->TextDim, EFTBHudFont::Light);
	WatchText->SetJustification(ETextJustify::Right);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(WatchText)) { S->SetHorizontalAlignment(HAlign_Fill); }
	RangeText = Txt(FText::GetEmpty(), Style->BodySize, Style->TextDim, EFTBHudFont::Light, 40);
	RangeText->SetJustification(ETextJustify::Right);
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(RangeText), FMargin(0, -6, 0, 14))) { S->SetHorizontalAlignment(HAlign_Fill); }

	UOverlay* Chip = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	ChipPlate = Blk(FLinearColor::Transparent, FVector2D(40.0f, 26.0f)); // spacer only: no plate is drawn
	FWidgetTransform Shear;
	Shear.Shear = FVector2D(Style->ChipShear, 0.0f);
	ChipPlate->SetRenderTransform(Shear);
	if (UOverlaySlot* S = Chip->AddChildToOverlay(ChipPlate)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	ChipText = Txt(FText::GetEmpty(), Style->LabelSize, Style->Text, EFTBHudFont::Medium, 150);
	ChipText->SetJustification(ETextJustify::Right);
	if (UOverlaySlot* S = Chip->AddChildToOverlay(ChipText)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0, 3)); }
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Chip)) { S->SetHorizontalAlignment(HAlign_Fill); }
	BatteryText = Txt(FText::GetEmpty(), Style->LabelSize, Style->TextDim, EFTBHudFont::Medium, Style->LabelTracking);
	BatteryText->SetJustification(ETextJustify::Right);
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(BatteryText), FMargin(0, 8, 0, 0))) { S->SetHorizontalAlignment(HAlign_Fill); }
	// Fixed width, every line filled and right-justified: the lines change length, and right-aligned slots drew the
	// longest battery line past the screen edge.
	USizeBox* Fixed = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Fixed->SetWidthOverride(Style->BottomRightMinWidth);
	Fixed->SetContent(Col);
	UWidget* BR = Panel(Fixed);
	CornerPanels.Add(BR);
	Place(BR, FAnchors(1, 1), FVector2D(1, 1), FVector2D(-(Style->Margin - Style->PanelPadding.Right), -28.0f));
}

void UFTBHUDWidget::BuildLegend()
{
	struct FRowDef { FName Id; FText Key; FText Label; };
	const TArray<FRowDef> Rows = {
		{ FTB::KeyWatch, LOCTEXT("KeySpace", "SPACE"), LOCTEXT("LegWatch", "Time the flash") },
		{ FTB::KeyFire, LOCTEXT("KeyLMB", "LMB"), LOCTEXT("LegFire", "Call fire") },
		{ FTB::KeyBinoculars, LOCTEXT("KeyRMB", "RMB"), LOCTEXT("LegBino", "Binoculars") },
		{ FTB::KeyPost, LOCTEXT("KeyPosts", "1  2  3"), LOCTEXT("LegPost", "Change post") },
	};
	LegendBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (const FRowDef& R : Rows)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* KeyCol = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeyCol->SetWidthOverride(Style->LegendKeyWidth);
		UTextBlock* Key = Txt(R.Key, Style->LabelSize, Style->Text, EFTBHudFont::Medium, 160);
		KeyCol->SetContent(Key);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(KeyCol)) { S->SetVerticalAlignment(VAlign_Center); }
		UTextBlock* Label = Txt(R.Label, Style->BodySize, Style->TextDim, EFTBHudFont::Light, 20);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Label)) { S->SetVerticalAlignment(VAlign_Center); }
		Pad(LegendBox->AddChildToVerticalBox(Line), FMargin(0, 1));
		Legend.Add({ R.Id, Key, Label, 0.0f });
	}
	UWidget* BL = Panel(LegendBox);
	CornerPanels.Add(BL);
	Place(BL, FAnchors(0, 1), FVector2D(0, 1), FVector2D(Style->Margin - Style->PanelPadding.Left, -28.0f));
	// Controls live on the start screen only (design guide C7 / 2.5.16); in play the kit's key flash and the centre
	// prompt name the next key. Kept built (pulses stay harmless) but not shown, so the HUD stays small.
	BL->SetVisibility(ESlateVisibility::Collapsed);
}

void UFTBHUDWidget::BuildBanner()
{
	AddFade(4, 230.0f, FLinearColor(0.0f, 0.004f, 0.01f, 0.55f), 26, &BannerBand);
	BannerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BannerTitle = Txt(FText::GetEmpty(), Style->HeadlineSize, Style->Text, EFTBHudFont::Medium, 300);
	BannerTitle->SetJustification(ETextJustify::Center);
	BannerRule = Blk(Style->Accent, FVector2D(56.0f, 2.0f));
	BannerSub = Txt(FText::GetEmpty(), Style->BodySize, Style->Text, EFTBHudFont::Light, 30);
	BannerSub->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = BannerBox->AddChildToVerticalBox(BannerTitle)) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = Pad(BannerBox->AddChildToVerticalBox(BannerRule), FMargin(0, 6, 0, 10))) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = BannerBox->AddChildToVerticalBox(BannerSub)) { S->SetHorizontalAlignment(HAlign_Center); }
	BannerBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(BannerBox, FAnchors(0.5f, 0.3f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
}

void UFTBHUDWidget::BuildEndScreen()
{
	// The end report is a field message on paper (2.5.16): a rubber-stamp headline, the story line, shell icons as the
	// rating, the log stats, and an olive canvas tag for the restart key. Ink text carries no drop shadow.
	auto Ink = [](UTextBlock* T, const FLinearColor& C) { T->SetColorAndOpacity(FSlateColor(C)); T->SetShadowColorAndOpacity(FLinearColor::Transparent); return T; };
	EndBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	EndHeader = Ink(Txt(LOCTEXT("EndHeader", "FIELD MESSAGE   ·   OBSERVER TO BATTERY"), Style->LabelSize, Style->InkDim, EFTBHudFont::Medium, Style->LabelTracking), Style->InkDim);
	EndTitle = Ink(Txt(FText::GetEmpty(), Style->HeadlineSize, Style->Stamp, EFTBHudFont::Medium, 360), Style->Stamp);
	EndTitle->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.0f), FVector2D::ZeroVector, Style->StampAngle));
	EndReason = Ink(Txt(FText::GetEmpty(), Style->BodySize, Style->Ink, EFTBHudFont::Light, 20), Style->Ink);
	EndStats = Ink(Txt(FText::GetEmpty(), Style->LabelSize, Style->InkDim, EFTBHudFont::Medium, Style->LabelTracking), Style->InkDim);
	UHorizontalBox* Rating = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTexture2D* ShellTex = Style->ShellIconTexture.LoadSynchronous();
	for (int32 K = 0; K < 3; ++K)
	{
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush B;
		if (ShellTex) { B.SetResourceObject(ShellTex); }
		B.ImageSize = Style->ShellIconSize;
		I->SetBrush(B);
		I->SetColorAndOpacity(Style->ShellEmpty);
		Pad(Rating->AddChildToHorizontalBox(I), FMargin(7.0f, 0.0f));
		RatingIcons.Add(I);
	}
	EndPrompt = Txt(LOCTEXT("EndPrompt", "R    Back to the posts"), Style->BodySize, Style->Text, EFTBHudFont::Medium, 80);
	UWidget* PromptHolder = EndPrompt;
	if (UTexture2D* TagTex = Style->TagTexture.LoadSynchronous())
	{
		UBorder* Tag = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Tag->SetBrush(FactoryKitStyle::BoxBrush(TagTex, Style->TagMargin));
		Tag->SetPadding(Style->TagPadding);
		Tag->SetContent(EndPrompt);
		PromptHolder = Tag;
	}
	const TPair<UWidget*, FMargin> Parts[] = {
		{ EndHeader.Get(), FMargin(0, 0, 0, 18) }, { EndTitle.Get(), FMargin(0) }, { EndReason.Get(), FMargin(0, 12, 0, 18) },
		{ Rating, FMargin(0, 0, 0, 16) }, { EndStats.Get(), FMargin(0, 0, 0, 26) }, { PromptHolder, FMargin(0) } };
	for (const TPair<UWidget*, FMargin>& P : Parts)
	{
		if (UVerticalBoxSlot* S = EndBox->AddChildToVerticalBox(P.Key)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(P.Value); }
	}
	UWidget* Card = EndBox;
	if (UTexture2D* PaperTex = Style->PaperTexture.LoadSynchronous())
	{
		UBorder* Paper = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Paper->SetBrush(FactoryKitStyle::BoxBrush(PaperTex, Style->PaperMargin));
		Paper->SetPadding(Style->PaperPadding);
		Paper->SetContent(EndBox);
		Card = Paper;
	}
	else
	{
		EndHeader->SetColorAndOpacity(FSlateColor(Style->TextDim));
		EndReason->SetColorAndOpacity(FSlateColor(Style->Text));
		EndStats->SetColorAndOpacity(FSlateColor(Style->TextDim));
	}
	EndCard = Card;
	EndCard->SetVisibility(ESlateVisibility::HitTestInvisible);
	EndCard->SetRenderOpacity(0.0f);
	Place(EndCard, FAnchors(0.5f, 0.47f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
}

// ---------------------------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------------------------

void UFTBHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (bBound) { return; }
	UFTBEventHub* Hub = FTB::Hub(this);
	if (!Hub) { return; }
	Hub->OnStateChanged.AddDynamic(this, &UFTBHUDWidget::HandleState);
	Hub->OnCountdown.AddDynamic(this, &UFTBHUDWidget::HandleCountdown);
	Hub->OnFlash.AddDynamic(this, &UFTBHUDWidget::HandleFlash);
	Hub->OnBoom.AddDynamic(this, &UFTBHUDWidget::HandleBoom);
	Hub->OnWatch.AddDynamic(this, &UFTBHUDWidget::HandleWatch);
	Hub->OnFixPlotted.AddDynamic(this, &UFTBHUDWidget::HandleFix);
	Hub->OnShellCalled.AddDynamic(this, &UFTBHUDWidget::HandleShellCalled);
	Hub->OnShellImpact.AddDynamic(this, &UFTBHUDWidget::HandleImpact);
	Hub->OnTankKilled.AddDynamic(this, &UFTBHUDWidget::HandleKill);
	Hub->OnPostTraced.AddDynamic(this, &UFTBHUDWidget::HandleTraced);
	Hub->OnPostChanged.AddDynamic(this, &UFTBHUDWidget::HandlePost);
	Hub->OnInput.AddDynamic(this, &UFTBHUDWidget::HandleInput);
	Hub->OnPrompt.AddDynamic(this, &UFTBHUDWidget::HandlePrompt);
	Hub->OnVignette.AddDynamic(this, &UFTBHUDWidget::HandleVignette);
	bBound = true;
}

void UFTBHUDWidget::NativeDestruct()
{
	if (UFTBEventHub* Hub = FTB::Hub(this))
	{
		Hub->OnStateChanged.RemoveAll(this); Hub->OnCountdown.RemoveAll(this); Hub->OnFlash.RemoveAll(this); Hub->OnBoom.RemoveAll(this);
		Hub->OnWatch.RemoveAll(this); Hub->OnFixPlotted.RemoveAll(this); Hub->OnShellCalled.RemoveAll(this); Hub->OnShellImpact.RemoveAll(this);
		Hub->OnTankKilled.RemoveAll(this); Hub->OnPostTraced.RemoveAll(this); Hub->OnPostChanged.RemoveAll(this); Hub->OnInput.RemoveAll(this);
		Hub->OnPrompt.RemoveAll(this); Hub->OnVignette.RemoveAll(this);
	}
	bBound = false;
	Super::NativeDestruct();
}

void UFTBHUDWidget::ShowBanner(const FText& Title, const FText& Sub, const FLinearColor& Colour, float Seconds)
{
	BannerTitle->SetText(Title);
	BannerTitle->SetColorAndOpacity(FSlateColor(Style->Text));
	BannerRule->SetColorAndOpacity(Colour);
	BannerSub->SetText(Sub);
	BannerTime = 0.0f;
	BannerDuration = Seconds;
}

void UFTBHUDWidget::ShowPrompt(const FText& Text, float Seconds, bool bOnceKey, FName OnceId)
{
	if (bOnceKey)
	{
		if (GPromptsShown.Contains(OnceId)) { return; }
		GPromptsShown.Add(OnceId);
	}
	// Prompts yield to an active banner (no overlap).
	PromptText->SetText(Text);
	PromptTime = 0.0f;
	PromptDuration = Seconds;
}

void UFTBHUDWidget::HandleState(EFTBState NewState, EFTBState OldState)
{
	StateAge = 0.0f;
	if (NewState == EFTBState::Active)
	{
		ShowBanner(LOCTEXT("Go", "GO"), FText::GetEmpty(), Style->Accent, 0.9f);
	}
	if (NewState == EFTBState::Victory || NewState == EFTBState::Defeat)
	{
		BannerDuration = 0.0f;
		PromptDuration = 0.0f;
		const IFTBWorldQuery* Q = Query();
		const bool bWin = NewState == EFTBState::Victory;
		EndTitle->SetText(bWin ? LOCTEXT("WinTitle", "COLUMN STOPPED") : (Q && Q->FTB_GetDefeatReason() == EFTBDefeatReason::PostHit ? LOCTEXT("PostTitle", "POST HIT") : LOCTEXT("BridgeTitle", "THEY CROSSED")));
		const bool bPaper = !Style->PaperTexture.IsNull();
		EndTitle->SetColorAndOpacity(FSlateColor(bPaper ? (bWin ? Style->StampWin : Style->Stamp) : (bWin ? Style->Text : Style->Danger)));
		// Rating in shells: a clean win with no wasted calls earns three; a defeat earns one per pair of tanks stopped.
		int32 Shells = 1;
		if (Q)
		{
			const int32 Wasted = FMath::Max(0, Q->FTB_GetCalls() - Q->FTB_GetKills());
			Shells = bWin ? (Wasted == 0 ? 3 : (Wasted <= 2 ? 2 : 1)) : FMath::Clamp(Q->FTB_GetKills() / 2, 0, 2);
		}
		for (int32 K = 0; K < RatingIcons.Num(); ++K)
		{
			RatingIcons[K]->SetColorAndOpacity(K < Shells ? Style->Brass : Style->ShellEmpty);
		}
		FText Reason = LOCTEXT("WinReason", "The bridge holds. The town is safe.");
		if (!bWin)
		{
			Reason = (Q && Q->FTB_GetDefeatReason() == EFTBDefeatReason::PostHit)
				? LOCTEXT("PostReason", "They traced your radio and shelled your post. Move after two calls.")
				: LOCTEXT("BridgeReason", "A tank reached the bridge. Lead the shells further ahead.");
		}
		EndReason->SetText(Reason);
		if (Q)
		{
			const float Best = Q->FTB_GetBestFixErrorM();
			EndStats->SetText(FText::Format(LOCTEXT("EndStats", "{0} / {1} TANKS     {2} CALLS     {3}     BEST FIX {4}"),
				FText::AsNumber(Q->FTB_GetKills()), FText::AsNumber(Q->FTB_GetKillsToWin()), FText::AsNumber(Q->FTB_GetCalls()),
				Clock(Q->FTB_GetActiveTime()), Best >= 0.0f ? Metres(Best) : LOCTEXT("NoFix", "NONE")));
		}
	}
}

void UFTBHUDWidget::HandleCountdown(int32 SecondsLeft) {}

void UFTBHUDWidget::HandleFlash(const FFTBFlashEvent& Flash)
{
	const IFTBWorldQuery* Q = Query();
	if (!Q || Q->FTB_GetState() != EFTBState::Active || !Flash.bVisible) { return; }
	LastVisibleFlash = FTB::Now(this);
	if (!bWatchRunning)
	{
		ShowPrompt(LOCTEXT("PromptFlash", "Flash   ·   SPACE starts the watch"), Style->PromptSeconds, true, TEXT("FirstFlash"));
	}
}

void UFTBHUDWidget::HandleBoom(const FFTBFlashEvent& Flash)
{
	const IFTBWorldQuery* Q = Query();
	if (Q && Q->FTB_GetState() == EFTBState::Active && !Flash.bVisible && !bWatchRunning)
	{
		ShowPrompt(LOCTEXT("PromptSmoke", "Boom with no flash   ·   smoke is hiding it"), Style->PromptSeconds, true, TEXT("Smoke"));
	}
}

void UFTBHUDWidget::HandleWatch(bool bRunning, float Delay, float Range)
{
	bWatchRunning = bRunning;
	WatchFlashAlpha = 1.0f;
	if (bRunning) { WatchStartTime = FTB::Now(this); }
	if (bRunning)
	{
		ShowPrompt(LOCTEXT("PromptBoom", "Wait for the boom   ·   SPACE stops it"), Style->PromptSeconds, true, TEXT("WaitBoom"));
	}
	else if (Range > 0.0f)
	{
		LastDelay = Delay;
		LastRange = Range;
	}
}

void UFTBHUDWidget::HandleFix(const FFTBFix& Fix)
{
	LastFix = Fix;
	if (Fix.bOnRoad)
	{
		ShowPrompt(LOCTEXT("PromptLead", "Put the cross on the LEAD ring   ·   click to call fire"), Style->PromptSeconds + 0.6f, true, TEXT("FirstFix"));
	}
	else
	{
		ShowPrompt(LOCTEXT("PromptOffRoad", "Fix is off the road   ·   time the next flash"), Style->PromptSeconds, true, TEXT("OffRoad"));
	}
	if (const IFTBWorldQuery* Q = Query())
	{
		TArray<FVector> Wrecks;
		Q->FTB_GetWrecks(Wrecks);
		for (const FVector& W : Wrecks)
		{
			if (FVector::Dist2D(W, Fix.Location) < 6000.0f)
			{
				ShowPrompt(LOCTEXT("PromptCookOff", "That flash was a burning wreck   ·   it is already dead"), Style->PromptSeconds, true, TEXT("CookOffFix"));
				break;
			}
		}
	}
}

void UFTBHUDWidget::HandleShellCalled(const FFTBShell& Shell)
{
	FShellTag* Free = nullptr;
	for (FShellTag& T : ShellTags) { if (!T.bUsed) { Free = &T; break; } }
	if (!Free)
	{
		Free = &ShellTags.AddDefaulted_GetRef();
		Free->Tag = MakeTag(Style->Accent);
	}
	Free->Id = Shell.Id;
	Free->Target = Shell.Target;
	Free->ImpactTime = Shell.ImpactTime;
	Free->bUsed = true;
}

void UFTBHUDWidget::HandleImpact(FVector Location, bool bKill)
{
	for (FShellTag& T : ShellTags)
	{
		if (T.bUsed && FTB::Now(this) >= T.ImpactTime - 0.05f)
		{
			T.bUsed = false;
			T.Tag.Box->SetRenderOpacity(0.0f);
		}
	}
}

void UFTBHUDWidget::HandleKill(FVector Location, int32 Kills)
{
	KillPop = 1.0f;
	FPopup* Free = nullptr;
	for (FPopup& P : Popups) { if (!P.bUsed) { Free = &P; break; } }
	if (!Free)
	{
		Free = &Popups.AddDefaulted_GetRef();
		Free->Tag = MakeTag(Style->Accent);
	}
	Free->Location = Location + FVector(0, 0, 900);
	Free->Age = 0.0f;
	Free->bUsed = true;
	Free->Tag.Text->SetText(LOCTEXT("PlusOne", "+1"));
	const IFTBWorldQuery* Q = Query();
	const int32 Left = Q ? Q->FTB_GetKillsToWin() - Kills : 0;
	if (Left > 0)
	{
		ShowBanner(LOCTEXT("TankDown", "TANK DOWN"), Left == 1 ? LOCTEXT("OneMore", "One more") : FText::Format(LOCTEXT("NMore", "{0} more"), FText::AsNumber(Left)), Style->Accent, Style->EventBannerSeconds);
	}
}

void UFTBHUDWidget::HandleTraced(int32 PostIndex, float Trace, bool bTraced)
{
	ShowBanner(LOCTEXT("Traced", "TRACED"), LOCTEXT("TracedSub", "They heard your radio   ·   move   1  2  3"), Style->Danger, 2.2f);
	PostTagsUntil = FTB::Now(this) + 6.0f;
}

void UFTBHUDWidget::HandlePost(int32 PostIndex, bool bRunning)
{
	PostTagsUntil = FTB::Now(this) + (bRunning ? 8.0f : 1.5f);
}

void UFTBHUDWidget::HandleInput(FName KeyId, bool bSimulated)
{
	SinceInput = 0.0f;
	for (FLegendRow& R : Legend)
	{
		if (R.Id == KeyId) { R.Lit = 1.0f; }
	}
}

void UFTBHUDWidget::HandlePrompt(FName PromptId)
{
	const float Now = FTB::Now(this);
	if (Now - LastPromptRepeat < 1.2f) { return; }
	LastPromptRepeat = Now;
	if (PromptId == FTB::PromptCantTimeRunning) { ShowPrompt(LOCTEXT("PromptRunning", "Not while running"), 1.4f, false, NAME_None); }
	else if (PromptId == FTB::PromptReloading) { ShowPrompt(LOCTEXT("PromptReload", "Battery reloading"), 1.4f, false, NAME_None); }
	else if (PromptId == FTB::PromptNoTarget) { ShowPrompt(LOCTEXT("PromptNoTarget", "Aim at the ground"), 1.4f, false, NAME_None); }
	else if (PromptId == FTB::PromptTooShort) { ShowPrompt(LOCTEXT("PromptShort", "Too quick   ·   wait for the boom"), 1.6f, false, NAME_None); }
}

void UFTBHUDWidget::HandleVignette(float Strength, FLinearColor Colour)
{
	Vignette = FMath::Max(Vignette, Strength);
}

// ---------------------------------------------------------------------------------------------
// Per frame
// ---------------------------------------------------------------------------------------------

void UFTBHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bBound) { NativeConstruct(); }
	// FactoryKit: the start screen and fly-in own the screen; the HUD waits (opacity, so it keeps ticking).
	const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	const bool bFlowOpening = Flow && (Flow->GetState() == EFactoryFlowState::Menu || Flow->GetState() == EFactoryFlowState::Intro);
	SetRenderOpacity(bFlowOpening ? 0.0f : 1.0f);
	if (bFlowOpening)
	{
		StateAge = 0.0f;
		return;
	}
	StateAge += InDeltaTime;
	SinceInput += InDeltaTime;
	UpdateReadouts(InDeltaTime);
	UpdateBanner(InDeltaTime);
	UpdateTags(InDeltaTime);
	UpdateFeedback(InDeltaTime);
}

void UFTBHUDWidget::UpdateReadouts(float Dt)
{
	const IFTBWorldQuery* Q = Query();
	IFTBObserver* O = Observer();
	if (!Q) { return; }
	const EFTBState State = Q->FTB_GetState();
	const bool bEnd = State == EFTBState::Victory || State == EFTBState::Defeat;
	const float Corner = bEnd ? FMath::Max(0.0f, 1.0f - StateAge / Style->FadeSeconds) : 1.0f;

	// Kills.
	const int32 Kills = Q->FTB_GetKills();
	const int32 Need = FMath::Max(1, Q->FTB_GetKillsToWin());
	KillsText->SetText(FText::AsNumber(Kills));
	KillsText->SetRenderScale(FVector2D(1.0f + 0.14f * KillPop));
	KillsTotal->SetText(FText::FromString(FString::Printf(TEXT(" / %d"), Need)));
	const float Frac = FMath::Clamp(static_cast<float>(Kills) / Need, 0.0f, 1.0f);
	SetSize(ProgressFill, FVector2D(Style->ProgressWidth * Frac, 2.0f));
	SetSize(ProgressRest, FVector2D(Style->ProgressWidth * (1.0f - Frac), 2.0f));
	for (UWidget* P : CornerPanels) { P->SetRenderOpacity(Corner); }

	// Bridge ETA: calm until the last 15 s.
	const float Eta = Q->FTB_GetBridgeEta();
	EtaText->SetText(Clock(Eta));
	const bool bUrgent = State == EFTBState::Active && Eta <= 15.0f;
	EtaText->SetColorAndOpacity(FSlateColor(bUrgent ? Style->Danger : Style->Text));

	// Stopwatch + range.
	WatchFlashAlpha = FMath::Max(0.0f, WatchFlashAlpha - Dt * 4.0f);
	const APawn* Pawn = GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr;
	float Running = 0.0f;
	if (O && O->Obs_IsWatchRunning())
	{
		Running = FTB::Now(this) - WatchStartTime;
		WatchText->SetText(FText::FromString(FString::Printf(TEXT("%.2f s"), Running)));
		WatchText->SetColorAndOpacity(FSlateColor(Style->Accent));
		const IFTBWorldQuery* QQ = Query();
		const float Speed = QQ && QQ->FTB_GetConfig() ? QQ->FTB_GetConfig()->SpeedOfSoundMps : 343.0f;
		RangeText->SetText(Metres(Running * Speed));
		RangeText->SetColorAndOpacity(FSlateColor(Style->Text));
	}
	else
	{
		WatchText->SetText(FText::FromString(FString::Printf(TEXT("%.2f s"), LastDelay)));
		WatchText->SetColorAndOpacity(FSlateColor(LastRange > 0.0f ? FMath::Lerp(Style->Text, Style->Accent, WatchFlashAlpha) : Style->TextDim));
		RangeText->SetText(LastRange > 0.0f ? Metres(LastRange) : LOCTEXT("NoRange", "time a flash"));
		RangeText->SetColorAndOpacity(FSlateColor(Style->TextDim));
	}
	WatchText->SetRenderScale(FVector2D(1.0f + 0.06f * WatchFlashAlpha));

	// Post chip: name + calls left; red when traced.
	const UFTBRadioTraceComponent* Trace = Pawn ? Pawn->FindComponentByClass<UFTBRadioTraceComponent>() : nullptr;
	const UFTBArtilleryComponent* Art = Pawn ? Pawn->FindComponentByClass<UFTBArtilleryComponent>() : nullptr;
	if (O)
	{
		const int32 Post = O->Obs_GetPost();
		const FText Name = Q->FTB_GetPostName(Post);
		const bool bTraced = Trace && Trace->IsTraced(Post);
		FText Status;
		if (O->Obs_IsRunning()) { Status = LOCTEXT("Moving", "MOVING"); }
		else if (bTraced) { Status = LOCTEXT("TracedChip", "TRACED"); }
		else if (Trace)
		{
			const int32 Left = Trace->GetCallsLeft(Post);
			Status = Left <= 1 ? LOCTEXT("OneCall", "1 CALL LEFT") : FText::Format(LOCTEXT("NCalls", "{0} CALLS LEFT"), FText::AsNumber(Left));
		}
		ChipText->SetText(FText::Format(LOCTEXT("ChipFmt", "{0}   ·   {1}"), Name, Status));
		const bool bWarn = bTraced || (Trace && Trace->GetCallsLeft(Post) <= 1 && !O->Obs_IsRunning());
		// Text only, no plate: red when traced, warm red when one call is left.
		ChipText->SetColorAndOpacity(FSlateColor(bTraced ? Style->Danger : (bWarn ? FMath::Lerp(Style->Text, Style->Danger, 0.55f) : Style->Text)));
	}
	if (Art)
	{
		const float Reload = Art->GetReloadRemaining();
		BatteryText->SetText(Art->NumInFlight() >= Art->MaxInFlight ? LOCTEXT("BattFull", "2 SHELLS UP") :
			(Reload > 0.0f ? LOCTEXT("BattReload", "BATTERY RELOADING") : LOCTEXT("BattReady", "BATTERY READY")));
	}

	// Legend: quiet, dims after a while, rows light on input.
	const float LegendAlpha = SinceInput > Style->LegendDimAfter ? 0.5f : 1.0f;
	LegendBox->SetRenderOpacity(Corner * FMath::FInterpTo(LegendBox->GetRenderOpacity() / FMath::Max(Corner, 0.01f), LegendAlpha, Dt, 4.0f));
	for (FLegendRow& R : Legend)
	{
		R.Lit = FMath::Max(0.0f, R.Lit - Dt / FMath::Max(Style->KeyHighlightSeconds, 0.05f));
		R.Key->SetColorAndOpacity(FSlateColor(FMath::Lerp(Style->Text, Style->Accent, R.Lit)));
		R.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(Style->TextDim, Style->Text, R.Lit)));
	}

	// Reticle, bearing, binocular overlay.
	const float Zoom = O ? O->Obs_GetZoomAlpha() : 0.0f;
	const float Ret = bEnd ? 0.0f : 1.0f;
	for (UImage* L : ReticleLines) { L->SetRenderOpacity(Ret * (O && O->Obs_IsRunning() ? 0.25f : 0.85f)); }
	for (UImage* T : MilTicks) { T->SetRenderOpacity(Ret * Zoom); }
	BinocularImage->SetRenderOpacity(Zoom);
	if (O)
	{
		const float Bearing = FRotator::ClampAxis(90.0f - O->Obs_GetView().Yaw);
		BearingText->SetText(FText::FromString(FString::Printf(TEXT("%03d°"), FMath::RoundToInt(Bearing) % 360)));
	}
	BearingText->SetRenderOpacity(Ret * 0.8f);
	KillPop = FMath::Max(0.0f, KillPop - Dt * 3.0f);
}

void UFTBHUDWidget::UpdateBanner(float Dt)
{
	const IFTBWorldQuery* Q = Query();
	if (!Q) { return; }
	float Opacity = 0.0f;
	float Slide = 0.0f;
	float Scale = 1.0f;
	switch (Q->FTB_GetState())
	{
	case EFTBState::Ready:
	{
		const float Remaining = Q->FTB_GetCountdownRemaining();
		BannerSub->SetText(FText::Format(LOCTEXT("ObjectiveSub", "Knock out {0} tanks before they cross the bridge"), FText::AsNumber(Q->FTB_GetKillsToWin())));
		BannerRule->SetColorAndOpacity(Style->Accent);
		if (Remaining > 3.0f)
		{
			BannerTitle->SetText(LOCTEXT("ObjectiveTitle", "STOP THE COLUMN"));
			BannerTitle->SetColorAndOpacity(FSlateColor(Style->Text));
			Slide = 16.0f * FMath::Square(FMath::Max(0.0f, 1.0f - StateAge / Style->FadeSeconds));
		}
		else
		{
			const float FracPart = Remaining - FMath::FloorToFloat(Remaining);
			BannerTitle->SetText(FText::AsNumber(FMath::Max(1, FMath::CeilToInt(Remaining))));
			BannerTitle->SetColorAndOpacity(FSlateColor(Style->Accent));
			Scale = 1.3f + 0.35f * FracPart * FracPart * FracPart;
		}
		Opacity = FMath::Clamp(StateAge / Style->FadeSeconds, 0.0f, 1.0f);
		break;
	}
	case EFTBState::Active:
		if (BannerTime < BannerDuration)
		{
			BannerTime += Dt;
			Opacity = FMath::Clamp(BannerTime / 0.15f, 0.0f, 1.0f) * FMath::Clamp((BannerDuration - BannerTime) / 0.25f, 0.0f, 1.0f);
			Slide = 14.0f * FMath::Square(FMath::Max(0.0f, 1.0f - BannerTime / Style->FadeSeconds));
		}
		break;
	default:
		break;
	}
	BannerTitle->SetRenderScale(FVector2D(Scale));
	BannerBox->SetRenderOpacity(Opacity);
	BannerBox->SetRenderTranslation(FVector2D(0.0f, Slide));
	for (UImage* S : BannerBand) { S->SetRenderOpacity(Opacity); }

	// Prompt: hidden while a banner is up, then fades in/out.
	float PromptAlpha = 0.0f;
	if (PromptTime < PromptDuration)
	{
		PromptTime += Dt;
		PromptAlpha = FMath::Clamp(PromptTime / Style->FadeSeconds, 0.0f, 1.0f) * FMath::Clamp((PromptDuration - PromptTime) / 0.3f, 0.0f, 1.0f);
	}
	if (Opacity > 0.05f) { PromptAlpha = 0.0f; }
	PromptPanel->SetRenderOpacity(PromptAlpha);
	for (UImage* I : PromptBand) { I->SetRenderOpacity(PromptAlpha); }

	// End screen.
	const bool bEnd = Q->FTB_GetState() == EFTBState::Victory || Q->FTB_GetState() == EFTBState::Defeat;
	const float EndAlpha = bEnd ? FMath::Clamp((StateAge - 0.3f) / 0.35f, 0.0f, 1.0f) : 0.0f;
	EndCard->SetRenderOpacity(EndAlpha);
	EndCard->SetRenderTranslation(FVector2D(0.0f, 18.0f * (1.0f - EndAlpha) * (1.0f - EndAlpha)));
	EndPrompt->SetRenderOpacity(0.65f + 0.35f * FMath::Abs(FMath::Sin(StateAge * 2.0f)));
	DimImage->SetRenderOpacity(bEnd ? FMath::Clamp(StateAge / 0.4f, 0.0f, 1.0f) : 0.0f);
}

bool UFTBHUDWidget::PinTag(const FTag& Tag, const FVector& World, float Opacity, float RaisePx, int32 InPriority, float ScaleMul)
{
	APlayerController* PC = GetOwningPlayer();
	FVector2D Screen;
	if (!PC || Opacity <= 0.01f || !UGameplayStatics::ProjectWorldToScreen(PC, World, Screen, false))
	{
		Tag.Box->SetRenderOpacity(0.0f);
		return false;
	}
	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	// Far labels shrink a little and lose a little opacity, but stay crisp HUD text (never fogged).
	float DistM = 0.0f;
	if (PC->PlayerCameraManager)
	{
		DistM = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), World) * 0.01f;
	}
	const float Far = FMath::Clamp(DistM / FMath::Max(Style->TagFarDistanceM, 1.0f), 0.0f, 1.0f);
	FPendingTag P;
	P.Tag = &Tag;
	P.Screen = Screen / ViewportScale - FVector2D(0.0f, 10.0f + RaisePx);
	// Keep labels inside the screen margin (a tag at the edge was cut off: "DGE" for BRIDGE).
	const FVector2D View = UWidgetLayoutLibrary::GetViewportSize(this) / ViewportScale;
	// Off-screen spots get no label (clamping them stacked labels on the edge).
	if (P.Screen.X < 0.0f || P.Screen.X > View.X)
	{
		Tag.Box->SetRenderOpacity(0.0f);
		return false;
	}
	P.Screen.X = FMath::Clamp(P.Screen.X, Style->TagEdgeMargin, FMath::Max(Style->TagEdgeMargin, View.X - Style->TagEdgeMargin));
	P.Opacity = Opacity * FMath::Lerp(1.0f, Style->TagFarOpacity, Far);
	P.Priority = InPriority;
	P.Scale = FMath::Lerp(1.0f, Style->TagFarScale, Far) * ScaleMul;
	PendingTags.Add(P);
	return true;
}

void UFTBHUDWidget::ResolveTags()
{
	// Highest priority keeps its spot; others move up until they clear every placed label (or give up and fade).
	PendingTags.Sort([](const FPendingTag& A, const FPendingTag& B) { return A.Priority != B.Priority ? A.Priority > B.Priority : A.Screen.Y > B.Screen.Y; });
	TArray<FBox2D> Placed;
	const IFTBObserver* Obs = Observer();
	const float Fade = Obs ? 1.0f - Obs->Obs_GetFadeAlpha() : 1.0f;
	for (const FPendingTag& P : PendingTags)
	{
		const FString Str = P.Tag->Text->GetText().ToString();
		const float CharW = Style->LabelSize * 0.58f * (1.0f + Style->LabelTracking / 1000.0f);
		const float W = (FMath::Max(2, Str.Len()) * CharW + 12.0f) * P.Scale;
		const float H = (Style->LabelSize * 1.35f + 8.0f) * P.Scale;
		float Push = 0.0f;
		auto RectAt = [&](float Up) { const FVector2D Bottom(P.Screen.X, P.Screen.Y - Up); return FBox2D(Bottom - FVector2D(W * 0.5f, H), Bottom + FVector2D(W * 0.5f, 0.0f)); };
		bool bClear = false;
		for (int32 Iter = 0; Iter < 8; ++Iter)
		{
			const FBox2D R = RectAt(Push);
			float Need = 0.0f;
			for (const FBox2D& O : Placed)
			{
				if (R.Intersect(O))
				{
					Need = FMath::Max(Need, R.Max.Y - O.Min.Y + Style->TagGap);
				}
			}
			if (Need <= 0.0f) { bClear = true; break; }
			Push += Need;
			if (Push > Style->TagMaxPush) { break; }
		}
		Push = FMath::Min(Push, Style->TagMaxPush);
		Placed.Add(RectAt(Push));
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(P.Tag->Box->Slot))
		{
			S->SetPosition(P.Screen);
		}
		SetSize(P.Tag->Stem, FVector2D(2.0f, Push > 3.0f ? Push : 0.0f));
		P.Tag->Box->SetRenderScale(FVector2D(P.Scale));
		P.Tag->Box->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
		P.Tag->Box->SetRenderOpacity(P.Opacity * Fade * (bClear ? 1.0f : 0.45f));
	}
	PendingTags.Reset();
}

void UFTBHUDWidget::UpdateTags(float Dt)
{
	const IFTBWorldQuery* Q = Query();
	IFTBObserver* O = Observer();
	if (!Q || !O) { return; }
	const float Now = FTB::Now(this);
	const bool bPlay = Q->FTB_GetState() == EFTBState::Active;
	const UFTBRoundConfig* Config = Q->FTB_GetConfig();

	// Fix + lead.
	const float FixAge = Now - LastFix.Time;
	const float FixAlpha = (bPlay && LastFix.bValid && FixAge < 9.0f) ? 1.0f - FMath::Square(FMath::Clamp((FixAge - 6.0f) / 3.0f, 0.0f, 1.0f)) : 0.0f;
	FixTag.Text->SetText(Metres(LastFix.RangeM));
	PinTag(FixTag, LastFix.Location + FVector(0, 0, 1500), FixAlpha * 0.95f, 0.0f, 80);
	if (LastFix.bOnRoad && Config)
	{
		const FVector Lead = Q->FTB_GetRoad().At(FTB::LeadS(LastFix, Now, Config->TankSpeedMps * 100.0f));
		LeadTag.Text->SetText(LOCTEXT("LeadTag", "LEAD"));
		PinTag(LeadTag, Lead + FVector(0, 0, 1500), FixAlpha, 0.0f, 90);
	}
	else
	{
		LeadTag.Box->SetRenderOpacity(0.0f);
	}

	// Shells in flight: seconds to impact over the ring.
	for (FShellTag& T : ShellTags)
	{
		if (!T.bUsed) { continue; }
		const float Left = T.ImpactTime - Now;
		if (Left <= 0.0f || !bPlay) { T.bUsed = false; T.Tag.Box->SetRenderOpacity(0.0f); continue; }
		T.Tag.Text->SetText(FText::AsNumber(FMath::CeilToInt(Left)));
		PinTag(T.Tag, T.Target + FVector(0, 0, 1200), 1.0f, 14.0f, 100);
	}

	// Bridge: always marked, brighter as the column closes.
	BridgeTag.Text->SetText(LOCTEXT("BridgeTag", "BRIDGE"));
	const float Eta = Q->FTB_GetBridgeEta();
	PinTag(BridgeTag, Q->FTB_GetBridgeLocation() + FVector(0, 0, 900), bPlay ? FMath::Lerp(0.7f, 1.0f, FMath::Clamp(1.0f - Eta / 30.0f, 0.0f, 1.0f)) : 0.0f, 0.0f, 60);

	// Wrecks.
	TArray<FVector> Wrecks;
	Q->FTB_GetWrecks(Wrecks);
	for (int32 i = 0; i < WreckTags.Num(); ++i)
	{
		WreckTags[i].Text->SetText(LOCTEXT("WreckTag", "WRECK"));
		PinTag(WreckTags[i], Wrecks.IsValidIndex(i) ? Wrecks[i] + FVector(0, 0, 700) : FVector::ZeroVector, (bPlay && Wrecks.IsValidIndex(i)) ? 0.7f : 0.0f, 0.0f, 30, 0.9f);
	}

	// Posts: shown while moving or right after being traced.
	const bool bPosts = bPlay && (Now < PostTagsUntil || O->Obs_IsRunning());
	for (int32 i = 0; i < PostTags.Num(); ++i)
	{
		if (i >= Q->FTB_GetPostCount()) { PostTags[i].Box->SetRenderOpacity(0.0f); continue; }
		PostTags[i].Text->SetText(FText::Format(LOCTEXT("PostTag", "{0}   {1}"), FText::AsNumber(i + 1), Q->FTB_GetPostName(i)));
		PinTag(PostTags[i], Q->FTB_GetPostEye(i) + FVector(0, 0, 600), (bPosts && i != O->Obs_GetPost()) ? 0.95f : 0.0f, 0.0f, 50);
	}

	// "+1" popups float up and fade.
	for (FPopup& P : Popups)
	{
		if (!P.bUsed) { continue; }
		P.Age += Dt;
		if (P.Age > 1.4f || !bPlay) { P.bUsed = false; P.Tag.Box->SetRenderOpacity(0.0f); continue; }
		const float E = 1.0f - FMath::Square(1.0f - FMath::Clamp(P.Age / 1.4f, 0.0f, 1.0f));
		PinTag(P.Tag, P.Location, 1.0f - FMath::Clamp((P.Age - 0.9f) / 0.5f, 0.0f, 1.0f), 40.0f * E, 95, 1.0f + 0.4f * (1.0f - E));
	}
	ResolveTags();
}

void UFTBHUDWidget::UpdateFeedback(float Dt)
{
	const IFTBWorldQuery* Q = Query();
	Vignette = FMath::Max(0.0f, Vignette - Dt * 1.6f);
	float V = Vignette;
	if (Q && Q->FTB_GetState() == EFTBState::Defeat)
	{
		V = FMath::Max(V, 0.3f + 0.2f * FMath::Abs(FMath::Sin(StateAge * 3.0f)));
	}
	// Last 15 seconds: a faint red edge breathes (escalation is the only time the HUD raises its voice).
	if (Q && Q->FTB_GetState() == EFTBState::Active && Q->FTB_GetBridgeEta() < 15.0f)
	{
		V = FMath::Max(V, 0.12f + 0.08f * FMath::Sin(StateAge * 4.0f));
	}
	for (UImage* I : VignetteImages) { I->SetRenderOpacity(V); }
	const IFTBObserver* Obs = Observer();
	FadeImage->SetRenderOpacity(Obs ? Obs->Obs_GetFadeAlpha() : 0.0f);
}

#undef LOCTEXT_NAMESPACE
