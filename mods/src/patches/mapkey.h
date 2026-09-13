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
  struct ActionDefinition {
    std::string key, defaultBinding;
  };
  MapKey();

  static MapKey Parse(std::string_view key);
  static void   AddMappedKey(GameFunction gameFunction, MapKey mappedKey);
  // Config registers canonical storage identities, including unbound actions.
  static void RegisterAction(GameFunction gameFunction, std::string_view key, std::string_view defaultBinding);
  static const ActionDefinition&    Definition(GameFunction gameFunction);
  static const std::vector<MapKey>& Bindings(GameFunction gameFunction);
  // UI-thread replacement of one complete action. Prepare before publishing so
  // a failed allocation cannot leave a partially edited binding list.
  static bool ReplaceBindings(GameFunction gameFunction, std::vector<MapKey> bindings);
  static bool HasBinding(GameFunction gameFunction);
  static bool IsPressed(GameFunction gameFunction);
  static bool IsDown(GameFunction gameFunction);
  static bool HasCorrectModifiers(const MapKey& mapKey, bool requiredShift = false);
  // Whether some held modifier combination can match both bindings under the
  // current layout. Editor-only advisory; action contexts are not considered.
  static bool MayOverlap(const MapKey& first, const MapKey& second);
  // Configured identity, independent of layout: modifier order and aliases do
  // not create another binding, but generic and sided modifiers stay distinct.
  static bool SameBinding(const MapKey& first, const MapKey& second);

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
  static std::array<ActionDefinition, (int)GameFunction::Max>    definitions;

  bool        hasModifiers;
  std::string shortcutHint;
};
