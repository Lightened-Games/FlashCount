#include "FactoryLegendWidget.h"
#include "FactoryKitStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UFactoryLegendWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UFactoryLegendWidget::BuildTree()
{
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LegendList"));
	UWidget* Holder = List;
	if (UTexture2D* PanelTex = S->LegendPanelTexture.LoadSynchronous())
	{
		// Themed surface behind the start-screen controls (shown on the start screen only).
		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LegendPanel"));
		Panel->SetBrush(FactoryKitStyle::BoxBrush(PanelTex, S->PanelBoxMargin));
		Panel->SetPadding(S->LegendPanelPadding);
		Panel->SetContent(List);
		Holder = Panel;
	}
	UCanvasPanelSlot* LS = Root->AddChildToCanvas(Holder);
	LS->SetAutoSize(true);
	SetMenuPlacement(bMenuPlacement);

	FlashText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeyFlash"));
	FactoryKitStyle::Apply(FlashText, S->TitleTypeface, S->SmallSize, S->AccentColor, 80);
	UCanvasPanelSlot* FS = Root->AddChildToCanvas(FlashText);
	FS->SetAnchors(FAnchors(0.0f, 1.0f));
	FS->SetAlignment(FVector2D(0.0f, 1.0f));
	FS->SetPosition(FVector2D(S->LegendMargin.X, -S->LegendMargin.Y));
	FS->SetAutoSize(true);
	FlashText->SetRenderOpacity(0.0f);

	Rows.Reset();
	if (!S->LegendTitle.IsEmpty())
	{
		UTextBlock* Head = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LegendTitle"));
		FactoryKitStyle::Apply(Head, S->TitleTypeface, S->SmallSize, S->AccentColor, 120);
		Head->SetText(S->LegendTitle);
		HeadText = Head;
		if (UVerticalBoxSlot* HS0 = List->AddChildToVerticalBox(Head)) { HS0->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f)); }
	}
	for (const FFactoryLegendRow& R : Pending)
	{
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeyBox->SetMinDesiredWidth(S->SmallSize * 6.0f); // key column fits "MOUSE" / "SPACE" (PlazaDuty lesson)
		UTextBlock* K = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(K, S->TitleTypeface, S->SmallSize, S->TextColor, 80);
		K->SetText(R.KeyText);
		KeyBox->AddChild(K);
		H->AddChildToHorizontalBox(KeyBox);
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(L, S->BodyTypeface, S->SmallSize, S->DimColor, 60);
		L->SetText(R.Label);
		H->AddChildToHorizontalBox(L);
		UVerticalBoxSlot* VS = List->AddChildToVerticalBox(H);
		VS->SetPadding(FMargin(0.0f, 3.0f));
		Rows.Add({ R, K, L, 0.0f });
	}

	Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AutoplayHint"));
	FactoryKitStyle::Apply(Hint, S->BodyTypeface, S->SmallSize, S->DimColor, 60);
	Hint->SetText(S->AutoplayHintText);
	UCanvasPanelSlot* HS = Root->AddChildToCanvas(Hint);
	HS->SetAnchors(FAnchors(1.0f, 1.0f));
	HS->SetAlignment(FVector2D(1.0f, 1.0f));
	HS->SetPosition(FVector2D(-S->LegendMargin.X, -S->LegendMargin.Y));
	HS->SetAutoSize(true);
	Hint->SetRenderOpacity(0.0f);
}

void UFactoryLegendWidget::SetRows(const TArray<FFactoryLegendRow>& InRows)
{
	Pending = InRows; // call before AddToViewport (the flow subsystem does)
}

void UFactoryLegendWidget::Pulse(FName ActionId)
{
	for (FRowWidgets& R : Rows)
	{
		if (R.Data.ActionId == ActionId) { R.Glow = 1.0f; R.Flash = 1.0f; }
	}
}

void UFactoryLegendWidget::PulseKey(const FKey& Key)
{
	for (FRowWidgets& R : Rows)
	{
		if (R.Data.Keys.Contains(Key.GetFName())) { R.Glow = 1.0f; R.Flash = 1.0f; }
	}
}

void UFactoryLegendWidget::SetAutoplayHint(bool bVisible)
{
	if (bVisible && !bHint) { HintTime = 0.0f; }
	bHint = bVisible;
}

void UFactoryLegendWidget::SetLegendVisible(bool bVisible)
{
	bLegendVisible = bVisible;
}

