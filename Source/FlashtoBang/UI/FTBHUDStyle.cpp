// Flash to Bang - HUD style defaults.

#include "UI/FTBHUDStyle.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"

UFTBHUDStyle::UFTBHUDStyle()
{
	Text = FLinearColor::FromSRGBColor(FColor(0xE9, 0xE4, 0xD8));
	Text.A = 1.0f;
	TextDim = Text;
	TextDim.A = 0.82f;
	Accent = FLinearColor::FromSRGBColor(FColor(0xE0, 0xA8, 0x4A));
	Danger = FLinearColor::FromSRGBColor(FColor(0xD9, 0x48, 0x3B));
	Track = FLinearColor(1.0f, 1.0f, 1.0f, 0.16f);
	Scrim = FLinearColor(0.004f, 0.006f, 0.01f, 0.7f);
	RegularFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/FlashtoBang/UI/Fonts/FF_FTB_Medium.FF_FTB_Medium")));
	BoldFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/FlashtoBang/UI/Fonts/FF_FTB_Bold.FF_FTB_Bold")));
	PanelTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/FlashtoBang/Textures/T_FTB_SoftPanel.T_FTB_SoftPanel")));
	BinocularMask = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/FlashtoBang/Textures/T_FTB_BinocularMask.T_FTB_BinocularMask")));
}
