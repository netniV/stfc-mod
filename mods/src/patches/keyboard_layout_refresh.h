#pragma once

#include <atomic>

namespace keyboard_layout
{
// Shared with the notification callback; all other mapping state stays on the game thread.
class RefreshState
{
public:
  void Invalidate() noexcept
  { dirty_.store(true); }

  bool Consume()
  {
    // Quiet queries do not need a read-modify-write, a frame clock, or Unity calls.
    return dirty_.load() && dirty_.exchange(false);
  }

private:
  std::atomic<bool> dirty_{true};
};
} // namespace keyboard_layout
