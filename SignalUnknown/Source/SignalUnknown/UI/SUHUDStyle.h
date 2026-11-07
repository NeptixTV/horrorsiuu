// SIGNAL UNKNOWN – colours shared by the HUD source files (a header, so that Unreal's unity
// builds, which merge several .cpp files, do not see the constants defined twice).
#pragma once

#include "CoreMinimal.h"

namespace SUHUDStyle
{
	inline const FLinearColor Text(0.92f, 0.90f, 0.85f);
	inline const FLinearColor Dim(0.62f, 0.60f, 0.56f);
	inline const FLinearColor Accent(1.f, 0.72f, 0.32f);
	inline const FLinearColor Good(0.55f, 0.95f, 0.6f);
	inline const FLinearColor Bad(1.f, 0.42f, 0.36f);
	inline const FLinearColor PanelBg(0.03f, 0.035f, 0.04f);
	inline const FLinearColor ButtonBg(0.12f, 0.12f, 0.12f, 0.9f);
	inline const FLinearColor ButtonFocus(0.32f, 0.22f, 0.1f, 0.95f);
	inline const FLinearColor Terminal(0.55f, 1.f, 0.65f);
	inline const FLinearColor TerminalDim(0.3f, 0.6f, 0.38f);
}
