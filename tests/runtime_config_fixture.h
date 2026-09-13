#pragma once
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <variant>

namespace spdlog
{
template <class... Args> void warn(const char*, Args&&...) {}
} // namespace spdlog
namespace config_edit
{
using Value = std::variant<bool, std::string, double, std::int64_t>;
enum class Outcome { Conflict, InvalidDocument, Unsupported };
// Controllable boundary; the fixture below includes the actual adapter bodies.
struct RuntimeConfigWriter {
  bool failures = false;
  bool HasFailures() const
  { return failures; }
  bool HasFailure(std::string_view, std::string_view) const
  { return failures; }
  bool     work = false, stopped = false, finished = false, cancelled = false;
  bool     block_cancel = false;
  unsigned submissions  = 0;
  HANDLE   handle       = nullptr;
  bool     HasWork() const
  { return work; }
  void RequestCancelPending()
  { cancelled = true; }
  void Stop(bool cancel)
  {
    stopped   = true;
    cancelled = cancel;
    if (cancel && block_cancel) {
      std::puts("pending cancellation requested");
      std::fflush(stdout);
      Sleep(INFINITE); // Deadline must be independent of this stalled caller.
    }
  }
  bool PollStopped() const
  { return finished; }
  void* NativeHandle() const
  { return handle; }
  unsigned Submit(const char*)
  { return stopped || failures ? 0 : ++submissions; }
  unsigned Submit(const char*, const char*, Value, std::chrono::milliseconds)
  { return Submit(""); }
};
} // namespace config_edit

// Observe actual callback registration; tests must not manually call an Update
// that the adapter failed to arrange for the no-persistence case.
inline void (*fixture_update_callback)() = nullptr;
inline bool install_screen_manager_update_hook()
{ return true; }
inline bool register_screen_manager_update_callback(void (*callback)())
{
  fixture_update_callback = callback;
  return true;
}
