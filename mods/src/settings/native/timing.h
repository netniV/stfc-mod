#pragma once

// Opt-in measurement at existing settings boundaries. No new hook or frame
// callback, and no logging while interacting: aggregate when leaving a page.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <string_view>

namespace mod_settings::native::timing
{
enum class Operation { BuildTree, ShowPage, RefreshActions, Count };
#ifdef _MODDBG
inline bool Enabled()
{
  static const bool enabled = [] {
    const auto* value = std::getenv("STFC_MOD_SETTINGS_TIMING");
    return value && std::string_view(value) == "1";
  }();
  return enabled;
}
struct Measurement {
  std::size_t count = 0;
  double      total = 0, maximum = 0;
};
inline std::array<Measurement, static_cast<std::size_t>(Operation::Count)> measurements;
class Scope
{
public:
  explicit Scope(Operation operation)
      : operation_(operation)
  {
    if (Enabled())
      start_ = std::chrono::steady_clock::now();
  }
  ~Scope()
  {
    if (!Enabled())
      return;
    const auto ms     = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_).count();
    auto&      result = measurements[static_cast<std::size_t>(operation_)];
    ++result.count;
    result.total += ms;
    result.maximum = std::max(result.maximum, ms);
  }

private:
  Operation                             operation_;
  std::chrono::steady_clock::time_point start_;
};
inline void Flush() noexcept
{
  if (!Enabled())
    return;
  constexpr std::array names{"build-tree", "show-page", "refresh-actions"};
  for (std::size_t i = 0; i < measurements.size(); ++i) {
    auto& result = measurements[i];
    if (!result.count)
      continue;
    try {
      spdlog::info("[SettingsTiming] {} count={} mean_ms={:.3f} max_ms={:.3f}", names[i], result.count,
                   result.total / result.count, result.maximum);
    } catch (...) {
    }
    result = {};
  }
}
#else
struct Scope {
  explicit Scope(Operation) {}
};
inline void Flush() noexcept {}
#endif
} // namespace mod_settings::native::timing
