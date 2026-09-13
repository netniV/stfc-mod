#pragma once

#if _WIN32
#include "keyboard_layout_mapping.h"

#include <Windows.h>
#include <string_view>

namespace keyboard_layout
{
inline constexpr unsigned kWindowsLayoutScans[]{
    0x29, 0x28, 0x27, 0x33, 0x34, 0x35, 0x2b, 0x1a, 0x1b, 0x0c, 0x0d, 0x1e, 0x30, 0x2e, 0x20, 0x12,
    0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16,
    0x2f, 0x11, 0x2d, 0x15, 0x2c, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
};

// Scan codes for the same supported US-reference positions as ToLegacyKey(4..50).
// These are physical positions, not a German (or other language) character table.
inline KeyCode FindUnshiftedDeadKey(char character, HKL layout)
{
  if (!layout)
    return KeyCode::None;
  KeyCode result = KeyCode::None;
  for (unsigned index = 0; index < std::size(kWindowsLayoutScans); ++index) {
    const auto vk    = MapVirtualKeyExW(kWindowsLayoutScans[index], MAPVK_VSC_TO_VK_EX, layout);
    const auto value = vk ? MapVirtualKeyExW(vk, MAPVK_VK_TO_CHAR, layout) : 0;
    // MAPVK_VK_TO_CHAR marks dead keys with its top bit. Unlike ToUnicodeEx,
    // this query does not perform text composition or consume a pending accent.
    if ((value & 0x80000000u) && (value & 0xffffu) == static_cast<unsigned char>(character)) {
      if (result != KeyCode::None)
        return KeyCode::None; // Ambiguous positions must not choose an arbitrary key.
      result = ToLegacyKey(4 + index);
    }
  }
  return result;
}

inline ResolvedChord FindWindowsChord(char character, HKL layout)
{
  if (!layout)
    return {};
  // Uppercase config tokens name letters, not uppercase text.
  if (character >= 'A' && character <= 'Z')
    character += 'a' - 'A';
  const auto translated = VkKeyScanExW(static_cast<unsigned char>(character), layout);
  if (translated == -1)
    return {FindUnshiftedDeadKey(character, layout), false};
  const auto modifiers = (static_cast<unsigned>(translated) >> 8) & 0xff;
  // Inferring Ctrl/Alt (including AltGr) needs a separate modifier policy.
  if (modifiers & ~1u)
    return {};
  const auto scan = MapVirtualKeyExW(static_cast<unsigned>(translated) & 0xff, MAPVK_VK_TO_VSC_EX, layout);
  for (unsigned index = 0; index < std::size(kWindowsLayoutScans); ++index) {
    if (scan == kWindowsLayoutScans[index])
      return {ToLegacyKey(4 + index), (modifiers & 1) != 0};
  }
  return {};
}

inline ResolvedChord ResolveWindowsChord(char character, std::string_view unity_layout)
{
  const auto layout = GetKeyboardLayout(0);
  char       name[KL_NAMELENGTH]{};
  if (!layout || !GetKeyboardLayoutNameA(name) || unity_layout != name)
    return {};
  const auto chord = FindWindowsChord(character, layout);
  return GetKeyboardLayout(0) == layout ? chord : ResolvedChord{};
}
} // namespace keyboard_layout
#endif
