#include "preview_settings.h"
#include "config.h"
#include "patches/runtime_config.h"
#include <array>

namespace mod_settings
{
namespace
{
  Definition PreviewDefinition(const char* key, const char* label, bool Config::* member, bool inverted = false)
  {
    return {std::string("community_mod.ui.") + key, label,
            [member, inverted] { return ReadResult::Known((Config::Get().*member) != inverted, 1); },
            [key, member, inverted](bool desired, std::uint64_t generation) {
              if (generation != 1)
                return ApplyResult::Rejected;
              const bool stored     = desired != inverted;
              Config::Get().*member = stored;
              runtime_config::SaveSetting("ui", key, stored);
              return ApplyResult::Applied;
            }};
  }
} // namespace

BooleanSetting& PreviewSetting(PreviewOption option)
{
  static std::array settings{
      BooleanSetting(PreviewDefinition("disable_preview_locate", "Allow Locate while a preview is open",
                                       &Config::disable_preview_locate, true)),
      BooleanSetting(PreviewDefinition("disable_preview_recall", "Allow Recall while a preview is open",
                                       &Config::disable_preview_recall, true)),
      BooleanSetting(PreviewDefinition("show_cargo_default", "Automatically open cargo", &Config::show_cargo_default)),
      BooleanSetting(PreviewDefinition("show_player_cargo", "Player fleets", &Config::show_player_cargo)),
      BooleanSetting(PreviewDefinition("show_station_cargo", "Stations", &Config::show_station_cargo)),
      BooleanSetting(PreviewDefinition("show_hostile_cargo", "Hostiles", &Config::show_hostile_cargo)),
      BooleanSetting(PreviewDefinition("show_armada_cargo", "Armada targets", &Config::show_armada_cargo))};
  static_assert(settings.size() == static_cast<std::size_t>(PreviewOption::Count));
  return settings.at(static_cast<std::size_t>(option));
}

void TogglePreviewSetting(PreviewOption option)
{
  auto&      setting = PreviewSetting(option);
  const auto before  = setting.Observe();
  if (before.state.known())
    setting.SetFromUser(!*before.state.value, before);
}
} // namespace mod_settings
