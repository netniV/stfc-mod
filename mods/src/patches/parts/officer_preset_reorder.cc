#include "errormsg.h"
#include "file.h"
#include "patches/key.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
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

using ClearAndGenerateContentsFn = void(void*, Il2CppObject*, Il2CppObject*);
using GetScrollPositionFn        = float(void*);
using RestoreScrollPositionFn    = void(void*, float);

std::vector<int64_t>        session_order;
Il2CppObject*               active_controller          = nullptr;
ptrdiff_t                   widget_context_offset      = 0;
ptrdiff_t                   controller_scroller_offset = 0;
ptrdiff_t                   presets_items_offset       = 0;
ClearAndGenerateContentsFn* clear_and_generate         = nullptr;
GetScrollPositionFn*        get_scroll_position        = nullptr;
RestoreScrollPositionFn*    restore_scroll_position    = nullptr;

std::filesystem::path state_path()
{ return std::filesystem::path{File::MakePath("community_patch_state.json", true)}; }

void load_session_order()
{
  const auto    path = state_path();
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file) {
    return;
  }

  try {
    const auto state = nlohmann::json::parse(file);
    const auto order = state.find("officer_preset_order");
    if (order == state.end() || !order->is_array()) {
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
    spdlog::warn("[OfficerPresetReorder] ignored invalid state file '{}': {}", path.string(), error.what());
  }
}

void save_session_order()
{
  const auto     path  = state_path();
  nlohmann::json state = nlohmann::json::object();
  try {
    std::ifstream existing(path, std::ios::in | std::ios::binary);
    if (existing) {
      state = nlohmann::json::parse(existing);
      if (!state.is_object()) {
        state = nlohmann::json::object();
      }
    }
  } catch (const std::exception& error) {
    spdlog::warn("[OfficerPresetReorder] replacing invalid state file '{}': {}", path.string(), error.what());
    state = nlohmann::json::object();
  }

  auto order = nlohmann::json::array();
  for (const auto slot_id : session_order) {
    order.push_back(std::to_string(slot_id));
  }
  state["version"]              = 1;
  state["officer_preset_order"] = std::move(order);

  std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!file) {
    spdlog::warn("[OfficerPresetReorder] unable to write state file '{}'", path.string());
    return;
  }
  file << state.dump(2) << '\n';
}

static_assert(offsetof(OfficerPresetItemContext, presentation) == 0x18);
static_assert(offsetof(OfficerPresetItemContext, is_occupied) == 0x1D);
static_assert(offsetof(OfficerPresetItemContext, order_id) == 0x20);
static_assert(offsetof(OfficerPresetItemContext, slot_id) == 0x28);
static_assert(offsetof(OfficerPresetItemContext, preset_name) == 0x30);

bool is_reorderable_preset(const OfficerPresetItemContext* context)
{
  return context != nullptr && context->slot_id >= 0 && context->order_id >= 0 && context->preset_name != nullptr
         && context->officers != nullptr && reinterpret_cast<Il2CppArraySize*>(context->officers)->max_length > 0;
}

template <typename T> T* read_object_field(void* object, ptrdiff_t offset)
{ return object != nullptr ? *reinterpret_cast<T**>(reinterpret_cast<char*>(object) + offset) : nullptr; }

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

void apply_session_order(OfficerPresetItemContext** items, il2cpp_array_size_t size)
{
  remember_slots(items, size);
  if (session_order.empty()) {
    return;
  }

  std::vector<il2cpp_array_size_t>       occupied_positions;
  std::vector<OfficerPresetItemContext*> occupied_contexts;
  occupied_positions.reserve(size);
  occupied_contexts.reserve(size);
  for (il2cpp_array_size_t index = 0; index < size; ++index) {
    if (is_reorderable_preset(items[index])) {
      occupied_positions.push_back(index);
      occupied_contexts.push_back(items[index]);
    }
  }

  std::stable_sort(occupied_contexts.begin(), occupied_contexts.end(), [](const auto* left, const auto* right) {
    const auto left_order  = std::find(session_order.begin(), session_order.end(), left->slot_id);
    const auto right_order = std::find(session_order.begin(), session_order.end(), right->slot_id);
    return left_order < right_order;
  });

  std::vector<int32_t> presentations;
  presentations.reserve(occupied_positions.size());
  for (const auto position : occupied_positions) {
    presentations.push_back(items[position]->presentation);
  }
  for (size_t index = 0; index < occupied_positions.size(); ++index) {
    items[occupied_positions[index]]       = occupied_contexts[index];
    occupied_contexts[index]->presentation = presentations[index];
  }
}

