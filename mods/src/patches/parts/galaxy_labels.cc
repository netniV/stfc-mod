#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <spdlog/spdlog.h>
#include <spud/detour.h>
#include "galaxy_labels.h"
#include "galaxy_policy.h"
#include "config.h"
#include "settings/galaxy_labels.h"
#include "patches/native_hook_extent.h"

// Galaxy label composition and zoom profiles. Native layout and visibility
// bindings are validated before installation; macOS also checks loaded native entries.
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
#include <array>
#include <cstring>
#include <optional>

#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <prime/NavigationZoom.h>
#include <prime/NavigationLOD.h>

namespace
{
using namespace galaxy_controls;

constexpr float hostile_icon_gap = 10.f;
constexpr float system_marker_width = 36.f;

class GalaxyRuntime {
private:
  GalaxyRuntime() = default;
  // Native restoration is explicit at game lifecycle boundaries. Do not call
  // IL2CPP from static destruction after the game runtime has shut down.
  bool ready = false, multi = false;
  FieldInfo* mode_field = nullptr;
  void* star_type = nullptr;
  void* horizontal_type = nullptr;
  void* element_type = nullptr;
  const MethodInfo* get_component = nullptr;
  const MethodInfo* add_component = nullptr;
  const MethodInfo* resource_update = nullptr;
  const MethodInfo* hostile_update = nullptr;
  void (*update_name)(void*, bool, bool, bool, bool) = nullptr;
  void* batching = nullptr;
  std::array<bool, 4> combined{};
  bool saw_name = false;
  void* releasing = nullptr;
  bool cleanup_layouts = false;
  bool restoring_layout = false;
  std::array<ZoomProfile, 2> profiles{}; // major, minor
  bool galaxy_zoom_valid = false, refresh_profiles = false, refreshing = false;
  float galaxy_zoom = 0.f;
  int galaxy_level = 0;
  void* galaxy_camera = nullptr;
  const MethodInfo* world_instance = nullptr;
  const MethodInfo* is_minor = nullptr;
  using Tracked = std::unordered_map<void*, Il2CppGCHandle>;
  Tracked stars, lods, handlers, filters;
  int Mode()
  {
    int value = 0;
    il2cpp_field_static_get_value(mode_field, &value);
    return value;
  }
  OverlaySelection Selection()
  {
    return mod_settings::GalaxyOverlaySelection();
  }
  void* Call(void* object, const char* name, int count = 0, void** args = nullptr)
  {
    if (!object) return nullptr;
    auto* obj = static_cast<Il2CppObject*>(object);
    auto* method = il2cpp_class_get_method_from_name(obj->klass, name, count);
    if (!method) return nullptr;
    Il2CppException* error = nullptr;
    auto* result = il2cpp_runtime_invoke(method, object, args, &error);
    return error ? nullptr : result;
  }
  void* Field(void* widget, std::size_t offset)
  { return *reinterpret_cast<void**>(static_cast<char*>(widget) + offset); }
  bool Active(void* object)
  {
    auto* result = Call(object, "get_activeSelf");
    return result && *static_cast<bool*>(il2cpp_object_unbox(static_cast<Il2CppObject*>(result)));
  }
  void SetActive(void* object, bool value)
  {
    if (!object || Active(object) == value) return;
    void* args[]{&value};
    Call(object, "SetActive", 1, args);
  }

  struct Position { float x, y; };
  struct Shift {
    Il2CppGCHandle transform; Position before; Il2CppGCHandle parent = 0; int sibling = 0;
    Position anchor_min{}, anchor_max{}, size{}, pivot{};
    Il2CppGCHandle layout = 0; float spacing = 0;
    Il2CppGCHandle element = 0; float min_width = -1, preferred_width = -1; bool ignore_layout = false;
  };
  struct Layout {
    Il2CppGCHandle owner = 0;
    std::vector<Shift> shifts;
    bool restoration_warned = false;
  };
  std::unordered_map<void*, Layout> layouts;
  void ReleaseShift(const Shift& shift)
  {
    for (auto handle : {shift.transform, shift.parent, shift.layout, shift.element})
      if (handle) il2cpp_gchandle_free(handle);
  }
  bool HasShift(void* widget, void* transform)
  {
    const auto it = layouts.find(widget);
    if (it == layouts.end()) return false;
    // Do not adopt snapshots from a collected owner whose address was reused.
    if (il2cpp_gchandle_get_target(it->second.owner) != widget) return true;
    for (const auto& shift : it->second.shifts) {
      if (il2cpp_gchandle_get_target(shift.transform) == transform) return true;
    }
    return false;
  }
  bool SaveShift(void* widget, Shift shift)
  {
    auto it = layouts.find(widget);
    if (it == layouts.end()) {
      const auto owner = il2cpp_gchandle_new_weakref(static_cast<Il2CppObject*>(widget), false);
      if (!owner) { ReleaseShift(shift); return false; }
      it = layouts.emplace(widget, Layout{owner, {}}).first;
    }
    if (il2cpp_gchandle_get_target(it->second.owner) != widget) {
      ReleaseShift(shift);
      return false;
    }
    it->second.shifts.push_back(shift);
    return true;
  }
  void* HostileTextTransform(void* widget)
  { return Call(Call(Field(widget, 0x90), "get_Target"), "get_transform"); }
  bool PositionOf(void* transform, Position& value, const char* property = "get_anchoredPosition")
  {
    auto* boxed = static_cast<Il2CppObject*>(Call(transform, property));
    if (!boxed || std::strcmp(il2cpp_class_get_name(boxed->klass), "Vector2")) return false;
    value = *static_cast<Position*>(il2cpp_object_unbox(boxed));
    return std::isfinite(value.x) && std::isfinite(value.y);
  }
  bool Move(void* transform, Position value, const char* property = "set_anchoredPosition")
  {
    auto* method = il2cpp_class_get_method_from_name(static_cast<Il2CppObject*>(transform)->klass,
                                                    property, 1);
    if (!method) return false;
    void* args[]{&value};
    Il2CppException* error = nullptr;
    il2cpp_runtime_invoke(method, transform, args, &error);
    return !error;
  }
  bool Parent(void* transform, void* parent)
  {
    if (!transform || !parent) return false;
    auto* method = il2cpp_class_get_method_from_name(static_cast<Il2CppObject*>(transform)->klass, "SetParent", 2);
    if (!method) return false;
    bool world = false;
    void* args[]{parent, &world};
    Il2CppException* error = nullptr;
    il2cpp_runtime_invoke(method, transform, args, &error);
    return !error;
  }
  template<class T> bool ReadScalar(void* object, const char* getter, const char* type, T& value)
  {
    auto* result = static_cast<Il2CppObject*>(Call(object, getter));
    if (!result || std::strcmp(il2cpp_class_get_name(result->klass), type)) return false;
    value = *static_cast<T*>(il2cpp_object_unbox(result));
    return true;
  }
  template<class T> bool WriteScalar(void* object, const char* setter, T value)
  {
    if (!object) return false;
    auto* method = il2cpp_class_get_method_from_name(static_cast<Il2CppObject*>(object)->klass, setter, 1);
    if (!method) return false;
    void* args[]{&value};
    Il2CppException* error = nullptr;
    il2cpp_runtime_invoke(method, object, args, &error);
    return !error;
  }

