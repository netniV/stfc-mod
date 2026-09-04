#include "patches/screen_update_hook.h"

#include <algorithm>
#include <vector>

namespace
{
std::vector<ScreenManagerUpdateCallback> s_callbacks;
}

bool register_screen_manager_update_callback(ScreenManagerUpdateCallback callback)
{
  if (!callback) {
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
  for (const auto callback : s_callbacks) {
    callback();
  }
}
