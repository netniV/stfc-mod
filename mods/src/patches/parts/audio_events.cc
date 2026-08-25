#include "config.h"
#include "errormsg.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>

#include <spud/detour.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

struct FabricEventManager {
};

// Fabric.EventManager.PostEvent(string, EventAction, object, GameObject,
// InitialiseParameters, bool, OnEventNotify)
bool FabricEventManager_PostEvent_Hook(auto original, FabricEventManager* _this, Il2CppString* event_name,
                                       int event_action, Il2CppObject* parameter, Il2CppObject* parent_game_object,
                                       Il2CppObject* initialise_parameters, bool add_to_queue,
                                       Il2CppObject* on_event_notify)
{
  if (event_name == nullptr) {
    return original(_this, event_name, event_action, parameter, parent_game_object, initialise_parameters,
                    add_to_queue, on_event_notify);
  }

  const auto event = to_string(event_name);
  auto&      config = Config::Get();

  if (config.trace_audio_events) {
    spdlog::info("Audio event: {}", event);
  }

  if (std::ranges::find(config.disabled_audio_events, event) != config.disabled_audio_events.end()) {
    spdlog::debug("Suppressed audio event: {}", event);
    return false;
  }

  return original(_this, event_name, event_action, parameter, parent_game_object, initialise_parameters,
                  add_to_queue, on_event_notify);
}

void InstallAudioEventHooks()
{
  auto helper = il2cpp_get_class_helper("Fabric.Core", "Fabric", "EventManager");
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Fabric", "EventManager");
    return;
  }

  // The shorter PostEvent overloads funnel into this seven-argument overload.
  const auto method = helper.GetMethodInfo("PostEvent", 7);
  if (method == nullptr || method->methodPointer == nullptr) {
    ErrorMsg::MissingMethod("EventManager", "PostEvent");
    return;
  }

  SPUD_STATIC_DETOUR(method->methodPointer, FabricEventManager_PostEvent_Hook);
}
