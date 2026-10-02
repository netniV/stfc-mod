#include "errormsg.h"
#include "config.h"
#include "mod_state.h"
#include "prime/KeyCode.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <il2cpp/runtime.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <limits>
#include <vector>

namespace
{
struct OfficerPresetItemContext {
  Il2CppObject  object;
  void*         synthetic_fleet_player_data;
  int32_t       presentation;
  bool          is_save_available;
  bool          is_occupied;
  int32_t       order_id;
  int64_t       slot_id;
  Il2CppString* preset_name;
  Il2CppArray*  officers;
  Il2CppArray*  below_deck_officers;
  bool          below_deck_slots_unlocked;
  void*         officer_presets_view_context;
};

std::vector<int64_t> session_order;
std::vector<int32_t> active_presentations;
Il2CppObject* active_controller = nullptr;
Il2CppObject* active_view_context = nullptr;
Il2CppClass* item_context_class = nullptr;
Il2CppClass* view_context_class = nullptr;
Il2CppClass* scroller_class = nullptr;
FieldInfo* widget_context_field = nullptr;
FieldInfo* controller_context_field = nullptr;
FieldInfo* controller_scroller_field = nullptr;
FieldInfo* presets_items_field = nullptr;
FieldInfo* scroller_data_field = nullptr;
FieldInfo* scroller_held_data_field = nullptr;
const MethodInfo* clear_and_generate = nullptr;
const MethodInfo* get_scroll_position = nullptr;
const MethodInfo* restore_scroll_position = nullptr;
bool hooks_ready = false;
bool custom_presentations = false;
bool view_failed = false;
bool processing = false;
bool order_loaded = false;
uint64_t view_epoch = 0;

bool enabled() { return hooks_ready && Config::Get().allow_officer_preset_reordering; }

struct ProcessingScope {
  bool previous = processing;
  ProcessingScope() { processing = true; }
  ~ProcessingScope() { processing = previous; }
};

bool instance_field(FieldInfo* field)
{
  return field && field->type && !field->type->byref && field->offset >= 0
         && !(field->type->attrs & FIELD_ATTRIBUTE_STATIC);
}

FieldInfo* reference_field(Il2CppClass* owner, const char* name, Il2CppClass* expected = nullptr)
{
  auto* field = owner ? il2cpp_class_get_field_from_name(owner, name) : nullptr;
  auto* cls = field && field->type ? il2cpp_class_from_type(field->type) : nullptr;
  return instance_field(field) && cls && !il2cpp_class_is_valuetype(cls)
         && (!expected || il2cpp_class_is_assignable_from(expected, cls)) ? field : nullptr;
}

Il2CppObject* read_reference(void* object, FieldInfo* field)
{
  Il2CppObject* value = nullptr;
  if (!object || !instance_field(field)) return nullptr;
  auto* instance = static_cast<Il2CppObject*>(object);
  if (!instance->klass || !il2cpp_class_is_assignable_from(field->parent, instance->klass)) return nullptr;
  il2cpp_field_get_value(instance, field, &value);
  auto* expected = il2cpp_class_from_type(field->type);
  return value && value->klass && expected && il2cpp_class_is_assignable_from(expected, value->klass) ? value : nullptr;
}

bool write_reference(Il2CppObject* object, FieldInfo* field, Il2CppObject* value)
{
  auto* expected = field && field->type ? il2cpp_class_from_type(field->type) : nullptr;
  if (!object || !object->klass || !instance_field(field) || !expected
      || !il2cpp_class_is_assignable_from(field->parent, object->klass)
      || (value && (!value->klass || !il2cpp_class_is_assignable_from(expected, value->klass)))) return false;
  il2cpp_field_set_value_object(object, field, value);
  Il2CppObject* observed = nullptr;
  il2cpp_field_get_value(object, field, &observed);
  return observed == value;
}

void clear_active_view()
{
  ++view_epoch;
  active_controller = nullptr;
  active_view_context = nullptr;
  active_presentations.clear();
  custom_presentations = false;
  view_failed = false;
}

void load_session_order()
{
  const auto state = mod_state::Read();
  if (!state) {
    return;
  }

  try {
    const auto order = state->find("officer_preset_order");
    if (order == state->end() || !order->is_array()) {
      return;
    }

    std::vector<int64_t> loaded_order;
    loaded_order.reserve(order->size());
    for (const auto& item : *order) {
      int64_t slot_id = -1;
      if (item.is_string()) {
        const auto& value  = item.get_ref<const std::string&>();
        const auto  result = std::from_chars(value.data(), value.data() + value.size(), slot_id);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
          continue;
        }
      } else if (item.is_number_integer()) {
        slot_id = item.get<int64_t>();
      } else {
        continue;
      }

      if (slot_id >= 0 && std::find(loaded_order.begin(), loaded_order.end(), slot_id) == loaded_order.end()) {
        loaded_order.push_back(slot_id);
      }
    }

    session_order = std::move(loaded_order);
    spdlog::info("[OfficerPresetReorder] loaded {} persisted slot positions", session_order.size());
  } catch (const std::exception& error) {
    spdlog::warn("[OfficerPresetReorder] ignored invalid persisted order: {}", error.what());
  }
}

