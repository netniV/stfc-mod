#include "config.h"
#include "file.h"
#include "patches/mapkey.h"
#include "prime/KeyCode.h"
#include "str_utils.h"
#include "version.h"
#include <prime/Toast.h>

#include <EASTL/tuple.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
#include "defaultconfig.h"

namespace DCP = DefaultConfig::Patches;
namespace DCG = DefaultConfig::Graphics;
namespace DCC = DefaultConfig::Control;
namespace DCU = DefaultConfig::UI;
namespace DCBS = DefaultConfig::Buffs;
namespace DCS = DefaultConfig::Sync;
namespace DCSC = DefaultConfig::SystemConfig;
namespace DCSH = DefaultConfig::Shortcuts;

static const eastl::tuple<const char*, int> bannerTypes[] = {
    {"Standard", ToastState::Standard},
    {"FactionWarning", ToastState::FactionWarning},
    {"FactionLevelUp", ToastState::FactionLevelUp},
    {"FactionLevelDown", ToastState::FactionLevelDown},
    {"FactionDiscovered", ToastState::FactionDiscovered},
    {"IncomingAttackFaction", ToastState::IncomingAttackFaction},
    {"FleetBattle", ToastState::FleetBattle},
    {"Victory", ToastState::Victory},
    {"Defeat", ToastState::Defeat},
    {"Event", ToastState::Tournament},
    {"ArmadaCreated", ToastState::ArmadaCreated},
    {"ArmadaCanceled", ToastState::ArmadaCanceled},
    {"ArmadaIncomingAttack", ToastState::ArmadaIncomingAttack},
    {"ArmadaBattleWon", ToastState::ArmadaBattleWon},
    {"ArmadaBattleLost", ToastState::ArmadaBattleLost},
    {"DiplomacyUpdated", ToastState::DiplomacyUpdated},
    {"JoinedTakeover", ToastState::JoinedTakeover},
    {"CompetitorJoinedTakeover", ToastState::CompetitorJoinedTakeover},
    {"AbandonedTerritory", ToastState::AbandonedTerritory},
    {"TakeoverVictory", ToastState::TakeoverVictory},
    {"TakeoverDefeat", ToastState::TakeoverDefeat},
    {"TreasuryProgress", ToastState::TreasuryProgress},
    {"TreasuryFull", ToastState::TreasuryFull},
    {"Achievement", ToastState::Achievement},
    {"AssaultVictory", ToastState::AssaultVictory},
    {"AssaultDefeat", ToastState::AssaultDefeat},
    {"ChallengeComplete", ToastState::ChallengeComplete},
    {"ChallengeFailed", ToastState::ChallengeFailed},
    {"StrikeHit", ToastState::StrikeHit},
    {"StrikeDefeat", ToastState::StrikeDefeat},
    {"WarchestProgress", ToastState::WarchestProgress},
    {"WarchestFull", ToastState::WarchestFull},
    {"PartialVictory", ToastState::PartialVictory},
    {"ArenaTimeLeft", ToastState::ArenaTimeLeft},
    {"ChainedEventScored", ToastState::ChainedEventScored},
    {"FleetPresetApplied", ToastState::FleetPresetApplied},
    {"SurgeWarmUpEnded", ToastState::SurgeWarmUpEnded},
    {"SurgeHostileGroupDefeated", ToastState::SurgeHostileGroupDefeated},
    {"SurgeTimeLeft", ToastState::SurgeTimeLeft},
};

bool SyncConfig::enabled(SyncConfig::Type type) const
{
  for (const auto& opt : SyncOptions) {
    if (opt.type == type) {
      return this->*opt.option;
    }
  }

  return false;
}

Config::Config()
{
  Load();
}

void Config::Save(const toml::table& config, const std::string_view filename, bool apply_warning)
{
  std::ofstream config_file;

  auto config_path = File::MakePath(filename, true);
  config_file.open(config_path);

  if (apply_warning) {
    char defaultFile[255], configFile[255];
    snprintf(defaultFile, 255, "%s", File::Default());
    snprintf(configFile, 255, "%s", File::Config());

    config_file << "#######################################################################\n";
    config_file << "#######################################################################\n";
    config_file << "####                                                               ####\n";
    config_file << "#### NOTE: This file is not the configuration file that is used    ####\n";
    config_file << "####       by the STFC Community Mod.  It is provided to help      ####\n";
    config_file << "####       see what configuration is being used by the runtime     ####\n";
    config_file << "####       and any desired settings should be copied to the same   ####\n";
    config_file << "####       section in: " << defaultFile << "\n";
    config_file << "####                                                               ####\n";
    config_file << "####        Config in: " << configFile << "\n";
    config_file << "####                                                               ####\n";
    config_file << "#######################################################################\n";
    config_file << "#######################################################################\n\n";
  }

  config_file << config;
  config_file.close();
}

Config& Config::Get()
{
  static Config config;
  return config;
}

#if _WIN32
static HMONITOR lastMonitor = (HMONITOR)-1;
static float    dpi         = 1.0f;

HWND Config::WindowHandle()
{
  static HWND hwnd = nullptr;

  if (hwnd == nullptr) {
    DWORD processId = GetCurrentProcessId();
    hwnd            = GetTopWindow(nullptr); // Start with the first top-level window

    while (hwnd != nullptr) {
      DWORD windowProcessId;
      GetWindowThreadProcessId(hwnd, &windowProcessId);

      // Check if the window belongs to the current process and is the main window
      if (windowProcessId == processId && GetWindow(hwnd, GW_OWNER) == nullptr && IsWindowVisible(hwnd)) {
        break;
      }

      hwnd = GetNextWindow(hwnd, GW_HWNDNEXT); // Move to the next top-level window
    }
  }

  return hwnd;
}

