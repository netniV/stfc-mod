#include "mission_hud.h"
#include "config.h"
#include "patches/runtime_config.h"
#include <array>

namespace mod_settings
{
namespace
{
ValueDefinition<int> Visibility(const char* name, const char* key, const char* label)
{
  return {std::string("community_mod.hud.") + name, label,
          [name] { return ValueReadResult<int>::Known(static_cast<int>(Config::Get().MissionHudButtonVisibility(name)), 1); },
          [name, key](int value, std::uint64_t generation) {
            if (generation != 1 || value < 0 || value > 2)
              return ApplyResult::Rejected;
            // The HUD patch snapshots modes at startup. Store the next-launch
            // preference here; do not install detours from a UI callback.
            Config::Get().mission_hud_buttons[std::string(name)] = static_cast<MissionHudVisibility>(value);
            constexpr const char* modes[]{"auto", "always", "never"};
            runtime_config::SaveSetting("ui", key, std::string(modes[value]));
            return ApplyResult::Applied;
          }};
}
} // namespace

ChoiceSetting& MissionHudSetting(MissionHudOption option)
{
  static std::array settings{
      ChoiceSetting(Visibility("q_trials", "hud_q_trials", "Q's Trials"), {"Auto", "Always", "Never"}),
      ChoiceSetting(Visibility("field_training", "hud_field_training", "Field Training"), {"Auto", "Always", "Never"}),
      ChoiceSetting(Visibility("outposts", "hud_outposts", "Outposts"), {"Auto", "Always", "Never"}),
      ChoiceSetting(Visibility("missions", "hud_missions", "Missions"), {"Auto", "Always", "Never"})};
  static_assert(settings.size() == static_cast<std::size_t>(MissionHudOption::Count));
  return settings.at(static_cast<std::size_t>(option));
}
} // namespace mod_settings
