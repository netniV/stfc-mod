#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include <toml++/toml.h>

#if _WIN32
#include <Windows.h>
#endif

class SyncConfig
{
public:
  enum class Type {
    Battles,
    Buffs,
    Buildings,
    EmeraldChain,
    Inventory,
    Jobs,
    Missions,
    Officer,
    Research,
    Resources,
    Ships,
    Slots,
    Tech,
    Traits
  };

  struct Option {
    Type             type;
    std::string_view type_str;   // used in JSON body
    std::string_view option_str; // used in TOML file
    bool SyncConfig::* option;
  };

  std::string proxy;
  bool verify_ssl = true;

  bool battlelogs = false;
  bool buffs      = false;
  bool buildings  = false;
  bool inventory  = false;
  bool jobs       = false;
  bool missions   = false;
  bool officer    = false;
  bool research   = false;
  bool resources  = false;
  bool ships      = false;
  bool slots      = false;
  bool tech       = false;
  bool traits     = false;

  [[nodiscard]] bool enabled(Type type) const;
};

constexpr std::array SyncOptions{
    SyncConfig::Option{SyncConfig::Type::Battles, "battlelog", "battlelogs", &SyncConfig::battlelogs},
    SyncConfig::Option{SyncConfig::Type::Buffs, "buff", "buffs", &SyncConfig::buffs},
    SyncConfig::Option{SyncConfig::Type::Buildings, "module", "buildings", &SyncConfig::buildings},
    SyncConfig::Option{SyncConfig::Type::EmeraldChain, "emerald_chain", "buffs", &SyncConfig::buffs},
    SyncConfig::Option{SyncConfig::Type::Inventory, "inventory", "inventory", &SyncConfig::inventory},
    SyncConfig::Option{SyncConfig::Type::Jobs, "job", "jobs", &SyncConfig::jobs},
    SyncConfig::Option{SyncConfig::Type::Missions, "mission", "missions", &SyncConfig::missions},
    SyncConfig::Option{SyncConfig::Type::Officer, "officer", "officer", &SyncConfig::officer},
    SyncConfig::Option{SyncConfig::Type::Research, "research", "research", &SyncConfig::research},
    SyncConfig::Option{SyncConfig::Type::Resources, "resource", "resources", &SyncConfig::resources},
    SyncConfig::Option{SyncConfig::Type::Ships, "ship", "ships", &SyncConfig::ships},
    SyncConfig::Option{SyncConfig::Type::Slots, "slot", "slots", &SyncConfig::slots},
    SyncConfig::Option{SyncConfig::Type::Tech, "ft", "tech", &SyncConfig::tech},
    SyncConfig::Option{SyncConfig::Type::Traits, "trait", "traits", &SyncConfig::traits},
};

constexpr std::string to_string(const SyncConfig::Type type)
{
  for (const auto& opt : SyncOptions) {
    if (opt.type == type) {
      return std::string(opt.type_str);
    }
  }

  return {};
}

constexpr std::string operator+(const std::string& prefix, const SyncConfig::Type type)
{
  return prefix + to_string(type);
}

constexpr std::string operator+(const SyncConfig::Type type, const std::string& suffix)
{
  return to_string(type) + suffix;
}

class SyncTargetConfig : public SyncConfig
{
public:
  std::string url;
  std::string token;
};

class GitHubSyncConfig
{
public:
  bool        enabled  = false;
  std::string gist_id;
  std::string username;  // GitHub username â€” auto-populated from API response, or set manually
  std::string token;
  std::string filename_player    = "stfc_player.json";
  std::string filename_ships     = "stfc_ships.json";
  std::string filename_resources = "stfc_resources.json";
  std::string filename_research   = "stfc_research.json";
  std::string filename_officers   = "stfc_officers.json";
  std::string filename_missions   = "stfc_missions.json";
  std::string filename_faction    = "stfc_faction.json";
  std::string filename_buffs      = "stfc_buffs.json";
  std::string filename_territory  = "stfc_territory.json";
  std::string filename_buildings  = "stfc_buildings.json";
  std::string filename_battlelog  = "stfc_battlelog.json";
  std::string filename_manifest   = "stfc_manifest.json";
};

