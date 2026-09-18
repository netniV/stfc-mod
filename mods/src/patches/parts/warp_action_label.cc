#include "config.h"
#include "errormsg.h"

#include <spdlog/spdlog.h>
#include "patches/instant_warp_policy.h"
#include "patches/screen_update_hook.h"
#include "prime/DeploymentManager.h"
#include "prime/FleetsManager.h"
#include "prime/StarNodeObjectViewerWidget.h"
#include "str_utils.h"
#include <il2cpp-tabledefs.h>

#include <chrono>
#include <cstring>
#include <stdexcept>

namespace
{
// This is a client of the existing frame dispatcher, not another UI detour.
// Never mutate the button context, timer, resource cost, or click handler.
Il2CppObject* Invoke(Il2CppObject* object, const char* name, int count = 0, void** args = nullptr)
{
  if (!object)
    return nullptr;
  auto* method = il2cpp_class_get_method_from_name(object->klass, name, count);
  if (!method || !method->invoker_method || (method->flags & METHOD_ATTRIBUTE_STATIC) || !method->return_type
      || method->return_type->byref)
    throw std::runtime_error("missing instance method");
  for (int i = 0; i < count; ++i) {
    auto* parameter = method->parameters[i];
    auto* expected  = parameter ? il2cpp_class_from_type(parameter) : nullptr;
    auto* argument  = args ? static_cast<Il2CppObject*>(args[i]) : nullptr;
    if (!expected || parameter->byref || il2cpp_class_is_valuetype(expected)
        || (argument && !il2cpp_class_is_assignable_from(expected, argument->klass)))
      throw std::runtime_error("reference argument contract changed");
  }
  Il2CppException* exception = nullptr;
  auto*            result    = il2cpp_runtime_invoke(method, object, args, &exception);
  if (exception)
    throw std::runtime_error("managed invocation failed");
  return result;
}

bool Boolean(Il2CppObject* result)
{
  if (!result || il2cpp_class_get_type(result->klass)->type != IL2CPP_TYPE_BOOLEAN)
    throw std::runtime_error("expected Boolean");
  return *static_cast<bool*>(il2cpp_object_unbox(result));
}

FieldInfo* Field(Il2CppObject* object, const char* name, const char* expected)
{
  auto* field = object ? il2cpp_class_get_field_from_name(object->klass, name) : nullptr;
  auto* type  = field ? il2cpp_class_from_type(field->type) : nullptr;
  if (!field || (field->type->attrs & FIELD_ATTRIBUTE_STATIC) || !type || std::strcmp(type->name, expected) != 0)
    throw std::runtime_error("field contract changed");
  return field;
}

Il2CppObject* Read(Il2CppObject* object, const char* name, const char* expected)
{
  if (!object)
    return nullptr;
  auto* field = Field(object, name, expected);
  if (field->type->byref || il2cpp_class_is_valuetype(il2cpp_class_from_type(field->type)))
    throw std::runtime_error("expected reference field");
  return il2cpp_field_get_value_object(field, object);
}

bool Flag(Il2CppObject* object, const char* name)
{ return object && Boolean(il2cpp_field_get_value_object(Field(object, name, "Boolean"), object)); }

bool Alive(Il2CppObject* object)
{
  if (!object)
    return false;
  static auto  helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Object");
  static auto* method = helper.GetMethodInfo("op_Implicit", 1);
  if (!method)
    return false;
  void*            args[]    = {object};
  Il2CppException* exception = nullptr;
  auto*            result    = il2cpp_runtime_invoke(method, nullptr, args, &exception);
  return !exception && Boolean(result);
}

std::string CurrentText(Il2CppObject* label)
{
  auto* text = Invoke(label, "CurrentText");
  if (!text || il2cpp_class_get_type(text->klass)->type != IL2CPP_TYPE_STRING)
    throw std::runtime_error("expected label text");
  return to_string(reinterpret_cast<Il2CppString*>(text));
}

struct OwnedLabel {
  Il2CppGCHandle handle = nullptr;
  std::string    text;