bool try_get_preset_list(void* view_context, Il2CppObject** list, Il2CppArraySize** backing_items, int32_t* size)
{
  *list = read_object_field<Il2CppObject>(view_context, presets_items_offset);
  if (*list == nullptr) {
    return false;
  }

  auto list_helper = IL2CppClassHelper{(*list)->klass};
  auto items_field = list_helper.GetField("_items");
  auto size_field  = list_helper.GetField("_size");
  if (!items_field.isValidHelper() || !size_field.isValidHelper()) {
    return false;
  }

  *backing_items = read_object_field<Il2CppArraySize>(*list, items_field.offset());
  *size          = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(*list) + size_field.offset());
  return *backing_items != nullptr && *size >= 0
         && static_cast<il2cpp_array_size_t>(*size) <= (*backing_items)->max_length;
}

bool move_preset(OfficerPresetItemContext* context, int direction)
{
  if (!is_reorderable_preset(context) || context->officer_presets_view_context == nullptr
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

  auto**  items         = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
  int32_t current_index = -1;
  for (int32_t index = 0; index < size; ++index) {
    if (items[index] == context) {
      current_index = index;
      break;
    }
  }
  if (current_index < 0) {
    return false;
  }

  int32_t target_index = current_index + direction;
  while (target_index >= 0 && target_index < size && !is_reorderable_preset(items[target_index])) {
    target_index += direction;
  }
  if (target_index < 0 || target_index >= size) {
    spdlog::info("[OfficerPresetReorder] slot={} is already at the {}", context->slot_id,
                 direction < 0 ? "top" : "bottom");
    return true;
  }

  auto* target = items[target_index];
  remember_slots(items, static_cast<il2cpp_array_size_t>(size));
  const auto current_order = std::find(session_order.begin(), session_order.end(), context->slot_id);
  const auto target_order  = std::find(session_order.begin(), session_order.end(), target->slot_id);
  if (current_order != session_order.end() && target_order != session_order.end()) {
    std::iter_swap(current_order, target_order);
    save_session_order();
  }

  std::swap(context->presentation, target->presentation);
  std::swap(items[current_index], items[target_index]);

  auto* scroller = read_object_field<void>(active_controller, controller_scroller_offset);
  if (scroller == nullptr) {
    return false;
  }

  const auto scroll_position = get_scroll_position != nullptr ? get_scroll_position(scroller) : 0.0f;
  spdlog::info("[OfficerPresetReorder] moved slot={} {} across slot={} (persisted locally)", context->slot_id,
               direction < 0 ? "up" : "down", target->slot_id);
  clear_and_generate(scroller, active_controller, list);
  if (restore_scroll_position != nullptr) {
    restore_scroll_position(scroller, scroll_position);
  }
  return true;
}

bool OfficerManager_TryGetPresetItemContext_Hook(auto original, void* _this, Il2CppArraySize** preset_contexts,
                                                 void* view_context)
{
  const bool result = original(_this, preset_contexts, view_context);
  if (!result || preset_contexts == nullptr || *preset_contexts == nullptr) {
    spdlog::info("[OfficerPresetReorder] preset context load returned result={} array={}", result,
                 preset_contexts != nullptr ? static_cast<void*>(*preset_contexts) : nullptr);
    return result;
  }

  auto*  contexts = *preset_contexts;
  auto** items    = reinterpret_cast<OfficerPresetItemContext**>(contexts->vector);
  apply_session_order(items, contexts->max_length);
  spdlog::debug("[OfficerPresetReorder] applied local ordering to {} preset rows", contexts->max_length);

  return result;
}

void OfficerPresetItemWidget_OnEditNameButtonClicked_Hook(auto original, void* _this)
{
  const bool move_up   = Key::HasShift();
  const bool move_down = Key::HasCtrl();
  if (move_up != move_down) {
    auto* context = read_object_field<OfficerPresetItemContext>(_this, widget_context_offset);
    if (move_preset(context, move_up ? -1 : 1)) {
      return;
    }
  }
  original(_this);
}

void OfficerPresetsViewController_OnDidBindCanvasContext_Hook(auto original, Il2CppObject* _this)
{
  original(_this);
  active_controller = _this;
}

void OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook(auto original, Il2CppObject* _this)
{
  if (active_controller == _this) {
    active_controller = nullptr;
  }
  original(_this);
}
} // namespace

