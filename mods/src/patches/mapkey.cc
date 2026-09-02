#include "mapkey.h"
#include "gamefunctions.h"
#include "modifierkey.h"
#include "str_utils.h"
#include <prime/KeyCode.h>

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::string CompactShortcutToken(std::string_view token, bool is_primary_key)
{
  // Use AutoHotkey's established ASCII modifier notation so the native game's limited font can render every badge.
  if (token == "SHIFT")
    return "+";
  if (token == "CTRL")
    return "^";
  if (token == "ALT" || token == "ALTGR")
    return "!";
  if (token == "APPLE" || token == "CMD" || token == "WIN")
    return "#";
  if (token == "LSHIFT")
    return "<+";
  if (token == "RSHIFT")
    return ">+";
  if (token == "LCTRL")
    return "<^";
  if (token == "RCTRL")
    return ">^";
  if (token == "LALT")
    return "<!";
  if (token == "RALT")
    return ">!";
  if (token == "LAPPLE" || token == "LCOM" || token == "LWIN")
    return "<#";
  if (token == "RAPPLE" || token == "RCOM" || token == "RWIN")
    return ">#";
  if (is_primary_key && token == "+")
    return "PLS";
  if (is_primary_key && token == "^")
    return "CAR";
  if (is_primary_key && token == "!")
    return "EXC";
  if (is_primary_key && token == "#")
    return "HSH";
  if (token == "SPACE")
    return "SPC";
  if (token == "MOUSE0")
    return "M0";
  if (token == "MOUSE1")
    return "M1";
  if (token == "MOUSE2")
    return "M2";
  if (token == "MOUSE3")
    return "M3";
  if (token == "MOUSE4")
    return "M4";
  if (token == "MOUSE5")
    return "M5";
  if (token == "MOUSE6")
    return "M6";
  if (token == "ENTER" || token == "RETURN")
    return "ENT";
  if (token == "ESCAPE")
    return "ESC";
  if (token == "TAB")
    return "TAB";
  if (token == "BACKSPACE")
    return "BS";
  if (token == "DELETE")
    return "DEL";
  if (token == "MINUS")
    return "-";
  if (token == "EQUAL")
    return "=";
  if (token == "LEFT")
    return "LT";
  if (token == "RIGHT")
    return "RT";
  if (token == "UP")
    return "UP";
  if (token == "DOWN")
    return "DN";
  if (token == "PGUP")
    return "PU";
  if (token == "PGDOWN")
    return "PD";
  if (token == "HOME")
    return "HM";
  if (token == "END")
    return "END";
  return std::string(token);
}

std::string CompactShortcutForHint(const std::vector<std::string>& shortcuts)
{
  std::string compact;
  for (size_t index = 0; index < shortcuts.size(); ++index) {
    compact.append(CompactShortcutToken(shortcuts[index], index + 1 == shortcuts.size()));
  }
  return compact;
}
} // namespace

MapKey::MapKey()
{
  this->Key          = KeyCode::None;
  this->hasModifiers = false;
}

MapKey MapKey::Parse(std::string_view key)
{
  auto strippedKey = StripTrailingAsciiWhitespace(key);
  auto lowerKey    = AsciiStrToUpper(strippedKey);
  auto wantedKeys  = StrSplit(lowerKey, '-');

  auto mapKey = new MapKey();
  for (std::string_view wantedKey : wantedKeys) {
    auto modifier = ModifierKey::Parse(wantedKey);
    if (modifier.HasModifiers()) {
      mapKey->hasModifiers = true;
      mapKey->Modifiers.emplace_back(modifier);
      mapKey->Shortcuts.emplace_back(wantedKey);
    } else {
      auto parsedKey = Key::Parse(wantedKey);

      if (Key::IsModifier(parsedKey)) {
        continue;
      }

      if (parsedKey != KeyCode::None) {
        mapKey->Key = parsedKey;
        mapKey->Shortcuts.emplace_back(wantedKey);
      }
    }

#ifndef NDEBUG
    if (mapKey->Key == KeyCode::X) {
      std::cout << "\n\n----------\nX key:\n" << mapKey << "\n----------\n\n";
    }
#endif
  }

  mapKey->shortcutHint = CompactShortcutForHint(mapKey->Shortcuts);
  return *mapKey;
}

std::string MapKey::GetShortcuts(GameFunction gameFunction)
{
  const auto &mapKeys = MapKey::mappedKeys[gameFunction];

  bool appendPipe = false;

  std::string shortcuts = "";
  for (const MapKey &mapKey : mapKeys) {
    if (appendPipe) {
      shortcuts.append(" | ");
    }
    shortcuts.append(mapKey.GetParsedValues());
    appendPipe = true;
  }

  return shortcuts;
}

std::string MapKey::GetShortcutHint(GameFunction gameFunction)
{
  const auto& mapKeys = MapKey::mappedKeys[gameFunction];
  return mapKeys.empty() ? "" : mapKeys.front().shortcutHint;
}

void MapKey::AddMappedKey(GameFunction gameFunction, MapKey mappedKey)
{
  MapKey::mappedKeys[gameFunction].emplace_back(mappedKey);
}

bool MapKey::IsPressed(GameFunction gameFunction)
{
  const auto &mapKeys = MapKey::mappedKeys[(int)gameFunction];
  for (const MapKey &mapKey : mapKeys) {
    if (mapKey.Key != KeyCode::None) {
      if (Key::Pressed(mapKey.Key)) {
        if (MapKey::HasCorrectModifiers(mapKey)) {
          return true;
        }
      }
    }
  }

  return false;
}

bool MapKey::IsDown(GameFunction gameFunction)
{
  const auto &mapKeys = MapKey::mappedKeys[(int)gameFunction];
  for (const MapKey &mapKey : mapKeys) {
    if (mapKey.Key != KeyCode::None) {
      if (Key::Down(mapKey.Key)) {
        if (MapKey::HasCorrectModifiers(mapKey)) {
          return true;
        }
      }
    }
  }

  return false;
}

bool MapKey::HasCorrectModifiers(MapKey mapKey)
{
  auto        result  = false;
  std::string section = "non set";
  if (!mapKey.hasModifiers) {
    section = "no modifiers";
    result  = !Key::IsModified();
  } else {
    result = true;
    for (ModifierKey modifier : mapKey.Modifiers) {
      if (!modifier.IsPressed()) {
        section = modifier.GetParsedValues();
        result  = false;
        break;
      }
    }
  }

#ifndef NDEBUG
  if (mapKey.Key == KeyCode::Backslash && Key::Pressed(KeyCode::Backslash)) {
    std::cout << "HasCorrectModifiers(" << mapKey.GetParsedValues() << "): [" << section << "] " << result << "\n";
  }
#endif

  return result;
}

std::string MapKey::GetParsedValues() const
{
  std::string output = "";
  for (const std::string_view key : this->Shortcuts) {
    if (output.length()) {
      output.append("-");
    }
    output.append(key);
  }

  return output;
}

std::array<std::vector<MapKey>, (int)GameFunction::Max> MapKey::mappedKeys = {};

std::vector<std::string> Shortcuts = {};
std::vector<ModifierKey> Modifiers = {};

bool    hasModifiers = false;
KeyCode Key          = KeyCode::None;
