#pragma once
#include "boolean_settings.h"
#include "patches/parts/galaxy_policy.h"
#include "choice_setting.h"
#include "slider_setting.h"

namespace mod_settings
{
BooleanSetting& GalaxyMultiSelectSetting();
BooleanSetting& GalaxyOverlaySetting(int native_mode);
void SetGalaxyOverlay(int native_mode, bool enabled);
galaxy_controls::OverlaySelection GalaxyOverlaySelection();
ChoiceSetting& GalaxyLabelDetailSetting(bool minor);
SliderSetting& GalaxyLabelThresholdSetting(bool minor);
std::string GalaxyLabelSummary(bool minor);
bool GalaxyLabelControlsAvailable();
void RefreshGalaxyLabelControls();
} // namespace mod_settings
