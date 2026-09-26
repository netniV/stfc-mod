#pragma once
#include "boolean_settings.h"

namespace mod_settings
{
enum class PreviewOption { Locate, Recall, Cargo, PlayerCargo, StationCargo, HostileCargo, ArmadaCargo, Count };

// The UI and existing toggle shortcuts share these process-lifetime owners.
// Changes take effect on the next preview/action; no game action is invoked here.
BooleanSetting& PreviewSetting(PreviewOption option);
void            TogglePreviewSetting(PreviewOption option);

// Native page admission only. Shortcut behavior remains available independently
// of whether all the hooks needed to expose a complete page were installed.
bool PreviewShortcutsAvailable();
bool CargoPreviewsAvailable();
} // namespace mod_settings