  void Track(Tracked& objects, void* object)
  {
    if (!object || refreshing) return;
    if (auto it = objects.find(object); it != objects.end()) {
      if (il2cpp_gchandle_get_target(it->second) == object) return;
      il2cpp_gchandle_free(it->second);
      objects.erase(it);
    }
    if (auto handle = il2cpp_gchandle_new_weakref(static_cast<Il2CppObject*>(object), false))
      objects.emplace(object, handle);
  }

  void Prune(Tracked& objects)
  {
    for (auto it = objects.begin(); it != objects.end();) {
      if (!il2cpp_gchandle_get_target(it->second)) {
        il2cpp_gchandle_free(it->second);
        it = objects.erase(it);
      } else ++it;
    }
  }

  void* StarFor(void* component)
  {
    if (!component) return nullptr;
    void* args[]{star_type};
    Il2CppException* error = nullptr;
    auto* star = il2cpp_runtime_invoke(get_component, component, args, &error);
    return !error && star && Field(star, 0x50) ? star : nullptr;
  }

  bool Minor(void* star, bool& minor)
  {
    if (!star || !Field(star, 0x50) || !world_instance || !is_minor) return false;
    Il2CppException* error = nullptr;
    auto* world = il2cpp_runtime_invoke(world_instance, nullptr, nullptr, &error);
    if (error || !world) return false;
    // GalaxyNode is an inline value at Widget<GalaxyNode>.m_context, not an object.
    auto* node = static_cast<char*>(star) + 0x50;
    void* args[]{node};
    auto* result = il2cpp_runtime_invoke(is_minor, world, args, &error);
    if (error || !result || std::strcmp(il2cpp_class_get_name(result->klass), "Boolean")) return false;
    minor = *static_cast<bool*>(il2cpp_object_unbox(result));
    return true;
  }

  int Effective(void* star, int native)
  {
    auto choice = ResolveDetail(profiles[0], galaxy_zoom_valid, galaxy_zoom);
    const auto minor_choice = ResolveDetail(profiles[1], galaxy_zoom_valid, galaxy_zoom);
    bool minor = false;
    // Identical decisions need no per-widget managed classification call.
    if (choice != minor_choice) {
      if (!Minor(star, minor)) return native;
      if (minor) choice = minor_choice;
    }
    return choice == Detail::Expanded ? 1 : choice == Detail::Compact ? 3 : galaxy_zoom_valid ? galaxy_level : native;
  }

