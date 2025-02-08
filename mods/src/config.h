#pragma once

#include <map>
#include <vector>

#include <toml++/toml.h>

#define CONFIG_FILE_DEFAULT "community_patch_settings.toml"
#define CONFIG_FILE_RUNTIME "community_patch_runtime.vars"
#define CONFIG_FILE_PARSED "community_patch_settings_parsed.toml"

class Config
{
public:
  Config();

  static Config& Get();
  static float   GetDPI();
  static float   RefreshDPI();

  static std::string_view Filename();

  static void Save(toml::table config, std::string_view filename, bool apply_warning = true);
  void        Load();
  void        AdjustUiScale(bool scaleUp);
  void        AdjustUiViewerScale(bool scaleUp);

public:
  float ui_scale;
  float ui_scale_adjust;
  float ui_scale_viewer;
  float zoom;
  bool  free_resize;
  bool  adjust_scale_res;
  bool  show_all_resolutions;

  bool  use_out_of_dock_power;
  float system_pan_momentum;
  float system_pan_momentum_falloff;

  float keyboard_zoom_speed;

  bool  hotkeys_enabled;
  bool  hotkeys_extended;
  bool  use_scopely_hotkeys;
  bool  use_presets_as_default;
  bool  enable_experimental;
  float default_system_zoom;

  float system_zoom_preset_1;
  float system_zoom_preset_2;
  float system_zoom_preset_3;
  float system_zoom_preset_4;
  float system_zoom_preset_5;

  int32_t target_framerate;
  int32_t vsync;
  float   transition_time;

  bool             borderless_fullscreen_f11;
  std::vector<int> disabled_banner_types;

  int  extend_donation_max;
  bool extend_donation_slider;
  bool disable_move_keys;
  bool disable_preview_locate;
  bool disable_preview_recall;
  bool disable_escape_exit;
  bool disable_galaxy_chat;
  bool disable_first_popup;
  bool disable_toast_banners;
  bool fix_unity_web_requests;

  bool show_cargo_default;
  bool show_player_cargo;
  bool show_station_cargo;
  bool show_hostile_cargo;
  bool show_armada_cargo;

  bool always_skip_reveal_sequence;

  std::map<std::string, std::string> sync_targets;
  std::string sync_proxy;
  std::string sync_file;
  bool        sync_logging;
  bool        sync_resources;
  bool        sync_battlelogs;
  bool        sync_officer;
  bool        sync_missions;
  bool        sync_research;
  bool        sync_tech;
  bool        sync_traits;
  bool        sync_buildings;
  bool        sync_ships;

  std::string config_settings_url;
  std::string config_assets_url_override;
};