void ensure_order_loaded()
{
  if (!order_loaded) {
    load_session_order();
    order_loaded = true;
  }
}

bool save_session_order()
{
  auto order = nlohmann::json::array();
  for (const auto slot_id : session_order) {
    order.push_back(std::to_string(slot_id));
  }
  return mod_state::Update([&order](nlohmann::json& state) { state["officer_preset_order"] = std::move(order); });
}

static_assert(offsetof(OfficerPresetItemContext, presentation) == 0x18);
static_assert(offsetof(OfficerPresetItemContext, is_occupied) == 0x1D);
static_assert(offsetof(OfficerPresetItemContext, order_id) == 0x20);
static_assert(offsetof(OfficerPresetItemContext, slot_id) == 0x28);
static_assert(offsetof(OfficerPresetItemContext, preset_name) == 0x30);

bool validate_context_field(Il2CppClass* context_class, const char* name, ptrdiff_t expected_offset,
                            Il2CppTypeEnum expected_type)
{
  auto* field = il2cpp_class_get_field_from_name(context_class, name);
  if (!instance_field(field)) {
    spdlog::error("[OfficerPresetReorder] required context field '{}' is unavailable", name);
    return false;
  }
  if (field->offset != expected_offset || field->type->type != expected_type) {
    spdlog::error("[OfficerPresetReorder] context field '{}' layout mismatch: offset=0x{:X} type={} expected "
                  "offset=0x{:X} type={}; feature disabled",
                  name, field->offset, static_cast<int>(field->type->type), expected_offset,
                  static_cast<int>(expected_type));
    return false;
  }
  return true;
}

bool validate_context_layout(Il2CppClass* context_class)
{
  auto* presentation = context_class ? il2cpp_class_get_field_from_name(context_class, "Presentation") : nullptr;
  auto* enum_class = presentation && presentation->type ? il2cpp_class_from_type(presentation->type) : nullptr;
  const auto* underlying = enum_class && il2cpp_class_is_enum(enum_class) ? il2cpp_class_enum_basetype(enum_class) : nullptr;
  auto* owner_field = context_class ? reference_field(context_class, "_officerPresetsViewContext", view_context_class) : nullptr;
  auto* officers_field = context_class ? il2cpp_class_get_field_from_name(context_class, "Officers") : nullptr;
  auto* officers_array = officers_field && officers_field->type ? il2cpp_class_from_type(officers_field->type) : nullptr;
  return context_class != nullptr && enum_class && enum_class->declaringType == context_class
         && std::strcmp(enum_class->name, "PresentationType") == 0
         && underlying && !underlying->byref && underlying->type == IL2CPP_TYPE_I4
         && owner_field && officers_array && officers_array->element_class
         && !il2cpp_class_is_valuetype(officers_array->element_class)
         && validate_context_field(context_class, "Presentation", offsetof(OfficerPresetItemContext, presentation),
                                   IL2CPP_TYPE_VALUETYPE)
         && validate_context_field(context_class, "IsOccupied", offsetof(OfficerPresetItemContext, is_occupied),
                                   IL2CPP_TYPE_BOOLEAN)
         && validate_context_field(context_class, "OrderId", offsetof(OfficerPresetItemContext, order_id),
                                   IL2CPP_TYPE_I4)
         && validate_context_field(context_class, "SlotId", offsetof(OfficerPresetItemContext, slot_id), IL2CPP_TYPE_I8)
         && validate_context_field(context_class, "PresetName", offsetof(OfficerPresetItemContext, preset_name),
                                   IL2CPP_TYPE_STRING)
         && validate_context_field(context_class, "Officers", offsetof(OfficerPresetItemContext, officers),
                                   IL2CPP_TYPE_SZARRAY)
         && validate_context_field(context_class, "_officerPresetsViewContext",
                                   offsetof(OfficerPresetItemContext, officer_presets_view_context), IL2CPP_TYPE_CLASS);
}