  Il2CppObject* Get() const
  { return handle ? il2cpp_gchandle_get_target(handle) : nullptr; }

  void Clear()
  {
    auto* label    = Get();
    auto  previous = std::move(text);
    if (handle)
      il2cpp_gchandle_free(handle);
    handle = nullptr;
    text.clear();
    // Forget ownership even if native cleanup fails.
    if (Alive(label) && Flag(label, "_textOverride") && CurrentText(label) == previous)
      Invoke(label, "ClearTextOverride");
  }

  void Apply(Il2CppObject* label, const char* desired)
  {
    if (!label || !desired) {
      Clear();
      return;
    }
    if (Get() != label) {
      Clear();
      // Don't take ownership from an existing text override.
      if (Flag(label, "_textOverride"))
        return;
      handle = il2cpp_gchandle_new_weakref(label, false);
      if (!handle)
        return;
    }
    const auto current = CurrentText(label);
    if (!text.empty() && Flag(label, "_textOverride") && current != text) {
      Clear();
      return;
    }
    if (text == desired && Flag(label, "_textOverride") && current == desired)
      return;
    auto* value = il2cpp_string_new(desired);
    if (!value)
      return;
    void* args[] = {value};
    Invoke(label, "OverrideLocalizedText", 1, args);
    text = desired;
  }
};
OwnedLabel action_label;

void Update()
{
  static auto next = std::chrono::steady_clock::time_point{};
  const auto  now  = std::chrono::steady_clock::now();
  if (now < next)
    return;
  next = now + std::chrono::milliseconds(100);
  try {
    auto*       widget  = reinterpret_cast<Il2CppObject*>(ObjectFinder<StarNodeObjectViewerWidget>::Get());
    const bool  active  = Alive(widget) && Boolean(Invoke(widget, "get_isActiveAndEnabled"));
    auto*       button  = active ? Read(widget, "_visitButton", "GenericButtonWidget") : nullptr;
    const bool  enabled = Alive(button) && Boolean(Invoke(button, "get_isActiveAndEnabled"))
                          && Boolean(Invoke(button, "get_Interactable"));
    const char* desired = nullptr;
    // The ordinary visit path uses the currently selected fleet. The card's
    // cached popup CourseData is populated on click, so it is not a preview source.
    // Toll/wormhole buttons have separate confirmation paths and retain native text.
    if (enabled) {
      auto* fleets     = reinterpret_cast<Il2CppObject*>(FleetsManager::Instance());
      auto* deployment = reinterpret_cast<Il2CppObject*>(DeploymentManger::Instance());
      auto* fleet      = Alive(fleets) ? Invoke(fleets, "GetSelectedFleetData") : nullptr;
      void* args[]     = {fleet};
      if (fleet && Alive(deployment) && Boolean(Invoke(deployment, "HasInstantWarpAbility", 1, args))) {
        switch (ResolveInstantWarpConfirmation(reinterpret_cast<FleetPlayerData*>(fleet))) {
          case InstantWarpConfirmation::Warp:
            desired = "WARP";
            break;
          case InstantWarpConfirmation::Jump:
            desired = "JUMP";
            break;
          case InstantWarpConfirmation::None:
            break;
        }
      }
    }
    auto* label = desired ? Read(button, "_mainText", "TextLocalizer") : nullptr;
    action_label.Apply(Alive(label) ? label : nullptr, desired);
  } catch (const std::exception& error) {
    static bool warned = false;
    if (!warned) {
      warned = true;
      spdlog::warn("[WarpActionLabel] native label unavailable: {}", error.what());
    }
    try {
      action_label.Clear();
    } catch (...) {
    }
  }
}
} // namespace

void InstallWarpActionLabel()
{
  if (install_screen_manager_update_hook() && register_screen_manager_update_callback(Update))
    spdlog::info("[WarpActionLabel] system-card action labels enabled");
}
