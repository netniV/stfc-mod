#pragma once
#include "slider_setting.h"

namespace mod_settings
{
SliderSetting& KeyboardZoomSpeedSetting();
SliderSetting& PanGlideSetting();
bool           KeyboardZoomControlAvailable();
bool           PanGlideControlAvailable();
} // namespace mod_settings