bool is_reorderable_preset(const OfficerPresetItemContext* context)
{
  return context != nullptr && context->object.klass == item_context_class && context->slot_id >= 0 && context->order_id >= 0 && context->preset_name != nullptr
         && context->officers != nullptr && reinterpret_cast<Il2CppArraySize*>(context->officers)->max_length > 0;
}

bool key_pressed(KeyCode key)
{
  static auto get_key = il2cpp_resolve_icall_typed<bool(KeyCode)>("UnityEngine.Input::GetKeyInt(UnityEngine.KeyCode)");
  return get_key != nullptr && get_key(key);
}

bool shift_pressed()
{ return key_pressed(KeyCode::LeftShift) || key_pressed(KeyCode::RightShift); }

bool control_pressed()
{
#ifdef __APPLE__
  return key_pressed(KeyCode::LeftCommand) || key_pressed(KeyCode::RightCommand);
#else
  return key_pressed(KeyCode::LeftControl) || key_pressed(KeyCode::RightControl);
#endif
}

void remember_slots(OfficerPresetItemContext** items, il2cpp_array_size_t size)
{
  for (il2cpp_array_size_t index = 0; index < size; ++index) {
    const auto* context = items[index];
    if (!is_reorderable_preset(context)) {
      continue;
    }
    if (std::find(session_order.begin(), session_order.end(), context->slot_id) == session_order.end()) {
      session_order.push_back(context->slot_id);
    }
  }
}

bool has_native_identity(const OfficerPresetItemContext* context)
{ return context != nullptr && context->object.klass == item_context_class && context->slot_id >= 0 && context->order_id >= 0; }

bool validate_unique_preset_identity(OfficerPresetItemContext** items, int32_t size)
{
  std::vector<int64_t> slot_ids;
  std::vector<int32_t> order_ids;
  slot_ids.reserve(size);
  order_ids.reserve(size);
  for (int32_t index = 0; index < size; ++index) {
    const auto* context = items[index];
    if (context && context->object.klass != item_context_class) return false;
    if (!has_native_identity(context)) {
      continue;
    }
    if (std::find(slot_ids.begin(), slot_ids.end(), context->slot_id) != slot_ids.end()
        || std::find(order_ids.begin(), order_ids.end(), context->order_id) != order_ids.end()) {
      spdlog::error("[OfficerPresetReorder] duplicate preset identity detected at index {}: slot={} order={}", index,
                    context->slot_id, context->order_id);
      return false;
    }
    slot_ids.push_back(context->slot_id);
    order_ids.push_back(context->order_id);
  }
  return true;
}

bool validate_canonical_order(OfficerPresetItemContext** items, int32_t size)
{
  if (!validate_unique_preset_identity(items, size)) {
    return false;
  }
  for (int32_t index = 0; index < size; ++index) {
    const auto* context = items[index];
    if (!has_native_identity(context)) {
      continue;
    }
    if (context->order_id != index) {
      spdlog::error("[OfficerPresetReorder] canonical preset invariant failed at index {}: slot={} order={}; "
                    "local ordering disabled for this view",
                    index, context->slot_id, context->order_id);
      return false;
    }
  }
  return true;
}

