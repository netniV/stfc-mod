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

namespace
{
enum class FabricEventAction : int {
  PlaySound       = 0,
  StopSound       = 1,
  UnpauseSound    = 3,
  AdvanceSequence = 16,
  StopAll         = 21,
  UnloadAudio     = 23,
  PlayScheduled   = 33,
};

constexpr bool can_start_audio(int event_action)
{
  switch (static_cast<FabricEventAction>(event_action)) {
    case FabricEventAction::PlaySound:
    case FabricEventAction::UnpauseSound:
    case FabricEventAction::AdvanceSequence:
    case FabricEventAction::PlayScheduled:
      return true;
    default:
      return false;
  }
}

constexpr bool should_suppress_audio_event(bool disable_all, bool event_is_disabled, int event_action)
{ return (disable_all || event_is_disabled) && can_start_audio(event_action); }

static_assert(should_suppress_audio_event(true, false, static_cast<int>(FabricEventAction::PlaySound)));
static_assert(should_suppress_audio_event(true, false, static_cast<int>(FabricEventAction::UnpauseSound)));
static_assert(should_suppress_audio_event(false, true, static_cast<int>(FabricEventAction::PlayScheduled)));
static_assert(!should_suppress_audio_event(true, false, static_cast<int>(FabricEventAction::StopSound)));
static_assert(!should_suppress_audio_event(true, false, static_cast<int>(FabricEventAction::StopAll)));
static_assert(!should_suppress_audio_event(true, false, static_cast<int>(FabricEventAction::UnloadAudio)));
} // namespace

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
    spdlog::info("Audio event: {} (action {})", event, event_action);
  }

  const bool event_is_disabled =
      std::ranges::find(config.disabled_audio_events, event) != config.disabled_audio_events.end();
  if (should_suppress_audio_event(config.disable_all_audio_events, event_is_disabled, event_action)) {
    spdlog::debug("Suppressed audio event: {} (action {})", event, event_action);
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
