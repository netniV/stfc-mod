#include "galaxy_labels.h"
#include "config.h"
#include "patches/runtime_config.h"
#include <format>
namespace mod_settings
{
namespace
{
  galaxy_controls::ZoomProfile& Profile(bool minor)
  { return minor ? Config::Get().galaxy_label_minor : Config::Get().galaxy_label_major; }
  ValueDefinition<int> Detail(bool minor)
  {
    return {minor ? "community_mod.galaxy.minor.detail" : "community_mod.galaxy.major.detail", "Label visibility",
            [minor] {
              return GalaxyLabelControlsAvailable()
                         ? ValueReadResult<int>::Known(static_cast<int>(Profile(minor).mode), 1)
                         : ValueReadResult<int>{};
            },
            [minor](int value, std::uint64_t generation) {
              if (generation != 1 || !GalaxyLabelControlsAvailable() || value < 0 || value > 2)
                return ApplyResult::Rejected;
              Profile(minor).mode = static_cast<galaxy_controls::ZoomMode>(value);
              constexpr const char* names[]{"native", "always", "threshold"};
              runtime_config::SaveSetting("graphics",
                                          minor ? "galaxy_label_minor_detail" : "galaxy_label_major_detail",
                                          std::string(names[value]));
              RefreshGalaxyLabelControls();
              return ApplyResult::Applied;
            }};
  }
  ValueDefinition<float> Threshold(bool minor)
  {
    return {minor ? "community_mod.galaxy.minor.threshold" : "community_mod.galaxy.major.threshold",
            "Label zoom threshold",
            [minor] {
              return GalaxyLabelControlsAvailable() ? ValueReadResult<float>::Known(Profile(minor).threshold, 1)
                                                   : ValueReadResult<float>{};
            },
            [minor](float value, std::uint64_t generation) {
              if (generation != 1 || !GalaxyLabelControlsAvailable())
                return ApplyResult::Rejected;
              Profile(minor).threshold = value;
              // Keep the TOML decimal at the slider's whole percentage, not float noise.
              runtime_config::SaveSetting(
                  "graphics", minor ? "galaxy_label_minor_threshold" : "galaxy_label_major_threshold",
                  std::round(static_cast<double>(value) * 100.0) / 100.0, std::chrono::milliseconds(150));
              RefreshGalaxyLabelControls();
              return ApplyResult::Applied;
            }};
  }
} // namespace
galaxy_controls::OverlaySelection GalaxyOverlaySelection()
{
  const auto& flags = Config::Get().galaxy_overlays;
  return galaxy_controls::OverlaySelection::FromFlags(flags[0], flags[1], flags[2], flags[3]);
}
void SetGalaxyOverlay(int mode, bool enabled)
{
  if (mode < 0 || mode > 3 || !GalaxyLabelControlsAvailable() || !Config::Get().galaxy_multi_select) return;
  const auto before = GalaxyOverlaySelection();
  if (before.Contains(mode) == enabled) return;
  const auto after = before.Toggle(mode);
  constexpr const char* keys[]{"galaxy_overlay_default", "galaxy_overlay_mining",
                               "galaxy_overlay_hostiles", "galaxy_overlay_hazards"};
  for (int i = 0; i < 4; ++i) {
    Config::Get().galaxy_overlays[i] = after.Contains(i);
    // Save the entire effective selection, including Default fallback, so the
    // map buttons and settings restart with the same choices.
    runtime_config::SaveSetting("graphics", keys[i], after.Contains(i));
  }
  RefreshGalaxyLabelControls();
}
BooleanSetting& GalaxyOverlaySetting(int mode)
{
  static const auto definition = [](int index, const char* key, const char* label) -> Definition {
    return {std::string("community_mod.galaxy.overlays.") + key, label,
        [index] {
          return GalaxyLabelControlsAvailable() && Config::Get().galaxy_multi_select
                     ? ReadResult::Known(GalaxyOverlaySelection().Contains(index), 1) : ReadResult{};
        },
        [index](bool enabled, std::uint64_t generation) {
          if (generation != 1 || !GalaxyLabelControlsAvailable() || !Config::Get().galaxy_multi_select)
            return ApplyResult::Rejected;
          SetGalaxyOverlay(index, enabled);
          return ApplyResult::Applied;
        }};
  };
  static std::array settings{
      BooleanSetting(definition(0, "default", "Default (system names)")),
      BooleanSetting(definition(1, "mining", "Mining")),
      BooleanSetting(definition(2, "hostiles", "Hostiles")),
      BooleanSetting(definition(3, "hazards", "Hazards"))};
  return settings.at(static_cast<std::size_t>(mode));
}
BooleanSetting& GalaxyMultiSelectSetting()
{
  static BooleanSetting setting({"community_mod.galaxy.multi_select", "Select multiple overlays",
      [] { return GalaxyLabelControlsAvailable()
                      ? ReadResult::Known(Config::Get().galaxy_multi_select, 1) : ReadResult{}; },
      [](bool value, std::uint64_t generation) {
        if (generation != 1 || !GalaxyLabelControlsAvailable()) return ApplyResult::Rejected;
        Config::Get().galaxy_multi_select = value;
        runtime_config::SaveSetting("graphics", "galaxy_multi_select", value);
        RefreshGalaxyLabelControls();
        return ApplyResult::Applied;
      }});
  return setting;
}
ChoiceSetting& GalaxyLabelDetailSetting(bool minor)
{
  static ChoiceSetting minor_systems(Detail(true), {"Native", "Always (I wouldn’t)", "Threshold"});
  static ChoiceSetting major_systems(Detail(false), {"Native", "Always", "Threshold"});
  return minor ? minor_systems : major_systems;
}
SliderSetting& GalaxyLabelThresholdSetting(bool minor)
{
  static SliderSetting minor_systems(Threshold(true), 0, 1, 0.01f,
                               [] { return Profile(true).mode == galaxy_controls::ZoomMode::Threshold; },
                               SliderLabel::Percentage, 2, "Select Threshold");
  static SliderSetting major_systems(Threshold(false), 0, 1, 0.01f,
                              [] { return Profile(false).mode == galaxy_controls::ZoomMode::Threshold; },
                              SliderLabel::Percentage, 2, "Select Threshold");
  return minor ? minor_systems : major_systems;
}
std::string GalaxyLabelSummary(bool minor)
{
  auto&      setting = GalaxyLabelDetailSetting(minor);
  const auto state   = setting.state().Observe().state;
  if (!state.known())
    return "Unavailable";
  auto text = setting.labels().at(*state.value);
  if (*state.value == static_cast<int>(galaxy_controls::ZoomMode::Threshold)) {
    const auto threshold = GalaxyLabelThresholdSetting(minor).state().Observe().state;
    text += threshold.known() ? std::format(" · {:.2f}", *threshold.value) : " · Unavailable";
  }
  return text;
}
} // namespace mod_settings
