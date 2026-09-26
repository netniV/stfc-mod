#pragma once
#include "choice_setting.h"

namespace mod_settings
{
enum class MissionHudOption { Trials, FieldTraining, Outposts, Missions, Count };
ChoiceSetting& MissionHudSetting(MissionHudOption option);
} // namespace mod_settings