class ShieldAlertsConfig
{
public:
  bool enabled                      = false;
  int  poll_interval_seconds        = 60;   // how often to evaluate alert conditions
  int  reminder_interval_minutes    = 30;   // minimum gap between repeated warnings
  // Peace shield expiry thresholds (hours before expiry to warn); empty = no threshold warnings
  std::vector<int> shield_warn_hours = {4, 2, 1};
};

class Config final
{
public:
  Config();

  [[nodiscard]] static Config& Get();
  [[nodiscard]] static float   GetDPI();
  static float                 RefreshDPI();

#ifdef _WIN32
  [[nodiscard]] static HWND WindowHandle();
#endif

  static void Save(const toml::table& config, std::string_view filename, bool apply_warning = true);
  void        Load();
  void        AdjustUiScale(bool scaleUp);
  void        AdjustUiViewerScale(bool scaleUp);

  // Disallow copying/moving to enforce singleton
  Config(const Config&)            = delete;
  Config& operator=(const Config&) = delete;
  Config(Config&&)                 = delete;
  Config& operator=(Config&&)      = delete;

  float ui_scale;
  float ui_scale_adjust;
  float ui_scale_viewer;
  float zoom;
  bool  allow_cursor;
  bool  free_resize;
  bool  adjust_scale_res;
  bool  show_all_resolutions;

  bool  use_out_of_dock_power;
  float system_pan_momentum;
  float system_pan_momentum_falloff;

  float keyboard_zoom_speed;
  int   select_timer;

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

  bool             borderless_fullscreen;
  std::vector<int> disabled_banner_types;

  int  extend_donation_max;
  bool extend_donation_slider;
  bool disable_move_keys;
  bool disable_preview_locate;
  bool disable_preview_recall;
  bool disable_escape_exit;
  bool disable_galaxy_chat;
  bool disable_veil_chat;
  bool disable_first_popup;
  bool disable_toast_banners;

  bool show_cargo_default;
  bool show_player_cargo;
  bool show_station_cargo;
  bool show_hostile_cargo;
  bool show_armada_cargo;

  bool always_skip_reveal_sequence;

  bool       sync_logging;
  bool       sync_debug;
  int        sync_resolver_cache_ttl;
  SyncConfig sync_options;

  std::map<std::string, SyncTargetConfig> sync_targets;

  // Game state export settings
  bool        game_state_enabled;
  int         game_state_interval;        // seconds, 0 = manual/shutdown only
  std::string game_state_path;            // empty = game directory
  bool        game_state_on_startup;      // export immediately on mod load
  std::string game_state_player_id;       // player userid for accurate player data capture
  bool        game_state_battle_log;      // enable battle log CSV watcher (default: false)
  GitHubSyncConfig     game_state_github;        // GitHub Gist sync config
  ShieldAlertsConfig   shield_alerts;            // peace shield alert settings
  // Per-type opt-out flags for game state export. Derived from [sync] flags:
  // true by default when game_state_enabled=true; false only if explicitly set
  // to false in [sync]. Completely separate from sync_options (legacy targets).
  SyncConfig game_state_options;

  bool installUiScaleHooks;
  bool installZoomHooks;
  bool installBuffFixHooks;
  bool installToastBannerHooks;
  bool installPanHooks;
  bool installImproveResponsivenessHooks;
  bool installHotkeyHooks;
  bool installFreeResizeHooks;
  bool installTempCrashFixes;
  bool installTestPatches;
  bool installMiscPatches;
  bool installChatPatches;
  bool installResolutionListFix;
  bool installSyncPatches;
  bool installObjectTracker;

  std::string config_settings_url;
  std::string config_assets_url_override;

  // Loading Screen Background
  bool        loader_transition;
  bool        loader_enabled;
  std::string loader_image;

  bool installLoadingScreenBgHooks;
};
