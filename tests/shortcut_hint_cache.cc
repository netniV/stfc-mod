// Link production MapKey/ModifierKey parsing, action dispatch and hint caching from mods.lib.
// Key token parsing and input are test fixtures; layout lookup is injected below.
// These tests do not exercise Unity lookup, native notifications or legacy input caching.
#include "patches/keyboard_layout_mapping.h"
#include "patches/mapkey.h"
#include "settings/shortcut_draft.h"

#include <cstdlib>
#include <iostream>
#include <utility>

static std::array<bool, static_cast<int>(KeyCode::Max)> pressed{};
static std::array<bool, static_cast<int>(KeyCode::Max)> down{};
static keyboard_layout::BindingState layout_bindings;
static bool layout_enabled = false;
static std::array<keyboard_layout::ResolvedChord, keyboard_layout::LayoutKeyCount> chords;

KeyCode Key::Parse(std::string_view key)
{
  static constexpr std::pair<std::string_view, KeyCode> tokens[] = {
      {"=", KeyCode::Equals},
      {"Z", KeyCode::Z},
      {"LSHIFT", KeyCode::LeftShift},
      {"F7", KeyCode::F7},
      {"F8", KeyCode::F8},
      {"G", KeyCode::G},
      {"+", KeyCode::Plus},
      {"/", KeyCode::Slash},
      {"(", KeyCode::LeftParen},
      {"1", KeyCode::Alpha1},
      {"'", KeyCode::Quote},
      {"^", KeyCode::Caret},
      {"I", KeyCode::I},
      {"7", KeyCode::Alpha7},
      {"RSHIFT", KeyCode::RightShift},
      {"LCTRL", KeyCode::LeftControl},
      {"RCTRL", KeyCode::RightControl},
      {"EQUAL", KeyCode::Equals},
  };
  for (const auto& [token, code] : tokens) {
    if (key == token)
      return code;
  }
  return KeyCode::None;
}
bool Key::IsModifier(KeyCode key)
{
  return key == KeyCode::LeftShift || key == KeyCode::RightShift || key == KeyCode::LeftControl
         || key == KeyCode::RightControl;
}
bool Key::Pressed(KeyCode key) { return pressed[static_cast<int>(key)]; }
bool Key::Down(KeyCode key) { return down[static_cast<int>(key)]; }
bool Key::IsModified() {
  for (auto key : {KeyCode::LeftShift, KeyCode::RightShift, KeyCode::LeftControl, KeyCode::RightControl,
                   KeyCode::LeftAlt, KeyCode::RightAlt, KeyCode::AltGr, KeyCode::LeftCommand,
                   KeyCode::RightCommand, KeyCode::LeftWindows, KeyCode::RightWindows})
    if (Key::Pressed(key)) return true;
  return false;
}
void Key::ClaimDirectionalInput(KeyCode) {}

namespace keyboard_layout
{
void RegisterShortcut(KeyCode) {} // Runtime registration boundary, no Unity in this fixture.
KeyCode Resolve(KeyCode configured) { return layout_enabled ? layout_bindings.Resolve(configured, [] { return 1; }, Key::Pressed) : configured; }
ResolvedChord ResolveChord(KeyCode configured) {
  return {Resolve(configured), layout_enabled && IsLayoutKey(configured)
                               && (chords[static_cast<int>(configured)].shift) != 0};
}
ResolvedChord DescribeChord(KeyCode configured)
{
  return layout_enabled && IsLayoutKey(configured) ? chords[static_cast<int>(configured)]
                                                   : ResolvedChord{configured, false};
}
} // namespace keyboard_layout

