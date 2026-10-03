#pragma once
#include <utility>

namespace config_edit
{
// Settings accepted by the runtime writer. Configure and its contract fixture
// share this list so new settings cannot silently skip registration.
inline constexpr std::pair<const char*, const char*> persisted_settings[]{
    {"graphics", "zoom_label_player_detail"},
    {"graphics", "zoom_label_non_player_detail"},
    {"graphics", "zoom_label_player_threshold"},
    {"graphics", "zoom_label_non_player_threshold"},
    {"graphics", "galaxy_overlay_default"},
    {"graphics", "galaxy_overlay_mining"},
    {"graphics", "galaxy_overlay_hostiles"},
    {"graphics", "galaxy_overlay_hazards"},
    {"graphics", "galaxy_multi_select"},
    {"graphics", "galaxy_label_major_detail"},
    {"graphics", "galaxy_label_major_threshold"},
    {"graphics", "galaxy_label_minor_detail"},
    {"graphics", "galaxy_label_minor_threshold"},
    {"graphics", "keyboard_zoom_speed"},
    {"graphics", "system_pan_momentum_falloff"},
    {"ui", "auto_confirm_ft_upgrade"},
    {"ui", "disable_preview_locate"},
    {"ui", "disable_preview_recall"},
    {"ui", "show_cargo_default"},
    {"ui", "show_player_cargo"},
    {"ui", "show_station_cargo"},
    {"ui", "show_hostile_cargo"},
    {"ui", "show_armada_cargo"}};
} // namespace config_edit
