#include "mod_state.h"

#include "file.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#if _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace
{
std::filesystem::path state_path()
{
  if (File::hasCustomNames()) {
    std::filesystem::path config_path{File::Config()};
    auto                  state_name = config_path.stem();
    state_name += ".state.json";
    return config_path.parent_path() / state_name;
  }
  return std::filesystem::path{File::MakePath("community_patch_state.json", true)};
}

class StateFileLock
{
public:
  explicit StateFileLock(const std::filesystem::path& path, int maximum_attempts)
  {
    lock_path = path;
    lock_path += ".lock";

    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
#if _WIN32
      handle = CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
      if (handle != INVALID_HANDLE_VALUE) {
        return;
      }
      const auto error = GetLastError();
      if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION) {
        break;
      }
#else
      descriptor = open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
      if (descriptor < 0) {
        break;
      }
      if (flock(descriptor, LOCK_EX | LOCK_NB) == 0) {
        return;
      }
      const auto error = errno;
      close(descriptor);
      descriptor = -1;
      if (error != EWOULDBLOCK && error != EAGAIN) {
        break;
      }
#endif
      if (attempt + 1 < maximum_attempts) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }

  ~StateFileLock()
  {
#if _WIN32
    if (handle != INVALID_HANDLE_VALUE) {
      CloseHandle(handle);
    }
#else
    if (descriptor >= 0) {
      flock(descriptor, LOCK_UN);
      close(descriptor);
    }
#endif
  }

  StateFileLock(const StateFileLock&)            = delete;
  StateFileLock& operator=(const StateFileLock&) = delete;

  bool acquired() const
  {
#if _WIN32
    return handle != INVALID_HANDLE_VALUE;
#else
    return descriptor >= 0;
#endif
  }

private:
  std::filesystem::path lock_path;
#if _WIN32
  HANDLE handle = INVALID_HANDLE_VALUE;
#else
  int descriptor = -1;
#endif
};

std::filesystem::path temporary_state_path(const std::filesystem::path& path)
{
  static std::atomic_uint64_t sequence{0};
  auto                        temporary_path = path;
#if _WIN32
  const auto process_id = static_cast<uint64_t>(GetCurrentProcessId());
#else
  const auto process_id = static_cast<uint64_t>(getpid());
#endif
  temporary_path += "." + std::to_string(process_id) + "." + std::to_string(++sequence) + ".tmp";
  return temporary_path;
}

bool replace_state_file(const std::filesystem::path& temporary_path, const std::filesystem::path& path)
{
#if _WIN32
  if (MoveFileExW(temporary_path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
    return true;
  }
  spdlog::warn("[ModState] unable to replace state file '{}' (Windows error {})", path.string(), GetLastError());
#else
  std::error_code error;
  std::filesystem::rename(temporary_path, path, error);
  if (!error) {
    return true;
  }
  spdlog::warn("[ModState] unable to replace state file '{}': {}", path.string(), error.message());
#endif
  std::error_code cleanup_error;
  std::filesystem::remove(temporary_path, cleanup_error);
  return false;
}

constexpr int state_version = 1;

bool has_supported_state_version(const nlohmann::json& state, bool allow_missing)
{
  if (!state.is_object()) {
    return false;
  }
  const auto version = state.find("version");
  return version == state.end() ? allow_missing : version->is_number_integer() && *version == state_version;
}
} // namespace

namespace mod_state
{
std::optional<nlohmann::json> Read()
{
  const auto    path = state_path();
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file) {
    return std::nullopt;
  }

  try {
    auto state = nlohmann::json::parse(file);
    if (!has_supported_state_version(state, true)) {
      spdlog::warn("[ModState] ignored state file with unsupported shape or version '{}'", path.string());
      return std::nullopt;
    }
    return state;
  } catch (const std::exception& error) {
    spdlog::warn("[ModState] ignored invalid state file '{}': {}", path.string(), error.what());
    return std::nullopt;
  }
}

static bool update_state(const std::function<void(nlohmann::json&)>& update, int lock_attempts)
{
  const auto    path = state_path();
  StateFileLock lock(path, lock_attempts);
  if (!lock.acquired()) {
    spdlog::warn("[ModState] unable to lock state file '{}'", path.string());
    return false;
  }

  nlohmann::json state = nlohmann::json::object();
  try {
    std::ifstream existing(path, std::ios::in | std::ios::binary);
    if (existing) {
      state = nlohmann::json::parse(existing);
      if (!state.is_object()) {
        state = nlohmann::json::object();
      }
    }
  } catch (const std::exception& error) {
    spdlog::warn("[ModState] replacing invalid state file '{}': {}", path.string(), error.what());
    state = nlohmann::json::object();
  }

  if (!has_supported_state_version(state, true)) {
    spdlog::warn("[ModState] refusing to update unsupported state version in '{}'", path.string());
    return false;
  }

  try {
    if (!state.contains("version")) {
      state["version"] = state_version;
    }
    update(state);
    if (!has_supported_state_version(state, false)) {
      spdlog::warn("[ModState] state update changed the reserved version field");
      return false;
    }
  } catch (const std::exception& error) {
    spdlog::warn("[ModState] state update failed: {}", error.what());
    return false;
  }

  std::string serialized_state;
  try {
    serialized_state = state.dump(2);
  } catch (const std::exception& error) {
    spdlog::warn("[ModState] state serialization failed: {}", error.what());
    return false;
  }

  const auto    temporary_path = temporary_state_path(path);
  std::ofstream file(temporary_path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!file) {
    spdlog::warn("[ModState] unable to open temporary state file '{}'", temporary_path.string());
    return false;
  }
  file << serialized_state << '\n';
  file.flush();
  const bool write_succeeded = file.good();
  file.close();
  if (!write_succeeded || file.fail()) {
    spdlog::warn("[ModState] unable to finish temporary state file '{}'", temporary_path.string());
    std::error_code cleanup_error;
    std::filesystem::remove(temporary_path, cleanup_error);
    return false;
  }
  return replace_state_file(temporary_path, path);
}

bool Update(const std::function<void(nlohmann::json&)>& update)
{ return update_state(update, 50); }

bool TryUpdate(const std::function<void(nlohmann::json&)>& update)
{ return update_state(update, 1); }
} // namespace mod_state