std::vector<OfficerPresetItemContext*> make_view_order(OfficerPresetItemContext** canonical_items, int32_t size)
{
  std::vector<OfficerPresetItemContext*> view_items(canonical_items, canonical_items + size);
  remember_slots(canonical_items, static_cast<il2cpp_array_size_t>(size));

  std::vector<int32_t>                   occupied_positions;
  std::vector<OfficerPresetItemContext*> occupied_contexts;
  occupied_positions.reserve(size);
  occupied_contexts.reserve(size);
  for (int32_t index = 0; index < size; ++index) {
    if (is_reorderable_preset(canonical_items[index])) {
      occupied_positions.push_back(index);
      occupied_contexts.push_back(canonical_items[index]);
    }
  }

  std::stable_sort(occupied_contexts.begin(), occupied_contexts.end(), [](const auto* left, const auto* right) {
    const auto left_order  = std::find(session_order.begin(), session_order.end(), left->slot_id);
    const auto right_order = std::find(session_order.begin(), session_order.end(), right->slot_id);
    return left_order < right_order;
  });
  for (size_t index = 0; index < occupied_positions.size(); ++index) {
    view_items[occupied_positions[index]] = occupied_contexts[index];
  }
  return view_items;
}

bool try_get_preset_list(void* view_context, Il2CppObject** list, Il2CppArraySize** backing_items, int32_t* size)
{
  *list = read_reference(view_context, presets_items_field);
  if (!*list || !(*list)->klass) return false;
  auto* items_field = reference_field((*list)->klass, "_items");
  auto* size_field = il2cpp_class_get_field_from_name((*list)->klass, "_size");
  auto* array_class = items_field ? il2cpp_class_from_type(items_field->type) : nullptr;
  if (!array_class || items_field->type->type != IL2CPP_TYPE_SZARRAY
      || array_class->element_class != item_context_class || !instance_field(size_field)
      || size_field->type->type != IL2CPP_TYPE_I4) return false;
  *backing_items = reinterpret_cast<Il2CppArraySize*>(read_reference(*list, items_field));
  il2cpp_field_get_value(*list, size_field, size);
  return *backing_items && reinterpret_cast<Il2CppObject*>(*backing_items)->klass == array_class && *size >= 0
         && (*backing_items)->max_length <= static_cast<il2cpp_array_size_t>(std::numeric_limits<int32_t>::max())
         && static_cast<il2cpp_array_size_t>(*size) <= (*backing_items)->max_length;
}

bool capture_native_presentations(OfficerPresetItemContext** canonical_items, int32_t size)
{
  if (!validate_canonical_order(canonical_items, size)) {
    return false;
  }
  active_presentations.clear();
  active_presentations.reserve(size);
  for (int32_t index = 0; index < size; ++index) {
    active_presentations.push_back(canonical_items[index] != nullptr ? canonical_items[index]->presentation : 0);
  }
  return true;
}

bool restore_native_presentations(OfficerPresetItemContext** canonical_items, int32_t size)
{
  if (!validate_canonical_order(canonical_items, size) || active_presentations.size() != static_cast<size_t>(size)) {
    spdlog::error("[OfficerPresetReorder] native presentation map is unavailable; local ordering disabled for this "
                  "view");
    return false;
  }
  for (int32_t index = 0; index < size; ++index) {
    if (canonical_items[index] != nullptr) {
      canonical_items[index]->presentation = active_presentations[index];
    }
  }
  return true;
}

bool read_scroll(Il2CppObject* scroller, float& position)
{
  Il2CppObject* boxed = nullptr;
  if (!Il2CppRuntime::TryInvoke(get_scroll_position, scroller, nullptr, &boxed)
      || !boxed || !boxed->klass || il2cpp_class_get_type(boxed->klass)->type != IL2CPP_TYPE_R4) return false;
  auto* value = static_cast<float*>(il2cpp_object_unbox(boxed));
  if (!value) return false;
  position = *value;
  return true;
}

bool restore_scroll(Il2CppObject* scroller, float position)
{
  void* args[]{&position};
  return Il2CppRuntime::TryInvoke(restore_scroll_position, scroller, args);
}

