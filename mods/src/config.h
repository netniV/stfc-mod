#pragma once

#include <map>
#include <vector>

#include <toml++/toml.h>

#if _WIN32
#include <Windows.h>
#endif

class Config
{
public:
  Config();

  static Config& Get();
  static float   GetDPI();
  static float   RefreshDPI();

#ifdef _WIN32
  static HWND WindowHandle();
#endif

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

  bool  queue_enabled;
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
  float transition_time;

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

  bool show_cargo_default;
  bool show_player_cargo;
  bool show_station_cargo;
  bool show_hostile_cargo;
  bool show_armada_cargo;

  bool always_skip_reveal_sequence;

  std::map<std::string, std::string> sync_targets;

  std::string sync_proxy;
  std::string sync_file;

  bool sync_debug;
  bool sync_logging;
  bool sync_resources;
  bool sync_battlelogs;
  bool sync_officer;
  bool sync_missions;
  bool sync_research;
  bool sync_tech;
  bool sync_traits;
  bool sync_buildings;
  bool sync_ships;

  std::string config_settings_url;
  std::string config_assets_url_override;
};
