#include "errormsg.h"
#include "file.h"
#include "prime/KeyCode.h"
#include "str_utils.h"

#include <il2cpp/il2cpp_helper.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spud/detour.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#if !_WIN32
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

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
std::vector<int32_t>        active_presentations;
Il2CppObject*               active_controller          = nullptr;
Il2CppClass*                item_context_class         = nullptr;
ptrdiff_t                   widget_context_offset      = 0;
ptrdiff_t                   controller_context_offset  = 0;
ptrdiff_t                   controller_scroller_offset = 0;
ptrdiff_t                   presets_items_offset       = 0;
ClearAndGenerateContentsFn* clear_and_generate         = nullptr;
GetScrollPositionFn*        get_scroll_position        = nullptr;
RestoreScrollPositionFn*    restore_scroll_position    = nullptr;

std::filesystem::path state_path()
{
  if (File::hasCustomNames()) {
    std::filesystem::path config_path{File::Config()};
    auto                  state_name = config_path.stem();
    state_name += ".state.json";
    return config_path.parent_path() / state_name;
  }
  return std::filesystem::path{File::MakePath("community_patch_state.json", true)};
}

class StateFileLock
{
public:
  explicit StateFileLock(const std::filesystem::path& path)
  {
    lock_path = path;
    lock_path += ".lock";

    for (int attempt = 0; attempt < 50; ++attempt) {
#if _WIN32
      handle = CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
      if (handle != INVALID_HANDLE_VALUE) {
        return;
      }
      const auto error = GetLastError();
      if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION) {
        break;
      }
#else
      descriptor = open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
      if (descriptor < 0) {
        break;
      }
      if (flock(descriptor, LOCK_EX | LOCK_NB) == 0) {
        return;
      }
      const auto error = errno;
      close(descriptor);
      descriptor = -1;
      if (error != EWOULDBLOCK && error != EAGAIN) {
        break;
      }
#endif
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  ~StateFileLock()
  {
#if _WIN32
    if (handle != INVALID_HANDLE_VALUE) {
      CloseHandle(handle);
    }
#else
    if (descriptor >= 0) {
      flock(descriptor, LOCK_UN);
      close(descriptor);
    }
#endif
  }

  StateFileLock(const StateFileLock&)            = delete;
  StateFileLock& operator=(const StateFileLock&) = delete;

  bool acquired() const
  {
#if _WIN32
    return handle != INVALID_HANDLE_VALUE;
#else
    return descriptor >= 0;
#endif
  }

private:
  std::filesystem::path lock_path;
#if _WIN32
  HANDLE handle = INVALID_HANDLE_VALUE;
#else
  int descriptor = -1;
#endif
};

std::filesystem::path temporary_state_path(const std::filesystem::path& path)
{
  static std::atomic_uint64_t sequence{0};
  auto                        temporary_path = path;
#if _WIN32
  const auto process_id = static_cast<uint64_t>(GetCurrentProcessId());
#else
  const auto process_id = static_cast<uint64_t>(getpid());
#endif
  temporary_path += "." + std::to_string(process_id) + "." + std::to_string(++sequence) + ".tmp";
  return temporary_path;
}

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

bool replace_state_file(const std::filesystem::path& temporary_path, const std::filesystem::path& path)
{
#if _WIN32
  if (MoveFileExW(temporary_path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
    return true;
  }
  spdlog::warn("[OfficerPresetReorder] unable to replace state file '{}' (Windows error {})", path.string(),
               GetLastError());
#else
  std::error_code error;
  std::filesystem::rename(temporary_path, path, error);
  if (!error) {
    return true;
  }
  spdlog::warn("[OfficerPresetReorder] unable to replace state file '{}': {}", path.string(), error.message());
#endif
  std::error_code cleanup_error;
  std::filesystem::remove(temporary_path, cleanup_error);
  return false;
}

bool save_session_order()
{
  const auto    path = state_path();
  StateFileLock lock(path);
  if (!lock.acquired()) {
    spdlog::warn("[OfficerPresetReorder] unable to lock state file '{}'", path.string());
    return false;
  }

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

  const auto    temporary_path = temporary_state_path(path);
  std::ofstream file(temporary_path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!file) {
    spdlog::warn("[OfficerPresetReorder] unable to open temporary state file '{}'", temporary_path.string());
    return false;
  }
  file << state.dump(2) << '\n';
  file.flush();
  const bool write_succeeded = file.good();
  file.close();
  if (!write_succeeded || file.fail()) {
    spdlog::warn("[OfficerPresetReorder] unable to finish temporary state file '{}'", temporary_path.string());
    std::error_code cleanup_error;
    std::filesystem::remove(temporary_path, cleanup_error);
    return false;
  }
  return replace_state_file(temporary_path, path);
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
  if (field == nullptr || field->type == nullptr) {
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
  return context_class != nullptr
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
  return context != nullptr && context->slot_id >= 0 && context->order_id >= 0 && context->preset_name != nullptr
         && context->officers != nullptr && reinterpret_cast<Il2CppArraySize*>(context->officers)->max_length > 0;
}

template <typename T> T* read_object_field(void* object, ptrdiff_t offset)
{ return object != nullptr ? *reinterpret_cast<T**>(reinterpret_cast<char*>(object) + offset) : nullptr; }

bool key_pressed(KeyCode key)
{
  static auto get_key = il2cpp_resolve_icall_typed<bool(KeyCode)>("UnityEngine.Input::GetKeyInt(UnityEngine.KeyCode)");
  return get_key != nullptr && get_key(key);
}

bool shift_pressed()
{ return key_pressed(KeyCode::LeftShift) || key_pressed(KeyCode::RightShift); }

bool control_pressed()
{ return key_pressed(KeyCode::LeftControl) || key_pressed(KeyCode::RightControl); }

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
{ return context != nullptr && context->slot_id >= 0 && context->order_id >= 0; }

bool validate_unique_preset_identity(OfficerPresetItemContext** items, int32_t size)
{
  std::vector<int64_t> slot_ids;
  std::vector<int32_t> order_ids;
  slot_ids.reserve(size);
  order_ids.reserve(size);
  for (int32_t index = 0; index < size; ++index) {
    const auto* context = items[index];
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

bool render_local_order(Il2CppObject* controller, bool preserve_scroll)
{
  if (controller == nullptr || item_context_class == nullptr || clear_and_generate == nullptr) {
    return false;
  }

  auto*            view_context   = read_object_field<void>(controller, controller_context_offset);
  auto*            scroller       = read_object_field<void>(controller, controller_scroller_offset);
  Il2CppObject*    canonical_list = nullptr;
  Il2CppArraySize* backing_items  = nullptr;
  int32_t          size           = 0;
  if (scroller == nullptr || !try_get_preset_list(view_context, &canonical_list, &backing_items, &size)) {
    return false;
  }

  auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
  if (!validate_canonical_order(canonical_items, size) || active_presentations.size() != static_cast<size_t>(size)) {
    return false;
  }

  auto  view_items = make_view_order(canonical_items, size);
  auto* view_array = il2cpp_array_new(item_context_class, static_cast<il2cpp_array_size_t>(size));
  if (view_array == nullptr) {
    spdlog::warn("[OfficerPresetReorder] unable to allocate the scroller view array");
    return false;
  }
  auto** view_slots = reinterpret_cast<void**>(reinterpret_cast<Il2CppArraySize*>(view_array)->vector);
  for (int32_t index = 0; index < size; ++index) {
    auto* context = view_items[index];
    if (context != nullptr) {
      context->presentation = active_presentations[index];
    }
    il2cpp_gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(view_array), &view_slots[index], context);
  }

  const auto scroll_position = preserve_scroll && get_scroll_position != nullptr ? get_scroll_position(scroller) : 0.0f;
  clear_and_generate(scroller, controller, reinterpret_cast<Il2CppObject*>(view_array));
  if (preserve_scroll && restore_scroll_position != nullptr) {
    restore_scroll_position(scroller, scroll_position);
  }
  return true;
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

  auto* scroller = read_object_field<void>(active_controller, controller_scroller_offset);
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
  auto* view_context = read_object_field<void>(_this, controller_context_offset);
  auto* scroller     = read_object_field<void>(_this, controller_scroller_offset);

  Il2CppObject*    canonical_list = nullptr;
  Il2CppArraySize* backing_items  = nullptr;
  int32_t          size           = 0;
  const bool       canonical_available =
      scroller != nullptr && try_get_preset_list(view_context, &canonical_list, &backing_items, &size)
      && validate_canonical_order(reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector), size);
  const bool can_rerender =
      canonical_available && active_controller == _this
      && restore_native_presentations(reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector), size);
  const auto scroll_position = can_rerender && get_scroll_position != nullptr ? get_scroll_position(scroller) : 0.0f;
  if (canonical_available) {
    // Scopely's callback indexes SmartScroller._data by OrderId. Give it the canonical list for the duration of the
    // callback, then rebuild our separate presentation-only view.
    clear_and_generate(scroller, _this, canonical_list);
  }

  original(_this, increase_occupied_slots_count);

  if (!canonical_available) {
    spdlog::warn("[OfficerPresetReorder] save succeeded without an available canonical scroller source");
    return;
  }
  if (!can_rerender) {
    spdlog::debug("[OfficerPresetReorder] save succeeded in canonical order without an active local view");
    return;
  }

  if (!try_get_preset_list(read_object_field<void>(_this, controller_context_offset), &canonical_list, &backing_items,
                           &size)) {
    spdlog::warn("[OfficerPresetReorder] save succeeded but the canonical preset list became unavailable");
    return;
  }

  auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
  if (!validate_canonical_order(canonical_items, size)) {
    spdlog::error("[OfficerPresetReorder] canonical preset invariant failed after save; local ordering was not "
                  "reapplied");
    return;
  }
  if (active_presentations.size() != static_cast<size_t>(size)) {
    if (!capture_native_presentations(canonical_items, size)) {
      return;
    }
  }
  if (!render_local_order(_this, false)) {
    spdlog::warn("[OfficerPresetReorder] save reconciled safely but the local scroller order could not be restored");
    return;
  }
  if (restore_scroll_position != nullptr) {
    restore_scroll_position(scroller, scroll_position);
  }
  spdlog::info("[OfficerPresetReorder] reconciled presentation-only ordering after preset save");
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
  if (validate_canonical_order(items, static_cast<int32_t>(contexts->max_length))) {
    remember_slots(items, contexts->max_length);
    spdlog::debug("[OfficerPresetReorder] observed {} canonical preset rows", contexts->max_length);
  } else {
    spdlog::error("[OfficerPresetReorder] Scopely returned non-canonical preset rows; leaving them untouched");
  }

  return result;
}

void OfficerPresetItemWidget_OnEditNameButtonClicked_Hook(auto original, void* _this)
{
  const bool move_up   = shift_pressed();
  const bool move_down = control_pressed();
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

  auto*            view_context   = read_object_field<void>(_this, controller_context_offset);
  Il2CppObject*    canonical_list = nullptr;
  Il2CppArraySize* backing_items  = nullptr;
  int32_t          size           = 0;
  if (!try_get_preset_list(view_context, &canonical_list, &backing_items, &size)) {
    active_presentations.clear();
    spdlog::warn("[OfficerPresetReorder] unable to capture the bound canonical preset list");
    return;
  }

  auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
  if (!capture_native_presentations(canonical_items, size) || !render_local_order(_this, false)) {
    active_presentations.clear();
    spdlog::warn("[OfficerPresetReorder] local ordering was disabled for the bound preset view");
  }
}

void OfficerPresetsViewController_OnAboutToReleaseCanvasContext_Hook(auto original, Il2CppObject* _this)
{
  if (active_controller == _this) {
    auto*            view_context   = read_object_field<void>(_this, controller_context_offset);
    auto*            scroller       = read_object_field<void>(_this, controller_scroller_offset);
    Il2CppObject*    canonical_list = nullptr;
    Il2CppArraySize* backing_items  = nullptr;
    int32_t          size           = 0;
    if (scroller != nullptr && try_get_preset_list(view_context, &canonical_list, &backing_items, &size)) {
      auto** canonical_items = reinterpret_cast<OfficerPresetItemContext**>(backing_items->vector);
      if (validate_canonical_order(canonical_items, size)) {
        restore_native_presentations(canonical_items, size);
        clear_and_generate(scroller, _this, canonical_list);
      }
    }
    active_controller = nullptr;
    active_presentations.clear();
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
  auto item_context_helper =
      il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetItemContext");
  auto scroller_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "SmartScrollerBase");
  if (!widget_helper.isValidHelper() || !controller_helper.isValidHelper() || !view_context_helper.isValidHelper()
      || !item_context_helper.isValidHelper() || !scroller_helper.isValidHelper()) {
    ErrorMsg::MissingHelper("Digit.Prime.OfficerPresets", "reorder UI surface");
    return;
  }
  if (!validate_context_layout(item_context_helper.get_cls())) {
    return;
  }
  item_context_class = item_context_helper.get_cls();

  auto context_field            = widget_helper.GetField("m_context");
  auto controller_context_field = controller_helper.GetField("m_context");
  auto scroller_field           = controller_helper.GetField("_smartScroller");
  auto presets_field            = view_context_helper.GetField("PresetsItemsContext");
  if (!context_field.isValidHelper() || !controller_context_field.isValidHelper() || !scroller_field.isValidHelper()
      || !presets_field.isValidHelper()) {
    ErrorMsg::MissingMethod("OfficerPresetReorder", "required field");
    return;
  }
  widget_context_offset      = context_field.offset();
  controller_context_offset  = controller_context_field.offset();
  controller_scroller_offset = scroller_field.offset();
  presets_items_offset       = presets_field.offset();

  const auto clear_method          = scroller_helper.GetMethodInfo("ClearAndGenerateContents", 2);
  const auto get_scroll_method     = scroller_helper.GetMethodInfo("get_ScrollPosition", 0);
  const auto restore_scroll_method = scroller_helper.GetMethodInfo("RestoreScrollPosition", 1);
  const auto edit_method           = widget_helper.GetMethodInfo("OnEditNameButtonClicked", 0);
  const auto bind_method           = controller_helper.GetMethodInfo("OnDidBindCanvasContext", 0);
  const auto release_method        = controller_helper.GetMethodInfo("OnAboutToReleaseCanvasContext", 0);
  const auto save_success_method   = controller_helper.GetMethodInfo("OnSaveSlotsSuccess", 1);
  if (clear_method == nullptr || clear_method->methodPointer == nullptr || get_scroll_method == nullptr
      || get_scroll_method->methodPointer == nullptr || restore_scroll_method == nullptr
      || restore_scroll_method->methodPointer == nullptr || edit_method == nullptr
      || edit_method->methodPointer == nullptr || bind_method == nullptr || bind_method->methodPointer == nullptr
      || release_method == nullptr || release_method->methodPointer == nullptr || save_success_method == nullptr
      || save_success_method->methodPointer == nullptr) {
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
  SPUD_STATIC_DETOUR(save_success_method->methodPointer, OfficerPresetsViewController_OnSaveSlotsSuccess_Hook);
}
