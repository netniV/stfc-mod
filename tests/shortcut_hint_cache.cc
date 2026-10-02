// Link against the built mods library. Only Unity-dependent Key operations are stubbed;
// MapKey parsing, ModifierKey parsing, binding detection, and hint caching are production code.
#include "patches/mapkey.h"
#include "defaultconfig.h"
#include <array>

#include <cstdlib>
#include <iostream>

static std::array<bool, static_cast<int>(KeyCode::Max)> held{};
static KeyCode claimed = KeyCode::None;

KeyCode Key::Parse(std::string_view key)
{
  if (key == "F7") return KeyCode::F7;
  if (key == "F8") return KeyCode::F8;
  if (key == "G") return KeyCode::G;
  for (const auto [token, code] : {std::pair{"W", KeyCode::W}, {"A", KeyCode::A}, {"S", KeyCode::S},
                                   {"D", KeyCode::D}, {"UP", KeyCode::UpArrow}, {"DOWN", KeyCode::DownArrow},
                                   {"LEFT", KeyCode::LeftArrow}, {"RIGHT", KeyCode::RightArrow}})
    if (key == token) return code;
  return KeyCode::None;
}
bool Key::IsModifier(KeyCode) { return false; }
bool Key::Pressed(KeyCode key) { return held[static_cast<int>(key)]; }
bool Key::Down(KeyCode key) { return Key::Pressed(key); }
bool Key::IsModified() {
  for (auto key : {KeyCode::LeftControl, KeyCode::RightControl, KeyCode::LeftShift, KeyCode::LeftAlt, KeyCode::LeftCommand})
    if (Key::Pressed(key)) return true;
  return false;
}
void Key::ClaimDirectionalInput(KeyCode key) { claimed = key; }

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
  // Pan uses the production shortcut matcher: both default alternatives work,
  // but adding a modifier must not also match the unmodified movement action.
  for (const auto [action, defaults] : {
           std::pair{GameFunction::MoveUp, DefaultConfig::Shortcuts::move_up},
           std::pair{GameFunction::MoveDown, DefaultConfig::Shortcuts::move_down},
           std::pair{GameFunction::MoveLeft, DefaultConfig::Shortcuts::move_left},
           std::pair{GameFunction::MoveRight, DefaultConfig::Shortcuts::move_right}}) {
    std::string_view remaining = defaults;
    while (!remaining.empty()) {
      auto end = remaining.find('|');
      auto binding = MapKey::Parse(std::string(remaining.substr(0, end)));
      Check(binding.Key != KeyCode::None, "Invalid movement default");
      MapKey::AddMappedKey(action, binding);
      held[static_cast<int>(binding.Key)] = true;
      Check(MapKey::IsPressed(action), "Default movement key did not match");
      for (auto modifier : {KeyCode::LeftControl, KeyCode::RightControl, KeyCode::LeftShift,
                             KeyCode::LeftAlt, KeyCode::LeftCommand}) {
        held[static_cast<int>(modifier)] = true;
        Check(!MapKey::IsPressed(action), "Modified chord leaked into plain movement");
        held[static_cast<int>(modifier)] = false;
      }
      held[static_cast<int>(binding.Key)] = false;
      Check(!MapKey::IsPressed(action), "Released movement key remained active");
      if (end == std::string_view::npos) break;
      remaining.remove_prefix(end + 1);
    }
  }
  // A modified movement binding must not claim its own direction. Non-movement
  // actions still claim it, preserving the existing hold-until-release guard.
  MapKey::AddMappedKey(GameFunction::MoveDown, MapKey::Parse("CTRL-DOWN"));
  held[static_cast<int>(KeyCode::LeftControl)] = true;
  held[static_cast<int>(KeyCode::DownArrow)] = true;
  claimed = KeyCode::None;
  Check(MapKey::IsDown(GameFunction::MoveDown) && MapKey::IsPressed(GameFunction::MoveDown),
        "Explicit movement chord did not match");
  Check(claimed == KeyCode::None, "Movement claimed and blocked itself");
  MapKey::AddMappedKey(toggle, MapKey::Parse("CTRL-DOWN"));
  Check(MapKey::IsDown(toggle) && claimed == KeyCode::DownArrow, "Other chord lost directional ownership");
  held.fill(false);
  std::cout << "Shortcut hint and movement binding tests passed\n";

}
