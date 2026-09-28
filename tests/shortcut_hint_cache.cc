// Link production MapKey/ModifierKey parsing, action dispatch and hint caching from mods.lib.
// Key token parsing and input are test fixtures; layout lookup is injected below.
// These tests do not exercise Unity lookup, native notifications or legacy input caching.
#include "patches/mapkey.h"
#include "patches/keyboard_layout.h"
#include "patches/keyboard_layout_mapping.h"
#include "defaultconfig.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <utility>

static std::array<bool, static_cast<int>(KeyCode::Max)> pressed{};
static std::array<bool, static_cast<int>(KeyCode::Max)> down{};
static keyboard_layout::BindingState layout_bindings;
static bool layout_enabled = false;
static std::array<keyboard_layout::ResolvedChord, keyboard_layout::LayoutKeyCount> chords;
static KeyCode claimed = KeyCode::None;

KeyCode Key::Parse(std::string_view key)
{
  static constexpr std::pair<std::string_view, KeyCode> tokens[] = {
      {"=", KeyCode::Equals}, {"Z", KeyCode::Z}, {"LSHIFT", KeyCode::LeftShift},
      {"F7", KeyCode::F7}, {"F8", KeyCode::F8}, {"G", KeyCode::G}, {"+", KeyCode::Plus},
      {"/", KeyCode::Slash}, {"(", KeyCode::LeftParen}, {"1", KeyCode::Alpha1},
      {"'", KeyCode::Quote}, {"^", KeyCode::Caret},
      {"W", KeyCode::W}, {"A", KeyCode::A}, {"S", KeyCode::S}, {"D", KeyCode::D},
      {"UP", KeyCode::UpArrow}, {"DOWN", KeyCode::DownArrow},
      {"LEFT", KeyCode::LeftArrow}, {"RIGHT", KeyCode::RightArrow},
  };
  for (const auto& [token, code] : tokens) {
    if (key == token)
      return code;
  }
  return KeyCode::None;
}
bool Key::IsModifier(KeyCode key) { return key == KeyCode::LeftShift; }
bool Key::Pressed(KeyCode key) { return pressed[static_cast<int>(key)]; }
bool Key::Down(KeyCode key) { return down[static_cast<int>(key)]; }
bool Key::IsModified() {
  for (auto key : {KeyCode::LeftShift, KeyCode::RightShift, KeyCode::LeftControl, KeyCode::RightControl,
                   KeyCode::LeftAlt, KeyCode::RightAlt, KeyCode::AltGr, KeyCode::LeftCommand,
                   KeyCode::RightCommand, KeyCode::LeftWindows, KeyCode::RightWindows})
    if (Key::Pressed(key)) return true;
  return false;
}
void Key::ClaimDirectionalInput(KeyCode key) { claimed = key; }

namespace keyboard_layout
{
KeyCode Resolve(KeyCode configured) { return layout_enabled ? layout_bindings.Resolve(configured, [] { return 1; }, Key::Pressed) : configured; }
ResolvedChord ResolveChord(KeyCode configured) {
  return {Resolve(configured), layout_enabled && IsLayoutKey(configured)
                               && (chords[static_cast<int>(configured)].shift) != 0};
}
} // namespace keyboard_layout

void Check(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
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

  // Movement uses the production matcher. Physical defaults stay active, and
  // extra modifiers must not turn an unmodified movement chord into a match.
  pressed.fill(false);
  down.fill(false);
  for (const auto [action, defaults] : {
           std::pair{GameFunction::MoveUp, DefaultConfig::Shortcuts::move_up},
           std::pair{GameFunction::MoveDown, DefaultConfig::Shortcuts::move_down},
           std::pair{GameFunction::MoveLeft, DefaultConfig::Shortcuts::move_left},
           std::pair{GameFunction::MoveRight, DefaultConfig::Shortcuts::move_right}}) {
    std::string_view remaining = defaults;
    while (!remaining.empty()) {
      const auto end     = remaining.find('|');
      const auto binding = MapKey::Parse(std::string(remaining.substr(0, end)));
      Check(binding.Key != KeyCode::None, "Invalid movement default");
      MapKey::AddMappedKey(action, binding);
      pressed[static_cast<int>(binding.Key)] = true;
      down[static_cast<int>(binding.Key)]    = true;
      Check(MapKey::IsDown(action) && MapKey::IsPressed(action), "Default movement key did not match");
      for (auto modifier : {KeyCode::LeftControl, KeyCode::RightControl, KeyCode::LeftShift,
                             KeyCode::LeftAlt, KeyCode::LeftCommand}) {
        pressed[static_cast<int>(modifier)] = true;
        down[static_cast<int>(modifier)]    = true;
        Check(!MapKey::IsDown(action) && !MapKey::IsPressed(action), "Modified chord leaked into plain movement");
        pressed[static_cast<int>(modifier)] = false;
        down[static_cast<int>(modifier)]    = false;
      }
      pressed[static_cast<int>(binding.Key)] = false;
      down[static_cast<int>(binding.Key)]    = false;
      Check(!MapKey::IsDown(action) && !MapKey::IsPressed(action), "Released movement key remained active");
      if (end == std::string_view::npos)
        break;
      remaining.remove_prefix(end + 1);
    }
  }

  // A modified movement binding must not claim its own direction. Other
  // actions still claim the same chord through its resolved physical key.
  MapKey::AddMappedKey(GameFunction::MoveDown, MapKey::Parse("CTRL-DOWN"));
  pressed[static_cast<int>(KeyCode::LeftControl)] = true;
  down[static_cast<int>(KeyCode::LeftControl)]    = true;
  pressed[static_cast<int>(KeyCode::DownArrow)]   = true;
  down[static_cast<int>(KeyCode::DownArrow)]      = true;
  claimed = KeyCode::None;
  Check(MapKey::IsDown(GameFunction::MoveDown) && MapKey::IsPressed(GameFunction::MoveDown),
        "Explicit movement chord did not match");
  Check(claimed == KeyCode::None, "Movement claimed and blocked itself");
  MapKey::AddMappedKey(toggle, MapKey::Parse("CTRL-DOWN"));
  Check(MapKey::IsDown(toggle) && MapKey::IsPressed(toggle) && claimed == KeyCode::DownArrow,
        "Other chord lost directional ownership");
  std::cout << "Shortcut hint, layout and movement binding tests passed\n";
}
