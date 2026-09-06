#pragma once

#include <prime/KeyCode.h>

#include <array>

namespace keyboard_layout
{
constexpr bool IsLetter(KeyCode key)
{ return key >= KeyCode::A && key <= KeyCode::Z; }

// Unity.InputSystem.Key and legacy UnityEngine.KeyCode are different enums.
// Explicit US-reference positions, including punctuation (French M is at US ';').
constexpr KeyCode ToLegacyKey(int input_system_key)
{
  constexpr std::array keys = {
      KeyCode::None,
      KeyCode::Space,
      KeyCode::Return,
      KeyCode::Tab,
      KeyCode::BackQuote,
      KeyCode::Quote,
      KeyCode::Semicolon,
      KeyCode::Comma,
      KeyCode::Period,
      KeyCode::Slash,
      KeyCode::Backslash,
      KeyCode::LeftBracket,
      KeyCode::RightBracket,
      KeyCode::Minus,
      KeyCode::Equals,
      KeyCode::A,
      KeyCode::B,
      KeyCode::C,
      KeyCode::D,
      KeyCode::E,
      KeyCode::F,
      KeyCode::G,
      KeyCode::H,
      KeyCode::I,
      KeyCode::J,
      KeyCode::K,
      KeyCode::L,
      KeyCode::M,
      KeyCode::N,
      KeyCode::O,
      KeyCode::P,
      KeyCode::Q,
      KeyCode::R,
      KeyCode::S,
      KeyCode::T,
      KeyCode::U,
      KeyCode::V,
      KeyCode::W,
      KeyCode::X,
      KeyCode::Y,
      KeyCode::Z,
      KeyCode::Alpha1,
      KeyCode::Alpha2,
      KeyCode::Alpha3,
      KeyCode::Alpha4,
      KeyCode::Alpha5,
      KeyCode::Alpha6,
      KeyCode::Alpha7,
      KeyCode::Alpha8,
      KeyCode::Alpha9,
      KeyCode::Alpha0,
  };
  return input_system_key > 0 && input_system_key < static_cast<int>(keys.size()) ? keys[input_system_key]
                                                                                  : KeyCode::None;
}

using LetterKeys = std::array<KeyCode, 26>;

// Physical input caches remain physical. Only action bindings are translated.
// Suppress the transition frame and held keys until release, so a layout change
// cannot turn an existing hold into a different action.
class BindingState
{
public:
  template <typename Held> void BeginFrame(Held held)
  {
    transition_ = false;
    for (std::size_t i = 0; i < blocked_.size(); ++i) {
      if (blocked_[i] && !held(static_cast<KeyCode>(i)))
        blocked_[i] = false;
    }
  }

  template <typename Held> void Replace(const LetterKeys& keys, Held held)
  {
    keys_       = keys;
    transition_ = true;
    blocked_.fill(false);
    for (auto key : keys_) {
      if (key != KeyCode::None)
        blocked_[static_cast<int>(key)] = held(key);
    }
  }

  KeyCode Resolve(KeyCode configured) const
  {
    if (!IsLetter(configured))
      return configured;
    const auto key = keys_[static_cast<int>(configured) - static_cast<int>(KeyCode::A)];
    return transition_ || blocked_[static_cast<int>(key)] ? KeyCode::None : key;
  }

private:
  LetterKeys                                       keys_{};
  std::array<bool, static_cast<int>(KeyCode::Max)> blocked_{};
  bool                                             transition_ = false;
};
} // namespace keyboard_layout
