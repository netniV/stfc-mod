#pragma once

#include <patches/gamefunctions.h>

#include <span>

enum class DispatchDecision {
  NoMatch,
  HandledStop,
  HandledAllowOriginal
};

enum class InputMode {
  Down,
  Pressed
};

struct HotkeyEntry {
  GameFunction     game_function;
  DispatchDecision (*handler)();
  InputMode        input_mode = InputMode::Down;
};

std::span<const HotkeyEntry> GetHotkeyDispatchTable();