#include "shortcut_settings.h"
#include "config.h"
#include "page_catalog.h"
#include "patches/key.h"
#include "patches/keyboard_layout.h"
#include "patches/mapkey.h"
#include "patches/runtime_config.h"
#include "patches/screen_update_hook.h"
#include "shortcut_capture.h"
#include "shortcut_catalog.h"
#include "shortcut_draft.h"
#include "str_utils.h"
#include <algorithm>
#include <il2cpp/il2cpp_helper.h>
#include <memory>
#include <spdlog/spdlog.h>

namespace mod_settings
{
namespace
{
  void (*changed)() = nullptr;
  void Notify()
  {
    if (changed)
      changed();
  }
  std::string Join(const ShortcutList& list)
  {
    std::string result;
    for (const auto& text : list) {
      if (!result.empty())
        result += " | ";
      result += text;
    }
    return result.empty() ? "NONE" : result;
  }
  struct Editor {
    GameFunction                                function;
    ValueSetting<ShortcutList>                  state;
    ShortcutDraft                               draft;
    std::string                                 status, replacing;
    std::vector<std::string>                    overlaps;
    std::size_t                                 overlapIndex = 0;
    std::vector<std::unique_ptr<ActionSetting>> rows;
    Editor(GameFunction action, const std::string& label)
        : function(action)
        , state({"community_mod.shortcuts." + MapKey::Definition(action).key, label,
                 [action] {
                   ShortcutList list;
                   for (const auto& binding : MapKey::Bindings(action))
                     list.push_back(binding.GetParsedValues());
                   return ValueReadResult<ShortcutList>::Known(std::move(list), 1);
                 },
                 [action](ShortcutList list, std::uint64_t generation) {
                   if (generation != 1)
                     return ApplyResult::Rejected;
                   // Existing oversized lists may be reduced/rebound, but a UI
                   // addition must fit before either publication or persistence.
                   if (!ShortcutCountFitsEdit(MapKey::Bindings(action).size(), list.size()))
                     return ApplyResult::Rejected;
                   std::vector<MapKey> bindings;
                   for (const auto& text : list) {
                     auto parsed = MapKey::Parse(text);
                     if (parsed.Key == KeyCode::None)
                       return ApplyResult::Rejected;
                     bindings.push_back(std::move(parsed));
                   }
                   const auto serialized = Join(list); // Allocate before publishing live bindings.
                   if (!MapKey::ReplaceBindings(action, std::move(bindings)))
                     return ApplyResult::Rejected;
                   runtime_config::SaveSetting("shortcuts", MapKey::Definition(action).key.c_str(), serialized);
                   return ApplyResult::Applied;
                 }})
        , draft(state, [](const auto& first, const auto& second) {
          return MapKey::SameBinding(MapKey::Parse(first), MapKey::Parse(second));
        })
    {
    }
    ShortcutList Current()
    {
      auto snapshot = state.Observe();
      return snapshot.state.value.value_or(ShortcutList{});
    }
  };
  std::vector<std::unique_ptr<Editor>> editors;
  Editor*                              recording      = nullptr;
  std::size_t                          recordingIndex = 0;
  ShortcutCapture                      capture;
  std::vector<KeyCode>                 sampledKeys;
  bool (*isFocused)() = nullptr;

  // Compare the dispatcher's modifier rules as well as the physical key.
  // Contexts may still make an overlap intentional; warn without removing either.
  std::vector<std::string> Overlaps(Editor& editor, const std::string& token)
  {
    const auto candidate = MapKey::Parse(token);
    if (keyboard_layout::DescribeChord(candidate.Key).key == KeyCode::None)
      return {token + ": layout unavailable; check this binding"};
    std::vector<std::string> result;
    for (int i = 0; i < GameFunction::Max; ++i) {
      const auto action = static_cast<GameFunction>(i);
      if (action == editor.function)
        continue;
      bool overlap = false;
      for (const auto& binding : MapKey::Bindings(action))
        overlap |= MapKey::MayOverlap(candidate, binding);
      if (!overlap)
        continue;
      result.push_back(token + " may overlap: " + DescribeShortcut(action, MapKey::Definition(action).key).label);
    }
    return result;
  }

