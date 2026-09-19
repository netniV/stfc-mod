#include "runtime_config_keys.h"
#ifdef CONFIG_RUNTIME_TEST
#include CONFIG_RUNTIME_TEST // Isolated fixture substitutes Unity/worker boundaries only.
#else
#include "patches/runtime_config.h"
#include "file.h"
#include "patches/mapkey.h"
#include "runtime_config_writer.h"
#include <spdlog/spdlog.h>

#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
#include "patches/screen_update_hook.h"
#include "patches/native_hook_extent.h"
#if _WIN32
#include <Windows.h>
#endif
#include <il2cpp/il2cpp_helper.h>
#include <il2cpp/method_contract.h>
#include <spud/detour.h>
#endif
#endif

#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)

namespace
{
// Installed hooks live until process exit. Retain this one control object rather
// than joining a worker from DLL teardown. No worker is started during Configure.
config_edit::RuntimeConfigWriter* writer    = nullptr;
bool                              available = false;
std::atomic<std::uintptr_t>       owner{0};
std::atomic_bool                  forcing{false};
std::mutex                        lifecycle;
bool                              draining = false, stopped = false, resume = false;
std::uint64_t                     vote       = 0;
thread_local unsigned             quit_depth = 0;
void (*request_quit)(int)                    = nullptr;
void (*save_status_changed)()                = nullptr;
std::atomic_bool persistence_unavailable{false};
bool             reported_save_failure = false;

// Stable identity for the lifetime of the Unity owner thread on either platform.
std::uintptr_t CurrentThreadToken()
{
  static thread_local char token;
  return reinterpret_cast<std::uintptr_t>(&token);
}

void Report(std::string_view section, std::string_view key, config_edit::Outcome result)
{
  const char* reason = "write failed";
  switch (result) {
    case config_edit::Outcome::Conflict:
      reason = "file changed externally";
      break;
    case config_edit::Outcome::InvalidDocument:
      reason = "invalid TOML";
      break;
    case config_edit::Outcome::Unsupported:
      reason = "unsupported setting representation";
      break;
    default:
      break;
  }
  spdlog::warn("Could not persist {}.{}: {}; live setting is unchanged", section, key, reason);
}

bool WantsQuit(auto original)
{
  struct Depth {
    Depth()
    { ++quit_depth; }
    ~Depth()
    { --quit_depth; }
  } depth;
  std::uint64_t this_vote;
  {
    std::lock_guard lock(lifecycle);
    this_vote = ++vote;
  }
  const bool      allows = original();
  std::lock_guard lock(lifecycle);
  if (stopped || !writer)
    return allows;
  // A later/nested game veto must not be overwritten by an older returning vote.
  if (this_vote == vote)
    resume = allows;
  if (allows) {
    writer->Stop(false);
    // Close admission before checking: Submit shares lifecycle, so no later
    // request can race an idle exit. An already-deferred quit still observes
    // native thread exit through Update before resuming.
    if (!draining && !writer->HasWork() && resume) {
      stopped = true;
      resume  = false;
      return true;
    }
    draining = true;
  }
  return false; // Resume only after observing native worker exit, even after failure.
}

void Update()
{
  std::uintptr_t unset = 0;
  owner.compare_exchange_strong(unset, CurrentThreadToken());
  if (forcing || owner != CurrentThreadToken() || quit_depth)
    return;
  const bool failed = persistence_unavailable.load() || (writer && writer->HasFailures());
  if (failed != reported_save_failure) {
    reported_save_failure = failed;
    if (save_status_changed) {
      try {
        save_status_changed();
      } catch (...) { /* Presentation cannot interrupt shutdown. */
      }
    }
  }
  bool quit = false;
  {
    std::lock_guard lock(lifecycle);
    if (!draining || stopped || !writer || !writer->PollStopped())
      return;
    stopped = true;
    quit    = resume;
    resume  = false; // Consume before Unity callbacks; never retry a genuine veto.
  }
  if (quit)
    request_quit(0);
}


#if _WIN32
DWORD WINAPI FinishForceClose(void* handle)
{
  WaitForSingleObject(handle, 500);
  CloseHandle(handle);
  TerminateProcess(GetCurrentProcess(), 1);
  return 0;
}
#endif
} // namespace
#elif _WIN32
#include <Windows.h>
#endif

namespace runtime_config
{
bool SetSaveStatusObserver(void (*observer)())
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  if (save_status_changed && save_status_changed != observer)
    return false;
  // Status must also update if persistence/quit-hook validation failed, or no
  // writer was configured. Registration uses the existing idempotent dispatcher.
  if (!observer || !install_screen_manager_update_hook() || !register_screen_manager_update_callback(Update))
    return false;
  save_status_changed = observer;
  return true;
#else
  (void)observer;
  return false;
#endif
}
bool HasSaveFailures() noexcept
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  return persistence_unavailable.load() || (writer && writer->HasFailures());
#else
  return false;