float Config::RefreshDPI()
{
  lastMonitor = (HMONITOR)-1;

  return Config::GetDPI();
}

float Config::GetDPI()
{
  auto     activeWindow = GetActiveWindow();
  HMONITOR monitor      = MonitorFromWindow(activeWindow, MONITOR_DEFAULTTONEAREST);

  if (monitor != lastMonitor) {
    // Get the logical width and height of the monitor
    MONITORINFOEX monitorInfoEx;
    monitorInfoEx.cbSize = sizeof(monitorInfoEx);
    GetMonitorInfo(monitor, &monitorInfoEx);
    auto cxLogical = monitorInfoEx.rcMonitor.right - monitorInfoEx.rcMonitor.left;
    auto cyLogical = monitorInfoEx.rcMonitor.bottom - monitorInfoEx.rcMonitor.top;

    // Get the physical width and height of the monitor
    DEVMODE devMode;
    devMode.dmSize        = sizeof(devMode);
    devMode.dmDriverExtra = 0;
    EnumDisplaySettings(monitorInfoEx.szDevice, ENUM_CURRENT_SETTINGS, &devMode);
    auto cxPhysical = devMode.dmPelsWidth;
    auto cyPhysical = devMode.dmPelsHeight;

    // Calculate the scaling factor
    auto horizontalScale = ((double)cxPhysical / (double)cxLogical);
    auto verticalScale   = ((double)cyPhysical / (double)cyLogical);

    spdlog::trace("Horizontal scaling: {}", horizontalScale);
    spdlog::trace("Vertical scaling: {}", verticalScale);

    dpi         = horizontalScale;
    lastMonitor = monitor;
  }

  return dpi;
}
#else
float Config::RefreshDPI()
{
  return Config::GetDPI();
}

float Config::GetDPI()
{
  return 1.0f;
}
#endif

void Config::AdjustUiScale(bool scaleUp)
{
  if (this->ui_scale != 0.0f) {
    auto old_scale    = this->ui_scale;
    auto scale_factor = (scaleUp ? 1.0f : -1.0f) * this->ui_scale_adjust;
    auto new_scale    = this->ui_scale + scale_factor;
    this->ui_scale    = std::clamp(new_scale, 0.1f, 2.0f);

    auto dpi = Config::RefreshDPI();
    spdlog::info("UI has been scaled {}, was {}, now {} (unclamped {}) @ {} DPI Scaling", (scaleUp ? "UP" : "DOWN"),
                 old_scale, this->ui_scale, new_scale, dpi);
  }
}

void Config::AdjustUiViewerScale(bool scaleUp)
{
  if (this->ui_scale_viewer != 0.0f) {
    auto old_scale        = this->ui_scale_viewer;
    auto scale_factor     = (scaleUp ? 1.0f : -1.0f) * this->ui_scale_adjust;
    auto new_scale        = this->ui_scale_viewer + (scale_factor * 0.25f);
    this->ui_scale_viewer = std::clamp(new_scale, 0.1f, 2.0f);

    spdlog::info("UI Viewer has been scaled {}, was {}, now {} (unclamped {})", (scaleUp ? "UP" : "DOWN"), old_scale,
                 this->ui_scale_viewer, new_scale);
  }
}

inline std::string mask_token(const std::string& token)
{
  if (token.size() > 21) {
    std::string masked = token;
    for (size_t i = 9; i < token.size() - 12; ++i) {
      if (masked[i] != '-') {
        masked[i] = '*';
      }
    }
    return masked;
  } else {
    return token;
  }
}

std::string get_config_type_as_string(const toml::node_type type)
{
  switch (type) {
    case toml::node_type::none:
      return "Not-a-node.";
    case toml::node_type::table:
      return "toml::table.";
    case toml::node_type::array:
      return "toml::array.";
    case toml::node_type::string:
      return "toml::value<std::string>.";
    case toml::node_type::integer:
      return "toml::value<int64_t>.";
    case toml::node_type::floating_point:
      return "toml::value<double>.";
    case toml::node_type::boolean:
      return "toml::value<bool>.";
    case toml::node_type::date:
      return "toml::value<date>.";
    case toml::node_type::time:
      return "toml::value<time>.";
    case toml::node_type::date_time:
      return "toml::value<date_time>.";
  };

  return "The node type is unknown";
}

template <typename T>
T get_config_or_default(toml::table& config, toml::table& new_config, std::string_view section, std::string_view item,
                        T default_value, bool write_log)
{
  new_config.emplace<toml::table>(section, toml::table());

  auto sectionTable = new_config[section];
  T    final_value  = default_value;

  try {
    if (config.contains(section)) {
      auto parsed_value = (T)config[section][item].value_or(default_value);
      final_value       = parsed_value;
    }
  } catch (...) {
    spdlog::warn("invalid config value {}.{}", section, item);
  }

  sectionTable.as_table()->insert_or_assign(item, final_value);

  if (write_log) {
    spdlog::debug("config value {}.{} value: {}", section, item, final_value);
  }

  return (T)final_value;
}

