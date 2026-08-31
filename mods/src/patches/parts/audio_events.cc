#include "config.h"
#include "errormsg.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>

#include <spud/detour.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

struct FabricEventManager {
};

struct FabricEvent {
  Il2CppObject  object;
  Il2CppString* event_name;
  int32_t       event_id;
  int           event_action;
};

static_assert(offsetof(FabricEvent, event_name) == 0x10);
static_assert(offsetof(FabricEvent, event_id) == 0x18);
static_assert(offsetof(FabricEvent, event_action) == 0x1C);

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
{ return disable_all ? can_start_audio(event_action) : event_is_disabled; }

static_assert(should_suppress_audio_event(true, false, static_cast<int>(FabricEventAction::PlaySound)));
static_assert(should_suppress_audio_event(true, false, static_cast<int>(FabricEventAction::UnpauseSound)));
static_assert(should_suppress_audio_event(false, true, static_cast<int>(FabricEventAction::PlayScheduled)));
static_assert(!should_suppress_audio_event(true, false, static_cast<int>(FabricEventAction::StopSound)));
static_assert(!should_suppress_audio_event(true, false, static_cast<int>(FabricEventAction::StopAll)));
static_assert(!should_suppress_audio_event(true, false, static_cast<int>(FabricEventAction::UnloadAudio)));
static_assert(should_suppress_audio_event(false, true, static_cast<int>(FabricEventAction::StopSound)));
static_assert(!should_suppress_audio_event(true, true, static_cast<int>(FabricEventAction::StopSound)));
static_assert(!should_suppress_audio_event(true, true, static_cast<int>(FabricEventAction::StopAll)));
static_assert(!should_suppress_audio_event(true, true, static_cast<int>(FabricEventAction::UnloadAudio)));

bool should_suppress_audio_event(const FabricEvent* event)
{
  if (event == nullptr) {
    return false;
  }

  auto& config = Config::Get();

  // `All` is the common high-frequency path. Avoid converting every managed event name when tracing is off.
  if (config.disable_all_audio_events && !config.trace_audio_events) {
    if (can_start_audio(event->event_action)) {
      spdlog::debug("Suppressed audio event ID {} (action {})", event->event_id, event->event_action);
      return true;
    }
    return false;
  }

  if (!config.trace_audio_events && config.disabled_audio_events.empty()) {
    return false;
  }

  std::string event_name;
  if (event->event_name != nullptr) {
    event_name = to_string(event->event_name);
  }

  if (config.trace_audio_events) {
    if (event_name.empty()) {
      spdlog::info("Audio event ID: {} (action {})", event->event_id, event->event_action);
    } else {
      spdlog::info("Audio event: {} (ID {}, action {})", event_name, event->event_id, event->event_action);
    }
  }

  const bool event_is_disabled =
      !event_name.empty()
      && std::ranges::find(config.disabled_audio_events, event_name) != config.disabled_audio_events.end();
  if (!should_suppress_audio_event(config.disable_all_audio_events, event_is_disabled, event->event_action)) {
    return false;
  }

  if (event_name.empty()) {
    spdlog::debug("Suppressed audio event ID {} (action {})", event->event_id, event->event_action);
  } else {
    spdlog::debug("Suppressed audio event: {} (ID {}, action {})", event_name, event->event_id, event->event_action);
  }
  return true;
}

bool is_fabric_event_overload(int param_count, const Il2CppType** params, Il2CppClass* event_class)
{
  return param_count == 2 && params != nullptr && params[0] != nullptr && params[1] != nullptr
         && il2cpp_class_from_type(params[0]) == event_class && params[1]->type == IL2CPP_TYPE_BOOLEAN;
}

uintptr_t canonical_spud_target(uintptr_t address)
{
#if defined(__aarch64__) || defined(_M_ARM64)
  const auto instruction = *reinterpret_cast<const uint32_t*>(address);
  if ((instruction >> 26) != 0b000101) {
    return address;
  }
  const auto offset = static_cast<int64_t>(static_cast<int32_t>(instruction << 6)) >> 4;
  return address + offset;
#else
  const auto* memory = reinterpret_cast<const uint8_t*>(address);
  if (memory[0] == 0x55 && memory[1] == 0x48 && memory[2] == 0x89 && memory[3] == 0xE5 && memory[4] == 0x5D) {
    return canonical_spud_target(address + 5);
  }
  if (memory[0] != 0xE9) {
    return address;
  }
  int32_t offset = 0;
  std::memcpy(&offset, memory + 1, sizeof(offset));
  return canonical_spud_target(address + offset + 5);
#endif
}

