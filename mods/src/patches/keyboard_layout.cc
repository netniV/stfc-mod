#include "keyboard_layout.h"
#include "config.h"
#include "file.h"
#include "key.h"
#include "keyboard_layout_mapping.h"
#include "keyboard_layout_notifications.h"
#include "keyboard_layout_windows.h"
#include "str_utils.h"
#include <exception>
#include <spdlog/spdlog.h>

namespace keyboard_layout
{
namespace
{
  bool                             enabled = false, initialized = false, failed = false, vars_ready = false;
  RefreshState                     refresh;
  BindingState                     bindings;
  std::array<bool, LayoutKeyCount> requested{}, required_shift{};
  toml::table                      vars_snapshot;
  std::string                      layout_name, status = "physical", reason = "configured_physical";
  unsigned                         generation     = 0;
  const MethodInfo *               current_method = nullptr, *layout_method = nullptr;
  int (*frame_count)() = nullptr;

  void WriteDiagnostics(toml::table& vars)
  {
    vars.insert_or_assign(
        "keyboard_mapping",
        toml::table{{"layout", layout_name}, {"status", status}, {"reason", reason}, {"generation", generation}});
  }

  void Publish()
  {
    ++generation;
    if (vars_ready) {
      // Diagnostics are best effort: a checked save must not unwind an input callback.
      try {
        WriteDiagnostics(vars_snapshot);
        Config::Save(vars_snapshot, File::Vars());
      }
      catch (const std::exception& error) {
        static bool reported = false;
        if (!reported) {
          reported = true;
          spdlog::warn("[KeyboardLayout] Could not save diagnostics: {}", error.what());
        }
      }
    }
    spdlog::info("[KeyboardLayout] status={} layout='{}' generation={} reason={}", status, layout_name, generation,
                 reason);
  }

  void Unavailable(std::string_view why)
  {
    bindings.Clear();
    required_shift = {};
    layout_name.clear();
    if (status == "unavailable" && reason == why)
      return;
    status = "unavailable";
    reason = why;
    Publish();
  }

  void Disable(std::string_view why)
  {
    failed = true;
    notifications::Stop();
    Unavailable(why);
  }

  Il2CppObject* Invoke(const MethodInfo* method, void* self = nullptr)
  {
    Il2CppException* exception = nullptr;
    auto*            result    = il2cpp_runtime_invoke(method, self, nullptr, &exception);
    if (exception) {
      Disable("unity_invocation_failed");
      return nullptr;
    }
    return result;
  }

  void Initialize()
  {
    initialized             = true;
    const auto subscription = notifications::Start(refresh);
    if (subscription != notifications::Result::Started) {
      Disable(subscription == notifications::Result::Unsupported ? "platform_unsupported"
                                                                 : "notification_subscription_failed");
      return;
    }
    auto keyboard  = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "Keyboard");
    current_method = keyboard.GetMethodInfo("get_current", 0);
    layout_method  = keyboard.GetMethodInfo("get_keyboardLayout", 0);
    frame_count    = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
    if (!current_method || !layout_method || !frame_count)
      Disable("missing_unity_api");
  }

  void Update()
  {
    if (!initialized)
      Initialize();
    if (failed || !refresh.Consume())
      return;
    auto* keyboard = Invoke(current_method);
    if (!keyboard) {
      if (!failed)
        Unavailable("keyboard_absent");
      return;
    }
    auto* layout = reinterpret_cast<Il2CppString*>(Invoke(layout_method, keyboard));
    if (!layout || !layout->length) {
      if (!failed)
        Unavailable("layout_unavailable");
      return;
    }
    layout_name = to_string(layout);
    LayoutKeys                       keys{};
    std::array<bool, LayoutKeyCount> shift{};
    bool                             complete = true;
    for (std::size_t index = 0; index < requested.size(); ++index) {
      if (!requested[index])
        continue;
#if _WIN32
      const auto chord = ResolveWindowsChord(static_cast<char>(index), layout_name);
      keys[index]      = chord.key;
      shift[index]     = chord.shift;
#endif
      if (keys[index] == KeyCode::None) {
        complete = false;
        spdlog::warn("[KeyboardLayout] unresolved character '{}' ({}) on layout '{}'; binding disabled",
                     static_cast<char>(index), index, layout_name);
      }
    }
    bindings.Replace(keys, Key::Pressed, frame_count());
    required_shift = shift;
    status         = complete ? "resolved" : "partial";
    reason         = complete ? "layout_lookup" : "unresolved_keys_disabled";
    Publish();
  }
} // namespace

void Configure(std::string_view mode)
{
  enabled = mode == "layout";
  status  = enabled ? "pending" : "physical";
  reason  = enabled ? "awaiting_game_input" : "configured_physical";
}

void RegisterShortcut(KeyCode key)
{
  if (enabled && IsLayoutKey(key))
    requested[static_cast<int>(key)] = true;
}

void InitializeDiagnostics(toml::table& vars)
{
  WriteDiagnostics(vars);
  if (enabled) {
    vars_snapshot = vars;
    vars_ready    = true;
  }
}

ResolvedChord ResolveChord(KeyCode configured)
{
  if (!enabled || !IsLayoutKey(configured))
    return {configured, false};
  Update();
  return {bindings.Resolve(configured, frame_count, Key::Pressed), required_shift[static_cast<int>(configured)]};
}
} // namespace keyboard_layout