void read_sync_targets(toml::table& config, toml::table& new_config,
                       std::map<std::string, SyncTargetConfig>& sync_targets, const SyncConfig& defaults)
{
  if (!config.contains("sync")) {
    return;
  }

  const auto sync = config["sync"].as_table();
  if (!sync || !sync->contains("targets")) {
    return;
  }

  const auto targets = config["sync"]["targets"].as_table();
  if (!targets) {
    return;
  }

  for (const auto& [target_key, target_config] : *targets) {
    if (!target_config.is_table()) {
      continue;
    }

    const std::string target_section = "sync.targets." + std::string(target_key.str());

    SyncTargetConfig target;
    toml::table      parsed_target;

    const auto& values = *target_config.as_table();
    if (values.contains("url") && values.contains("token")) {
      auto url   = values["url"].value<std::string>();
      auto token = values["token"].value<std::string>();
      auto proxy = values["proxy"].value<std::string>();

      if (!url.has_value() || !token.has_value()) {
        continue;
      }

      target.url   = url.value();
      target.token = token.value();
      target.proxy = proxy.value_or(defaults.proxy);

      parsed_target.insert("url", target.url);
      parsed_target.insert("token", target.token);
      parsed_target.insert("proxy", target.proxy);
    } else {
      spdlog::warn("Skipping invalid target [{}]. Missing url or token.", target_section);
      continue;
    }

    for (const auto& opt : SyncOptions) {
      target.*opt.option = values[opt.option_str].value<bool>().value_or(defaults.*opt.option);
      parsed_target.insert(opt.option_str, target.*opt.option);
    }

    if (sync_targets.emplace(target_key.str(), target).second) {
      new_config["sync"]["targets"].as_table()->emplace<toml::table>(target_key.str(), parsed_target);
      spdlog::debug("config value {} url: {}, token: {}", target_section, target.url, mask_token(target.token));
    }
  }
}

void parse_config_shortcut(toml::table& config, toml::table& new_config, std::string_view item,
                           GameFunction gameFunction, std::string_view default_value)
{
  auto section = "shortcuts";

  config.emplace<toml::table>(section, toml::table());
  new_config.emplace<toml::table>(section, toml::table());

  auto sectionTable = new_config[section];
  auto config_value = config[section][item].value_or(default_value);

  auto valueTrimmed = StripTrailingAsciiWhitespace(config_value);
  auto valueLowered = AsciiStrToUpper(valueTrimmed);
  auto wantedKeys   = StrSplit(valueLowered, '|');

  bool keyAdded = false;
  for (std::string_view wantedKey : wantedKeys) {
    MapKey mapKey = MapKey::Parse(wantedKey);

    if (mapKey.Key != KeyCode::None) {
      keyAdded = true;
    }

    MapKey::AddMappedKey(gameFunction, mapKey);
  }

  if (!keyAdded) {
    MapKey mapKey = MapKey::Parse(default_value);
    MapKey::AddMappedKey(gameFunction, mapKey);
  }

  auto shortcut = MapKey::GetShortcuts(gameFunction);
  sectionTable.as_table()->insert_or_assign(item, shortcut);

  spdlog::debug("shortcut value {}.{} value: {}", section, item, shortcut);
}

void migrate_mac_config_if_needed(const char* filename)
{
#if !_WIN32
  namespace fs = std::filesystem;

  fs::path file_path = File::MakePath(filename);
  auto     new_dir   = file_path.parent_path();
  if (fs::exists(file_path) || fs::exists(new_dir))
    return;

  spdlog::info("mac config migration: config dir does not exist, checking for migration...");

  fs::path old_path = File::MakePath(filename, false, true);
  if (!fs::exists(old_path)) {
    spdlog::info("mac config migration: old config does not exist. nothing to migrate.");
    return;
  }

  auto stat = fs::status(old_path);
  if (stat.type() == fs::file_type::regular) {
    spdlog::info("mac config migration: old config found. attempting to migrate...");

    try {
      // move
      auto old_dir = old_path.parent_path();
      fs::rename(old_dir, new_dir);

      // re-create old dir, create symlink
      fs::create_directories(old_dir);
      fs::create_symlink(file_path, old_path);

      // drop update-info.txt
      std::ofstream info;
      info.open(old_dir / "update-info.txt");
      info << "Your config has been moved!\n\n";
      info << "You can now find your config at " << file_path << "\n\n";
      info << "A symlink has been placed for your convenience, but it is generally recommended, that you use the new "
              "path and delete this directory going forward.";
      info.close();

      spdlog::info("mac config migration: config migration done.");
    } catch (std::exception& ex) {
      spdlog::warn("mac config migration: migration failed = {}", ex.what());
    }
  }
#endif
}

void delete_old_vars()
{
  namespace fs = std::filesystem;

  fs::path        old_vars = fs::path(File::MakePath(File::Vars())).parent_path() / FILE_DEF_VARS_OLD;
  std::error_code ignore;
  fs::remove(old_vars, ignore);
}

