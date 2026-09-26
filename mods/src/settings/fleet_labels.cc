#include "fleet_labels.h"
#include "config.h"
#include "patches/runtime_config.h"
#include <format>
namespace mod_settings
{
namespace
{
  FleetLabelProfile& Profile(bool player)
  { return player ? Config::Get().zoom_label_player : Config::Get().zoom_label_non_player; }
  ValueDefinition<int> Detail(bool player)
  {
    return {player ? "community_mod.labels.player.detail" : "community_mod.labels.other.detail", "Label detail",
            [player] {
              return FleetLabelControlsAvailable()
                         ? ValueReadResult<int>::Known(static_cast<int>(Profile(player).detail), 1)
                         : ValueReadResult<int>{};
            },
            [player](int value, std::uint64_t generation) {
              if (generation != 1 || !FleetLabelControlsAvailable() || value < 0 || value > 3)
                return ApplyResult::Rejected;
              Profile(player).detail = static_cast<FleetLabelDetail>(value);
              constexpr const char* names[]{"native", "expanded", "compact", "threshold"};
              runtime_config::SaveSetting("graphics",
                                          player ? "zoom_label_player_detail" : "zoom_label_non_player_detail",
                                          std::string(names[value]));
              RefreshFleetLabelControls();
              return ApplyResult::Applied;
            }};
  }
  ValueDefinition<float> Threshold(bool player)
  {
    return {player ? "community_mod.labels.player.threshold" : "community_mod.labels.other.threshold",
            "Detail zoom threshold",
            [player] {
              return FleetLabelControlsAvailable() ? ValueReadResult<float>::Known(Profile(player).zoom_threshold, 1)
                                                   : ValueReadResult<float>{};
            },
            [player](float value, std::uint64_t generation) {
              if (generation != 1 || !FleetLabelControlsAvailable())
                return ApplyResult::Rejected;
              Profile(player).zoom_threshold = value;
              // Keep the TOML decimal at the slider's whole percentage, not float noise.
              runtime_config::SaveSetting(
                  "graphics", player ? "zoom_label_player_threshold" : "zoom_label_non_player_threshold",
                  std::round(static_cast<double>(value) * 100.0) / 100.0, std::chrono::milliseconds(150));
              RefreshFleetLabelControls();
              return ApplyResult::Applied;
            }};
  }
} // namespace
ChoiceSetting& FleetLabelDetailSetting(bool player)
{
  static ChoiceSetting players(Detail(true), {"Native", "Expanded", "Compact", "Threshold"});
  static ChoiceSetting others(Detail(false), {"Native", "Expanded", "Compact", "Threshold"});
  return player ? players : others;
}
SliderSetting& FleetLabelThresholdSetting(bool player)
{
  static SliderSetting players(Threshold(true), 0, 1, 0.01f,
                               [] { return Profile(true).detail == FleetLabelDetail::Threshold; },
                               SliderLabel::Percentage, 2, "Select Threshold");
  static SliderSetting others(Threshold(false), 0, 1, 0.01f,
                              [] { return Profile(false).detail == FleetLabelDetail::Threshold; },
                              SliderLabel::Percentage, 2, "Select Threshold");
  return player ? players : others;
}
std::string FleetLabelSummary(bool player)
{
  auto&      setting = FleetLabelDetailSetting(player);
  const auto state   = setting.state().Observe().state;
  if (!state.known())
    return "Unavailable";
  auto text = setting.labels().at(*state.value);
  if (*state.value == static_cast<int>(FleetLabelDetail::Threshold)) {
    const auto threshold = FleetLabelThresholdSetting(player).state().Observe().state;
    text += threshold.known() ? std::format(" · {:.2f}", *threshold.value) : " · Unavailable";
  }
  return text;
}
} // namespace mod_settings
