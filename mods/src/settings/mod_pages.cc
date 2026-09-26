#include "mod_pages.h"
#include "mission_hud.h"
#include "camera_settings.h"
#include "fleet_labels.h"
#include "preview_settings.h"
#include "warp_mode.h"

namespace mod_settings
{
PageCatalog& ModPages()
{
  static PageCatalog catalog("community_mod.settings", "Mod Settings");
  return catalog;
}
void RegisterModPages()
{
  auto& catalog = ModPages();
  // Group by player tasks. Stable identities still map to existing TOML keys;
  // a presentation move does not migrate configuration.
  if (KeyboardZoomControlAvailable() || PanGlideControlAvailable()) {
    catalog.AddPage("community_mod.graphics.camera", "Camera", "community_mod.settings");
    if (KeyboardZoomControlAvailable())
      catalog.AddSlider("community_mod.graphics.camera", KeyboardZoomSpeedSetting());
    if (PanGlideControlAvailable())
      catalog.AddSlider("community_mod.graphics.camera", PanGlideSetting());
  }
  catalog.AddPage("community_mod.navigation", "Map & Travel", "community_mod.settings");
  catalog.AddHeading("community_mod.navigation", "community_mod.navigation.warp", "Instant warp mode");
  catalog.AddChoice("community_mod.navigation", WarpModeSetting());
  catalog.AddPage("community_mod.previews", "Previews & Cargo", "community_mod.settings");
  if (PreviewShortcutsAvailable()) {
    for (auto option : {PreviewOption::Locate, PreviewOption::Recall})
      catalog.AddBoolean("community_mod.previews", PreviewSetting(option));
  }
  if (CargoPreviewsAvailable()) {
    catalog.AddBoolean("community_mod.previews", PreviewSetting(PreviewOption::Cargo));
    // Only presentation depends on the master; target choices remain saved.
    catalog.AddHeading("community_mod.previews", "community_mod.ui.cargo_targets", "Target types", false, [] {
      const auto state = PreviewSetting(PreviewOption::Cargo).Observe().state;
      return state.known() && *state.value;
    });
    for (auto option : {PreviewOption::PlayerCargo, PreviewOption::StationCargo, PreviewOption::HostileCargo,
                        PreviewOption::ArmadaCargo})
      catalog.AddBoolean("community_mod.previews", PreviewSetting(option));
  }
  catalog.AddPage("community_mod.hud", "HUD Buttons", "community_mod.settings");
  for (auto option : {MissionHudOption::Trials, MissionHudOption::FieldTraining,
                      MissionHudOption::Outposts, MissionHudOption::Missions}) {
    auto& setting = MissionHudSetting(option);
    catalog.AddHeading("community_mod.hud", setting.state().id() + ".section", setting.state().label(), true, {},
                       [option] {
                         auto& choice = MissionHudSetting(option);
                         const auto state = choice.state().Observe().state;
                         return state.known() ? choice.labels().at(*state.value) : std::string("Unavailable");
                       });
    catalog.AddChoice("community_mod.hud", setting);
  }
  catalog.AddPage("community_mod.labels", "Fleet Labels", "community_mod.settings");
  for (bool player : {true, false}) {
    catalog.AddHeading("community_mod.labels", player ? "community_mod.labels.player" : "community_mod.labels.other",
                       player ? "Player" : "Non-player", true, {}, [player] { return FleetLabelSummary(player); });
    catalog.AddChoice("community_mod.labels", FleetLabelDetailSetting(player));
    catalog.AddSlider("community_mod.labels", FleetLabelThresholdSetting(player));
  }
}
} // namespace mod_settings
