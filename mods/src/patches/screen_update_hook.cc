#include "patches/screen_update_hook.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <exception>
#include <vector>

namespace
{
std::vector<ScreenManagerUpdateCallback> s_callbacks;
bool                                     s_dispatching = false;
} // namespace

bool register_screen_manager_update_callback(ScreenManagerUpdateCallback callback)
{
  if (!callback) {
    return false;
  }
  if (s_dispatching) {
    spdlog::warn("[ScreenUpdate] rejected callback registration during dispatch");
    return false;
  }
  if (std::find(s_callbacks.begin(), s_callbacks.end(), callback) != s_callbacks.end()) {
    return true;
  }
  s_callbacks.push_back(callback);
  return true;
}

void dispatch_screen_manager_update_callbacks()
{
  if (s_dispatching) {
    spdlog::warn("[ScreenUpdate] suppressed recursive callback dispatch");
    return;
  }
  s_dispatching = true;
  for (const auto callback : s_callbacks) {
    try {
      callback();
    } catch (const std::exception& error) {
      spdlog::warn("[ScreenUpdate] callback failed: {}", error.what());
    } catch (...) {
      spdlog::warn("[ScreenUpdate] callback failed with an unknown exception");
    }
  }
  s_dispatching = false;
}