void UFactoryLegendWidget::SetMenuPlacement(bool bMenu)
{
	bMenuPlacement = bMenu;
	// With a themed panel the list sits inside it, so the panel owns the canvas slot.
	UWidget* Holder = Panel ? static_cast<UWidget*>(Panel) : static_cast<UWidget*>(List);
	UCanvasPanelSlot* LS = Holder ? Cast<UCanvasPanelSlot>(Holder->Slot) : nullptr;
	if (!LS)
	{
		return;
	}
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	LS->SetAnchors(FAnchors(bMenu ? 1.0f : 0.0f, 1.0f));
	LS->SetAlignment(FVector2D(bMenu ? 1.0f : 0.0f, 1.0f));
	LS->SetPosition(bMenu ? FVector2D(-S->LegendMargin.X, -S->LegendMargin.Y - S->MenuLegendLift) : FVector2D(S->LegendMargin.X, -S->LegendMargin.Y));
}

void UFactoryLegendWidget::SetFlashEnabled(bool bEnabled)
{
	bFlash = bEnabled;
	if (!bFlash)
	{
		for (FRowWidgets& R : Rows) { R.Flash = 0.0f; }
	}
}

void UFactoryLegendWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	LegendAlpha = FactoryKitStyle::EaseTo(LegendAlpha, bLegendVisible ? 1.0f : 0.0f, InDeltaTime, S->MotionSeconds);
	for (FRowWidgets& R : Rows)
	{
		R.Glow = FMath::Max(0.0f, R.Glow - InDeltaTime / FMath::Max(S->LegendPulseSeconds, 0.05f));
		const float G = FMath::InterpEaseOut(0.0f, 1.0f, R.Glow, 2.0f);
		const bool bInk = Panel && bMenuPlacement; // on the themed panel: the surface's ink colours
		const FLinearColor KeyC = bInk ? UFactoryKitSettings::Or(S->PanelTextColor, S->TextColor) : S->TextColor;
		const FLinearColor LabC = bInk ? UFactoryKitSettings::Or(S->PanelDimColor, S->DimColor) : S->DimColor;
		const FLinearColor AccC = bInk ? UFactoryKitSettings::Or(S->PanelAccentColor, S->AccentColor) : S->AccentColor;
		R.Key->SetColorAndOpacity(FSlateColor(FMath::Lerp(KeyC, AccC, G)));
		R.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(LabC, KeyC, G)));
		R.Key->SetRenderOpacity(LegendAlpha);
		R.Label->SetRenderOpacity(LegendAlpha);
	}
	if (HeadText) { HeadText->SetRenderOpacity(LegendAlpha); }
	if (Panel)
	{
		Panel->SetVisibility(bMenuPlacement ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Panel->SetRenderOpacity(LegendAlpha);
		if (HeadText) { HeadText->SetColorAndOpacity(FSlateColor(UFactoryKitSettings::Or(S->PanelAccentColor, S->AccentColor))); }
	}
	if (FlashText)
	{
		// Pressed keys, newest first, each fading over KeyFlashSeconds.
		FString Keys;
		float Alpha = 0.0f;
		for (FRowWidgets& R : Rows)
		{
			R.Flash = FMath::Max(0.0f, R.Flash - InDeltaTime / FMath::Max(S->KeyFlashSeconds, 0.05f));
			if (bFlash && R.Flash > 0.02f)
			{
				Keys += (Keys.IsEmpty() ? TEXT("") : TEXT("    ")) + R.Data.KeyText.ToString();
				Alpha = FMath::Max(Alpha, FMath::InterpEaseOut(0.0f, 1.0f, R.Flash, 2.0f));
			}
		}
		if (!Keys.IsEmpty() && Keys != LastFlash)
		{
			FlashText->SetText(FText::FromString(Keys));
			LastFlash = Keys;
		}
		FlashText->SetRenderOpacity(Alpha);
	}
	if (Hint)
	{
		HintTime += InDeltaTime;
		const float Target = !bHint ? 0.0f : (HintTime < S->AutoplayHintFullSeconds ? 1.0f : S->AutoplayHintDimAlpha);
		HintAlpha = FactoryKitStyle::EaseTo(HintAlpha, Target, InDeltaTime, S->MotionSeconds);
		Hint->SetRenderOpacity(HintAlpha);
	}
}
