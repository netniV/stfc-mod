#pragma once

#include "modifierkey.h"
#include <patches/gamefunctions.h>
#include <prime/KeyCode.h>

#include <array>
#include <string>
#include <vector>

class MapKey
{
public:
  MapKey();

  static MapKey Parse(std::string_view key);
  static void   AddMappedKey(GameFunction gameFunction, MapKey mappedKey);
  static bool   IsPressed(GameFunction gameFunction);
  static bool   IsDownUnsafe(GameFunction gameFunction);
  static bool   IsDown(GameFunction gameFunction);
  static bool   HasCorrectModifiers(MapKey mapKey);

  static std::string GetShortcuts(GameFunction gameFunction);

  std::string GetParsedValues() const;

  std::vector<ModifierKey> Modifiers;
  std::vector<std::string> Shortcuts;

  KeyCode Key;

private:
  static std::array<std::vector<MapKey>, (int)GameFunction::Max> mappedKeys;

  bool hasModifiers;
};