  void Filter(auto original, void* filter)
  {
    if (!ready) { original(filter); return; }
    Track(filters, filter);
    // RebuildCullingGroup inlines ShouldFilter. Its only current-level check is
    // the Far/minor exclusion; leave the camera and other culling inputs intact.
    auto* current = reinterpret_cast<int*>(static_cast<char*>(filter) + 0x7c);
    struct Restore { int* field; int value; ~Restore() { *field = value; } } restore{current, *current};
    const bool expanded = galaxy_zoom_valid
                          && ResolveDetail(profiles[1], true, galaxy_zoom) == Detail::Expanded;
    if (expanded && *current == 3) *current = 2;
    original(filter);
  }
  bool TryRestoreShifts(Layout& layout)
  {
    struct Scope {
      bool& flag;
      bool previous;
      ~Scope() { flag = previous; }
    } scope{restoring_layout, restoring_layout};
    restoring_layout = true;
    auto& shifts = layout.shifts;
    // Undo hierarchy edits in reverse order so sibling indices remain valid.
    // Stop at a failed restoration so earlier snapshots retain their ordering.
    while (!shifts.empty()) {

      const auto& shift = shifts.back();
      auto* target = il2cpp_gchandle_get_target(shift.transform);
      if (shift.element) {
        if (auto* element = il2cpp_gchandle_get_target(shift.element)) {
          if (!WriteScalar(element, "set_minWidth", shift.min_width)
              || !WriteScalar(element, "set_preferredWidth", shift.preferred_width)
              || !WriteScalar(element, "set_ignoreLayout", shift.ignore_layout)) return false;
        }
      }
      if (shift.layout) {
        if (auto* group = il2cpp_gchandle_get_target(shift.layout)) {
          if (!WriteScalar(group, "set_spacing", shift.spacing)) return false;
        }
      }
      if (target && shift.parent) {
        auto* parent = il2cpp_gchandle_get_target(shift.parent);
        if (!parent || !Parent(target, parent)) return false;
        if (!Move(target, shift.anchor_min, "set_anchorMin") || !Move(target, shift.anchor_max, "set_anchorMax")
            || !Move(target, shift.size, "set_sizeDelta") || !Move(target, shift.pivot, "set_pivot")
            || !WriteScalar(target, "SetSiblingIndex", shift.sibling)) return false;
      }
      if (target && !Move(target, shift.before)) return false;
      ReleaseShift(shift);
      shifts.pop_back();
    }
    return true;
  }
  bool RestoreShifts(Layout& layout)
  {
    if (TryRestoreShifts(layout)) return true;
    if (!layout.restoration_warned) {
      layout.restoration_warned = true;
      spdlog::warn("[GalaxyLabels] Layout restoration incomplete; retaining state for retry");
    }
    return false;
  }
  bool RestoreLayout(void* widget)
  {

    auto it = layouts.find(widget);
    if (it == layouts.end()) return true;
    if (!RestoreShifts(it->second)) return false;
    il2cpp_gchandle_free(it->second.owner);
    layouts.erase(it);
    return true;
  }
  void PruneLayouts()
  {
    // Sweep only at profile refreshes, not once per widget. Restore any surviving
    // children before discarding a collected owner's record.
    for (auto it = layouts.begin(); it != layouts.end();) {
      if (!il2cpp_gchandle_get_target(it->second.owner) && RestoreShifts(it->second)) {
        il2cpp_gchandle_free(it->second.owner);
        it = layouts.erase(it);
      } else ++it;
    }
  }

  void RestoreAllLayouts()
  {
    // A disable transition visits inactive widgets too. Ordinary Data callbacks
    // still restore only their own record; this is not a per-frame sweep.
    for (auto it = layouts.begin(); it != layouts.end();) {
      if (RestoreShifts(it->second)) {
        il2cpp_gchandle_free(it->second.owner);
        it = layouts.erase(it);
      } else {

        ++it; // Keep failed snapshots for the next Data/bind/release attempt.
      }
    }
  }

  void Bind(auto original, void* widget)
  {
    if (!ready || restoring_layout) { original(widget); return; }


    // Native binding populates the hostile list. Remove our text child before
    // that work, including a retry if restoration failed during release.
    RestoreLayout(widget);
    original(widget);
    Track(stars, widget);
  }

  void Release(auto original, void* widget)
  {
    if (!ready || restoring_layout) { original(widget); return; }


    // Reparenting and native release can synchronously trigger callbacks. Do not
    // allow Data to recreate a custom layout while this widget is being released.
    struct Scope {
      void*& current;
      void* previous;
      ~Scope() { current = previous; }
    } scope{releasing, releasing};
    releasing = widget;
    RestoreLayout(widget);
    original(widget);
    // Refresh iterators may be live in a synchronous native callback. Keep the
    // weak entry in that case; refresh already rejects widgets without context.
    if (!refreshing) {
      if (auto it = stars.find(widget); it != stars.end()) {
        il2cpp_gchandle_free(it->second);
        stars.erase(it);
      }
    }
  }