#endif
}
#ifndef CONFIG_RUNTIME_TEST
void Configure(const toml::table& loaded)
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  if (writer)
    return;
  std::optional<config_edit::Value> initial;
  if (auto value = loaded["ui"]["auto_confirm_instant_warp"].value<std::string>())
    initial = *value;
  try {
    writer = new config_edit::RuntimeConfigWriter(File::MakePath(File::Config()), initial, Report);
    for (const auto& [section, key] : config_edit::persisted_settings) {
      std::optional<config_edit::Value> value;
      auto                              node = loaded[section][key];
      if (node.is_boolean())
        value = node.value<bool>().value();
      else if (node.is_string())
        value = node.value<std::string>().value();
      else if (node.is_integer())
        value = node.value<std::int64_t>().value();
      else if (node.is_floating_point())
        value = node.value<double>().value();
      writer->Register(section, key, std::move(value));
    }
    for (int i = 0; i < GameFunction::Max; ++i) {
      const auto& definition = MapKey::Definition(static_cast<GameFunction>(i));
      if (definition.key.empty())
        continue;
      std::optional<config_edit::Value> value;
      if (auto text = loaded["shortcuts"][definition.key].value<std::string>())
        value = *text;
      writer->Register("shortcuts", definition.key, std::move(value));
    }
  } catch (...) {
    spdlog::warn("Runtime config persistence unavailable");
  }
#else
  (void)loaded;
#endif
}

void Install()
{
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
  static bool attempted = false;
  if (attempted || !writer)
    return;
  attempted = true;
  try {
    auto        helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Application");
    const auto* wants = method_contract::Resolve(helper.get_cls(), "Internal_ApplicationWantsToQuit", true,
                                                  "System.Boolean", {});
    const auto* quit = method_contract::Resolve(helper.get_cls(), "Quit", true, "System.Void", {"System.Int32"});
    if (!wants || !quit) {
      spdlog::warn("Runtime config persistence unavailable: incompatible Unity quit methods");
      return;
    }
#if __APPLE__
    if (!native_hooks::MacHookFits(method_contract::Pointer(wants))) {
      spdlog::warn("Runtime config persistence unavailable: Mac quit hook validation failed");
      return;
    }
#endif
    request_quit = reinterpret_cast<void (*)(int)>(quit->methodPointer);
    available    = install_screen_manager_update_hook() && register_screen_manager_update_callback(Update)
                   && SPUD_STATIC_DETOUR(wants->methodPointer, WantsQuit);
    spdlog::info("Runtime config persistence ready={}", available);
  } catch (...) {
    available = false;
  }
#endif
}
#endif

void SaveSetting(const char* section, const char* key, config_edit::Value value,
                 std::chrono::milliseconds delay) noexcept
{
  try {
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
    if (available && !forcing && owner == CurrentThreadToken() && !quit_depth) {
      std::lock_guard lock(lifecycle);
      if (!draining) {
        if (writer->Submit(section, key, std::move(value), delay))
          return;
        if (writer->HasFailure(section, key)) {
          spdlog::warn("{}.{} changed for this session; runtime save submission failed", section, key);
          return; // The writer owns this failure and its eventual same-key recovery.
        }
      }
    }
    persistence_unavailable.store(true);
#else
    (void)value;
#endif
    static bool reported = false;
    if (!reported) {
      reported = true;
      spdlog::warn("{}.{} changed for this session; runtime persistence unavailable", section, key);
    }
  } catch (...) { /* Persistence must not interrupt the shortcut's live effect. */
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
    persistence_unavailable.store(true);
#endif
  }
}

void SaveWarpMode(const char* mode) noexcept
{
  try {
    const std::string value(mode);
    if (value != "none" && value != "warp" && value != "jump")
      return;
    SaveSetting("ui", "auto_confirm_instant_warp", value, {});
  } catch (...) { // Keep value construction inside the shortcut's failure boundary.
#if (defined(_WIN32) && defined(_M_X64)) || defined(__APPLE__)
    persistence_unavailable.store(true);
#endif
  }
}

#if _WIN32
void ForceClose() noexcept
{
#if defined(_M_X64)
  if (writer && owner == CurrentThreadToken() && !quit_depth && writer->HasWork()) {
    forcing = true;
    writer->RequestCancelPending();
    HANDLE duplicate = nullptr;
    if (auto handle = writer->NativeHandle();
        handle
        && DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicate, SYNCHRONIZE, FALSE, 0)) {
      // Arm the independent deadline before taking any writer lock. A stalled
      // filesystem operation or Unity callback cannot prolong this best effort.
      if (auto closer = CreateThread(nullptr, 0, FinishForceClose, duplicate, 0, nullptr)) {
        CloseHandle(closer);
        writer->Stop(true);
        return;
      }
      CloseHandle(duplicate);
    }
  }
#endif
  TerminateProcess(GetCurrentProcess(), 1);
}
#endif
} // namespace runtime_config