void Config::Load()
{
  auto filename = File::Config();

  migrate_mac_config_if_needed(filename);
  delete_old_vars();

  toml::table config;
  toml::table parsed;
  bool        write_config = false;
  bool        write_log    = true;
  try {
    config       = std::move(toml::parse_file(File::MakePath(filename)));
    write_config = true;
  } catch (const toml::parse_error& e) {
    spdlog::warn("Failed to load config file, falling back to default settings: {}", e.description());
    spdlog::debug("");
    write_config = false;
    write_log    = false;
  } catch (...) {
    spdlog::warn("Failed to load config file, falling back to default settings");
    spdlog::debug("");
    write_config = false;
    write_log    = false;
  }

#if _MODDBG
  this->installUiScaleHooks     = get_config_or_default(config, parsed, "patches", "uiscalehooks", DCP::uiscalehooks, write_config);
  this->installZoomHooks        = get_config_or_default(config, parsed, "patches", "zoomhooks", DCP::zoomhooks, write_config);
  this->installBuffFixHooks     = get_config_or_default(config, parsed, "patches", "bufffixhooks", DCP::bufffixhooks, write_config);
  this->installToastBannerHooks = get_config_or_default(config, parsed, "patches", "toastbannerhooks", DCP::toastbannerhooks, write_config);
  this->installPanHooks         = get_config_or_default(config, parsed, "patches", "panhooks", DCP::panhooks, write_config);
  this->installImproveResponsivenessHooks = get_config_or_default(config, parsed, "patches", "improveresponsivenesshooks", DCP::improveresponsivenesshooks, write_config);
  this->installHotkeyHooks       = get_config_or_default(config, parsed, "patches", "hotkeyhooks", DCP::hotkeyhooks, write_config);
  this->installFreeResizeHooks   = get_config_or_default(config, parsed, "patches", "freeresizehooks", DCP::freeresizehooks, write_config);
  this->installTempCrashFixes    = get_config_or_default(config, parsed, "patches", "tempcrashfixes", DCP::tempcrashfixes, write_config);
  this->installTestPatches       = get_config_or_default(config, parsed, "patches", "testpatches", DCP::testpatches, write_config);
  this->installMiscPatches       = get_config_or_default(config, parsed, "patches", "miscpatches", DCP::miscpatches, write_config);
  this->installChatPatches       = get_config_or_default(config, parsed, "patches", "chatpatches", DCP::chatpatches, write_config);
  this->installResolutionListFix = get_config_or_default(config, parsed, "patches", "resolutionlistfix", DCP::resolutionlistfix, write_config);
  this->installSyncPatches       = get_config_or_default(config, parsed, "patches", "syncpatches", DCP::syncpatches, write_config);
  this->installObjectTracker     = get_config_or_default(config, parsed, "patches", "objecttracker", DCP::objecttracker, write_config);
  spdlog::debug("");
#else
  this->installUiScaleHooks               = true;
  this->installZoomHooks                  = true;
  this->installBuffFixHooks               = true;
  this->installToastBannerHooks           = true;
  this->installPanHooks                   = true;
  this->installImproveResponsivenessHooks = true;
  this->installHotkeyHooks                = true;
  this->installFreeResizeHooks            = true;
  this->installTempCrashFixes             = true;
  this->installTestPatches                = true;
  this->installMiscPatches                = true;
  this->installChatPatches                = true;
  this->installResolutionListFix          = true;
  this->installSyncPatches                = true;
  this->installObjectTracker              = true;
#endif
  
  this->queue_enabled       = get_config_or_default(config, parsed, "control", "queue_enabled", DCC::queue_enabled, write_config);
  this->hotkeys_enabled     = get_config_or_default(config, parsed, "control", "hotkeys_enabled", DCC::hotkeys_enabled, write_config);
  this->hotkeys_extended    = get_config_or_default(config, parsed, "control", "hotkeys_extended", DCC::hotkeys_extended, write_config);
  this->use_scopely_hotkeys = get_config_or_default(config, parsed, "control", "use_scopely_hotkeys", DCC::use_scopely_hotkeys, write_config);
  this->select_timer        = get_config_or_default(config, parsed, "control", "select_timer", DCC::select_timer, write_config);
  this->enable_experimental = get_config_or_default(config, parsed, "control", "enable_experimental", DCC::enable_experimental, write_config);

  spdlog::debug("");

  this->ui_scale            = get_config_or_default(config, parsed, "graphics", "ui_scale", DCG::ui_scale, write_config);
  this->ui_scale_adjust     = get_config_or_default(config, parsed, "graphics", "ui_scale_adjust", DCG::ui_scale_adjust, write_config);
  this->ui_scale_viewer     = get_config_or_default(config, parsed, "graphics", "ui_scale_viewer", DCG::ui_scale_viewer, write_config);
  this->zoom                = get_config_or_default(config, parsed, "graphics", "zoom", DCG::zoom, write_config);
  this->free_resize         = get_config_or_default(config, parsed, "graphics", "free_resize", DCG::free_resize, write_config);
  this->allow_cursor        = get_config_or_default(config, parsed, "graphics", "allow_cursor", DCG::allow_cursor, write_config);
  this->keyboard_zoom_speed = get_config_or_default(config, parsed, "graphics", "keyboard_zoom_speed", DCG::keyboard_zoom_speed, write_config);

  if (this->enable_experimental) {
    this->system_pan_momentum = get_config_or_default(config, parsed, "graphics", "system_pan_momentum", DCG::system_pan_momentum, write_config);
  }

  spdlog::debug("");

  this->system_pan_momentum_falloff =
      get_config_or_default(config, parsed, "graphics", "system_pan_momentum_falloff", DCG::system_pan_momentum_falloff, write_log);
  this->borderless_fullscreen =
      get_config_or_default(config, parsed, "graphics", "borderless_fullscreen", DCG::borderless_fullscreen, write_log);
  this->transition_time      = get_config_or_default(config, parsed, "graphics", "transition_time", DCG::transition_time, write_config);
  this->show_all_resolutions = get_config_or_default(config, parsed, "graphics", "show_all_resolutions", DCG::show_all_resolutions, write_config);
  this->default_system_zoom  = get_config_or_default(config, parsed, "graphics", "default_system_zoom", DCG::default_system_zoom, write_config);

  spdlog::debug("");

  this->system_zoom_preset_1   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_1", DCG::system_zoom_preset_1, write_config);
  this->system_zoom_preset_2   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_2", DCG::system_zoom_preset_2, write_config);
  this->system_zoom_preset_3   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_3", DCG::system_zoom_preset_3, write_config);
  this->system_zoom_preset_4   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_4", DCG::system_zoom_preset_4, write_config);
  this->system_zoom_preset_5   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_5", DCG::system_zoom_preset_5, write_config);
  this->use_presets_as_default = get_config_or_default(config, parsed, "graphics", "use_presets_as_default", DCG::use_presets_as_default, write_config);

  spdlog::debug("");

  this->use_out_of_dock_power = get_config_or_default(config, parsed, "buffs", "use_out_of_dock_power", DCBS::use_out_of_dock_power, write_config);

  spdlog::debug("");

  this->disable_escape_exit    = get_config_or_default(config, parsed, "ui", "disable_escape_exit", DCU::disable_escape_exit, write_config);
  this->disable_preview_locate = get_config_or_default(config, parsed, "ui", "disable_preview_locate", DCU::disable_preview_locate, write_config);
  this->disable_preview_recall = get_config_or_default(config, parsed, "ui", "disable_preview_recall", DCU::disable_preview_recall, write_config);
  this->disable_first_popup    = get_config_or_default(config, parsed, "ui", "disable_first_popup", DCU::disable_first_popup, write_config);
  this->disable_move_keys      = get_config_or_default(config, parsed, "ui", "disable_move_keys", DCU::disable_move_keys, write_config);
  this->disable_toast_banners  = get_config_or_default(config, parsed, "ui", "disable_toast_banners", DCU::disable_toast_banners, write_config);

#if _WIN32
  this->extend_donation_slider = get_config_or_default(config, parsed, "ui", "extend_donation_slider", DCU::extend_donation_slider, write_config);
  this->extend_donation_max    = get_config_or_default(config, parsed, "ui", "extend_donation_max", DCU::extend_donation_max, write_config);
#endif

  this->disable_galaxy_chat    = get_config_or_default(config, parsed, "ui", "disable_galaxy_chat", DCU::disable_galaxy_chat, write_config);
  this->disable_veil_chat      = get_config_or_default(config, parsed, "ui", "disable_veil_chat", DCU::disable_veil_chat, write_config);
  this->show_cargo_default     = get_config_or_default(config, parsed, "ui", "show_cargo_default", DCU::show_cargo_default, write_config);
  this->show_player_cargo      = get_config_or_default(config, parsed, "ui", "show_player_cargo", DCU::show_player_cargo, write_config);
  this->show_station_cargo     = get_config_or_default(config, parsed, "ui", "show_station_cargo", DCU::show_station_cargo, write_config);
  this->show_hostile_cargo     = get_config_or_default(config, parsed, "ui", "show_hostile_cargo", DCU::show_hostile_cargo, write_config);
  this->show_armada_cargo      = get_config_or_default(config, parsed, "ui", "show_armada_cargo", DCU::show_armada_cargo, write_config);

  this->always_skip_reveal_sequence = get_config_or_default(config, parsed, "ui", "always_skip_reveal_sequence", DCU::always_skip_reveal_sequence, write_config);

  spdlog::debug("");

  this->sync_debug              = get_config_or_default(config, parsed, "sync", "debug", DCS::debug, write_config);
  this->sync_logging            = get_config_or_default(config, parsed, "sync", "logging", DCS::logging, write_config);
  this->sync_resolver_cache_ttl = get_config_or_default(config, parsed, "sync", "resolver_cache_ttl", DCS::resolver_cache_ttl, write_config);

  SyncConfig sync_defaults;
  sync_defaults.proxy      = get_config_or_default<std::string>(config, parsed, "sync", "proxy", DCS::proxy , write_log);
  sync_defaults.verify_ssl = get_config_or_default(config, parsed, "sync", "verify_ssl", DCS::verify_ssl , write_config);

  for (const auto& opt : SyncOptions) {
    sync_defaults.*opt.option = get_config_or_default(config, parsed, "sync", opt.option_str, false, write_config);
  }

  spdlog::debug("");

  parsed["sync"].as_table()->emplace<toml::table>("targets", toml::table());
  read_sync_targets(config, parsed, this->sync_targets, sync_defaults);

  // handle legacy sync options
  auto sync_url   = config["sync"]["url"].value<std::string>();
  auto sync_token = config["sync"]["token"].value<std::string>();

  if (sync_url.has_value() && sync_token.has_value()) {
    spdlog::warn("Deprecation Warning: Legacy config options 'sync_url' and 'sync_token' have been moved to "
                 "[sync.targets.<name>] sections and may be removed in a future version.");

    SyncTargetConfig converted_target{.url = sync_url.value(), .token = sync_token.value()};

    if (this->sync_targets.emplace("default", converted_target).second) {
      parsed["sync"]["targets"].as_table()->emplace<toml::table>(
          "default", toml::table{{"url", sync_url.value()}, {"token", sync_token.value()}});
      spdlog::info(
          "Legacy config options 'sync_url' and 'sync_token' were converted to sync.targets.default url: {}, token: {}",
          sync_url.value(), sync_token.value());
    } else {
      spdlog::error("Failed to convert legacy config options sync_url: {} and sync_token: {} as [sync.targets.default] "
                    "was already specified.",
                    sync_url.value(), sync_token.value());
    }
  }

  if (auto sync_file = config["sync"]["file"].value<std::string>();
      sync_file.has_value() && !sync_file.value().empty()) {
    spdlog::error("Deprecation Notice: The 'sync_file' config option has been deprecated and removed. "
                  "For capturing sync output, please use a local HTTP server instead.");
  }

  // set global sync options to what's actually used in targets
  const auto targets_view = this->sync_targets | std::views::values;

  for (const auto& opt : SyncOptions) {
    this->sync_options.*opt.option =
        std::ranges::any_of(targets_view, [opt](const auto& target) { return target.*opt.option; });
  }

  spdlog::debug("");

  // must explicitly include std::string typing here, or we get back char * which fails us!
  auto disabled_banner_types_str =
      get_config_or_default<std::string>(config, parsed, "ui", "disabled_banner_types", DCU::disabled_banner_types, write_log);

  this->config_settings_url =
      get_config_or_default<std::string>(config, parsed, "config", "settings_url", DCSC::settings_url, write_log);
  this->config_assets_url_override =
      get_config_or_default<std::string>(config, parsed, "config", "assets_url_override", DCSC::assets_url_override, write_log);

  std::vector<std::string> types = StrSplit(disabled_banner_types_str, ',');

  spdlog::debug("");

  std::string       bannerString;
  std::stringstream message;
  message << "Parsing banner strings";

  spdlog::debug(message.str());

  for (const auto& [key, value] : bannerTypes) {
    auto upper_key = AsciiStrToUpper(key);

    for (const std::string_view _type : types) {
      auto stripped_type = StripLeadingAsciiWhitespace(_type);
      auto upper_type    = AsciiStrToUpper(stripped_type);

      if (upper_key == upper_type) {
        this->disabled_banner_types.emplace_back(value);
        if (!bannerString.empty()) {
          bannerString.append(", ");
        }
        bannerString.append(key);
      }
    }
  }

  message.str("");
  message << "Final disabledbanner types: " << bannerString;
  spdlog::debug(message.str());

  parsed["ui"].as_table()->insert_or_assign("disabled_banner_types", bannerString);

  spdlog::debug("");

  //if (this->enable_experimental) {
  //  parse_config_shortcut(config, parsed, "move_left",  GameFunction::MoveLeft,  DCSH::move_left);
  //  parse_config_shortcut(config, parsed, "move_right", GameFunction::MoveRight, DCSH::move_right);
  //  parse_config_shortcut(config, parsed, "move_down",  GameFunction::MoveDown,  DCSH::move_down);
  //  parse_config_shortcut(config, parsed, "move_up",    GameFunction::MoveUp,    DCSH::move_up);
  //}

  parse_config_shortcut(config, parsed, "set_hotkeys_disble", GameFunction::DisableHotKeys, DCSH::set_hotkeys_disabled);
  parse_config_shortcut(config, parsed, "set_hotkeys_enable", GameFunction::EnableHotKeys,  DCSH::set_hotkeys_enabled);

  parse_config_shortcut(config, parsed, "select_chatalliance", GameFunction::SelectChatAlliance, DCSH::select_chatalliance);
  parse_config_shortcut(config, parsed, "select_chatglobal",   GameFunction::SelectChatGlobal,   DCSH::select_chatglobal);
  parse_config_shortcut(config, parsed, "select_chatprivate",  GameFunction::SelectChatPrivate,  DCSH::select_chatprivate);
  parse_config_shortcut(config, parsed, "quit",                GameFunction::Quit,               DCSH::quit);

  parse_config_shortcut(config, parsed, "select_ship1", GameFunction::SelectShip1, DCSH::select_ship1);
  parse_config_shortcut(config, parsed, "select_ship2", GameFunction::SelectShip2, DCSH::select_ship2);
  parse_config_shortcut(config, parsed, "select_ship3", GameFunction::SelectShip3, DCSH::select_ship3);
  parse_config_shortcut(config, parsed, "select_ship4", GameFunction::SelectShip4, DCSH::select_ship4);
  parse_config_shortcut(config, parsed, "select_ship5", GameFunction::SelectShip5, DCSH::select_ship5);
  parse_config_shortcut(config, parsed, "select_ship6", GameFunction::SelectShip6, DCSH::select_ship6);
  parse_config_shortcut(config, parsed, "select_ship7", GameFunction::SelectShip7, DCSH::select_ship7);
  parse_config_shortcut(config, parsed, "select_ship8", GameFunction::SelectShip8, DCSH::select_ship8);
  parse_config_shortcut(config, parsed, "select_current", GameFunction::SelectCurrent, DCSH::select_current);

  parse_config_shortcut(config, parsed, "action_primary",        GameFunction::ActionPrimary,        DCSH::action_primary);
  parse_config_shortcut(config, parsed, "action_secondary",      GameFunction::ActionSecondary,      DCSH::action_secondary);
  parse_config_shortcut(config, parsed, "action_queue",          GameFunction::ActionQueue,          DCSH::action_queue);
  parse_config_shortcut(config, parsed, "action_queue_clear",    GameFunction::ActionQueueClear,     DCSH::action_queue_clear);
  parse_config_shortcut(config, parsed, "action_view",           GameFunction::ActionView,           DCSH::action_view);
  parse_config_shortcut(config, parsed, "action_recall",         GameFunction::ActionRecall,         DCSH::action_recall);
  parse_config_shortcut(config, parsed, "action_recall_cancel",  GameFunction::ActionRecallCancel,   DCSH::action_recall_cancel);
  parse_config_shortcut(config, parsed, "action_repair",         GameFunction::ActionRepair,         DCSH::action_repair);
  parse_config_shortcut(config, parsed, "show_chat",             GameFunction::ShowChat,             DCSH::show_chat);
  parse_config_shortcut(config, parsed, "show_chatside1",        GameFunction::ShowChatSide1,        DCSH::show_chatside1);
  parse_config_shortcut(config, parsed, "show_chatside2",        GameFunction::ShowChatSide2,        DCSH::show_chatside2);
  parse_config_shortcut(config, parsed, "show_galaxy",           GameFunction::ShowGalaxy,           DCSH::show_galaxy);
  parse_config_shortcut(config, parsed, "show_system",           GameFunction::ShowSystem,           DCSH::show_system);
  parse_config_shortcut(config, parsed, "zoom_preset1",          GameFunction::ZoomPreset1,          DCSH::zoom_preset1);
  parse_config_shortcut(config, parsed, "zoom_preset2",          GameFunction::ZoomPreset2,          DCSH::zoom_preset2);
  parse_config_shortcut(config, parsed, "zoom_preset3",          GameFunction::ZoomPreset3,          DCSH::zoom_preset3);
  parse_config_shortcut(config, parsed, "zoom_preset4",          GameFunction::ZoomPreset4,          DCSH::zoom_preset4);
  parse_config_shortcut(config, parsed, "zoom_preset5",          GameFunction::ZoomPreset5,          DCSH::zoom_preset5);
  parse_config_shortcut(config, parsed, "zoom_in",               GameFunction::ZoomIn,               DCSH::zoom_in);
  parse_config_shortcut(config, parsed, "zoom_out",              GameFunction::ZoomOut,              DCSH::zoom_out);
  parse_config_shortcut(config, parsed, "zoom_max",              GameFunction::ZoomMax,              DCSH::zoom_max);
  parse_config_shortcut(config, parsed, "zoom_min",              GameFunction::ZoomMin,              DCSH::zoom_min);
  parse_config_shortcut(config, parsed, "zoom_reset",            GameFunction::ZoomReset,            DCSH::zoom_reset);
  parse_config_shortcut(config, parsed, "ui_scaleup",            GameFunction::UiScaleUp,            DCSH::ui_scaleup);
  parse_config_shortcut(config, parsed, "ui_scaledown",          GameFunction::UiScaleDown,          DCSH::ui_scaledown);
  parse_config_shortcut(config, parsed, "ui_scaleviewerup",      GameFunction::UiViewerScaleUp,      DCSH::ui_scaleviewerup);
  parse_config_shortcut(config, parsed, "ui_scaleviewerdown",    GameFunction::UiViewerScaleDown,    DCSH::ui_scaleviewerdown);

  parse_config_shortcut(config, parsed, "log_debug",             GameFunction::LogLevelDebug,        DCSH::log_debug);
  parse_config_shortcut(config, parsed, "log_trace",             GameFunction::LogLevelTrace,        DCSH::log_trace);
  parse_config_shortcut(config, parsed, "log_info",              GameFunction::LogLevelInfo,         DCSH::log_info);
  parse_config_shortcut(config, parsed, "log_warn",              GameFunction::LogLevelWarn,         DCSH::log_warn);
  parse_config_shortcut(config, parsed, "log_error",             GameFunction::LogLevelError,        DCSH::log_error);
  parse_config_shortcut(config, parsed, "log_off",               GameFunction::LogLevelOff,          DCSH::log_off);

  parse_config_shortcut(config, parsed, "show_awayteam",         GameFunction::ShowAwayTeam,         DCSH::show_awayteam);
  parse_config_shortcut(config, parsed, "show_gifts",            GameFunction::ShowGifts,            DCSH::show_gifts);
  parse_config_shortcut(config, parsed, "show_artifacts",        GameFunction::ShowArtifacts,        DCSH::show_artifacts);
  parse_config_shortcut(config, parsed, "show_commander",        GameFunction::ShowCommander,        DCSH::show_commander);
  parse_config_shortcut(config, parsed, "show_daily",            GameFunction::ShowDaily,            DCSH::show_daily);
  parse_config_shortcut(config, parsed, "show_events",           GameFunction::ShowEvents,           DCSH::show_events);
  parse_config_shortcut(config, parsed, "show_exocomp",          GameFunction::ShowExoComp,          DCSH::show_exocomp);
  parse_config_shortcut(config, parsed, "show_factions",         GameFunction::ShowFactions,         DCSH::show_factions);
  parse_config_shortcut(config, parsed, "show_inventory",        GameFunction::ShowInventory,        DCSH::show_inventory);
  parse_config_shortcut(config, parsed, "show_missions",         GameFunction::ShowMissions,         DCSH::show_missions);
  parse_config_shortcut(config, parsed, "show_research",         GameFunction::ShowResearch,         DCSH::show_research);
  parse_config_shortcut(config, parsed, "show_scrapyard",        GameFunction::ShowScrapYard,        DCSH::show_scrapyard);
  parse_config_shortcut(config, parsed, "show_settings",         GameFunction::ShowSettings,         DCSH::show_settings);
  parse_config_shortcut(config, parsed, "show_officers",         GameFunction::ShowOfficers,         DCSH::show_officers);
  parse_config_shortcut(config, parsed, "show_qtrials",          GameFunction::ShowQTrials,          DCSH::show_qtrials);
  parse_config_shortcut(config, parsed, "show_refinery",         GameFunction::ShowRefinery,         DCSH::show_refinery);
  parse_config_shortcut(config, parsed, "show_ships",            GameFunction::ShowShips,            DCSH::show_ships);
  parse_config_shortcut(config, parsed, "show_stationexterior",  GameFunction::ShoWStationExterior,  DCSH::show_stationexterior);
  parse_config_shortcut(config, parsed, "show_stationinterior",  GameFunction::ShowStationInterior,  DCSH::show_stationinterior);
  parse_config_shortcut(config, parsed, "toggle_queue",          GameFunction::ToggleQueue,          DCSH::toggle_queue);

  if (this->hotkeys_extended) {
    parse_config_shortcut(config, parsed, "show_alliance", GameFunction::ShowAlliance, DCSH::show_alliance);

    if (this->enable_experimental) {
      parse_config_shortcut(config, parsed, "show_alliance_help",   GameFunction::ShowAllianceHelp,   DCSH::show_alliance_help);
      parse_config_shortcut(config, parsed, "show_alliance_armada", GameFunction::ShowAllianceArmada, DCSH::show_alliance_armada);
    }

    parse_config_shortcut(config, parsed, "show_bookmarks", GameFunction::ShowBookmarks, DCSH::show_bookmarks);

    if (this->enable_experimental) {
      parse_config_shortcut(config, parsed, "show_lookup", GameFunction::ShowLookup, DCSH::show_lookup);
    }

    parse_config_shortcut(config, parsed, "set_zoom_preset1", GameFunction::SetZoomPreset1, DCSH::set_zoom_preset1);
    parse_config_shortcut(config, parsed, "set_zoom_preset2", GameFunction::SetZoomPreset2, DCSH::set_zoom_preset2);
    parse_config_shortcut(config, parsed, "set_zoom_preset3", GameFunction::SetZoomPreset3, DCSH::set_zoom_preset3);
    parse_config_shortcut(config, parsed, "set_zoom_preset4", GameFunction::SetZoomPreset4, DCSH::set_zoom_preset4);
    parse_config_shortcut(config, parsed, "set_zoom_preset5", GameFunction::SetZoomPreset5, DCSH::set_zoom_preset5);
    parse_config_shortcut(config, parsed, "set_zoom_default", GameFunction::SetZoomDefault, DCSH::set_zoom_default);
    parse_config_shortcut(config, parsed, "toggle_preview_locate", GameFunction::TogglePreviewLocate, DCSH::toggle_preview_locate);
    parse_config_shortcut(config, parsed, "toggle_preview_recall", GameFunction::TogglePreviewRecall, DCSH::toggle_preview_recall);
    parse_config_shortcut(config, parsed, "toggle_cargo_default", GameFunction::ToggleCargoDefault, DCSH::toggle_cargo_default);
    parse_config_shortcut(config, parsed, "toggle_cargo_player",  GameFunction::ToggleCargoPlayer,  DCSH::toggle_cargo_player);
    parse_config_shortcut(config, parsed, "toggle_cargo_station", GameFunction::ToggleCargoStation, DCSH::toggle_cargo_station);
    parse_config_shortcut(config, parsed, "toggle_cargo_hostile", GameFunction::ToggleCargoHostile, DCSH::toggle_cargo_hostile);
    parse_config_shortcut(config, parsed, "toggle_cargo_armada",  GameFunction::ToggleCargoArmada,  DCSH::toggle_cargo_armada);
  }

  spdlog::debug("");

  if (!std::filesystem::exists(File::MakePath(File::Config()))) {
    message.str("");
    message << "Creating " << File::Config() << " (default config file)";
    spdlog::warn(message.str());

    Config::Save(parsed, File::Config(), false);
  }

  message.str("");
  message << "Creating " << File::Vars() << " (final config file)";
  spdlog::info(message.str());

  if (std::filesystem::exists(FILE_DEF_PARSED)) {
    message.str("");
    message << "Removing " << FILE_DEF_PARSED << " (old parsed file)";
    spdlog::info(message.str());

    std::filesystem::remove(FILE_DEF_PARSED);
  }

  Config::Save(parsed, File::Vars());

  std::cout << "\n\n-----------------------------\n\n"
            << parsed << "\n\n-----------------------------\nVersion "

#if VERSION_PATCH
            << "Loaded beta version " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << " (Patch "
            << VERSION_PATCH << ")\n\n"
            << "NOTE: Beta versions may have unexpected bugs and issues.\n\n"
#else
            << "Loaded beta version " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION
            << " (Release)"
#endif

            << "\n\nPlease see https://github.com/netniv/stfc-mod for latest configuration help, examples and future "
               "releases\n"
            << "or visit the STFC Community Mod discord server at https://discord.gg/PrpHgs7Vjs\n\n";
}
