#include "patches/keyboard_layout_windows.h"
#include <iostream>
#include <stdexcept>

void Check(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}

int main()
{
#if _WIN32
  using namespace keyboard_layout;
  HKL       layouts[256]{};
  const int count = GetKeyboardLayoutList(256, layouts);
  HKL       de = nullptr, us = nullptr;
  for (int i = 0; i < count && i < 256; ++i) {
    const auto id = reinterpret_cast<uintptr_t>(layouts[i]) & 0xffffffffu;
    if (id == 0x04070407u)
      de = layouts[i];
    if (id == 0x04090409u)
      us = layouts[i];
  }
  if (!de || !us) {
    std::cout << "SKIP: requires already-loaded standard US and German layouts\n";
    return 0;
  }
  // Query explicit layouts without activating, loading or unloading any layout.
  const auto original = GetKeyboardLayout(0);
  for (const auto [character, key] :
       {std::pair{'/', KeyCode::Alpha7}, {'=', KeyCode::Alpha0}, {'`', KeyCode::Equals}, {'\'', KeyCode::Backslash}}) {
    const auto chord = FindWindowsChord(character, de);
    Check(chord.key == key && chord.shift, "German shifted punctuation");
  }
  Check(FindWindowsChord('^', de).key == KeyCode::BackQuote && !FindWindowsChord('^', de).shift,
        "German unshifted dead key");
  Check(FindWindowsChord('Z', de).key == KeyCode::Y && !FindWindowsChord('Z', de).shift,
        "Uppercase config letter does not infer Shift");
  Check(FindWindowsChord('@', de).key == KeyCode::None, "AltGr stays unsupported");
  Check(FindWindowsChord('/', us).key == KeyCode::Slash && !FindWindowsChord('/', us).shift, "US slash");
  Check(FindWindowsChord('\'', us).key == KeyCode::Quote && !FindWindowsChord('\'', us).shift, "US apostrophe");
  Check(FindWindowsChord('/', nullptr).key == KeyCode::None, "Absent layout");
  Check(ResolveWindowsChord('/', "invalid").key == KeyCode::None, "Layout mismatch fails closed");
  Check(GetKeyboardLayout(0) == original, "Test did not switch layout");
#endif
  std::cout << "Keyboard chord tests passed\n";
}