void InstallOfficerPresetReorderHooks()
{
  auto helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Officers", "OfficerManager");
  if (!helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.Officers", "OfficerManager");
    return;
  }

  const auto method = helper.GetMethodInfo("TryGetPresetItemContext", 2);
  if (method == nullptr || method->methodPointer == nullptr) {
    ErrorMsg::MissingMethod("OfficerManager", "TryGetPresetItemContext");
    return;
  }

  auto widget_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetItemWidget");
  auto controller_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetsViewController");
  auto view_context_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetsViewContext");
  auto scroller_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "SmartScrollerBase");
  if (!widget_helper.isValidHelper() || !controller_helper.isValidHelper() || !view_context_helper.isValidHelper()
      || !scroller_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.OfficerPresets", "reorder UI surface");
    return;
  }

  auto context_field  = widget_helper.GetField("m_context");
  auto scroller_field = controller_helper.GetField("_smartScroller");
  auto presets_field  = view_context_helper.GetField("PresetsItemsContext");
  if (!context_field.isValidHelper() || !scroller_field.isValidHelper() || !presets_field.isValidHelper()) {
    ErrorMsg::MissingMethod("OfficerPresetReorder", "required field");
    return;
  }
  widget_context_offset      = context_field.offset();
  controller_scroller_offset = scroller_field.offset();
  presets_items_offset       = presets_field.offset();

  const auto clear_method          = scroller_helper.GetMethodInfo("ClearAndGenerateContents", 2);
  const auto get_scroll_method     = scroller_helper.GetMethodInfo("get_ScrollPosition", 0);
  const auto restore_scroll_method = scroller_helper.GetMethodInfo("RestoreScrollPosition", 1);
  const auto edit_method           = widget_helper.GetMethodInfo("OnEditNameButtonClicked", 0);
  const auto bind_method           = controller_helper.GetMethodInfo("OnDidBindCanvasContext", 0);
  const auto release_method        = controller_helper.GetMethodInfo("OnAboutToReleaseCanvasContext", 0);
  if (clear_method == nullptr || clear_method->methodPointer == nullptr || get_scroll_method == nullptr
      || get_scroll_method->methodPointer == nullptr || restore_scroll_method == nullptr
      || restore_scroll_method->methodPointer == nullptr || edit_method == nullptr
      || edit_method->methodPointer == nullptr || bind_method == nullptr || bind_method->methodPointer == nullptr
      || release_method == nullptr || release_method->methodPointer == nullptr) {
    ErrorMsg::MissingMethod("OfficerPresetReorder", "required UI method");
    return;
  }
  clear_and_generate      = reinterpret_cast<ClearAndGenerateContentsFn*>(clear_method->methodPointer);
  get_scroll_position     = reinterpret_cast<GetScrollPositionFn*>(get_scroll_method->methodPointer);
  restore_scroll_position = reinterpret_cast<RestoreScrollPositionFn*>(restore_scroll_method->methodPointer);

  load_session_order();

  SPUD_STATIC_DETOUR(method->methodPointer, OfficerManager_TryGetPresetItemContext_Hook);
  SPUD_STATIC_DETOUR(edit_method->methodPointer, OfficerPresetItemWidget_OnEditNameButtonClicked_Hook);
  SPUD_STATIC_DETOUR(bind_method->methodPointer, OfficerPresetsViewController_OnDidBindCanvasContext_Hook);
  SPUD_STATIC_DETOUR(release_method->methodPointer, OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook);
}