  void TextLayout(void* widget, bool names, bool level)
  {
    auto* hostile = Call(Field(widget, 0x90), "get_gameObject");
    const bool visible = Active(hostile);
    if (names) {
      // Let native localization populate the normal name, then retain the
      // separately populated hostile range from the native hostile pass.
      update_name(widget, false, false, level, false);
      SetActive(hostile, visible);
    }
    if (!visible) return;
    auto* transform = static_cast<Il2CppObject*>(HostileTextTransform(widget));
    Position before{};
    if (!transform || !PositionOf(transform, before)) return;
    if (HasShift(widget, transform)) return;
    const auto handle = il2cpp_gchandle_new_weakref(transform, false);
    if (!handle) return;
    auto* parent = static_cast<Il2CppObject*>(Call(transform, "get_parent"));
    // Join the list's content row, not its outer container: that lets native
    // layout place the range after every populated hostile icon.
    auto* list = Field(widget, 0xf8);
    auto* row = list ? Field(list, 0x28) : nullptr;
    auto* boxed = static_cast<Il2CppObject*>(Call(transform, "GetSiblingIndex"));
    const auto parent_handle = parent ? il2cpp_gchandle_new_weakref(parent, false) : 0;
    if (!parent_handle || !row || !boxed) {
      il2cpp_gchandle_free(handle);
      if (parent_handle) il2cpp_gchandle_free(parent_handle);
      return;
    }
    const int sibling = *static_cast<int*>(il2cpp_object_unbox(boxed));
    Shift shift{handle, before, parent_handle, sibling};
    if (!PositionOf(transform, shift.anchor_min, "get_anchorMin")
        || !PositionOf(transform, shift.anchor_max, "get_anchorMax")
        || !PositionOf(transform, shift.size, "get_sizeDelta") || !PositionOf(transform, shift.pivot, "get_pivot")) {
      il2cpp_gchandle_free(handle);
      il2cpp_gchandle_free(parent_handle);
      return;
    }
    void* args[]{horizontal_type};
    Il2CppException* error = nullptr;
    auto* layout = horizontal_type ? il2cpp_runtime_invoke(get_component, row, args, &error) : nullptr;
    auto* spacing = !error && layout ? static_cast<Il2CppObject*>(Call(layout, "get_spacing")) : nullptr;
    if (spacing && !std::strcmp(il2cpp_class_get_name(spacing->klass), "Single")) {
      shift.spacing = *static_cast<float*>(il2cpp_object_unbox(spacing));
      if (std::isfinite(shift.spacing)) shift.layout = il2cpp_gchandle_new_weakref(layout, false);
    }
    if (!shift.layout) {
      il2cpp_gchandle_free(handle);
      il2cpp_gchandle_free(parent_handle);
      return;
    }
    // The native label can stretch across its old parent with a zero sizeDelta.
    // Once left-anchored inside the icon row it needs its own text-sized rect.
    auto* graphic = Call(Field(widget, 0x90), "get_Target");
    float width = 0, height = 0;
    if (!ReadScalar(graphic, "get_preferredWidth", "Single", width)
        || !ReadScalar(graphic, "get_preferredHeight", "Single", height)
        || !std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) {
      il2cpp_gchandle_free(shift.layout);
      il2cpp_gchandle_free(handle);
      il2cpp_gchandle_free(parent_handle);
      return;
    }
    if (!SaveShift(widget, shift)) return;
    if (Parent(transform, row)) {
      if (shift.layout) {
        float gap = std::max(shift.spacing, hostile_icon_gap);
        void* spacing_args[]{&gap};
        Call(layout, "set_spacing", 1, spacing_args);
      }
      // Keep a usable rect when the icon row does not control child dimensions.
      Move(transform, {0, .5f}, "set_anchorMin");
      Move(transform, {0, .5f}, "set_anchorMax");
      Move(transform, {0, .5f}, "set_pivot");
      Move(transform, {width, height}, "set_sizeDelta");
      Call(transform, "SetAsLastSibling");
      // The content layout owns the position; do not assume one icon's width.
    }
  }
  void IconLayout(void* widget, std::size_t field)
  {
    if (!Active(Field(widget, field == 0x110 ? 0x118 : field))) return;
    auto* transform = static_cast<Il2CppObject*>(Call(Field(widget, field), "get_transform"));
    auto* name = Call(Call(Field(widget, 0x80), "get_Target"), "get_transform");
    auto* row = Call(name, "get_parent");
    Position before{};
    if (!transform || !name || !row || !PositionOf(transform, before)) return;
    if (HasShift(widget, transform)) return;
    auto* parent = static_cast<Il2CppObject*>(Call(transform, "get_parent"));
    auto* boxed = static_cast<Il2CppObject*>(Call(transform, "GetSiblingIndex"));
    if (!parent || !boxed) return;
    const int sibling = *static_cast<int*>(il2cpp_object_unbox(boxed));
    Shift shift{0, before, 0, sibling};
    if (!PositionOf(transform, shift.anchor_min, "get_anchorMin")
        || !PositionOf(transform, shift.anchor_max, "get_anchorMax")
        || !PositionOf(transform, shift.size, "get_sizeDelta") || !PositionOf(transform, shift.pivot, "get_pivot")) return;
    if (!element_type || !add_component) return;
    void* type_args[]{element_type};
    Il2CppException* error = nullptr;
    auto* element = il2cpp_runtime_invoke(get_component, transform, type_args, &error);
    if (error) return;
    if (!element) {
      auto* object = Call(transform, "get_gameObject");
      if (!object) return;
      element = il2cpp_runtime_invoke(add_component, object, type_args, &error);
    }
    if (error || !element || !ReadScalar(element, "get_minWidth", "Single", shift.min_width)
        || !ReadScalar(element, "get_preferredWidth", "Single", shift.preferred_width)
        || !ReadScalar(element, "get_ignoreLayout", "Boolean", shift.ignore_layout)) return;
    if (!std::isfinite(shift.min_width) || !std::isfinite(shift.preferred_width)) return;
    shift.element = il2cpp_gchandle_new_weakref(element, false);
    shift.transform = il2cpp_gchandle_new_weakref(transform, false);
    shift.parent = il2cpp_gchandle_new_weakref(parent, false);
    if (!shift.transform || !shift.parent || !shift.element) {
      if (shift.transform) il2cpp_gchandle_free(shift.transform);
      if (shift.parent) il2cpp_gchandle_free(shift.parent);
      if (shift.element) il2cpp_gchandle_free(shift.element);
      return;
    }
    if (!SaveShift(widget, shift)) return;
    // The widget root may have no intrinsic graphic width. Advertise an icon slot
    // to the row instead of allowing its children to paint over the name.
    WriteScalar(element, "set_ignoreLayout", false);
    WriteScalar(element, "set_minWidth", system_marker_width);
    WriteScalar(element, "set_preferredWidth", system_marker_width);
    if (!Parent(transform, row)) return;
    // Insert into the native eye/name/mining row immediately before its name.
    // Let the row reserve the hazard widget's width instead of using an offset.
    boxed = static_cast<Il2CppObject*>(Call(name, "GetSiblingIndex"));
    if (boxed) {
      int index = *static_cast<int*>(il2cpp_object_unbox(boxed));
      auto* current = static_cast<Il2CppObject*>(Call(transform, "GetSiblingIndex"));
      if (current && *static_cast<int*>(il2cpp_object_unbox(current)) < index) --index;
      void* args[]{&index};
      Call(transform, "SetSiblingIndex", 1, args);
    }
  }
  void Data(auto original, void* widget, int level)
  {
    if (!ready || restoring_layout || releasing == widget) { original(widget, level); return; }
    if (!batching && !RestoreLayout(widget)) {
      // Preserve failed snapshots and avoid layering more edits onto this widget.
      original(widget, level);
      return;
    }
    Track(stars, widget);
    const int effective = Effective(widget, level);
    const auto selection = multi ? Selection() : OverlaySelection::FromNativeMode(Mode());
    if (!multi || !selection.Multiple() || batching) {
      original(widget, effective);
      if (multi && !batching && selection.Contains(2)) TextLayout(widget, false, false);
      if (multi && !batching) {
        IconLayout(widget, 0x1d0);
        if (selection.Contains(3)) IconLayout(widget, 0x110);
      }
      return;
    }
    // Ask each native overlay to apply its own visited/level/hazard gates, then
    // retain only the selected containers' native results. No synthetic data.
    struct Scope {
      GalaxyRuntime& runtime;
      int mode;
      ~Scope() { il2cpp_field_static_set_value(runtime.mode_field, &mode); runtime.batching = nullptr; }
    } scope{*this, Mode()};
    batching = widget;
    combined = {};
    saw_name = false;
    const std::array<void*, 4> objects{Field(widget, 0x100), Field(widget, 0x108), Field(widget, 0x118),
                                       Call(Field(widget, 0xf0), "get_gameObject")};
    std::array<bool, 4> active{};
    bool composed = false;
    if (resource_update && hostile_update) {
      // One native Hazard pass retains its service/platform gates and updates all
      // cached text. In client261 its Name arguments expose has-range and
      // (Near || minor), shared by all three overlay text modes.
      int mode = 3;
      il2cpp_field_static_set_value(mode_field, &mode);
      original(widget, effective);
      if (saw_name && !combined[0] && !combined[1]) {
        const bool overlay_text = combined[3];
        bool range = combined[2], mining = selection.Contains(1) && effective == 1;
        bool hostiles = selection.Contains(2) && effective == 1, exclusive_hostiles = false;
        void* resource_args[]{&mining, &exclusive_hostiles, &range};
        void* hostile_args[]{&hostiles, &range};
        Il2CppException* error = nullptr;
        il2cpp_runtime_invoke(resource_update, widget, resource_args, &error);
        if (!error) il2cpp_runtime_invoke(hostile_update, widget, hostile_args, &error);
        if (!error) {
          // Multi-selection always contains a non-Hostiles layer, whose native
          // pass keeps the shared text container active. The resource helper's
          // exclusive-hostiles flag must therefore be false for this union.
          active[0] = Active(objects[0]);
          active[1] = Active(objects[1]);
          active[2] = selection.Contains(3) && Active(objects[2]);
          active[3] = Active(objects[3]);
          combined = {bool(selection.Contains(1) && overlay_text), bool(selection.Contains(2) && overlay_text),
                      range, bool(selection.Contains(3) && overlay_text)};
          composed = true;
        }
      }
    }
    if (!composed) {
      active = {};
      combined = {};
      saw_name = false;
      for (int mode = 0; mode <= 3; ++mode) {
        if (!selection.Contains(mode)) continue;
        il2cpp_field_static_set_value(mode_field, &mode);
        original(widget, effective);
        // Despite its resource name, this container also parents overlay text.
        active[0] |= Active(objects[0]);
        if (mode >= 2) active[mode - 1] = Active(objects[mode - 1]);
        if (mode == 1) active[3] = Active(objects[3]);
      }
    }
    il2cpp_field_static_set_value(mode_field, &scope.mode);
    batching = nullptr;
    for (std::size_t i = 0; i < objects.size(); ++i) SetActive(objects[i], active[i]);
    // Native text layouts are exclusive even though their icon containers are not.
    // Prefer hostile ranges, then mining level, then hazard text.
    if (saw_name) update_name(widget, combined[0] && !combined[1], combined[1], combined[2],
                              combined[3] && !combined[0] && !combined[1]);
    TextLayout(widget, selection.Contains(0), combined[2]);
    IconLayout(widget, 0x1d0);
    if (selection.Contains(3)) IconLayout(widget, 0x110);
  }
  void Name(auto original, void* widget, bool resources, bool hostiles, bool level, bool hazards)
  {
    if (batching == widget) {
      saw_name = true;
      combined[0] |= resources; combined[1] |= hostiles;
      combined[2] |= level; combined[3] |= hazards;
    }
    original(widget, resources, hostiles, level, hazards);
  }
  void Select(auto original, void* controller, int mode)
  {
    if (!ready) { original(controller, mode); return; }
    if (multi && mode >= 0 && mode <= 3) {
      mod_settings::SetGalaxyOverlay(mode, !Selection().Contains(mode));
      const int representative = Selection().Representative(mode);
      original(controller, representative);
    } else {
      original(controller, mode);
    }
  }
  void Animation(auto original, void* handler, bool normal, bool hostiles, bool mining, bool hazards)
  {
    if (ready && multi) {
      const auto selection = Selection();
      normal = selection.Contains(0); mining = selection.Contains(1);
      hostiles = selection.Contains(2); hazards = selection.Contains(3);
    }
    original(handler, normal, hostiles, mining, hazards);
  }
  void Zoom(auto original, void* handler, int level)
  {
    if (!ready) { original(handler, level); return; }
    // The serialized LOD can live on the widget root rather than this handler.
    auto* star = StarFor(handler);
    if (!star) star = StarFor(Field(handler, 0x40));
    if (star) Track(handlers, handler);
    const int effective = star ? Effective(star, level) : level;
    original(handler, effective);
  }
  static void FilterHook(auto original, void* filter)
  { Instance().Filter(original, filter); }
  static void BindHook(auto original, void* widget)
  { Instance().Bind(original, widget); }
  static void ReleaseHook(auto original, void* widget)
  { Instance().Release(original, widget); }
  static void DataHook(auto original, void* widget, int level)
  { Instance().Data(original, widget, level); }
  static void NameHook(auto original, void* widget, bool resources, bool hostiles, bool level, bool hazards)
  { Instance().Name(original, widget, resources, hostiles, level, hazards); }
  static void SelectHook(auto original, void* controller, int mode)
  { Instance().Select(original, controller, mode); }
  static void AnimationHook(auto original, void* handler, bool normal, bool hostiles, bool mining, bool hazards)
  { Instance().Animation(original, handler, normal, hostiles, mining, hazards); }
  static void ZoomHook(auto original, void* handler, int level)
  { Instance().Zoom(original, handler, level); }
public:
  static GalaxyRuntime& Instance()
  {
    static GalaxyRuntime runtime;
    return runtime;
  }
  GalaxyRuntime(const GalaxyRuntime&) = delete;
  GalaxyRuntime& operator=(const GalaxyRuntime&) = delete;


