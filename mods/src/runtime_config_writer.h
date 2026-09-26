#pragma once

#include "toml_editor.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string_view>
#include <thread>

namespace config_edit
{
// One owner for the configured file and all its registered runtime settings.
// Register additional keys here rather than creating another writer for the file.
class RuntimeConfigWriter
{
public:
  using Reporter = void (*)(std::string_view, std::string_view, Outcome);
  struct Completion {
    std::uint64_t revision = 0;
    Outcome       outcome  = Outcome::AlreadySaved;
  };
  RuntimeConfigWriter(std::filesystem::path path, std::optional<Value> initial, Reporter report = nullptr);
  ~RuntimeConfigWriter(); // Tests/explicit owners only; game adapter has process lifetime.
  std::uint64_t Submit(std::string mode);
  // Startup registration only. Runtime submissions may update only known keys.
  bool          Register(std::string section, std::string key, std::optional<Value> initial);
  std::uint64_t Submit(std::string section, std::string key, Value desired, std::chrono::milliseconds delay = {});
  void          Stop(bool cancel_pending);
  // Publish force-close cancellation before native deadline setup, without a lock.
  void       RequestCancelPending();
  Completion LastCompletion();
  bool       HasWork() const
  { return has_work_.load(); }
  bool HasFailures() const
  { return has_failures_.load(); }
  // Failure-path query: lets a caller distinguish a tracked failed attempt from
  // an untracked rejection. A same-key retry can clear the former normally.
  bool HasFailure(std::string_view section, std::string_view key);
  // Owner thread only, like Submit. On Windows this observes native thread exit
  // before joining; it never joins a still-running worker on a game callback.
  bool PollStopped();
#if _WIN32
  void* NativeHandle(); // Owner thread only; caller must duplicate before retaining.
#endif
private:
  struct Pending {
    std::uint64_t                         revision;
    Request                               edit;
    std::chrono::steady_clock::time_point ready;
  };
  void                  Run();
  std::filesystem::path path_;
  using Key = std::pair<std::string, std::string>;
  struct Saved {
    std::optional<Value> value;
    bool                 failed = false;
  };
  std::map<Key, Saved>    saved_;
  Reporter                report_;
  TomlEditor              editor_;
  std::mutex              mutex_;
  std::condition_variable wake_;
  std::thread             worker_;
  std::map<Key, Pending>  pending_;
  Completion              completion_;
  std::uint64_t           revision_ = 0;
  bool                    stopping_ = false;
  std::atomic_bool        has_work_{false}, finished_{false}, cancel_pending_{false};
  std::atomic_bool        has_failures_{false};
};
} // namespace config_edit