// This runs for an owned custom view even after the preference becomes false.
// A successful ClearAndGenerateContents can defer or omit AssignContextData.
// Its invocation status therefore cannot establish the canonical save source.
bool restore_canonical_view(Il2CppObject* controller, bool regenerate)
{
  if (active_controller != controller || !custom_presentations) return true;
  auto* view_context = read_reference(controller, controller_context_field);
  auto* scroller = read_reference(controller, controller_scroller_field);
  Il2CppObject* list = nullptr;
  Il2CppArraySize* backing = nullptr;
  int32_t size = 0;
  if (view_context != active_view_context || !scroller
      || !try_get_preset_list(view_context, &list, &backing, &size)
      || !restore_native_presentations(reinterpret_cast<OfficerPresetItemContext**>(backing->vector), size)) {
    view_failed = true;
    return false;
  }
  bool regenerated = true;
  if (regenerate) {
    ProcessingScope scope;
    void* args[]{controller, list};
    regenerated = Il2CppRuntime::TryInvoke(clear_and_generate, scroller, args);
  }
  // Reference writes use the reflected API and its GC barrier, never raw offsets.
  const bool source_restored = write_reference(scroller, scroller_data_field, list);
  Il2CppObject* held = nullptr;
  il2cpp_field_get_value(scroller, scroller_held_data_field, &held);
  const bool pending_restored = !held || write_reference(scroller, scroller_held_data_field, list);
  if (source_restored && pending_restored) custom_presentations = false;
  if (!regenerated || !source_restored || !pending_restored) {
    view_failed = true;
    spdlog::warn("[OfficerPresetReorder] canonical restoration incomplete; custom ordering suspended");
    return false;
  }
  return true;
}

bool render_local_order(Il2CppObject* controller, bool preserve_scroll)
{
  if (!enabled() || processing || view_failed || active_controller != controller) return false;
  auto* view_context = read_reference(controller, controller_context_field);
  auto* scroller = read_reference(controller, controller_scroller_field);
  Il2CppObject* list = nullptr;
  Il2CppArraySize* backing = nullptr;
  int32_t size = 0;
  if (view_context != active_view_context || !scroller
      || !try_get_preset_list(view_context, &list, &backing, &size)) return false;
  auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing->vector);
  if (!validate_canonical_order(canonical_items, size) || active_presentations.size() != static_cast<size_t>(size)) return false;
  auto view_items = make_view_order(canonical_items, size);
  auto* view_array = il2cpp_array_new(item_context_class, static_cast<il2cpp_array_size_t>(size));
  if (!view_array) return false;
  float scroll = 0.0f;
  const bool have_scroll = preserve_scroll && read_scroll(scroller, scroll);
  const auto epoch = view_epoch;
  custom_presentations = true; // retain the restoration snapshot before the first mutation
  auto** slots = reinterpret_cast<void**>(reinterpret_cast<Il2CppArraySize*>(view_array)->vector);
  for (int32_t i = 0; i < size; ++i) {
    auto* context = view_items[i];
    if (context) context->presentation = active_presentations[i];
    il2cpp_gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(view_array), &slots[i], context);
  }
  bool generated = false;
  {
    ProcessingScope scope;
    void* args[]{controller, view_array};
    generated = Il2CppRuntime::TryInvoke(clear_and_generate, scroller, args);
  }
  if (epoch != view_epoch || active_controller != controller || !custom_presentations) return false;
  // A normal return without installing this source is not a completed custom render.
  if (!generated || read_reference(scroller, scroller_data_field) != reinterpret_cast<Il2CppObject*>(view_array)) {
    view_failed = true;
    restore_canonical_view(controller, false);
    return false;
  }
  if (have_scroll && !restore_scroll(scroller, scroll)) {
    view_failed = true;
    restore_canonical_view(controller, true);
    return false;
  }
  return true;
}