  bool Available() const { return ready; }
  void ApplySettings()
  {
    const auto& cfg = Config::Get();
    const std::array next_profiles{cfg.galaxy_label_major, cfg.galaxy_label_minor};
    refresh_profiles = true; // Includes overlay-only edits from either UI surface.
    if (multi && !cfg.galaxy_multi_select) cleanup_layouts = true;
    multi = cfg.galaxy_multi_select;
    profiles = next_profiles;
    if (ready && multi) {
      int representative = Selection().Representative(Mode());
      il2cpp_field_static_set_value(mode_field, &representative);
    }
  }

  // Reuse zoom.cc's existing owner. Inspect the component on this live callback;
  // no retained Unity pointers or a second NavigationLOD detour.
  int Lod(void* lod, int level)
  {
    if (!ready) return level;
    if (auto* star = StarFor(lod)) {
      Track(lods, lod);
      return Effective(star, level);
    }
    return level;
  }

  void Frame(void* source)
  {
    if (!ready || refreshing) return;
    auto* zoom = static_cast<NavigationZoom*>(source);
    if (zoom->_depth != NodeDepth::Galaxy) {
      if (galaxy_camera == source) { galaxy_zoom_valid = false; galaxy_camera = nullptr; }
      return;
    }
    const float normalized = zoom->NormalizedZoom;
    const int native = static_cast<int>(zoom->_zoomLevel);
    if (!std::isfinite(normalized) || native < 1 || native > 3) {
      galaxy_zoom_valid = false;
      return;
    }
    const bool fresh = !galaxy_zoom_valid || galaxy_camera != source;
    bool changed = fresh || refresh_profiles || galaxy_level != native;
    for (const auto& profile : profiles)
      changed |= ResolveDetail(profile, galaxy_zoom_valid, galaxy_zoom)
                 != ResolveDetail(profile, true, normalized);
    galaxy_camera = source;
    galaxy_level = native;
    galaxy_zoom = normalized;
    galaxy_zoom_valid = true;
    refresh_profiles = false;
    if (!changed) return;
    refreshing = true;
    struct FinishRefresh {
      GalaxyRuntime& runtime;
      ~FinishRefresh() {
        runtime.refreshing = false;
      }
    } finish{*this};
    if (cleanup_layouts) {
      cleanup_layouts = false;
      RestoreAllLayouts();
    }
    Prune(filters); Prune(lods); Prune(handlers); Prune(stars);
    PruneLayouts();
    // Rebuild the filtered set on threshold changes within a native zoom tier.
    for (auto [object, handle] : filters)
      if (auto* filter = il2cpp_gchandle_get_target(handle))
        *reinterpret_cast<bool*>(reinterpret_cast<char*>(filter) + 0x80) = true;
    const auto active = [this](void* object) {
      bool result = false;
      return ReadScalar(Call(object, "get_gameObject"), "get_activeInHierarchy", "Boolean", result) && result;
    };
    for (auto [object, handle] : lods) {
      auto* lod = il2cpp_gchandle_get_target(handle);
      if (!lod || !active(lod) || !StarFor(lod)) continue;
      // Clear the previously forced target before restoring a Native profile.
      auto* target = reinterpret_cast<NavigationLOD*>(lod);
      target->SetTargetLevel(static_cast<ZoomLevels>(native));
      target->UpdateLOD(static_cast<ZoomLevels>(native));
    }
    void* args[]{&galaxy_level};
    for (auto [object, handle] : handlers)
      if (auto* handler = il2cpp_gchandle_get_target(handle);
          handler && active(handler) && (StarFor(handler) || StarFor(Field(handler, 0x40))))
        Call(handler, "OnZoomChanged", 1, args);
    for (auto [object, handle] : stars)
      if (auto* star = il2cpp_gchandle_get_target(handle); star && active(star) && Field(star, 0x50))
        Call(star, "UpdateStarData", 1, args);
  }

