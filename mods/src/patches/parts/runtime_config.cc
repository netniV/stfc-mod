#ifdef CONFIG_RUNTIME_TEST
#include CONFIG_RUNTIME_TEST // Isolated fixture substitutes Unity/worker boundaries only.
#else
#include "patches/runtime_config.h"
#include "file.h"
#include "patches/mapkey.h"
#include "runtime_config_writer.h"
#include <spdlog/spdlog.h>

#if defined(_WIN32) && defined(_M_X64)
#include "patches/screen_update_hook.h"
#include <Windows.h>
#include <cstring>
#include <il2cpp/il2cpp_helper.h>
#include <spud/detour.h>
#endif
#endif

#if defined(_WIN32) && defined(_M_X64)

namespace
{
// Installed hooks live until process exit. Retain this one control object rather
// than joining a worker from DLL teardown. No worker is started during Configure.
config_edit::RuntimeConfigWriter* writer    = nullptr;
bool                              available = false;
std::atomic<DWORD>                owner{0};
std::atomic_bool                  forcing{false};
std::mutex                        lifecycle;
bool                              draining = false, stopped = false, resume = false;
std::uint64_t                     vote       = 0;
thread_local unsigned             quit_depth = 0;
void (*request_quit)(int)                    = nullptr;
void (*save_status_changed)()                = nullptr;
std::atomic_bool persistence_unavailable{false};
bool             reported_save_failure = false;

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
  DWORD unset = 0;
  owner.compare_exchange_strong(unset, GetCurrentThreadId());
  if (forcing || owner != GetCurrentThreadId() || quit_depth)
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

#ifndef CONFIG_RUNTIME_TEST
bool MatchesQuitMethod(const MethodInfo* method)
{
  // Verified build261 Windows x64: 411-byte native body vs SPUD's 24-byte
  // overwrite. Pin complete initial instructions too; other builds stay session-only.
  constexpr unsigned char bytes[]{0x48, 0x89, 0x5c, 0x24, 0x08, 0x56, 0x57, 0x41, 0x56, 0x48,
                                  0x83, 0xec, 0x40, 0x80, 0x3d, 0xad, 0xd2, 0x8d, 0x01, 0x00,
                                  0x75, 0x29, 0x48, 0x8d, 0x0d, 0x9b, 0xdf, 0x63, 0x01};
  auto                    base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"GameAssembly.dll"));
  if (!base || !method || reinterpret_cast<std::uintptr_t>(method->methodPointer) != base + 0x43548c0)
    return false;
  DWORD64     image_base = 0;
  const auto* extent     = RtlLookupFunctionEntry(base + 0x43548c0, &image_base, nullptr);
  return extent && image_base == base && extent->BeginAddress == 0x43548c0 && extent->EndAddress == 0x4354a5b
         && std::memcmp(method->methodPointer, bytes, sizeof(bytes)) == 0;
}
#endif

DWORD WINAPI FinishForceClose(void* handle)
{
  WaitForSingleObject(handle, 500);
  CloseHandle(handle);
  TerminateProcess(GetCurrentProcess(), 1);
  return 0;
}
} // namespace
#elif _WIN32
#include <Windows.h>
#endif

namespace runtime_config
{
bool SetSaveStatusObserver(void (*observer)())
{
#if defined(_WIN32) && defined(_M_X64)
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
#if defined(_WIN32) && defined(_M_X64)
  return persistence_unavailable.load() || (writer && writer->HasFailures());
#else
  return false;
#endif
}
#ifndef CONFIG_RUNTIME_TEST
void Configure(const toml::table& loaded)
{
#if defined(_WIN32) && defined(_M_X64)
  if (writer)
    return;
  std::optional<config_edit::Value> initial;
  if (auto value = loaded["ui"]["auto_confirm_instant_warp"].value<std::string>())
    initial = *value;
  try {
    writer = new config_edit::RuntimeConfigWriter(File::MakePath(File::Config()), initial, Report);
    for (const auto& [section, key] : {std::pair{"graphics", "zoom_label_player_detail"},
                                       {"graphics", "zoom_label_non_player_detail"},
                                       {"graphics", "zoom_label_player_threshold"},
                                       {"graphics", "zoom_label_non_player_threshold"},
                                       {"graphics", "keyboard_zoom_speed"},
                                       {"graphics", "system_pan_momentum_falloff"},
                                       {"ui", "auto_confirm_ft_upgrade"},
                                       {"ui", "disable_preview_locate"},
                                       {"ui", "disable_preview_recall"},
                                       {"ui", "show_cargo_default"},
                                       {"ui", "show_player_cargo"},
                                       {"ui", "show_station_cargo"},
                                       {"ui", "show_hostile_cargo"},
                                       {"ui", "show_armada_cargo"}}) {
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
#if defined(_WIN32) && defined(_M_X64)
  static bool attempted = false;
  if (attempted || !writer)
    return;
  attempted = true;
  try {
    auto        helper = il2cpp_get_class_helper("UnityEngine.CoreModule", "UnityEngine", "Application");
    const auto* wants  = helper.GetMethodInfo("Internal_ApplicationWantsToQuit", 0);
    const auto* quit   = helper.GetMethodInfo("Quit", 1);
    auto        base   = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"GameAssembly.dll"));
    if (!MatchesQuitMethod(wants) || !quit || reinterpret_cast<std::uintptr_t>(quit->methodPointer) != base + 0x4351c00)
      return;
    request_quit = reinterpret_cast<void (*)(int)>(quit->methodPointer);
    available    = install_screen_manager_update_hook() && register_screen_manager_update_callback(Update)
                   && SPUD_STATIC_DETOUR(wants->methodPointer, WantsQuit);
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
#if defined(_WIN32) && defined(_M_X64)
    if (available && !forcing && owner == GetCurrentThreadId() && !quit_depth) {
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
#if defined(_WIN32) && defined(_M_X64)
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
#if defined(_WIN32) && defined(_M_X64)
    persistence_unavailable.store(true);
#endif
  }
}

#if _WIN32
void ForceClose() noexcept
{
#if defined(_M_X64)
  if (writer && owner == GetCurrentThreadId() && !quit_depth && writer->HasWork()) {
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
