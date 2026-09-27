#pragma once

#include <prime/KeyCode.h>

#include <array>

namespace keyboard_layout
{
struct ResolvedChord {
  KeyCode key = KeyCode::None;
  bool shift = false;
};
// Printable keys accepted by Key::Parse. Named controls (including Space and
// explicit numpad keys) keep their identity, independent of display-name lookup.
constexpr bool IsLayoutKey(KeyCode key)
{
  return (key >= KeyCode::Exclaim && key <= KeyCode::At)
         || (key >= KeyCode::LeftBracket && key <= KeyCode::Tilde);
}

constexpr std::size_t LayoutKeyCount = static_cast<int>(KeyCode::Tilde) + 1;

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

using LayoutKeys = std::array<KeyCode, LayoutKeyCount>;

// Physical input caches remain physical. Only action bindings are translated.
// Suppress the transition frame and held keys until release, so a layout change
// cannot turn an existing hold into a different action.
class BindingState
{
public:
  void Clear()
  {
    keys_ = {};
    blocked_.fill(false);
    transition_ = false;
  }

  template <typename Held> void Replace(const LayoutKeys& keys, Held held, int frame)
  {
    keys_       = keys;
    transition_ = true;
    transition_frame_ = frame;
    blocked_.fill(false);
    for (auto key : keys_) {
      if (key != KeyCode::None)
        blocked_[static_cast<int>(key)] = held(key);
    }
  }

  template <typename Frame, typename Held> KeyCode Resolve(KeyCode configured, Frame frame, Held held)
  {
    if (!IsLayoutKey(configured))
      return configured;
    const auto key = keys_[static_cast<int>(configured)];
    if (key == KeyCode::None)
      return key;
    if (transition_) {
      if (frame() == transition_frame_)
        return KeyCode::None;
      transition_ = false;
    }
    auto& blocked = blocked_[static_cast<int>(key)];
    if (blocked) {
      if (held(key))
        return KeyCode::None;
      blocked = false;
    }
    return key;
  }

private:
  LayoutKeys                                       keys_{};
  std::array<bool, static_cast<int>(KeyCode::Max)> blocked_{};
  bool                                             transition_ = false;
  int                                              transition_frame_ = -1;
};
} // namespace keyboard_layout
