/**
 * @file fleet_arrival.cc
 * @brief Fleet arrival detection driven by the bottom fleet bar state widgets.
 *
 * Hooks FleetStateWidget::SetWidgetData and watches FleetPlayerData state
 * transitions. The most useful arrival signal is Warping -> Impulsing, which
 * means the ship has dropped out of warp and entered the destination system.
 */
#include "errormsg.h"

#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>

#include <patches/notification_service.h>
#include <prime/FleetPlayerData.h>

#include <spdlog/spdlog.h>
#include <str_utils.h>

#include <string_view>
#include <unordered_map>

static std::unordered_map<uint64_t, FleetState> s_fleet_bar_states;

static std::string fleet_bar_ship_name(FleetPlayerData* fleet)
{
  auto* hull = fleet ? fleet->Hull : nullptr;
  auto  name = (hull && hull->Name) ? to_string(hull->Name) : std::string{"?"};

  constexpr std::string_view live_suffix = "_LIVE";
  if (name.size() >= live_suffix.size() &&
      name.compare(name.size() - live_suffix.size(), live_suffix.size(), live_suffix) == 0) {
    name.erase(name.size() - live_suffix.size());
  }

  for (auto& ch : name) {
    if (ch == '_') {
      ch = ' ';
    }
  }

  return name;
}

static FleetPlayerData* fleet_bar_widget_context(void* self)
{
  if (!self) {
    return nullptr;
  }

  auto helper      = IL2CppClassHelper{((Il2CppObject*)self)->klass};
  auto get_context = helper.GetMethod<FleetPlayerData*(void*)>("get_Context", 0);
  return get_context ? get_context(self) : nullptr;
}

static void maybe_notify_fleet_bar_transition(uint64_t fleetId, const std::string& shipName,
                                              FleetState oldState, FleetState newState)
{
  if (oldState == FleetState::Warping && newState == FleetState::Impulsing) {
    auto body = "Your " + shipName + " has arrived in-system";
    spdlog::info("[FleetBar] ARRIVED_IN_SYSTEM id={} ship='{}'", fleetId, shipName);
    notification_show("Fleet Arrived", body.c_str());
    return;
  }

  if (oldState == FleetState::Impulsing && newState == FleetState::IdleInSpace) {
    spdlog::info("[FleetBar] ARRIVED_AT_DESTINATION id={} ship='{}'", fleetId, shipName);
    return;
  }
}

static void FleetStateWidget_SetWidgetData_Hook(auto original, void* self)
{
  auto* fleet = fleet_bar_widget_context(self);
  if (fleet) {
    auto fleetId      = fleet->Id;
    auto currentState = fleet->CurrentState;
    auto shipName     = fleet_bar_ship_name(fleet);

    auto it = s_fleet_bar_states.find(fleetId);
    if (it != s_fleet_bar_states.end() && it->second != currentState) {
      maybe_notify_fleet_bar_transition(fleetId, shipName, it->second, currentState);
    }

    s_fleet_bar_states[fleetId] = currentState;
  }

  original(self);
}

void InstallFleetArrivalHooks()
{
  notification_init();

  auto fleet_state_widget = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetStateWidget");
  if (!fleet_state_widget.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.HUD", "FleetStateWidget");
    return;
  }

  auto set_widget_data = fleet_state_widget.GetMethod("SetWidgetData");
  if (set_widget_data == nullptr) {
    ErrorMsg::MissingMethod("FleetStateWidget", "SetWidgetData");
    return;
  }

  SPUD_STATIC_DETOUR(set_widget_data, FleetStateWidget_SetWidgetData_Hook);
}