bool validate_fabric_event_layout(Il2CppClass* event_class)
{
  struct ExpectedField {
    const char* name;
    size_t      offset;
  };
  constexpr ExpectedField expected_fields[] = {
      {"_eventName", offsetof(FabricEvent, event_name)},
      {"_eventID", offsetof(FabricEvent, event_id)},
      {"EventAction", offsetof(FabricEvent, event_action)},
  };

  for (const auto& expected : expected_fields) {
    const auto* field = il2cpp_class_get_field_from_name(event_class, expected.name);
    if (field == nullptr || static_cast<size_t>(field->offset) != expected.offset) {
      spdlog::error("Fabric.Event field '{}' is missing or has an unexpected layout", expected.name);
      return false;
    }
  }
  return true;
}
} // namespace

// Fabric.EventManager.PostEvent(Event, bool). String and EventTrigger callers converge here.
bool FabricEventManager_PostEvent_Hook(auto original, FabricEventManager* _this, FabricEvent* event, bool add_to_queue)
{
  if (should_suppress_audio_event(event)) {
    return false;
  }
  return original(_this, event, add_to_queue);
}

// Fabric.EventManager.PostEventID(Event, bool). Hashed integer-ID callers converge here.
bool FabricEventManager_PostEventID_Hook(auto original, FabricEventManager* _this, FabricEvent* event,
                                         bool add_to_queue)
{
  if (should_suppress_audio_event(event)) {
    return false;
  }
  return original(_this, event, add_to_queue);
}

void InstallAudioEventHooks()
{
  auto helper = il2cpp_get_class_helper("Fabric.Core", "Fabric", "EventManager");
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Fabric", "EventManager");
    return;
  }

  auto event_helper = il2cpp_get_class_helper("Fabric.Core", "Fabric", "Event");
  if (!event_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Fabric", "Event");
    return;
  }

  auto* event_class = event_helper.get_cls();
  if (event_class == nullptr || !validate_fabric_event_layout(event_class)) {
    return;
  }
  auto  event_filter = [event_class](int count, const Il2CppType** params) {
    return is_fabric_event_overload(count, params, event_class);
  };

  const auto post_event    = helper.GetMethodInfoSpecial("PostEvent", event_filter);
  const auto post_event_id = helper.GetMethodInfoSpecial("PostEventID", event_filter);
  if (post_event == nullptr || post_event->methodPointer == nullptr) {
    ErrorMsg::MissingMethod("EventManager", "PostEvent(Event, bool)");
    return;
  }
  if (post_event_id == nullptr || post_event_id->methodPointer == nullptr) {
    ErrorMsg::MissingMethod("EventManager", "PostEventID(Event, bool)");
    return;
  }
  if (post_event->return_type == nullptr || post_event->return_type->type != IL2CPP_TYPE_BOOLEAN
      || post_event_id->return_type == nullptr || post_event_id->return_type->type != IL2CPP_TYPE_BOOLEAN) {
    spdlog::error("Fabric EventManager event routes have an unexpected return type");
    return;
  }

  const auto post_event_target = canonical_spud_target(reinterpret_cast<uintptr_t>(post_event->methodPointer));
  const auto post_event_id_target = canonical_spud_target(reinterpret_cast<uintptr_t>(post_event_id->methodPointer));
  if (post_event_target == post_event_id_target) {
    spdlog::error("Fabric EventManager event and event-ID routes unexpectedly share a hook target");
    return;
  }

  SPUD_STATIC_DETOUR(post_event->methodPointer, FabricEventManager_PostEvent_Hook);
  SPUD_STATIC_DETOUR(post_event_id->methodPointer, FabricEventManager_PostEventID_Hook);
}
