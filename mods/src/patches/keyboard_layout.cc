#include "keyboard_layout.h"
#include "config.h"
#include "file.h"
#include "key.h"
#include "keyboard_layout_mapping.h"
#include "str_utils.h"

#include <cstdint>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace keyboard_layout
{
namespace
{
  struct Shortcut {
    std::string name, chord;
    KeyCode     key;
  };
  struct Letter {
    KeyCode     key = KeyCode::None;
    std::string physical, display, status = "pending";
  };

  bool                   enabled           = false;
  bool                   failed            = false;
  bool                   initialized       = false;
  bool                   vars_ready        = false;
  int                    last_frame        = -1;
  unsigned               generation        = 0;
  uintptr_t              keyboard_identity = 0; // Identity only; never retained/dereferenced as a managed pointer.
  std::u16string         layout_identity;
  std::string            layout_name;
  std::string            status = "physical";
  std::string            reason = "configured_physical";
  std::vector<Shortcut>  shortcuts;
  std::array<Letter, 26> letters;
  BindingState           bindings;
  toml::table            vars_snapshot;
  const MethodInfo *     current_method, *layout_method, *find_method, *key_method, *name_method, *display_method;
  int (*frame_count)() = nullptr;

  void WriteDiagnostics(toml::table& vars)
  {
    vars.insert_or_assign(
        "keyboard_mapping",
        toml::table{
            {"requested_mode", enabled ? "layout" : "physical"},
            {"effective_mode", enabled ? "layout" : "physical"},
            {"provider", enabled ? "Unity.InputSystem.Keyboard.FindKeyOnCurrentKeyboardLayout" : "legacy KeyCode"},
            {"layout", layout_name},
            {"status", status},
            {"reason", reason},
            {"generation", generation},
            {"scope", "configured A-Z only; modifiers and nonletters unchanged"},
            {"physical_key_reference", "US-QWERTY positions, not the user's printed keycaps"},
        });
    toml::table resolved;
    for (const auto& shortcut : shortcuts) {
      auto* alternatives = resolved[shortcut.name].as_array();
      if (!alternatives) {
        resolved.insert(shortcut.name, toml::array{});
        alternatives = resolved[shortcut.name].as_array();
      }
      toml::table entry{{"configured", shortcut.chord}};
      if (IsLetter(shortcut.key)) {
        const auto  index  = static_cast<int>(shortcut.key) - static_cast<int>(KeyCode::A);
        const auto& letter = letters[index];
        entry.insert("configured_letter", std::string(1, static_cast<char>('A' + index)));
        entry.insert("status", enabled ? letter.status : "physical");
        entry.insert("physical_us_key", enabled ? letter.physical : std::string(1, static_cast<char>('A' + index)));
        entry.insert("layout_display_name", enabled ? letter.display : "not_queried");
        entry.insert("legacy_key_code", static_cast<int>(enabled ? letter.key : shortcut.key));
      } else {
        entry.insert("status", "unchanged_nonletter");
        entry.insert("legacy_key_code", static_cast<int>(shortcut.key));
      }
      alternatives->push_back(std::move(entry));
    }
    vars.insert_or_assign("shortcuts_resolved", std::move(resolved));
  }

  void Publish()
  {
    ++generation;
    if (vars_ready) {
      WriteDiagnostics(vars_snapshot);
      Config::Save(vars_snapshot, File::Vars());
    }
    spdlog::info("[KeyboardLayout] status={} layout='{}' generation={} reason={}", status, layout_name, generation,
                 reason);
  }

  void Unavailable(std::string_view why)
  {
    if (status == "unavailable" && reason == why)
      return;
    status            = "unavailable";
    reason            = why;
    keyboard_identity = 0;
    layout_identity.clear();
    layout_name.clear();
    for (auto& letter : letters)
      letter = {KeyCode::None, {}, {}, "unavailable"};
    bindings.Replace(LetterKeys{}, [](KeyCode) { return false; });
    Publish();
  }

  Il2CppObject* Invoke(const MethodInfo* method, void* self = nullptr, void** args = nullptr)
  {
    if (failed || !method)
      return nullptr;
    Il2CppException* exception = nullptr;
    auto*            result    = il2cpp_runtime_invoke(method, self, args, &exception);
    if (exception) {
      failed = true;
      spdlog::warn("[KeyboardLayout] Unity method {} failed; layout letters disabled until restart", method->name);
    }
    return exception ? nullptr : result;
  }

  void Initialize()
  {
    initialized    = true;
    auto keyboard  = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "Keyboard");
    auto key       = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem.Controls", "KeyControl");
    auto control   = il2cpp_get_class_helper("Unity.InputSystem", "UnityEngine.InputSystem", "InputControl");
    current_method = keyboard.GetMethodInfo("get_current", 0);
    layout_method  = keyboard.GetMethodInfo("get_keyboardLayout", 0);
    find_method    = keyboard.GetMethodInfo("FindKeyOnCurrentKeyboardLayout", 1);
    key_method     = key.GetMethodInfo("get_keyCode", 0);
    name_method    = control.GetMethodInfo("get_name", 0);
    display_method = control.GetMethodInfo("get_displayName", 0);
    frame_count    = il2cpp_resolve_icall_typed<int()>("UnityEngine.Time::get_frameCount()");
    if (!current_method || !layout_method || !find_method || !key_method || !name_method || !display_method
        || !frame_count) {
      failed = true;
      Unavailable("missing_unity_api");
    }
  }

  void Update()
  {
    if (!initialized)
      Initialize();
    if (failed)
      return;
    const int frame = frame_count();
    if (frame == last_frame)
      return;
    last_frame = frame;
    bindings.BeginFrame(Key::Pressed);
    auto* keyboard = Invoke(current_method);
    if (!keyboard) {
      Unavailable(failed ? "unity_invocation_failed" : "keyboard_absent");
      return;
    }
    auto* layout = reinterpret_cast<Il2CppString*>(Invoke(layout_method, keyboard));
    if (!layout || layout->length == 0) {
      Unavailable(failed ? "unity_invocation_failed" : "layout_unavailable");
      return;
    }
    const std::u16string_view identity(reinterpret_cast<const char16_t*>(layout->chars), layout->length);
    if (keyboard_identity == reinterpret_cast<uintptr_t>(keyboard) && layout_identity == identity)
      return;

    // No retained managed objects or per-frame strings. Build all letters once per
    // keyboard/layout generation; a missing letter is disabled, never silently physical.
    LetterKeys             keys{};
    std::array<Letter, 26> next;
    bool                   all_resolved = true;
    for (int index = 0; index < 26 && !failed; ++index) {
      char  letter[]{static_cast<char>('a' + index), '\0'};
      auto* text = il2cpp_string_new(letter);
      void* args[]{text};
      auto* control = Invoke(find_method, keyboard, args);
      auto& result  = next[index];
      result.status = "letter_unavailable";
      if (control) {
        auto* boxed   = Invoke(key_method, control);
        auto* name    = reinterpret_cast<Il2CppString*>(Invoke(name_method, control));
        auto* display = reinterpret_cast<Il2CppString*>(Invoke(display_method, control));
        if (boxed && name && display) {
          result.key      = ToLegacyKey(*static_cast<int*>(il2cpp_object_unbox(boxed)));
          result.physical = to_string(name);
          result.display  = to_string(display);
          result.status   = result.key == KeyCode::None ? "unsupported_physical_key" : "resolved";
        }
      }
      keys[index] = result.key;
      all_resolved &= result.key != KeyCode::None;
    }
    if (failed) {
      Unavailable("unity_invocation_failed");
      return;
    }
    keyboard_identity = reinterpret_cast<uintptr_t>(keyboard);
    layout_identity   = identity;
    layout_name       = to_string(layout);
    letters           = std::move(next);
    bindings.Replace(keys, Key::Pressed);
    status = all_resolved ? "resolved" : "partial";
    reason = all_resolved ? "layout_lookup" : "unresolved_letters_disabled";
    Publish();
  }
} // namespace

void Configure(std::string_view mode)
{
  enabled = mode == "layout";
  status  = enabled ? "pending" : "physical";
  reason  = enabled ? "awaiting_game_input" : "configured_physical";
}

void RegisterShortcut(std::string_view name, std::string_view chord, KeyCode key)
{ shortcuts.push_back({std::string(name), std::string(chord), key}); }

void InitializeDiagnostics(toml::table& vars)
{
  WriteDiagnostics(vars);
  if (enabled) {
    vars_snapshot = vars;
    vars_ready    = true;
  }
}

KeyCode Resolve(KeyCode configured)
{
  if (!enabled || !IsLetter(configured))
    return configured;
  Update();
  return bindings.Resolve(configured);
}
} // namespace keyboard_layout
