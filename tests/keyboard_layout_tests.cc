#include "patches/keyboard_layout_mapping.h"
#include "patches/keyboard_layout_refresh.h"
#include "patches/keyboard_layout_windows.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace keyboard_layout;

void Check(bool condition, const char* name)
{
  if (!condition) {
    std::cerr << "FAIL: " << name << '\n';
    std::exit(1);
  }
}

int main()
{
  Check(ToLegacyKey(39) == KeyCode::Y && ToLegacyKey(40) == KeyCode::Z, "distinct enum translation Y/Z");
  Check(ToLegacyKey(6) == KeyCode::Semicolon, "French M punctuation position");
  for (const auto code : {-1, 0, 51, 9999})
    Check(ToLegacyKey(code) == KeyCode::None, "unsupported codes fail closed");

#if _WIN32
  // Use installed fixtures without loading/unloading system keyboard layouts.
  const auto before = GetKeyboardLayout(0);
  HKL existing[256]{};
  const auto count = GetKeyboardLayoutList(256, existing);
  Check(count > 0, "existing Windows layouts available");
  HKL german = nullptr, american = nullptr;
  for (int i = 0; i < count && i < 256; ++i) {
    const auto id = reinterpret_cast<uintptr_t>(existing[i]) & 0xffffffffu;
    if (id == 0x04070407u) german = existing[i];
    if (id == 0x04090409u) american = existing[i];
  }
  if (german && american) {
  Check(FindUnshiftedDeadKey('^', german) == KeyCode::BackQuote, "German circumflex physical position");
  BYTE keyboard_state[256]{};
  wchar_t translated[8]{};
  const auto accent_vk = MapVirtualKeyExW(0x29, MAPVK_VSC_TO_VK_EX, german);
  Check(ToUnicodeEx(accent_vk, 0x29, keyboard_state, translated, 8, 0, german) < 0, "seed pending accent");
  Check(FindUnshiftedDeadKey('^', german) == KeyCode::BackQuote, "lookup with pending accent");
  Check(ToUnicodeEx('A', 0x1e, keyboard_state, translated, 8, 0, german) == 1 && translated[0] == L'\u00e2',
        "native lookup preserves pending accent composition");
  for (const char symbol : {'/', '=', '`', 'a'})
    Check(FindUnshiftedDeadKey(symbol, german) == KeyCode::None, "shifted and non-dead symbols excluded");
  Check(FindUnshiftedDeadKey('^', american) == KeyCode::None, "US shifted caret excluded");
  Check(FindUnshiftedDeadKey('^', nullptr) == KeyCode::None, "missing Windows layout fails closed");
  Check(ResolveWindowsChord('^', "not-the-active-layout").key == KeyCode::None, "layout disagreement fails closed");
  Check(GetKeyboardLayout(0) == before, "lookup does not activate a layout");
  } else {
    std::cout << "SKIP: native dead-key fixtures require already-loaded US and German layouts\n";
  }
#endif

  LayoutKeys us{};
  for (int i = 0; i < 26; ++i) {
    us[static_cast<int>(KeyCode::A) + i] = ToLegacyKey(15 + i);
    Check(us[static_cast<int>(KeyCode::A) + i] == static_cast<KeyCode>(static_cast<int>(KeyCode::A) + i),
          "all US letter positions");
  }
  us[static_cast<int>(KeyCode::Alpha1)] = KeyCode::Alpha1;
  us[static_cast<int>(KeyCode::Slash)] = KeyCode::Slash;
  std::array<bool, static_cast<int>(KeyCode::Max)> held{};
  int frame = 10, clock_calls = 0, held_calls = 0;
  auto is_held = [&](KeyCode key) { return held[static_cast<int>(key)]; };
  BindingState state;
  auto resolve = [&](KeyCode key) {
    return state.Resolve(key, [&] { ++clock_calls; return frame; },
                         [&](KeyCode physical) { ++held_calls; return is_held(physical); });
  };
  Check(resolve(KeyCode::Z) == KeyCode::None && resolve(KeyCode::Alpha1) == KeyCode::None, "pending keys disabled");
  for (const auto key : {KeyCode::Escape, KeyCode::UpArrow, KeyCode::F1, KeyCode::Space, KeyCode::Return,
                        KeyCode::LeftControl, KeyCode::Keypad1, KeyCode::KeypadPlus, KeyCode::Mouse0})
    Check(resolve(key) == key, "named controls keep identity");
  Check(clock_calls == 0 && held_calls == 0, "pending and named keys never query frame or input");

  state.Replace(us, is_held, frame);
  Check(resolve(KeyCode::Z) == KeyCode::None && resolve(KeyCode::Alpha1) == KeyCode::None,
        "entire transition frame suppressed");
  ++frame;
  Check(resolve(KeyCode::Z) == KeyCode::Z, "first later query clears transition");
  clock_calls = held_calls = 0;
  for (int i = 0; i < 1000; ++i) {
    ++frame;
    Check(resolve(KeyCode::Z) == KeyCode::Z && resolve(KeyCode::Alpha1) == KeyCode::Alpha1
              && resolve(KeyCode::Slash) == KeyCode::Slash, "quiet cached mappings");
  }
  Check(clock_calls == 0 && held_calls == 0, "quiet frames perform no clock or held-state polling");

  auto de = us;
  de[static_cast<int>(KeyCode::Y)] = KeyCode::Z;
  de[static_cast<int>(KeyCode::Z)] = KeyCode::Y;
  de[static_cast<int>(KeyCode::Plus)] = KeyCode::RightBracket;
  held[static_cast<int>(KeyCode::Y)] = true;
  held[static_cast<int>(KeyCode::RightBracket)] = true;
  state.Replace(de, is_held, frame);
  Check(resolve(KeyCode::Z) == KeyCode::None, "German transition suppressed");
  ++frame;
  Check(resolve(KeyCode::Y) == KeyCode::Z, "German Y uses US Z");
  Check(resolve(KeyCode::Z) == KeyCode::None && resolve(KeyCode::Plus) == KeyCode::None,
        "held letter and punctuation blocked");
  ++frame;
  Check(resolve(KeyCode::Z) == KeyCode::None, "hold remains blocked");
  held.fill(false);
  Check(resolve(KeyCode::Z) == KeyCode::Y && resolve(KeyCode::Plus) == KeyCode::RightBracket,
        "release unblocks only the queried target");
  Check(resolve(KeyCode::LeftParen) == KeyCode::None, "missing symbol has no physical fallback");
  clock_calls = held_calls = 0;
  ++frame;
  Check(resolve(KeyCode::Plus) == KeyCode::RightBracket && resolve(KeyCode::Z) == KeyCode::Y,
        "released mapping cached");
  Check(clock_calls == 0 && held_calls == 0, "release checks stop once target is unblocked");

  auto fr = us;
  fr[static_cast<int>(KeyCode::A)] = KeyCode::Q;
  fr[static_cast<int>(KeyCode::Q)] = KeyCode::A;
  fr[static_cast<int>(KeyCode::W)] = KeyCode::Z;
  fr[static_cast<int>(KeyCode::Z)] = KeyCode::W;
  fr[static_cast<int>(KeyCode::M)] = ToLegacyKey(6);
  fr[static_cast<int>(KeyCode::Y)] = KeyCode::None;
  state.Replace(fr, is_held, frame);
  ++frame;
  Check(resolve(KeyCode::A) == KeyCode::Q && resolve(KeyCode::Q) == KeyCode::A, "French A/Q");
  Check(resolve(KeyCode::W) == KeyCode::Z && resolve(KeyCode::Z) == KeyCode::W, "French W/Z");
  Check(resolve(KeyCode::M) == KeyCode::Semicolon && resolve(KeyCode::Y) == KeyCode::None, "French M and missing key");

  RefreshState refresh;
  // A failed symbol lookup publishes a partial map, without losing unrelated
  // letters/digits. A later layout generation must restore that symbol.
  auto partial = us;
  partial[static_cast<int>(KeyCode::Slash)] = KeyCode::None;
  state.Replace(partial, is_held, frame);
  ++frame;
  Check(resolve(KeyCode::Slash) == KeyCode::None && resolve(KeyCode::Z) == KeyCode::Z
            && resolve(KeyCode::Alpha1) == KeyCode::Alpha1, "unavailable symbol isolates letters and digits");
  state.Replace(us, is_held, frame);
  ++frame;
  Check(resolve(KeyCode::Slash) == KeyCode::Slash, "later mapping restores unavailable symbol");
  Check(refresh.Consume(), "one initial lookup");
  for (int i = 0; i < 1000; ++i)
    Check(!refresh.Consume(), "no notification means no refresh, regardless of elapsed frames");
  refresh.Invalidate();
  refresh.Invalidate();
  Check(refresh.Consume() && !refresh.Consume(), "notifications coalesce");
  refresh.Invalidate(); // A callback during rebuild must survive consumption.
  Check(refresh.Consume(), "notification during refresh is preserved");
  state.Replace(us, is_held, frame);
  refresh.Invalidate();
  Check(refresh.Consume(), "same-frame event refreshes too");
  state.Replace(de, is_held, frame);
  Check(resolve(KeyCode::Y) == KeyCode::None, "same-frame rebuild cannot clear suppression");
  ++frame;
  Check(resolve(KeyCode::Z) == KeyCode::Y, "next frame resolves after same-frame event");
  state.Clear();
  clock_calls = held_calls = 0;
  Check(resolve(KeyCode::Z) == KeyCode::None && resolve(KeyCode::Plus) == KeyCode::None
            && resolve(KeyCode::Alpha1) == KeyCode::None, "device/setup failure clears all printable mappings");
  Check(clock_calls == 0 && held_calls == 0, "unavailable mappings never poll");
  for (const auto key : {KeyCode::Alpha0, KeyCode::Alpha9, KeyCode::Exclaim, KeyCode::At,
                        KeyCode::LeftBracket, KeyCode::Tilde, KeyCode::Pipe, KeyCode::Minus})
    Check(IsLayoutKey(key), "digits and punctuation eligible");
  Check(!IsLayoutKey(KeyCode::None) && !IsLayoutKey(KeyCode::Delete)
            && !IsLayoutKey(static_cast<KeyCode>(65)), "enum gaps excluded");
  std::cout << "PASS: mapping, event-only refresh, zero quiet-frame polling, transitions, held keys, failure clearing\n";
}