void Check(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

// Compare the editor prediction against production modifier dispatch over every
// held modifier combination, including either side and layout-required Shift.
void CheckOverlap(const char* first, const char* second, bool expected)
{
  const auto     a = MapKey::Parse(first), b = MapKey::Parse(second);
  const auto     ca = keyboard_layout::DescribeChord(a.Key), cb = keyboard_layout::DescribeChord(b.Key);
  bool           reachable = false;
  constexpr auto modifiers =
      std::to_array<KeyCode>({KeyCode::LeftShift, KeyCode::RightShift, KeyCode::LeftControl, KeyCode::RightControl,
                              KeyCode::LeftAlt, KeyCode::RightAlt, KeyCode::AltGr, KeyCode::LeftCommand,
                              KeyCode::RightCommand, KeyCode::LeftWindows, KeyCode::RightWindows});
  for (unsigned mask = 0; mask < (1u << modifiers.size()); ++mask) {
    pressed.fill(false);
    for (std::size_t i = 0; i < modifiers.size(); ++i)
      pressed[static_cast<int>(modifiers[i])] = (mask & (1u << i)) != 0;
    reachable |= ca.key != KeyCode::None && ca.key == cb.key && MapKey::HasCorrectModifiers(a, ca.shift)
                 && MapKey::HasCorrectModifiers(b, cb.shift);
  }
  pressed.fill(false);
  if (reachable != expected || MapKey::MayOverlap(a, b) != reachable || MapKey::MayOverlap(b, a) != reachable) {
    std::cerr << "Overlap disagrees with dispatch: " << first << " / " << second << '\n';
    std::exit(1);
  }
}

void CheckDuplicate(const char* existing, const char* recorded, bool duplicate)
{
  using namespace mod_settings;
  ShortcutList               live{existing};
  int                        writes = 0;
  ValueSetting<ShortcutList> owner({"shortcuts.duplicate_test", "Test",
                                    [&] { return ValueReadResult<ShortcutList>::Known(live, 1); },
                                    [&](ShortcutList value, std::uint64_t) {
                                      live = std::move(value);
                                      ++writes;
                                      return ApplyResult::Applied;
                                    }});
  ShortcutDraft              draft(
      owner, [](const auto& a, const auto& b) { return MapKey::SameBinding(MapKey::Parse(a), MapKey::Parse(b)); });
  draft.Begin();
  const auto result = draft.Stage(1, recorded);
  Check((result == ShortcutStage::AlreadyBound) == duplicate, "Wrong same-action duplicate decision");
  Check(writes == 0 && live == ShortcutList{existing}, "Recording changed live bindings");
  Check(draft.Apply() == (duplicate ? Outcome::Suppressed : Outcome::AppliedVerified), "Wrong duplicate apply result");
  Check(writes == (duplicate ? 0 : 1) && live.size() == (duplicate ? 1 : 2), "Duplicate escaped to writer");
}

int main()
{

  constexpr auto toggle = GameFunction::ToggleShortcutHints;
  constexpr auto galaxy = GameFunction::ShowGalaxy;
  Check(!MapKey::HasBinding(toggle), "Absent binding must not enable hints");

  // Match config registration: only keys with a primary key are accepted.
  for (const auto* text : {"NONE", "", "INVALID", "CTRL"}) {
    auto parsed = MapKey::Parse(text);
    Check(parsed.Key == KeyCode::None, "Disabled/invalid binding acquired a key");
    if (parsed.Key != KeyCode::None) MapKey::AddMappedKey(toggle, std::move(parsed));
    Check(!MapKey::HasBinding(toggle), "Disabled/invalid binding enabled hints");
  }

  MapKey::AddMappedKey(galaxy, MapKey::Parse("CTRL-G"));
  MapKey::AddMappedKey(galaxy, MapKey::Parse("F8"));
  Check(MapKey::GetShortcutHint(galaxy).empty(), "Parsing must not populate hint cache");
  Check(MapKey::GetShortcuts(galaxy) == "CTRL-G | F8", "Normal shortcut labels changed");

  // The toggle may be parsed after bindings whose badges it enables.
  MapKey::AddMappedKey(toggle, MapKey::Parse("SHIFT-F7"));
  Check(MapKey::HasBinding(toggle), "Valid chord must enable hints");
  Check(MapKey::GetShortcutHint(toggle).empty(), "Binding registration must not populate hint cache");
  Check(MapKey::GetShortcutHint(galaxy).empty(), "Registering toggle must not format earlier bindings");
  MapKey::CacheShortcutHints();
  Check(MapKey::GetShortcutHint(galaxy) == "^G", "Cache must include earlier binding, first alternative only");
  Check(MapKey::GetShortcutHint(toggle) == "+F7", "Toggle badge missing");
  Check(MapKey::GetShortcutHint(GameFunction::ShowResearch).empty(), "Unbound action acquired a badge");
  MapKey::CacheShortcutHints();
  Check(MapKey::GetShortcutHint(galaxy) == "^G", "Repeated cache preparation changed label");

  MapKey::RegisterAction(galaxy, "show_galaxy", "CTRL-G | F8");
  Check(MapKey::Definition(galaxy).key == "show_galaxy", "Canonical storage key missing");
  Check(MapKey::ReplaceBindings(galaxy, {MapKey::Parse("F7"), MapKey::Parse("F8")}), "Replace action failed");
  Check(MapKey::GetShortcuts(galaxy) == "F7 | F8" && MapKey::GetShortcutHint(galaxy) == "F7",
        "Replacement did not publish full list and fresh hint together");
  Check(!MapKey::ReplaceBindings(galaxy, {MapKey::Parse("INVALID")}), "Invalid replacement was accepted");
  Check(MapKey::GetShortcuts(galaxy) == "F7 | F8", "Invalid replacement lost prior bindings");
  Check(MapKey::ReplaceBindings(galaxy, {}) && !MapKey::HasBinding(galaxy) && MapKey::GetShortcutHint(galaxy).empty(),
        "Unbind left active input or a stale hint");
  Check(MapKey::ReplaceBindings(galaxy, {MapKey::Parse("CTRL-G"), MapKey::Parse("F8")}), "Restore failed");

  // Supply a mapping fixture to test action dispatch through BindingState.
  // This does not assert that Unity returns these mappings on any real layout.
  keyboard_layout::LayoutKeys keys{};
  keys[static_cast<int>(KeyCode::Plus)] = KeyCode::RightBracket;
  keys[static_cast<int>(KeyCode::Slash)] = KeyCode::Alpha7;
  keys[static_cast<int>(KeyCode::Alpha1)] = KeyCode::Alpha1;
  layout_bindings.Replace(keys, Key::Pressed, 0);
  layout_enabled = true;
  MapKey::AddMappedKey(GameFunction::ShowDaily, MapKey::Parse("+"));
  MapKey::AddMappedKey(GameFunction::ShowScrapYard, MapKey::Parse("SHIFT-/"));
  MapKey::AddMappedKey(GameFunction::ShowResearch, MapKey::Parse("("));
  MapKey::AddMappedKey(GameFunction::ShowInventory, MapKey::Parse("1"));
  pressed[static_cast<int>(KeyCode::RightBracket)] = true;
  down[static_cast<int>(KeyCode::RightBracket)] = true;
  Check(MapKey::IsDown(GameFunction::ShowDaily) && MapKey::IsPressed(GameFunction::ShowDaily),
        "Punctuation action did not use resolved physical key");
  down[static_cast<int>(KeyCode::RightBracket)] = false;
  Check(!MapKey::IsDown(GameFunction::ShowDaily) && MapKey::IsPressed(GameFunction::ShowDaily),
        "Held action was mistaken for a new key-down edge");
  down[static_cast<int>(KeyCode::RightBracket)] = true;
  pressed[static_cast<int>(KeyCode::LeftShift)] = true;
  Check(!MapKey::IsDown(GameFunction::ShowDaily), "Unmodified symbol unexpectedly accepts Shift");
  pressed[static_cast<int>(KeyCode::Alpha7)] = true;
  down[static_cast<int>(KeyCode::Alpha7)] = true;
  Check(MapKey::IsDown(GameFunction::ShowScrapYard), "Explicit Shift chord did not use resolved symbol key");
  pressed[static_cast<int>(KeyCode::LeftShift)] = false;
  Check(!MapKey::IsDown(GameFunction::ShowScrapYard), "Explicit modifier requirement was lost");
  pressed[static_cast<int>(KeyCode::LeftParen)] = true;
  down[static_cast<int>(KeyCode::LeftParen)] = true;
  Check(!MapKey::IsDown(GameFunction::ShowResearch), "Unresolved symbol silently fell back to physical");
  pressed[static_cast<int>(KeyCode::Alpha1)] = true;
  down[static_cast<int>(KeyCode::Alpha1)] = true;
  Check(MapKey::IsDown(GameFunction::ShowInventory), "Digit action did not use resolved mapping");
  Check(MapKey::GetShortcuts(GameFunction::ShowScrapYard) == "SHIFT-/", "Resolution rewrote configured chord");

  // German slash needs Shift on either side; plain 7 must not fire slash.
  auto& slash = chords[static_cast<int>(KeyCode::Slash)];
  slash.shift = true;
  constexpr auto slashAction = GameFunction::ShowAlliance;
  MapKey::AddMappedKey(slashAction, MapKey::Parse("/"));
  MapKey::CacheShortcutHints();
  pressed.fill(false);
  down.fill(false);
  pressed[static_cast<int>(KeyCode::Alpha7)] = down[static_cast<int>(KeyCode::Alpha7)] = true;
  Check(!MapKey::IsDown(slashAction), "Bare 7 fired German slash");
  for (const auto shift : {KeyCode::LeftShift, KeyCode::RightShift}) {
    pressed[static_cast<int>(shift)] = true;
    Check(MapKey::IsDown(slashAction) && MapKey::IsPressed(slashAction), "Inferred Shift chord did not dispatch");
    for (const auto extra : {KeyCode::LeftControl, KeyCode::RightAlt, KeyCode::AltGr, KeyCode::LeftWindows}) {
      pressed[static_cast<int>(extra)] = true;
      Check(!MapKey::IsDown(slashAction), "Bare slash stole modified chord");
      pressed[static_cast<int>(extra)] = false;
    }
    pressed[static_cast<int>(shift)] = false;
  }
  Check(MapKey::GetShortcutHint(slashAction) == "/", "Hint replaced configured slash with physical recipe");
  Check(MapKey::GetShortcutHint(GameFunction::ShowScrapYard) == "+/", "Hint changed explicit configured Shift");
  Check(MapKey::GetShortcuts(slashAction) == "/", "Display rewrote configuration");
  const auto explicitCtrl = MapKey::Parse("CTRL-/");
  pressed[static_cast<int>(KeyCode::LeftControl)] = true;
  Check(!MapKey::HasCorrectModifiers(explicitCtrl, true), "Explicit Ctrl bypassed inferred Shift");
  pressed[static_cast<int>(KeyCode::RightShift)] = true;
  Check(MapKey::HasCorrectModifiers(explicitCtrl, true), "Explicit Ctrl was not combined with inferred Shift");
  const auto explicitSide = MapKey::Parse("LSHIFT-/");
  Check(!MapKey::HasCorrectModifiers(explicitSide, true), "Inferred Shift bypassed explicit left side");
  pressed[static_cast<int>(KeyCode::LeftShift)] = true;
  Check(MapKey::HasCorrectModifiers(explicitSide, true), "Explicit left Shift was not accepted");
  Check(MapKey::GetShortcutHint(slashAction) == "/", "Resolution status rewrote configured hint");
  // Simulate the next US generation; no stale German recipe may remain cached.
  slash.shift = false;
  Check(MapKey::GetShortcutHint(slashAction) == "/", "Hint retained previous layout's Shift");

  // The upstream German defaults overlap under minimum modifier matching.
  // Preserve that policy; verify the documented Help remap makes Armada distinct.
  pressed.fill(false);
  down.fill(false);
  keys[static_cast<int>(KeyCode::Quote)] = KeyCode::Backslash;
  keys[static_cast<int>(KeyCode::Caret)] = KeyCode::BackQuote;
  chords[static_cast<int>(KeyCode::Quote)].shift = true;
  layout_bindings.Replace(keys, Key::Pressed, 0);
  MapKey::AddMappedKey(GameFunction::ShowAllianceHelp, MapKey::Parse("SHIFT-'"));
  MapKey::AddMappedKey(GameFunction::ShowAllianceArmada, MapKey::Parse("CTRL-'"));
  pressed[static_cast<int>(KeyCode::LeftControl)] = true;
  pressed[static_cast<int>(KeyCode::LeftShift)] = true;
  pressed[static_cast<int>(KeyCode::Backslash)] = down[static_cast<int>(KeyCode::Backslash)] = true;
  Check(MapKey::IsDown(GameFunction::ShowAllianceHelp) && MapKey::IsDown(GameFunction::ShowAllianceArmada),
        "German default overlap changed without an explicit modifier policy change");
  // Use a spare action slot for the alternative configuration; the public API
  // intentionally has no live binding replacement operation.
  constexpr auto remappedHelp = GameFunction::ShowOfficers;
  MapKey::AddMappedKey(remappedHelp, MapKey::Parse("SHIFT-^"));
  Check(!MapKey::IsDown(remappedHelp) && MapKey::IsDown(GameFunction::ShowAllianceArmada),
        "Documented Help remap still captures Armada");
  pressed[static_cast<int>(KeyCode::LeftControl)] = false;
  pressed[static_cast<int>(KeyCode::Backslash)] = down[static_cast<int>(KeyCode::Backslash)] = false;
  pressed[static_cast<int>(KeyCode::BackQuote)] = down[static_cast<int>(KeyCode::BackQuote)] = true;
  Check(MapKey::IsDown(remappedHelp) && !MapKey::IsDown(GameFunction::ShowAllianceArmada),
        "Documented Shift-caret Help chord failed");
  layout_enabled = false;
  pressed.fill(false);
  down.fill(false);
  pressed[static_cast<int>(KeyCode::Plus)] = true;
  down[static_cast<int>(KeyCode::Plus)] = true;
  Check(MapKey::IsDown(GameFunction::ShowDaily), "Physical mode no longer uses configured key");

  CheckOverlap("I", "SHIFT-I", false); // Inventory must not appear for Shift-I.
  CheckOverlap("I", "I", true);
  CheckOverlap("SHIFT-I", "SHIFT-I", true);   // Artifacts must still appear.
  CheckOverlap("SHIFT-I", "CTRL-I", true);    // Both can match Ctrl+Shift+I.
  CheckOverlap("LSHIFT-I", "RSHIFT-I", true); // Both sides can be held.
  CheckOverlap("LCTRL-I", "CTRL-SHIFT-I", true);
  CheckOverlap("SHIFT-I", "SHIFT-G", false);
  CheckOverlap("NONE", "NONE", false);
  layout_enabled                            = true;
  chords[static_cast<int>(KeyCode::Slash)]  = {KeyCode::Alpha7, true};
  chords[static_cast<int>(KeyCode::Alpha7)] = {KeyCode::Alpha7, false};
  CheckOverlap("/", "7", false);
  CheckOverlap("/", "SHIFT-7", true);
  CheckOverlap("/", "RSHIFT-7", true);
  CheckOverlap("/", "CTRL-SHIFT-7", false);
  CheckOverlap("/", "CTRL-/", false);
  CheckOverlap("/", "/", true);
  CheckOverlap("(", "SHIFT-7", false); // Unresolved layout character.
  layout_enabled = false;
  CheckDuplicate("SHIFT-I", "SHIFT-I", true);
  CheckDuplicate("shift-i", "SHIFT-I", true);
  CheckDuplicate("SHIFT-CTRL-I", "CTRL-SHIFT-I", true);
  CheckDuplicate("SHIFT-SHIFT-I", "SHIFT-I", true);
  CheckDuplicate("CMD-I", "APPLE-I", true);
  CheckDuplicate("EQUAL", "=", true);
  CheckDuplicate("LSHIFT-I", "SHIFT-I", false);
  CheckDuplicate("LSHIFT-I", "RSHIFT-I", false);
  CheckDuplicate("SHIFT-I", "CTRL-I", false); // An overlap is not a duplicate.
  CheckDuplicate("SHIFT-I", "SHIFT-G", false);
  CheckDuplicate("I", "SHIFT-I", false);
  // Current layout equivalence must not collapse two configured identities.
  layout_enabled = true;
  CheckDuplicate("/", "SHIFT-7", false);
  layout_enabled = false;
  std::cout << "Shortcut hint cache tests passed\n";
}
