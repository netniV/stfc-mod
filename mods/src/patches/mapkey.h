#pragma once

#include "modifierkey.h"
#include <patches/gamefunctions.h>
#include <prime/KeyCode.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

class MapKey
{
public:
  MapKey();

  static MapKey Parse(std::string_view key);
  static void   AddMappedKey(GameFunction gameFunction, MapKey mappedKey);
  static bool   HasBinding(GameFunction gameFunction);
  static bool   IsPressed(GameFunction gameFunction);
  static bool   IsDown(GameFunction gameFunction);
  static bool   HasCorrectModifiers(const MapKey& mapKey);

  static std::string GetShortcuts(GameFunction gameFunction);
  static std::string GetShortcutHint(GameFunction gameFunction);
  // Call once after config parsing, only when shortcut hints are enabled.
  static void CacheShortcutHints();

  std::string GetParsedValues() const;

  std::vector<ModifierKey> Modifiers;
  std::vector<std::string> Shortcuts;

  KeyCode Key;

private:
  static std::array<std::vector<MapKey>, (int)GameFunction::Max> mappedKeys;

  bool        hasModifiers;
  std::string shortcutHint;
};
