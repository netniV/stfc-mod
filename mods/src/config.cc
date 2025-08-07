#include "config.h"
#include "patches/mapkey.h"
#include "prime/KeyCode.h"
#include "str_utils.h"
#include "version.h"
#include <prime\Toast.h>

#include <EASTL/tuple.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>

#include "file.h"

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
};

Config::Config()
{
  Load();
}

void Config::Save(toml::table config, std::string_view filename, bool apply_warning)
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

    spdlog::debug("Horizonzal scaling: {}", horizontalScale);
    spdlog::debug("Vertical scaling: {}", verticalScale);

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

std::string get_config_type_as_string(toml::node_type type)
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
inline T get_config_or_default(toml::table config, toml::table& new_config, std::string_view section,
                               std::string_view item, T default_value)
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

  spdlog::info("config value {}.{} value: {}", section, item, final_value);

  return (T)final_value;
}

void read_sync_targets(toml::table config, toml::table& new_config, std::map<std::string, std::string>& sync_targets)
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

  for (const auto& sync_iter : *targets) {
    if (!sync_iter.second.is_table()) {
      continue;
    }

    const auto& values = *sync_iter.second.as_table();
    if (!values.contains("url") || !values.contains("token")) {
      continue;
    }

    auto key   = sync_iter.first.str();
    auto url   = values["url"].value<std::string>();
    auto token = values["token"].value<std::string>();

    if (!url.has_value() || !token.has_value()) {
      continue;
    }
    if (sync_targets.emplace(url.value(), token.value()).second) {
      new_config["sync"]["targets"].as_table()->emplace<toml::table>(
          key, toml::table{{"url", url.value()}, {"token", token.value()}});
      spdlog::info("config value sync.targets.{} url: {}, token: {}", key, url.value(), token.value());
    }
  }
}

void parse_config_shortcut(toml::table config, toml::table& new_config, std::string_view item,
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

  spdlog::info("shortcut value {}.{} value: {}", section, item, shortcut);
}