  void Install()
  {
    if (!GalaxyLabelsRequested()) return;
    if (!GalaxyLabelZoomHooksReady()) {
      spdlog::warn("[GalaxyLabels] shared zoom hooks unavailable");
      return;
    }
    auto star = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationStarEntityWidget");
    auto hud = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation.UI", "HudNavigationViewController");
    auto toggle = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation.UI", "GalaxyToggleHandler");
    auto zoom = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoomEventHandler");
    auto director = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationDirector");
    auto component = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Component");
    auto filter = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.Core.Systems", "GalaxyDataFilterSystem");
    auto world = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "GameWorldManager");
    if (!star.isValidHelper() || !hud.isValidHelper() || !toggle.isValidHelper() || !director.isValidHelper()
        || !zoom.isValidHelper() || !filter.isValidHelper() || !world.isValidHelper()
        || !component.isValidHelper()) return;
    world_instance = il2cpp_class_get_method_from_name(il2cpp_class_get_parent(world.get_cls()), "get_Instance", 0);
    is_minor = world.GetMethodInfo("IsMinorNode");
    using method_contract::Resolve;
    using method_contract::Pointer;
    auto should_filter = Pointer(Resolve(filter.get_cls(), "RebuildCullingGroup", false, "System.Void", {}));
    auto* dirty = il2cpp_class_get_field_from_name(filter.get_cls(), "_cullingDirty");
    auto* current = il2cpp_class_get_field_from_name(filter.get_cls(), "_currentLevel");
    auto* context = il2cpp_class_get_field_from_name(star.get_cls(), "m_context");
    if (!world_instance || !is_minor || !dirty || dirty->offset != 0x80 || !current || current->offset != 0x7c
        || !context || context->offset != 0x50) return;
    const auto* data_info = Resolve(star.get_cls(), "UpdateStarData", false, "System.Void",
                                    {"Digit.Prime.Navigation.ZoomLevels"});
    auto data = Pointer(data_info);
    auto name = Pointer(Resolve(star.get_cls(), "UpdateStarName", false, "System.Void",
        {"System.Boolean", "System.Boolean", "System.Boolean", "System.Boolean"}));
    auto bind = Pointer(Resolve(star.get_cls(), "OnDidBindContext", false, "System.Void", {}));
    auto release = Pointer(Resolve(star.get_cls(), "OnAboutToReleaseContext", false, "System.Void", {}));
    // Optional helpers use managed invocation and keep the native multi-pass fallback.
    resource_update = Resolve(star.get_cls(), "UpdateResourcesList", false, "System.Void",
                              {"System.Boolean", "System.Boolean", "System.Boolean"});
    hostile_update = Resolve(star.get_cls(), "UpdateHostilesList", false, "System.Void",
                             {"System.Boolean", "System.Boolean"});
    spdlog::info("[GalaxyLabels] single-pass helpers available={}", resource_update && hostile_update);
    auto select = Pointer(Resolve(hud.get_cls(), "ChangeGalaxyViewInfo", false, "System.Void", {"System.Int32"}));
    auto animation = Pointer(Resolve(toggle.get_cls(), "UpdateSelectedAnimation", false, "System.Void",
        {"System.Boolean", "System.Boolean", "System.Boolean", "System.Boolean"}));
    const auto* zoom_info = Resolve(zoom.get_cls(), "OnZoomChanged", false, "System.Void",
                                    {"Digit.Prime.Navigation.ZoomLevels"});
    auto zoom_changed = Pointer(zoom_info);
    auto* level = data_info ? il2cpp_class_from_type(data_info->parameters[0]) : nullptr;
    const auto* underlying = level && il2cpp_class_is_enum(level) ? il2cpp_class_enum_basetype(level) : nullptr;
    mode_field = il2cpp_class_get_field_from_name(director.get_cls(), "GalaxyViewMode");
    get_component = component.GetMethodInfoSpecial("GetComponent", [](auto count, auto params) {
      return count == 1 && params[0]->type == IL2CPP_TYPE_CLASS;
    });
    star_type = star.GetType();
    auto horizontal = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "HorizontalLayoutGroup");
    if (horizontal.isValidHelper()) horizontal_type = horizontal.GetType();
    auto element = il2cpp_get_class_helper("UnityEngine.UI", "UnityEngine.UI", "LayoutElement");
    if (element.isValidHelper()) element_type = element.GetType();
    auto game_object = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "GameObject");
    add_component = game_object.GetMethodInfoSpecial("AddComponent", [](auto count, auto params) {
      return count == 1 && params[0]->type == IL2CPP_TYPE_CLASS;
    });
    const auto* lod_field = il2cpp_class_get_field_from_name(zoom.get_cls(), "_navigationLod");
    if (!lod_field || lod_field->offset != 0x40) return;
    auto list = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Client.UI", "BaseListContainer");
    const auto* container = list.isValidHelper() ? il2cpp_class_get_field_from_name(list.get_cls(), "m_container") : nullptr;
    if (!container || container->offset != 0x28) return;
