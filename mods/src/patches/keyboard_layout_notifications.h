#pragma once

#include "keyboard_layout_refresh.h"
#include <cstdint>

namespace keyboard_layout::notifications
{
// Game-thread calls only. The observer and refresh state must live until process exit.
enum class Result { Started, Unsupported, Failed };
// Unsupported platforms or subscription failure disable layout mode; never poll.
Result Start(RefreshState& refresh);
void Stop();
} // namespace keyboard_layout::notifications
