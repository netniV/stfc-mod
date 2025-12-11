#pragma once

namespace DefaultConfig
{
namespace Buffs
{
  constexpr bool use_out_of_dock_power = true;
} // namespace Buffs

namespace SystemConfig
{
  constexpr const char* assets_url_override = "";
  constexpr const char* settings_url        = "";
} // namespace SystemConfig

namespace Control
{
  constexpr bool enable_experimental = false;
  constexpr bool hotkeys_enabled     = true;
  constexpr bool hotkeys_extended    = true;
  constexpr bool use_scopely_hotkeys = false;
  constexpr bool queue_enabled       = true;
  constexpr auto select_timer        = 500;
} // namespace Control

namespace Graphics
{
  constexpr bool borderless_fullscreen       = true;
  constexpr bool allow_cursor                = true;
  constexpr auto default_system_zoom         = 1750;
  constexpr bool free_resize                 = true;
  constexpr auto keyboard_zoom_speed         = 350;
  constexpr bool show_all_resolutions        = false;
  constexpr auto system_pan_momentum_falloff = 0.8;
  constexpr auto system_pan_momentum         = 0.4;
  constexpr auto system_zoom_preset_1        = 50;
  constexpr auto system_zoom_preset_2        = 500;
  constexpr auto system_zoom_preset_3        = 1250;
  constexpr auto system_zoom_preset_4        = 2750;
  constexpr auto system_zoom_preset_5        = 5000;
  constexpr auto transition_time             = 0.01;
  constexpr auto ui_scale                    = 0.6;
  constexpr auto ui_scale_adjust             = 0.05;
  constexpr auto ui_scale_viewer             = 1.2;
  constexpr bool use_presets_as_default      = true;
  constexpr auto zoom                        = 5000;
} // namespace Graphics

namespace Patches
{
  constexpr bool bufffixhooks               = true;
  constexpr bool chatpatches                = true;
  constexpr bool freeresizehooks            = true;
  constexpr bool hotkeyhooks                = true;
  constexpr bool improveresponsivenesshooks = true;
  constexpr bool miscpatches                = true;
  constexpr bool objecttracker              = true;
  constexpr bool panhooks                   = true;
  constexpr bool resolutionlistfix          = true;
  constexpr bool syncpatches                = true;
  constexpr bool tempcrashfixes             = true;
  constexpr bool testpatches                = true;
  constexpr bool toastbannerhooks           = true;
  constexpr bool uiscalehooks               = true;
  constexpr bool zoomhooks                  = true;
} // namespace Patches

namespace Shortcuts
{
  constexpr const char* toggle_queue          = "CTRL-Q";
  constexpr const char* action_queue          = "SPACE|MOUSE1";
  constexpr const char* action_queue_clear    = "CTRL-C";
  constexpr const char* action_primary        = "SPACE|MOUSE1";
  constexpr const char* action_recall         = "R|MOUSE3";
  constexpr const char* action_recall_cancel  = "SPACE|MOUSE1";
  constexpr const char* action_repair         = "R|MOUSE3";
  constexpr const char* action_secondary      = "TAB|MOUSE4";
  constexpr const char* action_view           = "V|MOUSE2";
  constexpr const char* set_hotkeys_disabled  = "CTRL-ALT-MINUS";
  constexpr const char* set_hotkeys_enabled   = "CTRL-ALT-=";
  constexpr const char* log_off               = "CTRL-SHIFT-F12";
  constexpr const char* log_error             = "CTRL-SHIFT-F11";
  constexpr const char* log_warn              = "CTRL-SHIFT-F10";
  constexpr const char* log_debug             = "CTRL-SHIFT-F9";
  constexpr const char* log_info              = "CTRL-SHIFT-F8";
  constexpr const char* log_trace             = "CTRL-SHIFT-F7";
  constexpr const char* quit                  = "F10";
  constexpr const char* select_chatalliance   = "CTRL-2";
  constexpr const char* select_chatglobal     = "CTRL-1";
  constexpr const char* select_chatprivate    = "CTRL-3";
  constexpr const char* select_current        = "CTRL-SPACE";
  constexpr const char* select_ship1          = "1";
  constexpr const char* select_ship2          = "2";
  constexpr const char* select_ship3          = "3";
  constexpr const char* select_ship4          = "4";
  constexpr const char* select_ship5          = "5";
  constexpr const char* select_ship6          = "6";
  constexpr const char* select_ship7          = "7";
  constexpr const char* select_ship8          = "8";
  constexpr const char* set_zoom_default      = "CTRL-=";
  constexpr const char* set_zoom_preset1      = "SHIFT-F1";
  constexpr const char* set_zoom_preset2      = "SHIFT-F2";
  constexpr const char* set_zoom_preset3      = "SHIFT-F3";
  constexpr const char* set_zoom_preset4      = "SHIFT-F4";
  constexpr const char* set_zoom_preset5      = "SHIFT-F5";
  constexpr const char* show_alliance         = "ALT-'";
  constexpr const char* show_alliance_armada  = "CTRL-'";
  constexpr const char* show_alliance_help    = "SHIFT-'";
  constexpr const char* show_artifacts        = "SHIFT-I";
  constexpr const char* show_awayteam         = "T";
  constexpr const char* show_bookmarks        = "B";
  constexpr const char* show_chat             = "C";
  constexpr const char* show_chatside1        = "ALT-C";
  constexpr const char* show_chatside2        = "`";
  constexpr const char* show_commander        = "O";
  constexpr const char* show_daily            = "Z";
  constexpr const char* show_events           = "SHIFT-E";
  constexpr const char* show_exocomp          = "X";
  constexpr const char* show_factions         = "F";
  constexpr const char* show_galaxy           = "G";
  constexpr const char* show_gifts            = "/";
  constexpr const char* show_inventory        = "I";
  constexpr const char* show_lookup           = "L";
  constexpr const char* show_missions         = "M";
  constexpr const char* show_officers         = "SHIFT-O";
  constexpr const char* show_qtrials          = "SHIFT-Q";
  constexpr const char* show_refinery         = "SHIFT-F";
  constexpr const char* show_research         = "U";
  constexpr const char* show_scrapyard        = "Y";
  constexpr const char* show_settings         = "SHIFT-S";
  constexpr const char* show_ships            = "N";
  constexpr const char* show_stationexterior  = "SHIFT-G";
  constexpr const char* show_stationinterior  = "SHIFT-H";
  constexpr const char* show_system           = "H";
  constexpr const char* toggle_cargo_armada   = "ALT-5";
  constexpr const char* toggle_cargo_default  = "ALT-1";
  constexpr const char* toggle_cargo_hostile  = "ALT-4";
  constexpr const char* toggle_cargo_player   = "ALT-2";
  constexpr const char* toggle_cargo_station  = "ALT-3";
  constexpr const char* toggle_preview_locate = "CTRL-R";
  constexpr const char* toggle_preview_recall = "CTRL-T";
  constexpr const char* ui_scaledown          = "PGDOWN";
  constexpr const char* ui_scaleup            = "PGUP";
  constexpr const char* ui_scaleviewerdown    = "SHIFT-PGDOWN";
  constexpr const char* ui_scaleviewerup      = "SHIFT-PGUP";
  constexpr const char* zoom_in               = "Q";
  constexpr const char* zoom_max              = "MINUS";
  constexpr const char* zoom_min              = "BACKSPACE";
  constexpr const char* zoom_out              = "E";
  constexpr const char* zoom_reset            = "=";
  constexpr const char* zoom_preset1          = "F1";
  constexpr const char* zoom_preset2          = "F2";
  constexpr const char* zoom_preset3          = "F3";
  constexpr const char* zoom_preset4          = "F4";
  constexpr const char* zoom_preset5          = "F5";
  constexpr const char* move_up               = "W";
  constexpr const char* move_down             = "S";
  constexpr const char* move_left             = "A";
  constexpr const char* move_right            = "D";
} // namespace Shortcuts

namespace Sync
{
  constexpr bool        battlelogs         = true;
  constexpr bool        buffs              = true;
  constexpr bool        buildings          = true;
  constexpr bool        inventory          = true;
  constexpr bool        jobs               = true;
  constexpr bool        missions           = true;
  constexpr bool        officer            = true;
  constexpr const char* proxy              = "";
  constexpr bool        research           = true;
  constexpr bool        resources          = true;
  constexpr bool        ships              = true;
  constexpr bool        slots              = true;
  constexpr bool        tech               = true;
  constexpr bool        traits             = true;
  constexpr const char* token              = "";
  constexpr const char* url                = "";
  constexpr bool        debug              = false;
  constexpr bool        logging            = false;
  constexpr bool        verify_ssl         = true;
  constexpr auto        resolver_cache_ttl = 300;
} // namespace Sync

namespace UI
{
  constexpr bool        always_skip_reveal_sequence = true;
  constexpr bool        auto_confirm_discovery      = true;
  constexpr bool        disable_escape_exit         = true;
  constexpr bool        disable_first_popup         = false;
  constexpr bool        disable_galaxy_chat         = false;
  constexpr bool        disable_move_keys           = false;
  constexpr bool        disable_preview_locate      = false;
  constexpr bool        disable_preview_recall      = false;
  constexpr bool        disable_toast_banners       = false;
  constexpr bool        disable_veil_chat           = false;
  constexpr const char* disabled_banner_types       = "";
  constexpr auto        extend_donation_max         = 80;
  constexpr bool        extend_donation_slider      = true;
  constexpr bool        show_armada_cargo           = true;
  constexpr bool        show_cargo_default          = true;
  constexpr bool        show_hostile_cargo          = true;
  constexpr bool        show_player_cargo           = true;
  constexpr bool        show_station_cargo          = true;
} // namespace UI

} // namespace DefaultConfig