#if __APPLE__
    const auto instance = [](const FieldInfo* field) {
      return field && field->type && !(field->type->attrs & FIELD_ATTRIBUTE_STATIC);
    };
    const auto reference = [&](const FieldInfo* field) {
      auto* cls = instance(field) ? il2cpp_class_from_type(field->type) : nullptr;
      return cls && !il2cpp_class_is_valuetype(cls);
    };
    const auto enum32 = [](const Il2CppType* type) {
      auto* cls = type ? il2cpp_class_from_type(type) : nullptr;
      const auto* base = cls && il2cpp_class_is_enum(cls) ? il2cpp_class_enum_basetype(cls) : nullptr;
      return base && base->type == IL2CPP_TYPE_I4;
    };
    if (!instance(dirty) || dirty->type->type != IL2CPP_TYPE_BOOLEAN
        || !instance(current) || !enum32(current->type)
        || !instance(context) || context->type->type != IL2CPP_TYPE_VALUETYPE
        || !reference(lod_field) || !reference(container)
        || !mode_field || !(mode_field->type->attrs & FIELD_ATTRIBUTE_STATIC) || !enum32(mode_field->type)
        || is_minor->flags & METHOD_ATTRIBUTE_STATIC || is_minor->parameters_count != 1
        || !is_minor->parameters[0]->byref
        || il2cpp_class_from_type(is_minor->parameters[0]) != il2cpp_class_from_type(context->type)
        || !method_contract::Type(is_minor->return_type, "System.Boolean")) {
      spdlog::warn("[GalaxyLabels] incompatible Mac field or helper layout");
      return;
    }