bool move_preset(OfficerPresetItemContext* context, int direction)
{
  if (!enabled() || processing || view_failed || !is_reorderable_preset(context)
      || context->officer_presets_view_context != active_view_context
      || active_controller == nullptr || clear_and_generate == nullptr) {
    return false;
  }

  Il2CppObject*    list          = nullptr;
  Il2CppArraySize* backing_items = nullptr;
  int32_t          size          = 0;
  if (!try_get_preset_list(context->officer_presets_view_context, &list, &backing_items, &size)) {
    spdlog::warn("[OfficerPresetReorder] unable to read live preset list");
    return false;
  }

  auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
  if (!validate_canonical_order(canonical_items, size)) {
    return false;
  }
  auto view_items = make_view_order(canonical_items, size);

  int32_t current_index = -1;
  for (int32_t index = 0; index < size; ++index) {
    if (view_items[index] == context) {
      current_index = index;
      break;
    }
  }
  if (current_index < 0) {
    return false;
  }

  int32_t target_index = current_index + direction;
  while (target_index >= 0 && target_index < size && !is_reorderable_preset(view_items[target_index])) {
    target_index += direction;
  }
  if (target_index < 0 || target_index >= size) {
    spdlog::info("[OfficerPresetReorder] slot={} is already at the {}", context->slot_id,
                 direction < 0 ? "top" : "bottom");
    return true;
  }

  auto* scroller = read_reference(active_controller, controller_scroller_field);
  if (scroller == nullptr) {
    spdlog::warn("[OfficerPresetReorder] unable to access the active preset scroller");
    return false;
  }

  auto*      target        = view_items[target_index];
  const auto current_order = std::find(session_order.begin(), session_order.end(), context->slot_id);
  const auto target_order  = std::find(session_order.begin(), session_order.end(), target->slot_id);
  bool       persisted     = false;
  if (current_order != session_order.end() && target_order != session_order.end()) {
    std::iter_swap(current_order, target_order);
    persisted = save_session_order();
  }

  spdlog::info("[OfficerPresetReorder] moved slot={} {} across slot={} ({})", context->slot_id,
               direction < 0 ? "up" : "down", target->slot_id,
               persisted ? "persisted locally" : "local persistence failed");
  if (!render_local_order(active_controller, true)) {
    spdlog::warn("[OfficerPresetReorder] local order changed but the scroller could not be refreshed");
  }
  return true;
}

void OfficerPresetsViewController_OnSaveSlotsSuccess_Hook(auto original, Il2CppObject* _this,
                                                          bool increase_occupied_slots_count)
{
  const auto epoch = view_epoch;
  auto* context = active_view_context;
  const bool owned = active_controller == _this && custom_presentations;
  bool restored = false;
  float scroll = 0.0f;
  bool have_scroll = false;
  try {
    if (owned) {
      auto* scroller = read_reference(_this, controller_scroller_field);
      have_scroll = scroller && read_scroll(scroller, scroll);
      restored = restore_canonical_view(_this, !processing);
    }
  } catch (...) {
    view_failed = true;
    // Keep the snapshot; a later release can still restore it.
  }
  original(_this, increase_occupied_slots_count);
  if (!owned || !restored || !enabled() || processing || view_failed || epoch != view_epoch
      || active_controller != _this || read_reference(_this, controller_context_field) != context) return;
  try {
    Il2CppObject* list = nullptr;
    Il2CppArraySize* backing = nullptr;
    int32_t size = 0;
    if (!try_get_preset_list(context, &list, &backing, &size)
        || !capture_native_presentations(reinterpret_cast<OfficerPresetItemContext**>(backing->vector), size)
        || !render_local_order(_this, false)) return;
    if (have_scroll) restore_scroll(read_reference(_this, controller_scroller_field), scroll);
  } catch (...) {
    view_failed = true;
    restore_canonical_view(_this, false);
  }
}

bool OfficerManager_TryGetPresetItemContext_Hook(auto original, void* _this, Il2CppArraySize** preset_contexts,
                                                 void* view_context)
{
  const bool result = original(_this, preset_contexts, view_context);
  if (!enabled() || processing || !result || !preset_contexts || !*preset_contexts) return result;
  try {
    auto* contexts = *preset_contexts;
    if (!reinterpret_cast<Il2CppObject*>(contexts)->klass || reinterpret_cast<Il2CppObject*>(contexts)->klass->element_class != item_context_class
        || contexts->max_length > static_cast<il2cpp_array_size_t>(std::numeric_limits<int32_t>::max())) return result;
    auto** items = reinterpret_cast<OfficerPresetItemContext**>(contexts->vector);
    if (validate_canonical_order(items, static_cast<int32_t>(contexts->max_length))) {
      ensure_order_loaded();
      remember_slots(items, contexts->max_length);
    }
  } catch (...) {
    spdlog::warn("[OfficerPresetReorder] ignored a failed order observation");
  }
  return result;
}

