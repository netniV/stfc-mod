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
#include "shortcut_popup.h"
#include "shortcut_undo.h"
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
    ShortcutUndo                                undo;
    std::string                                 status, replacing;
    std::vector<std::string>                    overlaps;
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
                 [this, action](ShortcutList list, std::uint64_t generation) {
                   if (generation != 1)
                     return ApplyResult::Rejected;
                   // Existing oversized lists may be reduced/rebound, but a UI
                   // addition must fit before either publication or persistence.
                   if (!ShortcutCountFitsEdit(MapKey::Bindings(action).size(), list.size())
                       && !undo.Restoring(list))
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
        , undo(state)
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
  Editor* popupEditor = nullptr;

  // Compare the dispatcher's modifier rules as well as the physical key.
  // Contexts may still make an overlap intentional; warn without removing either.
  std::vector<std::string> Overlaps(Editor& editor, const std::string& token)
  {
    const auto candidate = MapKey::Parse(token);
    if (keyboard_layout::DescribeChord(candidate.Key).key == KeyCode::None)
      return {token + ": layout unavailable; check this binding"};
    std::vector<std::string> result;
    // These direct Enter handlers are outside the configurable MapKey bindings.
    if (candidate.Key == KeyCode::Return || candidate.Key == KeyCode::KeypadEnter) {
      result.push_back(token + " may also confirm game dialogs.");
      if (Config::Get().double_click_to_assign_ship)
        result.push_back(token + " also assigns the selected ship while the ship selection screen is open.");
    }
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

  void Cancel(Editor& editor, bool closePopup = true)
  {
    if (closePopup && popupEditor == &editor) {
      popupEditor = nullptr;
      native::CloseShortcutPopup();
      // Closing from preview still owns Escape/Enter/the mouse through release.
      capture.Begin();
      capture.Cancel();
      Key::shortcutCaptureActive = true;
    }
    if (recording == &editor) {
      recording = nullptr;
      capture.Cancel();
    }
    editor.draft.Cancel();
    editor.undo.Clear();
    editor.overlaps.clear();
    editor.status.clear();
  }
  void Begin(Editor& editor, std::size_t index, bool keepPopup = false)
  {
    if (capture.active())
      return;
    Cancel(editor, !keepPopup);
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
  void OpenEditor(Editor& editor, std::size_t index)
  {
    Begin(editor, index);
    if (recording != &editor)
      return;
    popupEditor = &editor;
    const bool opened = native::OpenShortcutPopup({
        [&editor, index] {
          ShortcutPopupPresentation view;
          view.title = editor.state.label();
          view.current = editor.replacing.empty() ? "Unbound" : editor.replacing;
          if (editor.draft.desired() && index < editor.draft.desired()->size())
            view.proposed = editor.draft.desired()->at(index);
          view.status = editor.status;
          for (const auto& warning : editor.overlaps)
            view.status += "\n<color=#FFC66D>" + warning + "</color>";
          view.confirmLabel = editor.overlaps.empty() ? "Confirm" : "Use anyway";
          view.canConfirm = editor.draft.pending() && !capture.active();
          view.canRecord = !capture.active();
          return view;
        },
        [&editor, index] {
          if (!editor.draft.pending() || capture.active())
            return;
          auto overlaps = Overlaps(editor, editor.draft.desired()->at(index));
          if (overlaps != editor.overlaps) {
            editor.overlaps = std::move(overlaps);
            editor.status = "Shortcut usage changed. Check the warnings and confirm again.";
            return;
          }
          const auto outcome = editor.draft.Apply();
          if (outcome == Outcome::AppliedVerified || outcome == Outcome::Unchanged) {
            Cancel(editor);
            spdlog::info("[Shortcuts] Popup binding applied: {}", editor.state.id());
          } else {
            editor.status = "Binding changed; cancel and reopen to try again.";
          }
          Notify();
        },
        [&editor, index] { Begin(editor, index, true); },
        [&editor] { Cancel(editor); Notify(); }});
    if (!opened) {
      popupEditor = nullptr;
      Cancel(editor);
      editor.status = "Popup unavailable; no binding changed.";
    }
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
        Notify();
      } else if (editor && primary) {
        const auto isHeld = [&](KeyCode key) { return held[static_cast<int>(key)]; };
        const auto key =
            keyboard_layout::CaptureIdentity(*primary, isHeld(KeyCode::LeftShift) || isHeld(KeyCode::RightShift));
        // Generic modifiers for new bindings, matching ordinary TOML bindings.
        // Existing sided modifiers remain untouched unless that binding is replaced.
        std::string token = CaptureModifierPrefix(isHeld);
        if (key == KeyCode::None || isHeld(KeyCode::AltGr)) {
          editor->status = "This key/layout is unavailable; record another";
        } else {
          token += Key::Token(key);
          const auto result = editor->draft.Stage(recordingIndex, token);
          editor->overlaps.clear();
          if (result == ShortcutStage::Staged) {
            editor->overlaps = Overlaps(*editor, token);
            editor->status   = editor->replacing.empty() ? "Add " + token
                                                         : editor->replacing + " -> " + token;
          } else {
            editor->status = result == ShortcutStage::AlreadyBound
                                 ? "Already bound: " + token
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
    add(
        "status", "Shortcut status",
        [&editor](std::size_t) {
          if (popupEditor == &editor)
            return P{"", "", "", false, false};
          if (editor.undo.available())
            return P{"<color=#FFC66D>" + editor.undo.message() + "</color>", "Undo", "", !capture.active()};
          return P{editor.status, "", "", false, !editor.status.empty()};
        },
        [&editor](std::size_t) {
          if (editor.undo.available()) {
            const auto result = editor.undo.Undo();
            editor.status = result == Outcome::AppliedVerified ? ""
                            : result == Outcome::Conflict ? "Bindings changed; undo cancelled."
                                                          : "Could not verify undo; check current bindings.";
          }
        });
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
          return P{list.at(index), "Change", "",
                   popupEditor != &editor && !capture.active() && !editor.draft.pending()};
        },
        [&editor](std::size_t index) { OpenEditor(editor, index); }, count);
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
                   popupEditor != &editor && !capture.active() && !editor.draft.pending()};
        },
        [&editor](std::size_t) { OpenEditor(editor, editor.Current().size()); });
    add(
        "remove", "Remove shortcut",
        [&editor](std::size_t index) {
          return P{editor.Current().at(index), "Remove", "", !capture.active() && !editor.draft.pending(),
                   popupEditor != &editor};
        },
        [&editor](std::size_t index) {
          const auto list = editor.Current();
          if (index >= list.size())
            return;
          Cancel(editor);
          const auto result = editor.undo.Remove(index);
          editor.status = result == Outcome::AppliedVerified ? "Removed " + list[index]
                                                             : "Could not verify removal; check current bindings.";
        },
        count);
    // Parse the same canonical defaults for visibility and restoration. An extra
    // alternative still makes Restore useful, even if the default is present.
    const auto defaults = [&editor]() -> std::optional<ShortcutList> {
      const auto& definition = MapKey::Definition(editor.function).defaultBinding;
      ShortcutList result;
      if (AsciiStrToUpper(StripAsciiWhitespace(definition)) != "NONE") {
        for (const auto& token : StrSplit(definition, '|')) {
          const auto parsed = MapKey::Parse(token);
          if (parsed.Key == KeyCode::None)
            return std::nullopt;
          result.push_back(parsed.GetParsedValues());
        }
      }
      return result;
    }();
    add(
        "default", "Restore default",
        [&editor, defaults](std::size_t) {
          return P{"Default: " + MapKey::Definition(editor.function).defaultBinding, "Restore", "",
                   defaults.has_value() && !capture.active() && !editor.draft.pending(),
                   popupEditor != &editor && (!defaults || editor.Current() != *defaults)};
        },
        [&editor, defaults](std::size_t) {
          if (!defaults)
            return;
          Cancel(editor);
          const auto result = editor.undo.Restore(*defaults);
          editor.status = result == Outcome::AppliedVerified ? "Restored defaults"
                          : result == Outcome::Unchanged ? "Already using defaults"
                                                         : "Could not verify restore; check current bindings.";
        });
  }
} // namespace

void SetShortcutPresentationObserver(void (*observer)())
{ changed = observer; }
void RegisterShortcutPages(PageCatalog& catalog)
{
  if (!editors.empty() || !Config::Get().installHotkeyHooks || Config::Get().use_scopely_hotkeys)
    return;
// macOS ports the shared native settings adapter; capture stays in physical
// keyboard mode and relies on the same runtime-resolved hooks as Windows.
// Only Windows x64 resolves layout-dependent printable chords.
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
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