#endif
    // The small selection callback requires native inspection when reviewing client
    // updates. Other bindings follow the normal IL2CPP resolution/SPUD hook path.
    if (!mode_field || !get_component || !star_type || !underlying || underlying->type != IL2CPP_TYPE_I4
        || !bind || !release || !should_filter || !data || !name || !select || !animation || !zoom_changed) {
      spdlog::warn("[GalaxyLabels] incompatible native bindings"); return;
    }
    for (auto [field, offset] : {std::pair{"_resourceList", 0xf0}, {"_resourceListContainer", 0x100},
                                  {"_hostilesListContainer", 0x108}, {"_hazardsListContainer", 0x118}, {"_hazardsWidget", 0x110},
                                  {"_hostilesLevelText", 0x90}, {"_starName", 0x80}, {"_hostilesList", 0xf8}, {"_galacticAnomalyIcon", 0x1d0}}) {
      auto* info = il2cpp_class_get_field_from_name(star.get_cls(), field);
      if (!info || info->offset != offset) return;
#if __APPLE__
      if (!reference(info)) return;
#endif
    }
#if __APPLE__
    // Do not install any member of the family until all exact loaded entries fit.
    // The selection callback may be short; its full SPUD overwrite is still decoded.
    const std::array targets{data, name, select, animation, zoom_changed, should_filter, bind, release};
    for (std::size_t i = 0; i < targets.size(); ++i) {
      if (!native_hooks::MacHookFits(targets[i], targets[i] == select ? 32 : 64)) {
        spdlog::warn("[GalaxyLabels] Mac native hook validation failed at target {}", i);
        return;
      }
      for (std::size_t j = 0; j < i; ++j) {
        if (targets[i] == targets[j]) {
          spdlog::warn("[GalaxyLabels] aliased Mac hook targets {} and {}", i, j);
          return;
        }
      }
    }
#endif
    update_name = reinterpret_cast<decltype(update_name)>(name);
    const bool a = SPUD_STATIC_DETOUR(data, DataHook), b = SPUD_STATIC_DETOUR(name, NameHook);
    const bool c = SPUD_STATIC_DETOUR(select, SelectHook), d = SPUD_STATIC_DETOUR(animation, AnimationHook);
    const bool e = SPUD_STATIC_DETOUR(zoom_changed, ZoomHook);
    const bool f = SPUD_STATIC_DETOUR(should_filter, FilterHook);
    const bool g = SPUD_STATIC_DETOUR(bind, BindHook), h = SPUD_STATIC_DETOUR(release, ReleaseHook);
    ready = a && b && c && d && e && f && g && h;
    if (ready) ApplySettings();
    spdlog::info("[GalaxyLabels] hooks ready={} data={} name={} select={} animation={} zoom={} filter={} bind={} release={}",
                 ready, a, b, c, d, e, f, g, h);
  }
};

} // namespace
#endif

bool GalaxyLabelsRequested()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  // Shared zoom hooks must be installed even when both profiles start Native,
  // so settings can enable them during play. Native bindings are validated in Install.
  return Config::Get().installZoomHooks;
#else
  return false;
#endif
}

int GalaxyLabelLOD(void* lod, int level)
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  return GalaxyRuntime::Instance().Lod(lod, level);
#else
  return level;
#endif
}
void GalaxyLabelFrame(void* zoom)
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  GalaxyRuntime::Instance().Frame(zoom);
#endif
}
void InstallGalaxyLabels()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  GalaxyRuntime::Instance().Install();
#endif
}

namespace mod_settings
{
bool GalaxyLabelControlsAvailable()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  return GalaxyRuntime::Instance().Available();
#else
  return false;
#endif
}
void RefreshGalaxyLabelControls()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  if (GalaxyRuntime::Instance().Available()) GalaxyRuntime::Instance().ApplySettings();
#endif
}
} // namespace mod_settings
