#pragma once

#include <prime/KeyCode.h>

#include <array>
#include <string>
#include <unordered_map>

class Key
{
private:
  static const std::unordered_map<std::string, KeyCode> mappedKeys;

  static int cacheInputFocused;
  static int cacheInputModified;
  static int cacheFrame;

  static std::array<int, (int)KeyCode::Max>  cacheKeyPressed;
  static std::array<int, (int)KeyCode::Max>  cacheKeyDown;
  static std::array<bool, (int)KeyCode::Max> claimedDirectionalInput;

  static void EnsureCurrentFrame();

public:
  static void    ClearInputFocus();
  static void    ResetCache();
  static KeyCode Parse(std::string_view key);

  static bool Pressed(KeyCode key);
  static bool Down(KeyCode key);

  static void ClaimDirectionalInput(KeyCode key);
  static bool IsDirectionalInputClaimed();

  static bool IsModifier(KeyCode key);

  static bool IsModified();
  static bool IsInputFocused();
  static bool HasShift();
  static bool HasAlt();
  static bool HasCtrl();
};
