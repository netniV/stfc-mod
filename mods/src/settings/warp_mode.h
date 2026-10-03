#pragma once
#include "choice_setting.h"

enum class InstantWarpConfirmation;

namespace mod_settings
{
// Shared mutation path for the shortcut and the forthcoming selection control.
// Changes live behavior immediately; persistence remains best-effort and reports
// failures through the existing runtime writer. Call on the game UI thread.
bool           SetWarpMode(InstantWarpConfirmation desired);
void           CycleWarpMode();
ChoiceSetting& WarpModeSetting();
} // namespace mod_settings
