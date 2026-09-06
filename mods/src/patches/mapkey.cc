#include "mapkey.h"
#include "gamefunctions.h"
#include "key.h"
#include "modifierkey.h"
#include "str_utils.h"
#include <prime/KeyCode.h>

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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
          if (mapKey.hasModifiers) {
            Key::ClaimDirectionalInput(mapKey.Key);
          }
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
          if (mapKey.hasModifiers) {
            Key::ClaimDirectionalInput(mapKey.Key);
          }
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