void OfficerPresetItemWidget_OnEditNameButtonClicked_Hook(auto original, void* _this)
{
  if (!enabled() || processing || view_failed) {
    if (hooks_ready && active_controller && custom_presentations && !processing) {
      try { restore_canonical_view(active_controller, true); } catch (...) { view_failed = true; }
    }
    original(_this);
    return;
  }
  try {
    const bool up = shift_pressed();
    const bool down = control_pressed();
    if (up != down) {
      auto* context = reinterpret_cast<OfficerPresetItemContext*>(read_reference(_this, widget_context_field));
      if (move_preset(context, up ? -1 : 1)) return;
    }
  } catch (...) {
    view_failed = true;
    if (active_controller && custom_presentations) restore_canonical_view(active_controller, false);
  }
  original(_this);
}

void OfficerPresetsViewController_OnDidBindCanvasContext_Hook(auto original, Il2CppObject* _this)
{
  if (hooks_ready && active_controller && custom_presentations) {
    try { restore_canonical_view(active_controller, false); } catch (...) { view_failed = true; }
  }
  original(_this);
  clear_active_view();
  if (!enabled() || processing) return;
  try {
    active_controller = _this;
    active_view_context = read_reference(_this, controller_context_field);
    Il2CppObject* list = nullptr;
    Il2CppArraySize* backing = nullptr;
    int32_t size = 0;
    ensure_order_loaded();
    if (!try_get_preset_list(active_view_context, &list, &backing, &size)
        || !capture_native_presentations(reinterpret_cast<OfficerPresetItemContext**>(backing->vector), size)
        || !render_local_order(_this, false)) view_failed = true;
  } catch (...) {
    view_failed = true;
    restore_canonical_view(_this, false);
  }
}

void OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook(auto original, Il2CppObject* _this)
{
  const bool owned = active_controller == _this;
  const auto epoch = view_epoch;
  if (hooks_ready && owned && custom_presentations) {
    try { restore_canonical_view(_this, !processing); } catch (...) { view_failed = true; }
  }
  original(_this);
  // Teardown is definitive; never clear a different view bound by the native callback.
  if (owned && epoch == view_epoch && active_controller == _this) clear_active_view();
}

const MethodInfo* resolve(Il2CppClass* cls, const char* name, const char* result,
                          std::initializer_list<const char*> parameters)
{
  auto* method = method_contract::Resolve(cls, name, false, result, parameters);
  return method && !method->has_full_generic_sharing_signature ? method : nullptr;
}

const MethodInfo* resolve_manager(Il2CppClass* cls)
{
  const MethodInfo* found = nullptr;
  void* iterator = nullptr;
  while (auto* method = il2cpp_class_get_methods(cls, &iterator)) {
    if (std::strcmp(method->name, "TryGetPresetItemContext") != 0 || !method->methodPointer
        || method->is_generic || method->is_inflated || method->has_full_generic_sharing_signature
        || (method->flags & METHOD_ATTRIBUTE_STATIC) || method->parameters_count != 2
        || !method_contract::Type(method->return_type, "System.Boolean") || !method->parameters
        || !method->parameters[0] || !method->parameters[0]->byref
        || method->parameters[0]->type != IL2CPP_TYPE_SZARRAY
        || !method_contract::Type(method->parameters[1], "Digit.Prime.OfficerPresets.OfficerPresetsViewContext")) continue;
    auto* array_class = il2cpp_class_from_type(method->parameters[0]);
    if (!array_class || array_class->element_class != item_context_class) continue;
    if (found) return nullptr;
    found = method;
  }
  return found;
}
} // namespace

void InstallOfficerPresetReorderHooks()
{
  auto manager = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Officers", "OfficerManager");
  auto widget = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetItemWidget");
  auto controller = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetsViewController");
  auto view = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetsViewContext");
  auto item = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetItemContext");
  auto scroller = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "SmartScrollerBase");
  auto list_interface = il2cpp_get_class_helper("mscorlib", "System.Collections", "IList");
  auto provider = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "IDataContextProvider");
  if (!manager.isValidHelper() || !widget.isValidHelper() || !controller.isValidHelper()
      || !view.isValidHelper() || !item.isValidHelper() || !scroller.isValidHelper()
      || !list_interface.isValidHelper() || !provider.isValidHelper()) {
    ErrorMsg::MissingHelper("OfficerPresetReorder", "required UI surface");
    return;
  }
