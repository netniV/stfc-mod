#pragma once
#include "choice_setting.h"
#include "slider_setting.h"
namespace mod_settings
{
ChoiceSetting& FleetLabelDetailSetting(bool player);
SliderSetting& FleetLabelThresholdSetting(bool player);
std::string    FleetLabelSummary(bool player);
} // namespace mod_settings
// Existing zoom adapter owns native fleet labels and reports installation.
bool FleetLabelControlsAvailable();
void RefreshFleetLabelControls();