void Config::Load()
{
  spdlog::info("=-=-=-==-=-=-=-=-=-=-=-=-=-=");
  spdlog::info("Loading Config :: {}", File::Config());
  spdlog::info("=-=-=-==-=-=-=-=-=-=-=-=-=-=");

  toml::table config;
  toml::table parsed;
  bool        write_config = false;
  try {
    config       = std::move(toml::parse_file(File::MakePath(File::Config())));
    write_config = true;
  } catch (const toml::parse_error& e) {
    spdlog::warn("Failed to load config file, falling back to default settings: {}", e.description());
    write_config = false;
  } catch (...) {
    spdlog::info("Failed to load config file, falling back to default settings");
    write_config = false;
  }

  this->queue_enabled       = get_config_or_default(config, parsed, "control", "queue_enabled", true);
  this->hotkeys_enabled     = get_config_or_default(config, parsed, "control", "hotkeys_enabled", true);
  this->hotkeys_extended    = get_config_or_default(config, parsed, "control", "hotkeys_extended", true);
  this->use_scopely_hotkeys = get_config_or_default(config, parsed, "control", "use_scopely_hotkeys", false);
  this->select_timer        = get_config_or_default(config, parsed, "control", "select_timer", 500);
#if DEBUG
  this->enable_experimental = get_config_or_default(config, parsed, "control", "enable_experimental", false);
#else
  this->enable_experimental = false;
#endif

  this->ui_scale            = get_config_or_default(config, parsed, "graphics", "ui_scale", 0.9f);
  this->ui_scale_adjust     = get_config_or_default(config, parsed, "graphics", "ui_scale_adjust", 0.05f);
  this->ui_scale_viewer     = get_config_or_default(config, parsed, "graphics", "ui_scale_viewer", 1.0f);
  this->zoom                = get_config_or_default(config, parsed, "graphics", "zoom", 2500.f);
  this->free_resize         = get_config_or_default(config, parsed, "graphics", "free_resize", true);
  this->keyboard_zoom_speed = get_config_or_default(config, parsed, "graphics", "keyboard_zoom_speed", 350.0f);

  if (this->enable_experimental) {
    this->system_pan_momentum = get_config_or_default(config, parsed, "graphics", "system_pan_momentum", 0.2f);
  }

  this->system_pan_momentum_falloff =
      get_config_or_default(config, parsed, "graphics", "system_pan_momentum_falloff", 0.8f);
  this->borderless_fullscreen_f11 =
      get_config_or_default(config, parsed, "graphics", "borderless_fullscreen_f11", true);
  this->transition_time      = get_config_or_default(config, parsed, "graphics", "transition_time", 0.01f);
  this->show_all_resolutions = get_config_or_default(config, parsed, "graphics", "show_all_resolutions", false);
  this->default_system_zoom  = get_config_or_default(config, parsed, "graphics", "default_system_zoom", 0.0f);

  this->system_zoom_preset_1   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_1", 0.0f);
  this->system_zoom_preset_2   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_2", 0.0f);
  this->system_zoom_preset_3   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_3", 0.0f);
  this->system_zoom_preset_4   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_4", 0.0f);
  this->system_zoom_preset_5   = get_config_or_default(config, parsed, "graphics", "system_zoom_preset_5", 0.0f);
  this->use_presets_as_default = get_config_or_default(config, parsed, "graphics", "use_presets_as_default", false);

  this->use_out_of_dock_power = get_config_or_default(config, parsed, "buffs", "use_out_of_dock_power", false);

  this->disable_escape_exit    = get_config_or_default(config, parsed, "ui", "disable_escape_exit", false);
  this->disable_preview_locate = get_config_or_default(config, parsed, "ui", "disable_preview_locate", false);
  this->disable_preview_recall = get_config_or_default(config, parsed, "ui", "disable_preview_recall", false);
  this->disable_first_popup    = get_config_or_default(config, parsed, "ui", "disable_first_popup", false);
  this->disable_move_keys      = get_config_or_default(config, parsed, "ui", "disable_move_keys", false);
  this->disable_toast_banners  = get_config_or_default(config, parsed, "ui", "disable_toast_banners", true);
  this->extend_donation_slider = get_config_or_default(config, parsed, "ui", "extend_donation_slider", false);
  this->extend_donation_max    = get_config_or_default(config, parsed, "ui", "extend_donation_max", 0);
  this->disable_galaxy_chat    = get_config_or_default(config, parsed, "ui", "disable_galaxy_chat", false);
  this->show_cargo_default     = get_config_or_default(config, parsed, "ui", "show_cargo_default", false);
  this->show_player_cargo      = get_config_or_default(config, parsed, "ui", "show_player_cargo", false);
  this->show_station_cargo     = get_config_or_default(config, parsed, "ui", "show_station_cargo", true);
  this->show_hostile_cargo     = get_config_or_default(config, parsed, "ui", "show_hostile_cargo", true);
  this->show_armada_cargo      = get_config_or_default(config, parsed, "ui", "show_armada_cargo", true);

  this->always_skip_reveal_sequence = get_config_or_default(config, parsed, "ui", "always_skip_reveal_sequence", false);

  this->sync_proxy      = get_config_or_default<std::string>(config, parsed, "sync", "proxy", "");
  this->sync_file       = get_config_or_default<std::string>(config, parsed, "sync", "file", "");
  this->sync_debug      = get_config_or_default(config, parsed, "sync", "debug", false);
  this->sync_logging    = get_config_or_default(config, parsed, "sync", "logging", false);
  this->sync_battlelogs = get_config_or_default(config, parsed, "sync", "battlelogs", false);
  this->sync_resources  = get_config_or_default(config, parsed, "sync", "resources", false);
  this->sync_officer    = get_config_or_default(config, parsed, "sync", "officer", false);
  this->sync_missions   = get_config_or_default(config, parsed, "sync", "missions", false);
  this->sync_research   = get_config_or_default(config, parsed, "sync", "research", false);
  this->sync_tech       = get_config_or_default(config, parsed, "sync", "tech", false);
  this->sync_traits     = get_config_or_default(config, parsed, "sync", "traits", false);
  this->sync_buildings  = get_config_or_default(config, parsed, "sync", "buildings", false);
  this->sync_ships      = get_config_or_default(config, parsed, "sync", "ships", false);

  parsed["sync"].as_table()->emplace<toml::table>("targets", toml::table());

  read_sync_targets(config, parsed, this->sync_targets);

  // handle legacy sync options
  auto sync_url   = config["sync"]["url"].value<std::string>();
  auto sync_token = config["sync"]["token"].value<std::string>();

  if (sync_url.has_value() && sync_token.has_value()) {
    spdlog::warn("Depreciation Warning: Legacy config options 'sync_url' and 'sync_token' have been moved to "
                 "[sync.targets.<name>] sections and may be removed in a future version.");

    if (this->sync_targets.emplace(sync_url.value(), sync_token.value()).second) {
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

  // must explicitly include std::string typing here, or we get back char * which fails us!
  std::string disabled_banner_types_str =
      get_config_or_default<std::string>(config, parsed, "ui", "disabled_banner_types", "");

  this->config_settings_url = get_config_or_default<std::string>(config, parsed, "config", "settings_url", "");
  this->config_assets_url_override =
      get_config_or_default<std::string>(config, parsed, "config", "assets_url_override", "");

  std::vector<std::string> types = StrSplit(disabled_banner_types_str, ',');

  std::string       bannerString = "";
  std::stringstream message;
  message << "Parsing banner strings";

  spdlog::info(message.str());

  for (const auto& [key, value] : bannerTypes) {
    auto upper_key = AsciiStrToUpper(key);

    for (const std::string_view _type : types) {
      auto stripped_type = StripLeadingAsciiWhitespace(_type);
      auto upper_type    = AsciiStrToUpper(stripped_type);

      if (upper_key == upper_type) {
        this->disabled_banner_types.emplace_back(value);
        if (bannerString.length() != 0) {
          bannerString.append(", ");
        }
        bannerString.append(key);
      }
    }
  }

  message.str("");
  message << "Final disabledbanner types: " << bannerString;
  spdlog::info(message.str());

  parsed["ui"].as_table()->insert_or_assign("disabled_banner_types", bannerString);

  if (this->enable_experimental) {
    parse_config_shortcut(config, parsed, "move_left", GameFunction::MoveLeft, "LEFT|A");
    parse_config_shortcut(config, parsed, "move_right", GameFunction::MoveRight, "RIGHT|D");
    parse_config_shortcut(config, parsed, "move_down", GameFunction::MoveDown, "DOWN|S");
    parse_config_shortcut(config, parsed, "move_up", GameFunction::MoveUp, "UP|W");
  }

  parse_config_shortcut(config, parsed, "hotkeys_disble", GameFunction::DisableHotKeys, "CTRL-ALT-MINUS");
  parse_config_shortcut(config, parsed, "hotkeys_enable", GameFunction::EnableHotKeys, "CTRL-ALT-=");
  parse_config_shortcut(config, parsed, "select_chatalliance", GameFunction::SelectChatAlliance, "CTRL-2");
  parse_config_shortcut(config, parsed, "select_chatglobal", GameFunction::SelectChatGlobal, "CTRL-1");
  parse_config_shortcut(config, parsed, "select_chatprivate", GameFunction::SelectChatPrivate, "CTRL-3");
  parse_config_shortcut(config, parsed, "quit", GameFunction::Quit, "F10");

  parse_config_shortcut(config, parsed, "select_ship1", GameFunction::SelectShip1, "1");
  parse_config_shortcut(config, parsed, "select_ship2", GameFunction::SelectShip2, "2");
  parse_config_shortcut(config, parsed, "select_ship3", GameFunction::SelectShip3, "3");
  parse_config_shortcut(config, parsed, "select_ship4", GameFunction::SelectShip4, "4");
  parse_config_shortcut(config, parsed, "select_ship5", GameFunction::SelectShip5, "5");
  parse_config_shortcut(config, parsed, "select_ship6", GameFunction::SelectShip6, "6");
  parse_config_shortcut(config, parsed, "select_ship7", GameFunction::SelectShip7, "7");
  parse_config_shortcut(config, parsed, "select_ship8", GameFunction::SelectShip8, "8");
  parse_config_shortcut(config, parsed, "select_current", GameFunction::SelectCurrent, "CTRL-SPACE");

  parse_config_shortcut(config, parsed, "action_primary", GameFunction::ActionPrimary, "SPACE");
  parse_config_shortcut(config, parsed, "action_secondary", GameFunction::ActionSecondary, "R");
  parse_config_shortcut(config, parsed, "action_queue", GameFunction::ActionQueue, "ALT-Q");
  parse_config_shortcut(config, parsed, "action_queue_clear", GameFunction::ActionQueueClear, "CTRL-C");
  parse_config_shortcut(config, parsed, "action_view", GameFunction::ActionView, "V");
  parse_config_shortcut(config, parsed, "action_recall", GameFunction::ActionRecall, "R");
  parse_config_shortcut(config, parsed, "action_recall_cancel", GameFunction::ActionRecallCancel, "SPACE");
  parse_config_shortcut(config, parsed, "action_repair", GameFunction::ActionRepair, "R");
  parse_config_shortcut(config, parsed, "show_chat", GameFunction::ShowChat, "C");
  parse_config_shortcut(config, parsed, "show_chatside1", GameFunction::ShowChatSide1, "ALT-C");
  parse_config_shortcut(config, parsed, "show_chatside2", GameFunction::ShowChatSide2, "`");
  parse_config_shortcut(config, parsed, "show_galaxy", GameFunction::ShowGalaxy, "G");
  parse_config_shortcut(config, parsed, "show_system", GameFunction::ShowSystem, "H");
  parse_config_shortcut(config, parsed, "zoom_preset1", GameFunction::ZoomPreset1, "F1");
  parse_config_shortcut(config, parsed, "zoom_preset2", GameFunction::ZoomPreset2, "F2");
  parse_config_shortcut(config, parsed, "zoom_preset3", GameFunction::ZoomPreset3, "F3");
  parse_config_shortcut(config, parsed, "zoom_preset4", GameFunction::ZoomPreset4, "F4");
  parse_config_shortcut(config, parsed, "zoom_preset5", GameFunction::ZoomPreset5, "F5");
  parse_config_shortcut(config, parsed, "zoom_in", GameFunction::ZoomIn, "Q");
  parse_config_shortcut(config, parsed, "zoom_out", GameFunction::ZoomOut, "E");
  parse_config_shortcut(config, parsed, "zoom_max", GameFunction::ZoomMax, "MINUS");
  parse_config_shortcut(config, parsed, "zoom_min", GameFunction::ZoomMin, "BACKSPACE");
  parse_config_shortcut(config, parsed, "zoom_reset", GameFunction::ZoomReset, "=");
  parse_config_shortcut(config, parsed, "ui_scaleup", GameFunction::UiScaleUp, "PGUP");
  parse_config_shortcut(config, parsed, "ui_scaledown", GameFunction::UiScaleDown, "PGDOWN");
  parse_config_shortcut(config, parsed, "ui_scaleviewerup", GameFunction::UiViewerScaleUp, "SHIFT-PGUP");
  parse_config_shortcut(config, parsed, "ui_scaleviewerdown", GameFunction::UiViewerScaleDown, "SHIFT-PGDOWN");
  parse_config_shortcut(config, parsed, "log_debug", GameFunction::LogLevelDebug, "F9");
  parse_config_shortcut(config, parsed, "log_trace", GameFunction::LogLevelTrace, "SHIFT-F9");
  parse_config_shortcut(config, parsed, "log_info", GameFunction::LogLevelInfo, "F11");

  if (this->hotkeys_extended) {
    parse_config_shortcut(config, parsed, "show_awayteam", GameFunction::ShowAwayTeam, "SHIFT-T");
    parse_config_shortcut(config, parsed, "show_gifts", GameFunction::ShowGifts, "/");
    parse_config_shortcut(config, parsed, "show_alliance", GameFunction::ShowAlliance, "\\");

    if (this->enable_experimental) {
      parse_config_shortcut(config, parsed, "show_alliance_help", GameFunction::ShowAllianceHelp, "SHIFT-\\");
      parse_config_shortcut(config, parsed, "show_alliance_armada", GameFunction::ShowAllianceArmada, "CTRL-\\");
    }

    parse_config_shortcut(config, parsed, "show_bookmarks", GameFunction::ShowBookmarks, "B");

    if (this->enable_experimental) {
      parse_config_shortcut(config, parsed, "show_lookup", GameFunction::ShowLookup, "L");
    }

    parse_config_shortcut(config, parsed, "show_artifacts", GameFunction::ShowArtifacts, "SHIFT-I");
    parse_config_shortcut(config, parsed, "show_commander", GameFunction::ShowCommander, "O");
    parse_config_shortcut(config, parsed, "show_daily", GameFunction::ShowDaily, "Z");
    parse_config_shortcut(config, parsed, "show_events", GameFunction::ShowEvents, "T");
    parse_config_shortcut(config, parsed, "show_exocomp", GameFunction::ShowExoComp, "X");
    parse_config_shortcut(config, parsed, "show_factions", GameFunction::ShowFactions, "F");
    parse_config_shortcut(config, parsed, "show_inventory", GameFunction::ShowInventory, "I");
    parse_config_shortcut(config, parsed, "show_missions", GameFunction::ShowMissions, "M");
    parse_config_shortcut(config, parsed, "show_research", GameFunction::ShowResearch, "U");
    parse_config_shortcut(config, parsed, "show_officers", GameFunction::ShowOfficers, "SHIFT-O");
    parse_config_shortcut(config, parsed, "show_qtrials", GameFunction::ShowQTrials, "SHIFT-Q");
    parse_config_shortcut(config, parsed, "show_refinery", GameFunction::ShowRefinery, "SHIFT-F");
    parse_config_shortcut(config, parsed, "show_ships", GameFunction::ShowShips, "N");
    parse_config_shortcut(config, parsed, "show_stationexterior", GameFunction::ShoWStationExterior, "SHIFT-G");
    parse_config_shortcut(config, parsed, "show_stationinterior", GameFunction::ShowStationInterior, "SHIFT-H");
    parse_config_shortcut(config, parsed, "set_zoom_preset1", GameFunction::SetZoomPreset1, "SHIFT-F1");
    parse_config_shortcut(config, parsed, "set_zoom_preset2", GameFunction::SetZoomPreset2, "SHIFT-F2");
    parse_config_shortcut(config, parsed, "set_zoom_preset3", GameFunction::SetZoomPreset3, "SHIFT-F3");
    parse_config_shortcut(config, parsed, "set_zoom_preset4", GameFunction::SetZoomPreset4, "SHIFT-F4");
    parse_config_shortcut(config, parsed, "set_zoom_preset5", GameFunction::SetZoomPreset5, "SHIFT-F5");
    parse_config_shortcut(config, parsed, "set_zoom_default", GameFunction::SetZoomDefault, "CTRL-=");
    parse_config_shortcut(config, parsed, "toggle_preview_locate", GameFunction::TogglePreviewLocate, "CTRL-L");
    parse_config_shortcut(config, parsed, "toggle_preview_recall", GameFunction::TogglePreviewRecall, "CTRL-R");
    parse_config_shortcut(config, parsed, "toggle_cargo_default", GameFunction::ToggleCargoDefault, "ALT-1");
    parse_config_shortcut(config, parsed, "toggle_cargo_player", GameFunction::ToggleCargoPlayer, "ALT-2");
    parse_config_shortcut(config, parsed, "toggle_cargo_station", GameFunction::ToggleCargoStation, "ALT-3");
    parse_config_shortcut(config, parsed, "toggle_cargo_hostile", GameFunction::ToggleCargoHostile, "ALT-4");
    parse_config_shortcut(config, parsed, "toggle_cargo_armada", GameFunction::ToggleCargoArmada, "ALT-5");
    parse_config_shortcut(config, parsed, "toggle_queue", GameFunction::ToggleQueue, "CTRL-Q");
  }

  if (!std::filesystem::exists(File::MakePath(File::Config()))) {
    message.str("");
    message << "Creating " << File::Config() << " (default config file)";
    spdlog::warn(message.str());

    Config::Save(config, File::Config(), false);
  }

  message.str("");
  message << "Creating " << File::Vars() << " (final config file)";
  spdlog::info(message.str());

  if (std::filesystem::exists(FILE_DEF_PARSED)) {
    message << "Removing " << FILE_DEF_PARSED << " (old parsed file)";
    std::filesystem::remove(FILE_DEF_PARSED);
  }

  Config::Save(parsed, File::Vars());

  std::cout
      << message.str() << ":\n-----------------------------\n\n"
      << parsed << "\n\n-----------------------------\nVersion "

#if VERSION_PATCH
      << "Loaded beta version " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << " (Patch "
      << VERSION_PATCH << ")\n\n"
      << "NOTE: Beta versions may have unexpected bugs and issues.\n\n"
#else
      << "Loaded beta version " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << " (Release)"
#endif

      << "\n\nPlease see https://github.com/netniv/stfc-mod for latest configuration help, examples and future releases\n"
      << "or visit the STFC Community Mod discord server at https://discord.gg/PrpHgs7Vjs\n\n";
}