#ifdef __APPLE__
  if (!il2cpp_field_set_value_object || !il2cpp_field_get_value || !il2cpp_class_enum_basetype) return;
#endif
  item_context_class = item.get_cls();
  view_context_class = view.get_cls();
  scroller_class = scroller.get_cls();
  if (!validate_context_layout(item_context_class)) return;
  widget_context_field = reference_field(widget.get_cls(), "m_context", item_context_class);
  controller_context_field = reference_field(controller.get_cls(), "m_context", view_context_class);
  controller_scroller_field = reference_field(controller.get_cls(), "_smartScroller", scroller_class);
  presets_items_field = reference_field(view_context_class, "PresetsItemsContext", list_interface.get_cls());
  scroller_data_field = reference_field(scroller_class, "_data", list_interface.get_cls());
  scroller_held_data_field = reference_field(scroller_class, "_dataHeldWhilstInitializing", list_interface.get_cls());
  if (!widget_context_field || !controller_context_field || !controller_scroller_field || !presets_items_field
      || !scroller_data_field || !scroller_held_data_field
      || il2cpp_class_from_type(scroller_data_field->type) != list_interface.get_cls()
      || il2cpp_class_from_type(scroller_held_data_field->type) != list_interface.get_cls()
      || !il2cpp_class_is_assignable_from(provider.get_cls(), controller.get_cls())) return;

  clear_and_generate = resolve(scroller_class, "ClearAndGenerateContents", "System.Void",
                                {"Digit.Client.UI.IDataContextProvider", "System.Collections.IList"});
  get_scroll_position = resolve(scroller_class, "get_ScrollPosition", "System.Single", {});
  restore_scroll_position = resolve(scroller_class, "RestoreScrollPosition", "System.Void", {"System.Single"});
  const auto* method = resolve_manager(manager.get_cls());
  const auto* edit = resolve(widget.get_cls(), "OnEditNameButtonClicked", "System.Void", {});
  const auto* bind = resolve(controller.get_cls(), "OnDidBindCanvasContext", "System.Void", {});
  const auto* release = resolve(controller.get_cls(), "OnAboutToReleaseCanvasContext", "System.Void", {});
  const auto* save = resolve(controller.get_cls(), "OnSaveSlotsSuccess", "System.Void", {"System.Boolean"});
  if (!clear_and_generate || !get_scroll_position || !restore_scroll_position
      || !method || !edit || !bind || !release || !save) {
    ErrorMsg::MissingMethod("OfficerPresetReorder", "full signature");
    return;
  }
  const MethodInfo* targets[]{method, edit, bind, release, save};
  for (size_t i = 0; i < std::size(targets); ++i)
    for (size_t j = 0; j < i; ++j)
      if (targets[i]->methodPointer == targets[j]->methodPointer) return;
  const bool manager_ok = SPUD_STATIC_DETOUR(method->methodPointer, OfficerManager_TryGetPresetItemContext_Hook) != nullptr;
  const bool edit_ok = SPUD_STATIC_DETOUR(edit->methodPointer, OfficerPresetItemWidget_OnEditNameButtonClicked_Hook) != nullptr;
  const bool bind_ok = SPUD_STATIC_DETOUR(bind->methodPointer, OfficerPresetsViewController_OnDidBindCanvasContext_Hook) != nullptr;
  const bool release_ok = SPUD_STATIC_DETOUR(release->methodPointer, OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook) != nullptr;
  const bool save_ok = SPUD_STATIC_DETOUR(save->methodPointer, OfficerPresetsViewController_OnSaveSlotsSuccess_Hook) != nullptr;
  hooks_ready = manager_ok && edit_ok && bind_ok && release_ok && save_ok;
  if (!hooks_ready) spdlog::warn("[OfficerPresetReorder] incomplete hook family; callbacks retain native behavior");
}