  void Cancel(Editor& editor)
  {
    if (recording == &editor) {
      recording = nullptr;
      capture.Cancel();
    }
    editor.draft.Cancel();
    editor.overlaps.clear();
    editor.overlapIndex = 0;
    editor.status.clear();
  }
  void Begin(Editor& editor, std::size_t index)
  {
    if (capture.active())
      return;
    Cancel(editor);
    editor.draft.Begin();
    const auto list = editor.Current();
    if (index > list.size() || (index == list.size() && list.size() >= ShortcutBindingDisplayLimit))
      return;
    recordingIndex   = index;
    editor.replacing = index < list.size() ? list[index] : "";
    recording      = &editor;
    capture.Begin();
    Key::shortcutCaptureActive = true;
    editor.status              = "Release keys, then press a shortcut. Esc cancels.";
  }
  void UpdateCapture()
  {
    if (!capture.active())
      return; // No input scan, layout query or logging while idle.
    try {
      ShortcutCapture::Keys held{}, down{};
      for (auto key : sampledKeys) {
        held[static_cast<int>(key)] = Key::RawPressed(key);
        down[static_cast<int>(key)] = Key::RawDown(key);
      }
      const bool hasFocus = isFocused();
      const bool cancel   = !hasFocus || down[static_cast<int>(KeyCode::Escape)];
      auto*      editor   = recording;
      const auto primary  = capture.Tick(held, down, hasFocus, Key::IsModifier);
      if (editor && cancel) {
        Cancel(*editor);
        editor->status = "Recording cancelled";
        Notify();
      } else if (editor && primary) {
        const auto isHeld = [&](KeyCode key) { return held[static_cast<int>(key)]; };
        const auto key =
            keyboard_layout::CaptureIdentity(*primary, isHeld(KeyCode::LeftShift) || isHeld(KeyCode::RightShift));
        // New bindings use generic modifiers, matching ordinary TOML bindings.
        // Existing sided modifiers remain untouched unless that binding is replaced.
        std::string token;
        if (isHeld(KeyCode::LeftControl) || isHeld(KeyCode::RightControl))
          token += "CTRL-";
        if (isHeld(KeyCode::LeftAlt) || isHeld(KeyCode::RightAlt))
          token += "ALT-";
        if (isHeld(KeyCode::LeftShift) || isHeld(KeyCode::RightShift))
          token += "SHIFT-";
        if (isHeld(KeyCode::LeftWindows) || isHeld(KeyCode::RightWindows))
          token += "WIN-";
        if (key == KeyCode::None || isHeld(KeyCode::AltGr)) {
          editor->status = "This key/layout is unavailable; record another";
        } else {
          token += Key::Token(key);
          const auto result = editor->draft.Stage(recordingIndex, token);
          editor->overlaps.clear();
          if (result == ShortcutStage::Staged) {
            editor->overlaps = Overlaps(*editor, token);
            editor->status   = editor->replacing.empty() ? "Add " + token : editor->replacing + " -> " + token;
          } else {
            editor->status = result == ShortcutStage::AlreadyBound ? "Already bound: " + token
                                                                   : "Binding unavailable; reopen and try again";
          }
        }
        recording = nullptr;
        Notify();
      } else if (editor && !capture.active()) {
        recording      = nullptr;
        editor->status = "Recording cancelled; press one primary key";
        Notify();
      }
      const bool wasBlocked      = Key::shortcutCaptureActive;
      Key::shortcutCaptureActive = capture.active();
      if (wasBlocked != Key::shortcutCaptureActive)
        Notify();
    } catch (...) {
      capture.Cancel();
      if (recording) {
        recording->draft.Cancel();
        recording = nullptr;
      }
      // Continue draining via raw input on subsequent frames; never leave a draft
      // from a failed capture eligible for application.
      static bool warned = false;
      if (!warned) {
        warned = true;
        spdlog::warn("[Shortcuts] Key recording unavailable");
      }
    }
  }
  void AddRows(PageCatalog& catalog, Editor& editor)
  {
    catalog.OnLeave(editor.state.id(), [&editor] { Cancel(editor); });
    catalog.SetSummary(editor.state.id(), [&editor] {
      const auto& bindings = MapKey::Bindings(editor.function);
      if (bindings.empty())
        return std::string{"Unbound"};
      return bindings.front().GetParsedValues()
             + (bindings.size() > 1 ? " +" + std::to_string(bindings.size() - 1) : "");
    });
    auto add = [&](const char* id, const char* label, auto read, auto invoke, std::function<std::size_t()> count = {}) {
      auto row      = std::make_unique<ActionSetting>();
      row->identity = editor.state.id() + "." + id;
      row->label    = label;
      row->read     = read;
      row->invoke   = [invoke](std::size_t index) {
        invoke(index);
        Notify();
      };
      row->count = std::move(count);
      catalog.AddAction(editor.state.id(), *row);
      editor.rows.push_back(std::move(row));
    };
    using P = ActionSetting::Presentation;
    const auto help = ShortcutExplanation(editor.function);
    if (!help.empty())
      add(
          "help", "About this shortcut", [help](std::size_t) { return P{std::string(help), "", "", false}; },
          [](std::size_t) {});
    auto count = [&editor] { return VisibleShortcutBindingCount(editor.Current().size()); };
    add(
        "binding", "Shortcut",
        [&editor](std::size_t index) {
          const auto list = editor.Current();
          return P{list.at(index), "Change", "", !capture.active() && !editor.draft.pending()};
        },
        [&editor](std::size_t index) { Begin(editor, index); }, count);
    add(
        "add", "Add shortcut",
        [&editor](std::size_t) {
          const auto size = editor.Current().size();
          if (size > ShortcutBindingDisplayLimit)
            return P{"Showing first " + std::to_string(ShortcutBindingDisplayLimit) + "; remove shortcuts or edit TOML",
                     "", "", false};
          if (size == ShortcutBindingDisplayLimit)
            return P{"Remove a shortcut before adding another", "", "", false};
          return P{size == 0 ? "No shortcut assigned" : "Add shortcut", "Record", "",
                   !capture.active() && !editor.draft.pending()};
        },
        [&editor](std::size_t) { Begin(editor, editor.Current().size()); });
    add(
        "apply", "Pending change",
        [&editor](std::size_t) {
          const auto button = !editor.draft.pending() ? "" : editor.overlaps.empty() ? "Apply" : "Apply anyway";
          return P{editor.status, button, "", editor.draft.pending() && !capture.active(), !editor.status.empty()};
        },
        [&editor](std::size_t) {
          const auto result = editor.draft.Apply();
          editor.status     = result == Outcome::AppliedVerified || result == Outcome::Unchanged
                                  ? "Applied"
                                  : "Binding changed; reopen and try again";
          // Keep the applied binding's warning visible. Clearing it on Apply
          // made a real overlap easy to miss after the button was pressed.
          if (result != Outcome::AppliedVerified && result != Outcome::Unchanged)
            editor.overlaps.clear();
        });
    add(
        "overlaps", "Overlap information",
        [&editor](std::size_t) {
          if (editor.overlaps.empty())
            return P{"", "", "", false, false};
          const auto count = editor.overlaps.size();
          const auto text  = editor.overlaps.at(editor.overlapIndex % count);
          const auto position =
              count > 1 ? " (" + std::to_string(editor.overlapIndex % count + 1) + "/" + std::to_string(count) + ")"
                        : "";
          return P{"<color=#FFC66D>" + text + position + "</color>", count > 1 ? "Next" : "", "", true};
        },
        [&editor](std::size_t) {
          if (!editor.overlaps.empty())
            editor.overlapIndex = (editor.overlapIndex + 1) % editor.overlaps.size();
        });
    add(
        "cancel", "Discard pending change",
        [&editor](std::size_t) {
          return P{"Discard change", "Cancel", "", true, editor.draft.pending() || recording == &editor};
        },
        [&editor](std::size_t) { Cancel(editor); });
    catalog.AddHeading(editor.state.id(), editor.state.id() + ".more", "More options", true);
    add(
        "default", "Restore default",
        [&editor](std::size_t) {
          return P{"Default: " + MapKey::Definition(editor.function).defaultBinding, "Restore", "",
                   !capture.active() && !editor.draft.pending()};
        },
        [&editor](std::size_t) {
          // Use the same definition and token parser as config loading. Never
          // infer a default from the current live binding or rewrite other actions.
          const auto&  definition = MapKey::Definition(editor.function).defaultBinding;
          ShortcutList defaults;
          if (AsciiStrToUpper(StripAsciiWhitespace(definition)) != "NONE") {
            for (const auto& token : StrSplit(definition, '|')) {
              const auto parsed = MapKey::Parse(token);
              if (parsed.Key == KeyCode::None) {
                editor.status = "Default unavailable; edit TOML";
                return;
              }
              defaults.push_back(parsed.GetParsedValues());
            }
          }
          Cancel(editor);
          editor.draft.Begin();
          const auto result = editor.draft.Restore(defaults);
          editor.status     = result == ShortcutStage::Staged         ? "Restore default: " + Join(defaults)
                              : result == ShortcutStage::AlreadyBound ? "Already using default"
                                                                      : "Default unavailable; reopen settings";
          if (result == ShortcutStage::Staged)
            for (const auto& token : defaults) {
              auto overlaps = Overlaps(editor, token);
              editor.overlaps.insert(editor.overlaps.end(), overlaps.begin(), overlaps.end());
            }
        });
    add(
        "remove", "Remove shortcut",
        [&editor](std::size_t index) {
          return P{editor.Current().at(index), "Remove", "", !capture.active() && !editor.draft.pending()};
        },
        [&editor](std::size_t index) {
          const auto list = editor.Current();
          if (index >= list.size())
            return;
          Cancel(editor);
          editor.draft.Begin();
          editor.draft.Stage(index, {});
          editor.status = "Remove " + list[index];
        },
        count);
  }
} // namespace

void SetShortcutPresentationObserver(void (*observer)())
{ changed = observer; }
void RegisterShortcutPages(PageCatalog& catalog)
{
  if (!editors.empty() || !Config::Get().installHotkeyHooks || Config::Get().use_scopely_hotkeys)
    return;
#if defined(_WIN32) && defined(_M_X64)
  // Capture must observe a focused release before returning keys to gameplay.
  // A missing query is not ordinary focus loss: never offer capture without it.
  isFocused = il2cpp_resolve_icall_typed<bool()>("UnityEngine.Application::get_isFocused()");
  if (!isFocused) {
    spdlog::warn("[Shortcuts] Editor unavailable: focus query not resolved");
    return;
  }
  if (!install_screen_manager_update_hook() || !register_screen_manager_update_callback(UpdateCapture))
    return;
  for (int i = 1; i < static_cast<int>(KeyCode::Max); ++i) {
    const auto key = static_cast<KeyCode>(i);
    if (!Key::Token(key).empty())
      sampledKeys.push_back(key);
  }
  catalog.AddPage("community_mod.shortcuts", "Shortcuts", "community_mod.settings");
  for (const auto& group : ShortcutGroups)
    catalog.AddPage(std::string("community_mod.shortcuts.") + std::string(group.id), std::string(group.label),
                    "community_mod.shortcuts");
  const auto ordered = DiscoverShortcuts([](GameFunction action) -> std::string_view {
    // A startup NONE binding opts out of installing the hints adapter. Do not
    // present a live editor for an action that cannot dispatch in this session.
    if (action == GameFunction::ToggleShortcutHints && !ShortcutHintControlAvailable())
      return {};
    return MapKey::Definition(action).key;
  });
  for (const auto& info : ordered) {
#ifdef _MODDBG
    if (info.fallback)
      spdlog::warn("[Shortcuts] {} has no presentation override; listed in Uncategorized",
                   MapKey::Definition(info.action).key);
#endif
    auto        editor = std::make_unique<Editor>(info.action, info.label);
    const auto  group  = std::find_if(ShortcutGroups.begin(), ShortcutGroups.end(),
                                      [&](const auto& group) { return group.group == info.group; });
    catalog.AddPage(editor->state.id(), editor->state.label(),
                    std::string("community_mod.shortcuts.") + std::string(group->id));
    AddRows(catalog, *editor);
    editors.push_back(std::move(editor));
  }
#endif
}
} // namespace mod_settings
