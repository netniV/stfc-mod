#include "patches/fleet_watch.h"

#include "errormsg.h"
#include "patches/notification_service.h"
#include "patches/screen_update_hook.h"

#include <il2cpp/il2cpp_helper.h>
#include <prime/FleetPlayerData.h>

#include <spud/detour.h>

#include <cstdint>

namespace
{
FleetPlayerData* fleet_widget_context(void* self)
{
  if (!self) {
    return nullptr;
  }

  static auto get_context =
      IL2CppClassHelper{reinterpret_cast<Il2CppObject*>(self)->klass}.GetMethod<FleetPlayerData*(void*)>("get_Context",
                                                                                                         0);
  return get_context ? get_context(self) : nullptr;
}

void FleetStateWidget_SetWidgetData_Hook(auto original, void* self)
{
  original(self);
  fleet_watch_observe_widget(fleet_widget_context(self));
}

void ToastFleetObserver_HandleMiningDepleted_Hook(auto original, void* self, int64_t fleet_id)
{
  original(self, fleet_id);
  fleet_watch_observe_node_depleted(fleet_id);
}
} // namespace

void InstallFleetWatchHooks()
{
#if !defined(_WIN32) && !defined(__APPLE__)
  return;
#endif

  fleet_watch_init();
  if (!fleet_watch_uses_state_observation() && !fleet_watch_uses_node_depleted_hook()) {
    return;
  }
  notification_init();

  if (fleet_watch_uses_state_observation()) {
    install_screen_manager_update_hook();
    auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "FleetStateWidget");
    if (!helper.isValidHelper()) {
      ErrorMsg::MissingHelper("HUD", "FleetStateWidget");
    } else if (auto method = helper.GetMethod("SetWidgetData"); method) {
      SPUD_STATIC_DETOUR(method, FleetStateWidget_SetWidgetData_Hook);
    } else {
      ErrorMsg::MissingMethod("FleetStateWidget", "SetWidgetData");
    }
  }

  if (fleet_watch_uses_node_depleted_hook()) {
    auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "ToastFleetObserver");
    if (!helper.isValidHelper()) {
      ErrorMsg::MissingHelper("HUD", "ToastFleetObserver");
    } else if (auto method = helper.GetMethod("HandleMiningDepleted", 1); method) {
      SPUD_STATIC_DETOUR(method, ToastFleetObserver_HandleMiningDepleted_Hook);
    } else {
      ErrorMsg::MissingMethod("ToastFleetObserver", "HandleMiningDepleted");
    }
  }
}
